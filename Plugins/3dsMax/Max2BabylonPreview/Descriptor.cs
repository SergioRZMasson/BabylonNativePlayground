using Autodesk.Max;

namespace Max2BabylonPreview
{
    public class Descriptor : Autodesk.Max.Plugins.ClassDesc2
    {
        public override object Create(bool loading)
        {
            return new GlobalUtility();
        }

        public override bool IsPublic => true;

        public override string ClassName => "Babylon Live Preview";

        public override SClass_ID SuperClassID => SClass_ID.Gup;

        public override IClass_ID ClassID => Loader.Class_ID;

        public override string Category => "Babylon";
    }
}
