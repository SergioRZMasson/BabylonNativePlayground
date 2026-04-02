using System;
using System.Drawing;
using System.Windows.Forms;
using LivePreview.Core;
using LivePreview.Protocol;

namespace Max2BabylonPreview
{
    public class LivePreviewPanel : Form
    {
        private Button _startButton;
        private Button _stopButton;
        private Label _statusLabel;
        private TextBox _portField;
        private TextBox _rendererPathField;
        private Label _portLabel;
        private Label _pathLabel;
        private MaxLivePreviewSession _session;

        public LivePreviewPanel()
        {
            InitializeComponents();
        }

        private void InitializeComponents()
        {
            Text = "Babylon Live Preview";
            Size = new Size(420, 260);
            FormBorderStyle = FormBorderStyle.FixedToolWindow;
            StartPosition = FormStartPosition.CenterScreen;

            _statusLabel = new Label
            {
                Text = "\u25CB Disconnected",
                ForeColor = Color.Gray,
                Location = new Point(12, 15),
                Size = new Size(380, 20),
                Font = new Font(Font.FontFamily, 10f, FontStyle.Bold)
            };

            _portLabel = new Label { Text = "Port:", Location = new Point(12, 50), Size = new Size(50, 20) };
            _portField = new TextBox
            {
                Text = ProtocolConstants.DefaultPort.ToString(),
                Location = new Point(70, 48),
                Size = new Size(80, 20)
            };

            _pathLabel = new Label { Text = "Renderer:", Location = new Point(12, 80), Size = new Size(65, 20) };
            _rendererPathField = new TextBox
            {
                Text = MaxLivePreviewSession.GetDefaultRendererPath(),
                Location = new Point(80, 78),
                Size = new Size(310, 20)
            };

            _startButton = new Button
            {
                Text = "Live Preview",
                Location = new Point(12, 115),
                Size = new Size(185, 35)
            };
            _startButton.Click += OnStartClick;

            _stopButton = new Button
            {
                Text = "Stop Preview",
                Location = new Point(205, 115),
                Size = new Size(185, 35),
                Enabled = false
            };
            _stopButton.Click += OnStopClick;

            Controls.AddRange(new Control[]
            {
                _statusLabel, _portLabel, _portField, _pathLabel,
                _rendererPathField, _startButton, _stopButton
            });
        }

        private async void OnStartClick(object sender, EventArgs e)
        {
            if (!int.TryParse(_portField.Text, out int port))
            {
                MessageBox.Show("Invalid port number.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            var rendererPath = _rendererPathField.Text;
            if (string.IsNullOrWhiteSpace(rendererPath))
            {
                MessageBox.Show("Renderer path is empty.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            _startButton.Enabled = false;
            _stopButton.Enabled = true;
            _portField.Enabled = false;
            _rendererPathField.Enabled = false;

            _session = new MaxLivePreviewSession(port, rendererPath);
            _session.OnStateChanged += OnSessionStateChanged;
            _session.OnError += OnSessionError;

            try
            {
                await _session.StartAsync();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to start preview: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                ResetUI();
            }
        }

        private async void OnStopClick(object sender, EventArgs e)
        {
            if (_session != null)
            {
                try
                {
                    await _session.StopAsync();
                }
                catch { /* best effort */ }
                _session = null;
            }
            ResetUI();
        }

        private void OnSessionStateChanged(SessionState state)
        {
            if (InvokeRequired)
            {
                BeginInvoke(new Action<SessionState>(OnSessionStateChanged), state);
                return;
            }

            switch (state)
            {
                case SessionState.Disconnected:
                    _statusLabel.Text = "\u25CB Disconnected";
                    _statusLabel.ForeColor = Color.Gray;
                    break;
                case SessionState.StartingServer:
                case SessionState.WaitingForRenderer:
                case SessionState.Handshaking:
                case SessionState.Exporting:
                    _statusLabel.Text = "\u21BB Starting...";
                    _statusLabel.ForeColor = Color.DarkOrange;
                    break;
                case SessionState.Streaming:
                    _statusLabel.Text = "\u25CF Connected";
                    _statusLabel.ForeColor = Color.Green;
                    break;
                case SessionState.Reconnecting:
                    _statusLabel.Text = "\u21BB Reconnecting...";
                    _statusLabel.ForeColor = Color.DarkOrange;
                    break;
                case SessionState.Error:
                    _statusLabel.Text = "\u25CF Error";
                    _statusLabel.ForeColor = Color.Red;
                    break;
            }
        }

        private void OnSessionError(string message)
        {
            if (InvokeRequired)
            {
                BeginInvoke(new Action<string>(OnSessionError), message);
                return;
            }
            MessageBox.Show(message, "Babylon Live Preview", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }

        private void ResetUI()
        {
            _startButton.Enabled = true;
            _stopButton.Enabled = false;
            _portField.Enabled = true;
            _rendererPathField.Enabled = true;
            _statusLabel.Text = "\u25CB Disconnected";
            _statusLabel.ForeColor = Color.Gray;
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            if (_session != null && _session.State != SessionState.Disconnected)
            {
                e.Cancel = true;
                OnStopClick(this, EventArgs.Empty);
                return;
            }
            base.OnFormClosing(e);
        }
    }
}
