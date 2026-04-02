using System;
using Newtonsoft.Json.Linq;

namespace LivePreview.ObjectMapping
{
    public static class MappingReconciler
    {
        public static void Reconcile(ObjectMap map, JObject querySceneData, Action<string> logger = null)
        {
            if (querySceneData == null) return;

            ReconcileArray(map, querySceneData["meshes"] as JArray, true, logger);
            ReconcileArray(map, querySceneData["transformNodes"] as JArray, false, logger);
            ReconcileArray(map, querySceneData["lights"] as JArray, false, logger);
            ReconcileArray(map, querySceneData["cameras"] as JArray, false, logger);
        }

        private static void ReconcileArray(ObjectMap map, JArray nodes, bool updateTopology, Action<string> logger)
        {
            if (nodes == null) return;

            for (int i = 0; i < nodes.Count; i++)
            {
                var node = nodes[i] as JObject;
                if (node == null) continue;

                var name = node["name"]?.ToString();
                var uniqueId = node["uniqueId"]?.Value<int>() ?? -1;

                if (string.IsNullOrEmpty(name)) continue;

                var entry = map.LookupByBabylonName(name);
                if (entry != null)
                {
                    entry.BabylonUniqueId = uniqueId;

                    if (updateTopology)
                    {
                        var verts = node["vertices"]?.Value<int>() ?? 0;
                        var indices = node["indices"]?.Value<int>() ?? 0;
                        entry.LastVertexCount = verts;
                        entry.LastIndexCount = indices;
                    }
                }
            }
        }
    }
}
