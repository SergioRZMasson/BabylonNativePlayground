using System;
using System.Collections.Concurrent;
using System.IO;
using System.Net;
using System.Net.WebSockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Newtonsoft.Json.Linq;

namespace LivePreview.Protocol
{
    public class WebSocketServer
    {
        private HttpListener _listener;
        private WebSocket _webSocket;
        private CancellationTokenSource _cts;
        private readonly ConcurrentDictionary<string, TaskCompletionSource<string>> _pending = new ConcurrentDictionary<string, TaskCompletionSource<string>>();
        private readonly SemaphoreSlim _sendLock = new SemaphoreSlim(1, 1);
        private int _nextId;

        public bool IsConnected => _webSocket != null && _webSocket.State == WebSocketState.Open;
        public event Action OnConnected;
        public event Action OnDisconnected;
        public event Action<string> OnMessageReceived;

        public void Start(int port)
        {
            _cts = new CancellationTokenSource();
            _listener = new HttpListener();
            _listener.Prefixes.Add($"http://127.0.0.1:{port}/");
            _listener.Start();
            Task.Run(() => ListenLoop(_cts.Token));
        }

        public void Stop()
        {
            _cts?.Cancel();

            if (_webSocket != null && _webSocket.State == WebSocketState.Open)
            {
                try { _webSocket.CloseAsync(WebSocketCloseStatus.NormalClosure, "Server stopping", CancellationToken.None).Wait(2000); }
                catch { /* best effort */ }
            }
            _webSocket = null;

            try { _listener?.Stop(); }
            catch { /* best effort */ }
            _listener = null;

            foreach (var kv in _pending)
            {
                kv.Value.TrySetCanceled();
            }
            _pending.Clear();
        }

        public async Task<string> SendCommandAsync(string jsonCommand, int timeoutMs = ProtocolConstants.DefaultTimeoutMs)
        {
            if (!IsConnected)
                throw new InvalidOperationException("No renderer connected");

            var parsed = JObject.Parse(jsonCommand);
            var id = parsed["id"]?.ToString();
            if (string.IsNullOrEmpty(id))
            {
                id = GenerateId();
                parsed["id"] = id;
                jsonCommand = parsed.ToString(Newtonsoft.Json.Formatting.None);
            }

            var tcs = new TaskCompletionSource<string>();
            _pending[id] = tcs;

            var bytes = Encoding.UTF8.GetBytes(jsonCommand);
            await _sendLock.WaitAsync().ConfigureAwait(false);
            try
            {
                await _webSocket.SendAsync(new ArraySegment<byte>(bytes), WebSocketMessageType.Text, true, _cts.Token).ConfigureAwait(false);
            }
            finally
            {
                _sendLock.Release();
            }

            using (var timeout = new CancellationTokenSource(timeoutMs))
            using (timeout.Token.Register(() => tcs.TrySetException(new TimeoutException($"Command timed out after {timeoutMs}ms"))))
            {
                try
                {
                    return await tcs.Task.ConfigureAwait(false);
                }
                finally
                {
                    _pending.TryRemove(id, out _);
                }
            }
        }

        public string GenerateId()
        {
            return Interlocked.Increment(ref _nextId).ToString();
        }

        private async Task ListenLoop(CancellationToken ct)
        {
            while (!ct.IsCancellationRequested)
            {
                try
                {
                    var context = await _listener.GetContextAsync().ConfigureAwait(false);
                    if (!context.Request.IsWebSocketRequest)
                    {
                        context.Response.StatusCode = 400;
                        context.Response.Close();
                        continue;
                    }

                    if (IsConnected)
                    {
                        context.Response.StatusCode = 409;
                        context.Response.Close();
                        continue;
                    }

                    var wsContext = await context.AcceptWebSocketAsync("babylon-preview").ConfigureAwait(false);
                    _webSocket = wsContext.WebSocket;
                    OnConnected?.Invoke();

                    await ReceiveLoop(ct).ConfigureAwait(false);
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                catch (ObjectDisposedException)
                {
                    break;
                }
                catch (HttpListenerException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine($"[WebSocketServer] Listen error: {ex.Message}");
                }
            }
        }

        private async Task ReceiveLoop(CancellationToken ct)
        {
            var buffer = new byte[64 * 1024];
            try
            {
                while (_webSocket.State == WebSocketState.Open && !ct.IsCancellationRequested)
                {
                    using (var ms = new MemoryStream())
                    {
                        WebSocketReceiveResult result;
                        do
                        {
                            result = await _webSocket.ReceiveAsync(new ArraySegment<byte>(buffer), ct).ConfigureAwait(false);
                            if (result.MessageType == WebSocketMessageType.Close)
                            {
                                HandleDisconnect();
                                return;
                            }
                            ms.Write(buffer, 0, result.Count);
                        } while (!result.EndOfMessage);

                        var json = Encoding.UTF8.GetString(ms.ToArray());
                        HandleMessage(json);
                    }
                }
            }
            catch (OperationCanceledException) { }
            catch (WebSocketException) { }
            finally
            {
                HandleDisconnect();
            }
        }

        private void HandleMessage(string json)
        {
            try
            {
                var obj = JObject.Parse(json);
                var id = obj["id"]?.ToString();
                if (id != null && _pending.TryRemove(id, out var tcs))
                {
                    tcs.TrySetResult(json);
                }
                else
                {
                    OnMessageReceived?.Invoke(json);
                }
            }
            catch
            {
                OnMessageReceived?.Invoke(json);
            }
        }

        private void HandleDisconnect()
        {
            _webSocket = null;
            foreach (var kv in _pending)
            {
                kv.Value.TrySetException(new InvalidOperationException("Renderer disconnected"));
            }
            _pending.Clear();
            OnDisconnected?.Invoke();
        }
    }
}
