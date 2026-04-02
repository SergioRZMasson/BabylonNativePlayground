using System;
using System.Threading;
using System.Threading.Tasks;
using LivePreview.ObjectMapping;
using LivePreview.Protocol;
using Newtonsoft.Json.Linq;

namespace LivePreview.Core
{
    public abstract class LivePreviewSession
    {
        private readonly int _port;
        private readonly string _rendererPath;
        private WebSocketServer _server;
        private RendererProcess _renderer;

        public SessionState State { get; private set; } = SessionState.Disconnected;
        public ObjectMap Map { get; } = new ObjectMap();
        public ProtocolClient Client { get; private set; }
        public ChangeThrottler Throttler { get; } = new ChangeThrottler();

        public event Action<SessionState> OnStateChanged;
        public event Action<string> OnError;
        public event Action OnRendererConnected;

        protected LivePreviewSession(int port, string rendererPath)
        {
            _port = port;
            _rendererPath = rendererPath;
        }

        protected abstract Task<byte[]> ExportSceneToGlbAsync();
        protected abstract void PopulateObjectMap(ObjectMap map);

        public void StartServerAndLaunchRenderer()
        {
            try
            {
                SetState(SessionState.StartingServer);
                _server = new WebSocketServer();
                Client = new ProtocolClient(_server);

                _server.OnConnected += HandleConnected;
                _server.OnDisconnected += HandleDisconnect;
                _server.Start(_port);

                SetState(SessionState.WaitingForRenderer);
                _renderer = new RendererProcess(_rendererPath);
                _renderer.Start($"ws://127.0.0.1:{_port}");
            }
            catch (Exception ex)
            {
                SetState(SessionState.Error);
                OnError?.Invoke($"Failed to start: {ex.Message}");
            }
        }

        public async Task ExportAndSendSceneAsync()
        {
            try
            {
                SetState(SessionState.Exporting);

                var glbBytes = await ExportSceneToGlbAsync().ConfigureAwait(false);
                var base64 = Convert.ToBase64String(glbBytes);

                await Client.ClearSceneAsync().ConfigureAwait(false);
                await Client.LoadGlbDataAsync(base64, "scene.glb").ConfigureAwait(false);
                await Client.EnsureDefaultLightsAsync().ConfigureAwait(false);

                var sceneData = await Client.QuerySceneAsync().ConfigureAwait(false);

                Map.Clear();
                PopulateObjectMap(Map);
                MappingReconciler.Reconcile(Map, sceneData, msg => OnError?.Invoke(msg));

                SetState(SessionState.Streaming);
            }
            catch (Exception ex)
            {
                SetState(SessionState.Error);
                OnError?.Invoke($"Export/load failed: {ex.Message}");
            }
        }

        public void Stop()
        {
            SetState(SessionState.Stopping);

            try { _server?.Stop(); } catch { }
            try { _renderer?.Stop(); } catch { }

            _server = null;
            _renderer = null;
            Client = null;
            Map.Clear();
            Throttler.Reset();

            SetState(SessionState.Disconnected);
        }

        private void HandleConnected()
        {
            SetState(SessionState.Handshaking);
            OnRendererConnected?.Invoke();
        }

        private async void HandleDisconnect()
        {
            if (State == SessionState.Stopping || State == SessionState.Disconnected)
                return;

            SetState(SessionState.Reconnecting);

            for (int attempt = 0; attempt < 10; attempt++)
            {
                await Task.Delay(3000).ConfigureAwait(false);

                if (_renderer != null && !_renderer.IsRunning)
                {
                    try
                    {
                        _renderer.Start($"ws://127.0.0.1:{_port}");
                    }
                    catch (Exception ex)
                    {
                        OnError?.Invoke($"Failed to restart renderer: {ex.Message}");
                        continue;
                    }
                }

                if (_server != null && _server.IsConnected)
                {
                    await ExportAndSendSceneAsync().ConfigureAwait(false);
                    return;
                }
            }

            SetState(SessionState.Error);
            OnError?.Invoke("Failed to reconnect after 10 attempts");
        }

        private void SetState(SessionState state)
        {
            State = state;
            OnStateChanged?.Invoke(state);
        }
    }
}
