using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace LivePreview.Protocol
{
    public static class CommandBuilder
    {
        public static string BuildCommand(string id, string command, JObject parameters = null)
        {
            var msg = new JObject
            {
                ["id"] = id,
                ["command"] = command,
                ["params"] = parameters ?? new JObject()
            };
            return msg.ToString(Formatting.None);
        }

        public static string BuildSetTransform(string id, string target, string property, double[] value)
        {
            var p = new JObject
            {
                ["target"] = target,
                ["property"] = property,
                ["value"] = new JArray(value)
            };
            return BuildCommand(id, ProtocolConstants.CmdSetTransform, p);
        }

        public static string BuildSetMaterial(string id, string target, string property, object value)
        {
            var p = new JObject
            {
                ["target"] = target,
                ["property"] = property,
                ["value"] = JToken.FromObject(value)
            };
            return BuildCommand(id, ProtocolConstants.CmdSetMaterial, p);
        }

        public static string BuildLoadGlbData(string id, string base64Data, string fileName)
        {
            var p = new JObject
            {
                ["data"] = base64Data,
                ["fileName"] = fileName
            };
            return BuildCommand(id, ProtocolConstants.CmdLoadGlbData, p);
        }

        public static string BuildClearScene(string id)
        {
            return BuildCommand(id, ProtocolConstants.CmdClearScene);
        }

        public static string BuildQueryScene(string id)
        {
            return BuildCommand(id, ProtocolConstants.CmdQueryScene);
        }

        public static string BuildUpdateGeometry(string id, string target, double[] positions, double[] normals, double[] uvs, int[] indices)
        {
            var p = new JObject
            {
                ["target"] = target,
                ["positions"] = new JArray(positions)
            };
            if (normals != null)
                p["normals"] = new JArray(normals);
            if (uvs != null)
                p["uvs"] = new JArray(uvs);
            if (indices != null)
                p["indices"] = new JArray(indices);
            return BuildCommand(id, ProtocolConstants.CmdUpdateGeometry, p);
        }

        public static string BuildRemoveNode(string id, string target)
        {
            var p = new JObject { ["target"] = target };
            return BuildCommand(id, ProtocolConstants.CmdRemoveNode, p);
        }

        public static string BuildHandshakeResponse(string id, string dccTool, string dccVersion)
        {
            var p = new JObject
            {
                ["type"] = "dcc_plugin",
                ["dcc"] = dccTool,
                ["dccVersion"] = dccVersion,
                ["protocolVersion"] = "1.0"
            };
            return BuildCommand(id, ProtocolConstants.CmdHandshake, p);
        }
    }
}
