using System;
using Autodesk.Max;
using Autodesk.Max.Plugins;

namespace Max2BabylonPreview
{
    public class LivePreviewActionItem : ActionItem
    {
        private static LivePreviewPanel _panel;

        public override bool ExecuteAction()
        {
            if (_panel == null || _panel.IsDisposed)
            {
                _panel = new LivePreviewPanel();
            }
            _panel.Show();
            _panel.BringToFront();
            return true;
        }

        public override bool IsChecked_ => false;
        public override bool IsEnabled_ => true;
        public override bool IsItemVisible => true;

        public override int Id_ => 0;
        public override string ButtonText => "Live Preview";
        public override string MenuText => "Live Preview";
        public override string DescriptionText => "Babylon Live Preview";
        public override string CategoryText => "Babylon";
    }

    public class GlobalUtility : GUP
    {
        private IActionTable _actionTable;
        private IActionCallback _actionCallback;
        private uint _idActionTable;

        public override uint Start
        {
            get
            {
                var actionManager = Loader.Core.ActionManager;
                _idActionTable = (uint)actionManager.NumActionTables;

                string actionTableName = "Babylon Preview Actions";
                _actionTable = Loader.Global.ActionTable.Create(
                    _idActionTable, 0, ref actionTableName);

                _actionTable.AppendOperation(new LivePreviewActionItem());

                _actionCallback = new LivePreviewActionCallback();
                actionManager.RegisterActionTable(_actionTable);
                actionManager.ActivateActionTable(
                    _actionCallback as ActionCallback, _idActionTable);

                InstallMenus();
                return 0;
            }
        }

        public override void Stop()
        {
            try
            {
                if (_actionTable != null)
                {
                    Loader.Global.COREInterface.ActionManager.DeactivateActionTable(
                        _actionCallback, _idActionTable);
                }
            }
            catch
            {
                // Fails silently
            }
        }

        private void InstallMenus()
        {
            try
            {
                var menuManager = Loader.Core.MenuManager;
                var menu = menuManager.FindMenu("Babylon Preview");
                if (menu != null)
                {
                    menuManager.UnRegisterMenu(menu);
                    Loader.Global.ReleaseIMenu(menu);
                }

                menu = Loader.Global.IMenu;
                menu.Title = "Babylon Preview";
                menuManager.RegisterMenu(menu, 0);

                var menuItem = Loader.Global.IMenuItem;
                menuItem.Title = "Live Preview";
                menuItem.ActionItem = _actionTable[0];
                menu.AddItem(menuItem, -1);

                var subMenuItem = Loader.Global.IMenuItem;
                subMenuItem.SubMenu = menu;
                menuManager.MainMenuBar.AddItem(subMenuItem, -1);

                Loader.Global.COREInterface.MenuManager.UpdateMenuBar();
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine(
                    $"[Max2BabylonPreview] Failed to install menus: {ex.Message}");
            }
        }
    }

    internal class LivePreviewActionCallback : ActionCallback
    {
    }
}
