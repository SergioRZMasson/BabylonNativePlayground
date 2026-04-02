namespace LivePreview.Protocol
{
    public static class ProtocolConstants
    {
        public const int DefaultPort = 8765;
        public const string DefaultHost = "127.0.0.1";

        public const string CmdQueryScene = "query_scene";
        public const string CmdSetTransform = "set_transform";
        public const string CmdSetMaterial = "set_material";
        public const string CmdSetLight = "set_light";
        public const string CmdSetVisibility = "set_visibility";
        public const string CmdSetEnabled = "set_enabled";
        public const string CmdLoadGlbData = "load_glb_data";
        public const string CmdClearScene = "clear_scene";
        public const string CmdUpdateGeometry = "update_geometry";
        public const string CmdRemoveNode = "remove_node";
        public const string CmdAddNode = "add_node";
        public const string CmdHandshake = "handshake";
        public const string CmdPing = "ping";

        public const int PropertyUpdateTimeoutMs = 10000;
        public const int GlbLoadTimeoutMs = 60000;
        public const int DefaultTimeoutMs = 10000;
    }
}
