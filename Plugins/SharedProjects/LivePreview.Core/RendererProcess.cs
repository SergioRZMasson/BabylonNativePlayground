using System;
using System.ComponentModel;
using System.Diagnostics;

namespace LivePreview.Core
{
    public class RendererProcess
    {
        private readonly string _executablePath;
        private Process _process;

        public bool IsRunning => _process != null && !_process.HasExited;
        public event Action OnExited;

        public RendererProcess(string executablePath)
        {
            _executablePath = executablePath;
        }

        public void Start(string wsUrl)
        {
            try
            {
                var startInfo = new ProcessStartInfo
                {
                    FileName = _executablePath,
                    Arguments = $"--ws-url {wsUrl}",
                    UseShellExecute = false,
                    CreateNoWindow = false
                };

                _process = new Process { StartInfo = startInfo, EnableRaisingEvents = true };
                _process.Exited += (s, e) => OnExited?.Invoke();
                _process.Start();
            }
            catch (Win32Exception ex)
            {
                throw new InvalidOperationException(
                    $"Failed to launch renderer at '{_executablePath}': {ex.Message}", ex);
            }
        }

        public void Stop()
        {
            if (_process == null) return;

            try
            {
                if (!_process.HasExited)
                {
                    _process.Kill();
                    _process.WaitForExit(5000);
                }
            }
            catch (InvalidOperationException)
            {
                // Process already exited
            }
            finally
            {
                _process.Dispose();
                _process = null;
            }
        }
    }
}
