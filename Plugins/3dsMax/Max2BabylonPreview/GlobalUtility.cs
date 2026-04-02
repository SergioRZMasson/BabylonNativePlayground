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
                var actionTableId = FindAdapterActionTableId("Babylon Live Preview");
                if (actionTableId >= 0)
                {
                    InstallMenus(actionTableId);
                }
                return 0;
            }
        }

        public override void Stop()
        {
        }

        private int FindAdapterActionTableId(string actionText)
        {
            var actionManager = Loader.Core.ActionManager;
            for (int i = 0; i < actionManager.NumActionTables; i++)
            {
                var table = actionManager.GetTable(i);
                for (int j = 0; j < table.Count; j++)
                {
                    var action = table[j];
                    if (action?.DescriptionText == actionText)
                    {
                        return (int)table.Id_;
                    }
                }
            }
            return -1;
        }

        private void InstallMenus(int actionTableId)
        {
            try
            {
                string script =
                    "(\r\n" +
                    "    function createBabylonPreviewMenuCB =\r\n" +
                    "    (\r\n" +
                    "        local menuMgr = callbacks.notificationParam()\r\n" +
                    "        local mainMenu = menuMgr.mainMenuBar\r\n" +
                    "        local babMenu = mainMenu.CreateSubMenu \"e7f1a2b3-c4d5-4e6f-a7b8-c9d0e1f2a3b4\" \"Babylon Preview\"\r\n" +
                    $"        babMenu.CreateAction \"d8e9f0a1-b2c3-4d5e-f6a7-b8c9d0e1f2a3\" {actionTableId} \"Babylon Live Preview\"\r\n" +
                    "    )\r\n" +
                    "    callbacks.removeScripts id:#BabylonPreviewMenus\r\n" +
                    "    callbacks.addScript #cuiRegisterMenus createBabylonPreviewMenuCB id:#BabylonPreviewMenus\r\n" +
                    ")\r\n";

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
