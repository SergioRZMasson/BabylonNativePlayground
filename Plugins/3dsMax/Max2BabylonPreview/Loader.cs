using Autodesk.Max;

namespace Max2BabylonPreview
{
    public static class Loader
    {
        public static IGlobal Global => GlobalInterface.Instance;

        public static IInterface14 Core => Global.COREInterface14;

        public static IClass_ID Class_ID;

        private static void Initialize()
        {
            if (Class_ID == null)
            {
                Class_ID = Global.Class_ID.Create(0x9317a234, 0xbc890567);
                Core.AddClass(new Descriptor());
            }
        }

        public static void AssemblyMain()
        {
            Initialize();
        }

        public static void AssemblyInitializationCleanup()
        {
        }

        public static void AssemblyShutdown()
        {
        }
    }
}
