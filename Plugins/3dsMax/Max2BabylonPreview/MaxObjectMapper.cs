using System.Collections.Generic;
using LivePreview.ObjectMapping;

namespace Max2BabylonPreview
{
    public static class MaxObjectMapper
    {
        public static void PopulateMap(ObjectMap map, Dictionary<string, string> nodeNameMapping)
        {
            if (nodeNameMapping == null) return;

            foreach (var kvp in nodeNameMapping)
            {
                map.AddEntry(kvp.Key, kvp.Value, NodeType.Mesh);
            }
        }
    }
}
