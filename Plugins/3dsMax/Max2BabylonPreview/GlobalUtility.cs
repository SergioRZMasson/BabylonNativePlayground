using System;
using Autodesk.Max;
using Autodesk.Max.Plugins;
using UiViewModels.Actions;

namespace Max2BabylonPreview
{
    public class DummyAdapter : CuiActionCommandAdapter
    {
        public const string DummyTitle = "BabylonPreviewDummy";
        public override string InternalActionText => DummyTitle;
        public override string InternalCategory => "Babylon Preview";
        public override string ActionText => InternalActionText;
        public override string Category => InternalCategory;
        public override void Execute(object parameter) { }

        public static IActionTable GetAndClearTable()
        {
            var actionManager = Loader.Core.ActionManager;
            for (int t = 0; t < actionManager.NumActionTables; t++)
            {
                var table = actionManager.GetTable(t);
                for (int a = 0; a < table.Count; a++)
                {
                    var action = table[a];
                    if (action?.DescriptionText == DummyTitle)
                    {
                        table.DeleteOperation(action);
                        return table;
                    }
                }
            }
            return null;
        }
    }

    public class LivePreviewActionItem : ActionItem
    {
        private static LivePreviewPanel _panel;

        public override bool ExecuteAction()
        {
            if (_panel == null || _panel.IsDisposed)
                _panel = new LivePreviewPanel();
            _panel.Show();
            _panel.BringToFront();
            return true;
        }

        public override int Id_ => 1;
        public override bool IsChecked_ => false;
        public override bool IsEnabled_ => true;
        public override bool IsItemVisible => true;
        public override string ButtonText => "Live Preview";
        public override string MenuText => "Live Preview";
        public override string DescriptionText => "Babylon Live Preview";
        public override string CategoryText => "Babylon Preview";
    }

    public class GlobalUtility : GUP
    {
        private uint _tableId;
        private IActionTable _table;
        private IActionCallback _callback;

        public override uint Start
        {
            get
            {
                var actionManager = Loader.Core.ActionManager;

                _table = DummyAdapter.GetAndClearTable();
                if (_table == null)
                    return 0;

                _tableId = _table.Id_;
                _table.AppendOperation(new LivePreviewActionItem());

                _callback = new LivePreviewCallback();
                actionManager.RegisterActionTable(_table);
                actionManager.ActivateActionTable(_callback as ActionCallback, _tableId);

                InstallMenus();
                return 0;
            }
        }

        public override void Stop()
        {
            try
            {
                if (_table != null)
                    Loader.Core.ActionManager.DeactivateActionTable(_callback, _tableId);
            }
            catch { }
        }

        private void InstallMenus()
        {
            try
            {
                string script =
                    "(\r\n" +
                    "    function createBabylonPreviewMenuCB =\r\n" +
                    "    (\r\n" +
                    $"        local actionTableId = {_tableId}\r\n" +
                    "        local menuMgr = callbacks.notificationParam()\r\n" +
                    "        local mainMenu = menuMgr.mainMenuBar\r\n" +
                    "        local babMenu = mainMenu.CreateSubMenu \"e7f1a2b3-c4d5-4e6f-a7b8-c9d0e1f2a3b4\" \"Babylon Preview\"\r\n" +
                    "        babMenu.CreateAction \"d8e9f0a1-b2c3-4d5e-f6a7-b8c9d0e1f2a3\" actionTableId \"1\"\r\n" +
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

    internal class LivePreviewCallback : ActionCallback { }
}
