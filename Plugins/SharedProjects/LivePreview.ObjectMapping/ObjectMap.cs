using System.Collections.Generic;

namespace LivePreview.ObjectMapping
{
    public enum NodeType
    {
        Mesh,
        Light,
        Camera,
        TransformNode,
        Material
    }

    public class MappingEntry
    {
        public string DccId { get; set; }
        public string BabylonName { get; set; }
        public int BabylonUniqueId { get; set; } = -1;
        public NodeType NodeType { get; set; }
        public int LastVertexCount { get; set; }
        public int LastIndexCount { get; set; }
    }

    public class ObjectMap
    {
        private readonly Dictionary<string, MappingEntry> _byDccId = new Dictionary<string, MappingEntry>();
        private readonly Dictionary<string, MappingEntry> _byBabylonName = new Dictionary<string, MappingEntry>();

        public void AddEntry(string dccId, string babylonName, NodeType type)
        {
            var entry = new MappingEntry
            {
                DccId = dccId,
                BabylonName = babylonName,
                NodeType = type
            };

            _byDccId[dccId] = entry;
            _byBabylonName[babylonName] = entry;
        }

        public void RemoveEntry(string dccId)
        {
            if (_byDccId.TryGetValue(dccId, out var entry))
            {
                _byDccId.Remove(dccId);
                _byBabylonName.Remove(entry.BabylonName);
            }
        }

        public void Clear()
        {
            _byDccId.Clear();
            _byBabylonName.Clear();
        }

        public MappingEntry LookupByDccId(string dccId)
        {
            _byDccId.TryGetValue(dccId, out var entry);
            return entry;
        }

        public MappingEntry LookupByBabylonName(string babylonName)
        {
            _byBabylonName.TryGetValue(babylonName, out var entry);
            return entry;
        }

        public string GetBabylonName(string dccId)
        {
            return LookupByDccId(dccId)?.BabylonName;
        }

        public void UpdateTopology(string dccId, int vertexCount, int indexCount)
        {
            var entry = LookupByDccId(dccId);
            if (entry != null)
            {
                entry.LastVertexCount = vertexCount;
                entry.LastIndexCount = indexCount;
            }
        }

        public bool IsTopologyChanged(string dccId, int currentVertexCount, int currentIndexCount)
        {
            var entry = LookupByDccId(dccId);
            if (entry == null) return true;
            return entry.LastVertexCount != currentVertexCount || entry.LastIndexCount != currentIndexCount;
        }

        public IEnumerable<MappingEntry> AllEntries => _byDccId.Values;
    }
}
