import unittest
import wx
from launch import Launcher,__VERSION__

class TestLauncher(unittest.TestCase):

    def setUp(self):
        self.app = wx.App()
        # start Launcher but do not load nor save the configuration file 
        self.frame = Launcher(None, "Launcher v{}.{}".format(*__VERSION__), load_config=False, save_config=False, is_testing=True)
        self.frame.Show()
        
        #log = self.frame.log
        self.app.SetTopWindow(self.frame)
        
        #do not start the self.app.MainLoop(), all events are driven by the test methods

    def tearDown(self):
        #after all tests are finished we can start self.app.MainLoop()
        
        #the the application to shutdown after event processing is finished
        wx.CallAfter(self.app.Exit)
        self.app.MainLoop()

    def test_user_id(self):
        def clickOK():
            clickEvent = wx.CommandEvent(wx.wxEVT_COMMAND_BUTTON_CLICKED, wx.ID_OK)
            self.dlg.ProcessEvent(clickEvent)
        def clickCancel():
            clickEvent = wx.CommandEvent(wx.wxEVT_COMMAND_BUTTON_CLICKED, wx.ID_CANCEL)
            self.frame.dlg.ProcessEvent(clickEvent)

        wx.CallAfter(clickCancel)
        self.frame.wUserId.SetValue('dafj')
        self.frame.user_id_has_changed()
#         self.ShowDialog()
# 
#     def ShowDialog(self):
#         self.dlg = MyDialog(self.frame)
#         self.dlg.ShowModal()
#         self.dlg.Destroy()

if __name__=="__main__":
    unittest.main()