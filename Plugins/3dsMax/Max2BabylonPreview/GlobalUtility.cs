using System;
using Autodesk.Max;
using Autodesk.Max.Plugins;
using UiViewModels.Actions;

namespace Max2BabylonPreview
{
    public class LivePreviewAdapter : CuiActionCommandAdapter
    {
        private static LivePreviewPanel _panel;

        public override string InternalActionText => "Babylon Live Preview";
        public override string InternalCategory => "Babylon Preview";
        public override string ActionText => InternalActionText;
        public override string Category => InternalCategory;

        public override void Execute(object parameter)
        {
            if (_panel == null || _panel.IsDisposed)
            {
                _panel = new LivePreviewPanel();
            }
            _panel.Show();
            _panel.BringToFront();
        }
    }

    public class GlobalUtility : GUP
    {
        public override uint Start
        {
            get
            {
                InstallMenus();
                return 0;
            }
        }

        public override void Stop()
        {
        }

        private void InstallMenus()
        {
            try
            {
                string script =
                    "if (menuMan.findMenu \"Babylon Preview\") != undefined do (\r\n" +
                    "    menuMan.unRegisterMenu (menuMan.findMenu \"Babylon Preview\")\r\n" +
                    ")\r\n" +
                    "local mainMenu = menuMan.createMenu \"Babylon Preview\"\r\n" +
                    "local livePreviewAction = menuMan.createActionItem \"Babylon Live Preview\" \"Babylon Preview\"\r\n" +
                    "mainMenu.addItem livePreviewAction -1\r\n" +
                    "local subItem = menuMan.createSubMenuItem \"Babylon Preview\" mainMenu\r\n" +
                    "local mainBar = menuMan.getMainMenuBar()\r\n" +
                    "mainBar.addItem subItem -1\r\n" +
                    "menuMan.updateMenuBar()\r\n";

                ManagedServices.MaxscriptSDK.ExecuteMaxscriptCommand(script,
                    ManagedServices.MaxscriptSDK.ScriptSource.NotSpecified);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine(
                    "[Max2BabylonPreview] Failed to install menus: " + ex.Message);
            }
        }
    }
}
