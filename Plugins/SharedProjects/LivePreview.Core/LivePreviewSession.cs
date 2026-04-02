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
        private TaskCompletionSource<bool> _connectionTcs;

        public SessionState State { get; private set; } = SessionState.Disconnected;
        public ObjectMap Map { get; } = new ObjectMap();
        public ProtocolClient Client { get; private set; }
        public ChangeThrottler Throttler { get; } = new ChangeThrottler();

        public event Action<SessionState> OnStateChanged;
        public event Action<string> OnError;

        protected LivePreviewSession(int port, string rendererPath)
        {
            _port = port;
            _rendererPath = rendererPath;
        }

        protected abstract Task<byte[]> ExportSceneToGlbAsync();
        protected abstract void PopulateObjectMap(ObjectMap map);

        public async Task StartAsync()
        {
            try
            {
                SetState(SessionState.StartingServer);
                _server = new WebSocketServer();
                Client = new ProtocolClient(_server);
                _connectionTcs = new TaskCompletionSource<bool>();

                _server.OnConnected += () => _connectionTcs.TrySetResult(true);
                _server.OnDisconnected += HandleDisconnect;
                _server.Start(_port);

                SetState(SessionState.WaitingForRenderer);
                _renderer = new RendererProcess(_rendererPath);
                _renderer.Start($"ws://127.0.0.1:{_port}");

                var connected = await WaitWithTimeout(_connectionTcs.Task, 10000, "Renderer did not connect within 10 seconds").ConfigureAwait(false);

                SetState(SessionState.Handshaking);
                // The renderer sends a handshake automatically on connect (Task 1.2).
                // We give it a brief moment then proceed.
                await Task.Delay(500).ConfigureAwait(false);

                await ExportAndLoadScene().ConfigureAwait(false);

                SetState(SessionState.Streaming);
            }
            catch (Exception ex)
            {
                SetState(SessionState.Error);
                OnError?.Invoke(ex.Message);
                throw;
            }
        }

        public async Task StopAsync()
        {
            SetState(SessionState.Stopping);

            _server?.Stop();
            _renderer?.Stop();

            _server = null;
            _renderer = null;
            Client = null;
            Map.Clear();
            Throttler.Reset();

            SetState(SessionState.Disconnected);
        }

        private async Task ExportAndLoadScene()
        {
            SetState(SessionState.Exporting);

            var glbBytes = await ExportSceneToGlbAsync().ConfigureAwait(false);
            var base64 = Convert.ToBase64String(glbBytes);

            await Client.ClearSceneAsync().ConfigureAwait(false);
            await Client.LoadGlbDataAsync(base64, "scene.glb").ConfigureAwait(false);

            var sceneData = await Client.QuerySceneAsync().ConfigureAwait(false);

            Map.Clear();
            PopulateObjectMap(Map);
            MappingReconciler.Reconcile(Map, sceneData, msg => OnError?.Invoke(msg));
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
                    try
                    {
                        await ExportAndLoadScene().ConfigureAwait(false);
                        SetState(SessionState.Streaming);
                        return;
                    }
                    catch (Exception ex)
                    {
                        OnError?.Invoke($"Re-sync failed: {ex.Message}");
                    }
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

        private static async Task<T> WaitWithTimeout<T>(Task<T> task, int timeoutMs, string message)
        {
            using (var cts = new CancellationTokenSource(timeoutMs))
            {
                var delayTask = Task.Delay(timeoutMs, cts.Token);
                var completed = await Task.WhenAny(task, delayTask).ConfigureAwait(false);
                if (completed == delayTask)
                    throw new TimeoutException(message);
                cts.Cancel();
                return await task.ConfigureAwait(false);
            }
        }
    }
}
