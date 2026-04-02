using System;
using System.Threading.Tasks;
using Newtonsoft.Json.Linq;

namespace LivePreview.Protocol
{
    public class ProtocolClient
    {
        private readonly WebSocketServer _server;

        public ProtocolClient(WebSocketServer server)
        {
            _server = server;
        }

        public async Task<JObject> SendCommandAsync(string command, JObject parameters = null, int timeoutMs = ProtocolConstants.DefaultTimeoutMs)
        {
            var id = _server.GenerateId();
            var json = CommandBuilder.BuildCommand(id, command, parameters);
            var responseJson = await _server.SendCommandAsync(json, timeoutMs).ConfigureAwait(false);
            var response = JObject.Parse(responseJson);

            var success = response["success"]?.Value<bool>() ?? false;
            if (!success)
            {
                var error = response["error"]?.ToString() ?? "Unknown error";
                throw new InvalidOperationException($"Command '{command}' failed: {error}");
            }

            return response["data"] as JObject;
        }

        public Task ClearSceneAsync()
        {
            return SendCommandAsync(ProtocolConstants.CmdClearScene);
        }

        public async Task<JObject> LoadGlbDataAsync(string base64Data, string fileName)
        {
            var id = _server.GenerateId();
            var json = CommandBuilder.BuildLoadGlbData(id, base64Data, fileName);
            var responseJson = await _server.SendCommandAsync(json, ProtocolConstants.GlbLoadTimeoutMs).ConfigureAwait(false);
            var response = JObject.Parse(responseJson);

            var success = response["success"]?.Value<bool>() ?? false;
            if (!success)
            {
                var error = response["error"]?.ToString() ?? "Unknown error";
                throw new InvalidOperationException($"LoadGlbData failed: {error}");
            }

            return response["data"] as JObject;
        }

        public Task<JObject> QuerySceneAsync()
        {
            return SendCommandAsync(ProtocolConstants.CmdQueryScene);
        }

        public Task SetTransformAsync(string target, string property, double[] value)
        {
            var p = new JObject
            {
                ["target"] = target,
                ["property"] = property,
                ["value"] = new JArray(value)
            };
            return SendCommandAsync(ProtocolConstants.CmdSetTransform, p);
        }

        public Task SetMaterialAsync(string target, string property, object value)
        {
            var p = new JObject
            {
                ["target"] = target,
                ["property"] = property,
                ["value"] = JToken.FromObject(value)
            };
            return SendCommandAsync(ProtocolConstants.CmdSetMaterial, p);
        }

        public async Task<JObject> UpdateGeometryAsync(string target, double[] positions, double[] normals = null, double[] uvs = null)
        {
            var id = _server.GenerateId();
            var json = CommandBuilder.BuildUpdateGeometry(id, target, positions, normals, uvs, null);
            var responseJson = await _server.SendCommandAsync(json).ConfigureAwait(false);
            var response = JObject.Parse(responseJson);

            var success = response["success"]?.Value<bool>() ?? false;
            if (!success)
            {
                var error = response["error"]?.ToString() ?? "Unknown error";
                throw new InvalidOperationException($"UpdateGeometry failed: {error}");
            }

            return response["data"] as JObject;
        }

        public Task RemoveNodeAsync(string target)
        {
            var p = new JObject { ["target"] = target };
            return SendCommandAsync(ProtocolConstants.CmdRemoveNode, p);
        }

        public Task EnsureDefaultLightsAsync()
        {
            return SendCommandAsync("ensure_default_lights");
        }
    }
}
