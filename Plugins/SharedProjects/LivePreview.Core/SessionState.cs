namespace LivePreview.Core
{
    public enum SessionState
    {
        Disconnected,
        StartingServer,
        WaitingForRenderer,
        Handshaking,
        Exporting,
        Streaming,
        Reconnecting,
        Stopping,
        Error
    }
}
