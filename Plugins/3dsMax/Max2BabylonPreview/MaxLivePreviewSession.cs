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
            _changeDetector.OnPositionChanged += HandlePositionChanged;
            _changeDetector.OnRotationChanged += HandleRotationChanged;
            _changeDetector.OnScaleChanged += HandleScaleChanged;
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

        private void HandlePositionChanged(string dccId, double[] position)
        {
            if (!Throttler.ShouldSend(dccId + "_pos")) return;
            var babylonName = Map.GetBabylonName(dccId);
            if (babylonName == null) return;
            _ = SendTransformSafe(babylonName, "position", position);
        }

        private void HandleRotationChanged(string dccId, double[] rotation)
        {
            if (!Throttler.ShouldSend(dccId + "_rot")) return;
            var babylonName = Map.GetBabylonName(dccId);
            if (babylonName == null) return;
            _ = SendTransformSafe(babylonName, "rotation", rotation);
        }

        private void HandleScaleChanged(string dccId, double[] scale)
        {
            if (!Throttler.ShouldSend(dccId + "_scl")) return;
            var babylonName = Map.GetBabylonName(dccId);
            if (babylonName == null) return;
            _ = SendTransformSafe(babylonName, "scaling", scale);
        }

        private async Task SendTransformSafe(string target, string property, double[] value)
        {
            try
            {
                if (Client != null)
                    await Client.SetTransformAsync(target, property, value);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine(
                    $"[MaxLivePreview] Transform send failed: {ex.Message}");
            }
        }
    }
}
