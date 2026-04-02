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
                        _cachedMatrices[handle] = ReadLocalMatrixForBabylon(node);
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
                        _cachedMatrices[h] = ReadLocalMatrixForBabylon(node);
                }
                OnSelectionChanged?.Invoke();
                return;
            }

            foreach (var handle in _selectedHandles)
            {
                if (_map?.GetBabylonName(handle) == null) continue;

                var node = FindNodeByHandle(handle);
                if (node == null) continue;

                var current = ReadLocalMatrixForBabylon(node);
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
        /// Reads the node's LOCAL transform matrix (relative to parent).
        /// Returns 16 elements matching Babylon's Matrix.FromArray() layout.
        /// NO coordinate conversion — the glTF root node handles Z-up to Y-up.
        /// </summary>
        private static double[] ReadLocalMatrixForBabylon(IINode node)
        {
            var worldTM = node.GetNodeTM(0, null);
            IMatrix3 localTM;

            if (node.ParentNode != null && !node.ParentNode.IsRootNode)
            {
                var parentTM = node.ParentNode.GetNodeTM(0, null);
                localTM = worldTM;
                // local = world * inverse(parent)
                var parentInv = parentTM;
                parentInv.Invert();
                localTM = Multiply(worldTM, parentInv);
            }
            else
            {
                localTM = worldTM;
            }

            // Max IMatrix3 rows → Babylon column-major layout
            // Row0=X-axis, Row1=Y-axis, Row2=Z-axis, Row3=Translation
            var r0 = localTM.GetRow(0);
            var r1 = localTM.GetRow(1);
            var r2 = localTM.GetRow(2);
            var r3 = localTM.GetRow(3);

            return new double[]
            {
                r0.X, r0.Y, r0.Z, 0,
                r1.X, r1.Y, r1.Z, 0,
                r2.X, r2.Y, r2.Z, 0,
                r3.X, r3.Y, r3.Z, 1
            };
        }

        private static IMatrix3 Multiply(IMatrix3 a, IMatrix3 b)
        {
            var result = Loader.Global.Matrix3.Create();
            result.SetRow(0, a.GetRow(0));
            result.SetRow(1, a.GetRow(1));
            result.SetRow(2, a.GetRow(2));
            result.SetRow(3, a.GetRow(3));
            result.MultiplyBy(b);
            return result;
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
