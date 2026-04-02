using System.Collections.Generic;
using Autodesk.Max;

namespace Max2BabylonPreview
{
    public static class MaxMaterialMapper
    {
        public static List<KeyValuePair<string, object>> GetMaterialProperties(IINode node)
        {
            var props = new List<KeyValuePair<string, object>>();
            var mtl = node.Mtl;
            if (mtl == null) return props;

            string className = "";
            try { mtl.GetClassName(ref className, false); }
            catch { return props; }

            if (className == "Standard" || className == "Standardmaterial")
            {
                var diffuse = mtl.GetDiffuse(0, false);
                props.Add(new KeyValuePair<string, object>("diffuseColor",
                    new double[] { diffuse.R, diffuse.G, diffuse.B }));

                var emissive = mtl.GetSelfIllumColor(0, false);
                props.Add(new KeyValuePair<string, object>("emissiveColor",
                    new double[] { emissive.R, emissive.G, emissive.B }));

                props.Add(new KeyValuePair<string, object>("alpha",
                    (double)(1.0f - mtl.GetXParency(0, false))));
            }
            else if (className == "Physical Material" || className == "PhysicalMaterial")
            {
                var diffuse = mtl.GetDiffuse(0, false);
                props.Add(new KeyValuePair<string, object>("albedoColor",
                    new double[] { diffuse.R, diffuse.G, diffuse.B }));

                props.Add(new KeyValuePair<string, object>("alpha",
                    (double)(1.0f - mtl.GetXParency(0, false))));
            }

            return props;
        }

        public static List<KeyValuePair<string, object>> GetMaterialDelta(
            IINode node,
            Dictionary<string, object> cachedProps)
        {
            var changes = new List<KeyValuePair<string, object>>();
            var current = GetMaterialProperties(node);

            foreach (var prop in current)
            {
                if (!cachedProps.TryGetValue(prop.Key, out var cached) ||
                    !ValuesEqual(prop.Value, cached))
                {
                    changes.Add(prop);
                    cachedProps[prop.Key] = prop.Value;
                }
            }

            return changes;
        }

        private static bool ValuesEqual(object a, object b)
        {
            if (a is double[] arrA && b is double[] arrB)
            {
                if (arrA.Length != arrB.Length) return false;
                for (int i = 0; i < arrA.Length; i++)
                {
                    if (System.Math.Abs(arrA[i] - arrB[i]) > 0.001) return false;
                }
                return true;
            }
            return Equals(a, b);
        }
    }
}
