using System;
using System.Collections.Generic;
using Autodesk.Max;

namespace Max2BabylonPreview
{
    public class MaxChangeDetector
    {
        private System.Timers.Timer _pollTimer;
        private HashSet<string> _selectedHandles = new HashSet<string>();
        private Dictionary<string, double[]> _cachedMatrices = new Dictionary<string, double[]>();
        private LivePreview.ObjectMapping.ObjectMap _map;
        private bool _active;

        // Fires with (dccId, 16-element world matrix in Babylon Y-up left-hand coords)
        public event Action<string, double[]> OnWorldMatrixChanged;
        public event Action<string> OnGeometryChanged;
        public event Action<string> OnMaterialChanged;
        public event Action OnSelectionChanged;
        public event Action OnTopologyChanged;

        public void Start(LivePreview.ObjectMapping.ObjectMap map)
        {
            _map = map;
            _active = true;
            UpdateSelection();

            _pollTimer = new System.Timers.Timer(33);
            _pollTimer.Elapsed += OnPollTick;
            _pollTimer.AutoReset = true;
            _pollTimer.Start();
        }

        public void Stop()
        {
            _active = false;
            if (_pollTimer != null)
            {
                _pollTimer.Stop();
                _pollTimer.Dispose();
                _pollTimer = null;
            }
            _selectedHandles.Clear();
            _cachedMatrices.Clear();
        }

        public void UpdateSelection()
        {
            _selectedHandles.Clear();
            _cachedMatrices.Clear();

            try
            {
                var selCount = Loader.Core.SelNodeCount;
                for (int i = 0; i < selCount; i++)
                {
                    var node = Loader.Core.GetSelNode(i);
                    if (node != null)
                    {
                        var handle = node.Handle.ToString();
                        _selectedHandles.Add(handle);
                        _cachedMatrices[handle] = ReadWorldMatrixForBabylon(node);
                    }
                }
            }
            catch { }
        }

        private void OnPollTick(object sender, System.Timers.ElapsedEventArgs e)
        {
            if (!_active) return;
            try { PollSelectedNodes(); }
            catch { }
        }

        private void PollSelectedNodes()
        {
            var currentSelCount = Loader.Core.SelNodeCount;
            var currentHandles = new HashSet<string>();
            for (int i = 0; i < currentSelCount; i++)
            {
                var node = Loader.Core.GetSelNode(i);
                if (node != null)
                    currentHandles.Add(node.Handle.ToString());
            }

            if (!currentHandles.SetEquals(_selectedHandles))
            {
                _selectedHandles = currentHandles;
                _cachedMatrices.Clear();
                foreach (var h in _selectedHandles)
                {
                    var node = FindNodeByHandle(h);
                    if (node != null)
                        _cachedMatrices[h] = ReadWorldMatrixForBabylon(node);
                }
                OnSelectionChanged?.Invoke();
                return;
            }

            foreach (var handle in _selectedHandles)
            {
                if (_map?.GetBabylonName(handle) == null) continue;

                var node = FindNodeByHandle(handle);
                if (node == null) continue;

                var current = ReadWorldMatrixForBabylon(node);
                if (!_cachedMatrices.TryGetValue(handle, out var cached))
                {
                    _cachedMatrices[handle] = current;
                    continue;
                }

                if (!ArrayEqual(current, cached))
                {
                    _cachedMatrices[handle] = current;
                    OnWorldMatrixChanged?.Invoke(handle, current);
                }
            }
        }

        private IINode FindNodeByHandle(string handle)
        {
            if (!uint.TryParse(handle, out var h)) return null;
            try { return Loader.Core.GetINodeByHandle(h); }
            catch { return null; }
        }

        /// <summary>
        /// Reads the node's world transform matrix and converts it from
        /// Max's Z-up right-hand to Babylon's Y-up left-hand coordinate system.
        /// Returns a 16-element array in row-major order for Babylon's Matrix.FromArray().
        /// </summary>
        private static double[] ReadWorldMatrixForBabylon(IINode node)
        {
            var tm = node.GetNodeTM(0, null);

            // Max matrix rows: Row0=X-axis, Row1=Y-axis, Row2=Z-axis, Row3=Translation
            var r0 = tm.GetRow(0); // X axis
            var r1 = tm.GetRow(1); // Y axis (Max)
            var r2 = tm.GetRow(2); // Z axis (Max)
            var r3 = tm.GetRow(3); // Translation

            // Convert from Max (Z-up, right-hand) to Babylon (Y-up, left-hand):
            // Babylon X = Max X
            // Babylon Y = Max Z
            // Babylon Z = -Max Y (negate for handedness flip)
            //
            // Babylon Matrix.FromArray expects ROW-MAJOR but Babylon stores column-major.
            // Actually, Babylon's Matrix.FromArray reads in column-major order.
            // So we fill: [m0,m1,m2,m3, m4,m5,m6,m7, m8,m9,m10,m11, m12,m13,m14,m15]
            // = [col0, col1, col2, col3] where col0 = first column

            // Column 0 (Babylon X-axis) = Max X-axis with Y↔Z swap and Z negate
            double c0x = r0.X;
            double c0y = r0.Z;
            double c0z = -r0.Y;

            // Column 1 (Babylon Y-axis) = Max Z-axis with Y↔Z swap and Z negate
            double c1x = r2.X;
            double c1y = r2.Z;
            double c1z = -r2.Y;

            // Column 2 (Babylon Z-axis) = -Max Y-axis with Y↔Z swap and Z negate
            double c2x = -r1.X;
            double c2y = -r1.Z;
            double c2z = r1.Y;

            // Column 3 (Translation) = position with Y↔Z swap and Z negate
            double c3x = r3.X;
            double c3y = r3.Z;
            double c3z = -r3.Y;

            return new double[]
            {
                c0x, c0y, c0z, 0,
                c1x, c1y, c1z, 0,
                c2x, c2y, c2z, 0,
                c3x, c3y, c3z, 1
            };
        }

        private static bool ArrayEqual(double[] a, double[] b)
        {
            if (a == null || b == null || a.Length != b.Length) return false;
            for (int i = 0; i < a.Length; i++)
            {
                if (Math.Abs(a[i] - b[i]) > 0.0001) return false;
            }
            return true;
        }
    }
}
