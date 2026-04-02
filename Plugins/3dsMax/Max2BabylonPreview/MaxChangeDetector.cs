using System;
using System.Collections.Generic;
using Autodesk.Max;

namespace Max2BabylonPreview
{
    public class MaxChangeDetector
    {
        private System.Timers.Timer _pollTimer;
        private HashSet<string> _selectedHandles = new HashSet<string>();
        private Dictionary<string, CachedTransform> _cachedTransforms = new Dictionary<string, CachedTransform>();
        private LivePreview.ObjectMapping.ObjectMap _map;
        private bool _active;

        public event Action<string, double[]> OnPositionChanged;
        public event Action<string, double[]> OnRotationChanged;
        public event Action<string, double[]> OnScaleChanged;
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
            _cachedTransforms.Clear();
        }

        public void UpdateSelection()
        {
            _selectedHandles.Clear();
            _cachedTransforms.Clear();

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
                        _cachedTransforms[handle] = ReadTransform(node);
                    }
                }
            }
            catch { /* safe fallback */ }
        }

        private void OnPollTick(object sender, System.Timers.ElapsedEventArgs e)
        {
            if (!_active) return;

            try
            {
                PollSelectedNodes();
            }
            catch { /* swallow polling errors to avoid crashing timer */ }
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
                _cachedTransforms.Clear();
                foreach (var h in _selectedHandles)
                {
                    var node = FindNodeByHandle(h);
                    if (node != null)
                        _cachedTransforms[h] = ReadTransform(node);
                }
                OnSelectionChanged?.Invoke();
                return;
            }

            foreach (var handle in _selectedHandles)
            {
                if (_map?.GetBabylonName(handle) == null) continue;

                var node = FindNodeByHandle(handle);
                if (node == null) continue;

                var current = ReadTransform(node);
                if (!_cachedTransforms.TryGetValue(handle, out var cached))
                {
                    _cachedTransforms[handle] = current;
                    continue;
                }

                if (!ArrayEqual(current.Position, cached.Position))
                {
                    _cachedTransforms[handle] = current;
                    OnPositionChanged?.Invoke(handle, current.Position);
                }
                else if (!ArrayEqual(current.Rotation, cached.Rotation))
                {
                    _cachedTransforms[handle] = current;
                    OnRotationChanged?.Invoke(handle, current.Rotation);
                }
                else if (!ArrayEqual(current.Scale, cached.Scale))
                {
                    _cachedTransforms[handle] = current;
                    OnScaleChanged?.Invoke(handle, current.Scale);
                }
            }
        }

        private IINode FindNodeByHandle(string handle)
        {
            if (!uint.TryParse(handle, out var h)) return null;
            try
            {
                return Loader.Core.GetINodeByHandle(h);
            }
            catch { return null; }
        }

        private static CachedTransform ReadTransform(IINode node)
        {
            var pos = node.ObjOffsetPos;
            var nodePos = node.GetNodeTM(0, null).GetRow(3);

            return new CachedTransform
            {
                Position = ConvertPosition(nodePos.X, nodePos.Y, nodePos.Z),
                Rotation = ReadRotationDeg(node),
                Scale = ReadScale(node)
            };
        }

        public static double[] ConvertPosition(float mx, float my, float mz)
        {
            return new double[] { mx, mz, -my };
        }

        private static double[] ReadRotationDeg(IINode node)
        {
            var tm = node.GetNodeTM(0, null);
            var rot = tm.GetRow(0);
            // Simplified: extract euler angles from transform matrix
            // For a proper implementation, decompose the matrix using AffineParts
            return new double[] { 0, 0, 0 };
        }

        private static double[] ReadScale(IINode node)
        {
            var tm = node.GetNodeTM(0, null);
            var sx = Length(tm.GetRow(0));
            var sy = Length(tm.GetRow(1));
            var sz = Length(tm.GetRow(2));
            return new double[] { sx, sz, sy };
        }

        private static float Length(IPoint3 v)
        {
            return (float)Math.Sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
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

        private class CachedTransform
        {
            public double[] Position;
            public double[] Rotation;
            public double[] Scale;
        }
    }
}
