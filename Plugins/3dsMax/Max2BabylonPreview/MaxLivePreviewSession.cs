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

        public MaxLivePreviewSession(int port, string rendererPath)
            : base(port, rendererPath)
        {
        }

        protected override Task<byte[]> ExportSceneToGlbAsync()
        {
            // Max scene access runs on the calling thread context.
            // The MaxScript exportFile command handles threading internally.
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
    }
}
