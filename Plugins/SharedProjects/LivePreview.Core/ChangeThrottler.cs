using System;
using System.Collections.Generic;

namespace LivePreview.Core
{
    public class ChangeThrottler
    {
        private readonly int _minIntervalMs;
        private readonly Dictionary<string, DateTime> _lastSendTime = new Dictionary<string, DateTime>();
        private readonly object _lock = new object();

        public ChangeThrottler(int minIntervalMs = 33)
        {
            _minIntervalMs = minIntervalMs;
        }

        public bool ShouldSend(string objectId)
        {
            lock (_lock)
            {
                var now = DateTime.UtcNow;
                if (_lastSendTime.TryGetValue(objectId, out var last))
                {
                    if ((now - last).TotalMilliseconds < _minIntervalMs)
                        return false;
                }
                _lastSendTime[objectId] = now;
                return true;
            }
        }

        public void Reset()
        {
            lock (_lock)
            {
                _lastSendTime.Clear();
            }
        }

        public void Reset(string objectId)
        {
            lock (_lock)
            {
                _lastSendTime.Remove(objectId);
            }
        }
    }
}
