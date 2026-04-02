using System;
using System.IO;
using System.Collections.Generic;
using Autodesk.Max;

namespace Max2BabylonPreview
{
    public class MaxSceneExporter
    {
        private Dictionary<string, string> _nodeNameMapping;

        public byte[] ExportToGlb()
        {
            var tempPath = Path.Combine(Path.GetTempPath(), $"babylon_preview_{Guid.NewGuid():N}.glb");
            _nodeNameMapping = new Dictionary<string, string>();

            try
            {
                BuildNodeNameMapping(Loader.Core.RootNode);

                var escapedPath = tempPath.Replace("\\", "\\\\");
                var script = $"exportFile \"{escapedPath}\" #noPrompt using:GLTF2Exp";
                ManagedServices.MaxscriptSDK.ExecuteMaxscriptCommand(script,
                    ManagedServices.MaxscriptSDK.ScriptSource.NotSpecified);

                if (!File.Exists(tempPath))
                    throw new InvalidOperationException("GLB export produced no output file. Ensure the glTF2 exporter plugin is loaded in Max.");

                return File.ReadAllBytes(tempPath);
            }
            finally
            {
                try { if (File.Exists(tempPath)) File.Delete(tempPath); }
                catch { /* best effort cleanup */ }
            }
        }

        public Dictionary<string, string> GetNodeNameMapping()
        {
            return _nodeNameMapping ?? new Dictionary<string, string>();
        }

        private void BuildNodeNameMapping(IINode node)
        {
            if (node == null) return;

            var name = node.Name;
            if (!string.IsNullOrEmpty(name) && name != "Scene Root")
            {
                var handle = node.Handle.ToString();
                _nodeNameMapping[handle] = name;
            }

            for (int i = 0; i < node.NumberOfChildren; i++)
            {
                BuildNodeNameMapping(node.GetChildNode(i));
            }
        }
    }
}
