using System;
using System.IO;
using System.Reflection;
using System.Threading.Tasks;
using LivePreview.Core;
using LivePreview.ObjectMapping;

namespace Max2BabylonPreview
{
    public class MaxLivePreviewSession : LivePreviewSession
    {
        private MaxSceneExporter _lastExporter;
        private MaxChangeDetector _changeDetector;

        public MaxLivePreviewSession(int port, string rendererPath)
            : base(port, rendererPath)
        {
            OnStateChanged += HandleStateChange;
        }

        protected override Task<byte[]> ExportSceneToGlbAsync()
        {
            try
            {
                var exporter = new MaxSceneExporter();
                var bytes = exporter.ExportToGlb();
                _lastExporter = exporter;
                return Task.FromResult(bytes);
            }
            catch (Exception ex)
            {
                return Task.FromException<byte[]>(ex);
            }
        }

        protected override void PopulateObjectMap(ObjectMap map)
        {
            if (_lastExporter != null)
            {
                MaxObjectMapper.PopulateMap(map, _lastExporter.GetNodeNameMapping());
            }
        }

        public static string GetDefaultRendererPath()
        {
            var pluginDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            return Path.Combine(pluginDir, "BabylonRenderer", "BabylonNativePlayground.exe");
        }

        private void HandleStateChange(SessionState state)
        {
            if (state == SessionState.Streaming)
            {
                StartChangeDetection();
            }
            else if (state == SessionState.Stopping || state == SessionState.Disconnected)
            {
                StopChangeDetection();
            }
        }

        private void StartChangeDetection()
        {
            StopChangeDetection();

            _changeDetector = new MaxChangeDetector();
            _changeDetector.OnWorldMatrixChanged += HandleWorldMatrixChanged;
            _changeDetector.Start(Map);
        }

        private void StopChangeDetection()
        {
            if (_changeDetector != null)
            {
                _changeDetector.Stop();
                _changeDetector = null;
            }
        }

        private void HandleWorldMatrixChanged(string dccId, double[] matrix)
        {
            if (!Throttler.ShouldSend(dccId)) return;
            var babylonName = Map.GetBabylonName(dccId);
            if (babylonName == null) return;
            _ = SendWorldMatrixSafe(babylonName, matrix);
        }

        private async Task SendWorldMatrixSafe(string target, double[] matrix)
        {
            try
            {
                if (Client != null)
                {
                    var p = new Newtonsoft.Json.Linq.JObject
                    {
                        ["target"] = target,
                        ["matrix"] = new Newtonsoft.Json.Linq.JArray(matrix)
                    };
                    await Client.SendCommandAsync("set_world_matrix", p);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine(
                    $"[MaxLivePreview] World matrix send failed: {ex.Message}");
            }
        }
    }
}
