from __future__ import print_function

import wx
import os

import xx
import wxtools
import sshtools
import LauncherM
from BashEditor import PbsEditor
import log
from Value import Value
from indent import Indent

def version():
    return LauncherM.__VERSION__

import types

def viewer_to_model(self):
    """
    function to be injected as a method in LauncherVC's widgets
    see: http://stackoverflow.com/questions/972/adding-a-method-to-an-existing-object
    """
    #transfer the Viewer's (=widget) value to the model
    self.model_value.set(self.GetValue())

def model_to_viewer(self):
    """
    function to be injected as a method in LauncherVC's widgets
    see: http://stackoverflow.com/questions/972/adding-a-method-to-an-existing-object
    """
    #transfer the Model's value to the Viewer (=widget) 
    self.ChangeValue(self.model_value.get())
    
    
# MVC - Modeler/Viewer/Controller
class LauncherVC(wx.Frame):
    """
    the Launcher gui
    """
    ID_MENU_SSH_PREFERENCES     = wx.NewId()
    ID_MENU_SSH_CONNECT         = wx.NewId()
    ID_MENU_LOGIN_NODE          = wx.NewId()
    ID_MENU_CLUSTER_MODULE_ADDED= wx.NewId()
    ID_MENU_CHECK_FOR_UPDATE    = wx.NewId()
    
    def __init__(self, parent, title, load_config=True, save_config=True, is_testing=False):
        """
        Create all the controls in the Launcher window and bind the necessary methods

        Some initialization options for (unit)testing. The default value correspond
        to production runs (not testing) 
        load_config = False: do not load the config file
        save_config = False: do not save the config file
        is_testing  = True : executing (unit)tests
        """
        super(LauncherVC,self).__init__(parent,title=title)

        self.is_initializing = True
        self.load_config=load_config
        self.save_config=save_config
        self.is_testing = is_testing
        
        LauncherM.Config().start_log(wx.version(),sshtools.paramiko.__version__)
           
        #allow sshtools to set the statusbar of the Launcher    
        sshtools.Client.frame = self #todo > model
            
        self.initialize_widgets()

        wxtools.set_names(self)
        
        self.Bind(wx.EVT_CLOSE, self.on_EVT_CLOSE)
        
        wxtools.bind( self, 'wCluster'                , 'EVT_COMBOBOX')
        wxtools.bind( self, 'wNodeSet'                , 'EVT_COMBOBOX')
        wxtools.bind( self, 'wNotebook'               , 'EVT_NOTEBOOK_PAGE_CHANGING')
        wxtools.bind( self, 'wNotebook'               , 'EVT_NOTEBOOK_PAGE_CHANGED')
        wxtools.bind( self, 'wNNodesRequested'        , 'EVT_SPINCTRL')
        wxtools.bind( self, 'wNCoresPerNodeRequested' , 'EVT_SPINCTRL')
        wxtools.bind( self, 'wNCoresRequested'        , 'EVT_SPINCTRL')
        wxtools.bind( self, 'wGbPerCoreRequested'     , 'EVT_SPINCTRL')            
#         wxtools.bind( self, 'wEnforceNNodes'          , 'EVT_CHECKBOX')
#         wxtools.bind( self, 'wNodeSet'                , 'EVT_COMBOBOX')
#         wxtools.bind( self, 'wSelectModule'           , 'EVT_COMBOBOX')
#         wxtools.bind( self, 'wScript'                 , 'EVT_SET_FOCUS')
#         wxtools.bind( self, 'wScript'                 , 'EVT_KILL_FOCUS')
#         wxtools.bind( self, 'wWalltime'               , 'EVT_SPINCTRL')
#         wxtools.bind( self, 'wWalltimeUnit'           , 'EVT_COMBOBOX')
#         wxtools.bind( self, 'wNotifyAbort'            , 'EVT_CHECKBOX')
#         wxtools.bind( self, 'wNotifyBegin'            , 'EVT_CHECKBOX')
#         wxtools.bind( self, 'wNotifyEnd'              , 'EVT_CHECKBOX')
#         wxtools.bind( self, 'wNotifyAddress'          , 'EVT_TEXT_ENTER')
#         wxtools.bind( self, 'wJobName'                , 'EVT_TEXT_ENTER')
#         wxtools.bind( self, 'wRemoteSubfolder'        , 'EVT_TEXT')
#         wxtools.bind( self, 'wUserId'                 , 'EVT_TEXT_ENTER')
#         wxtools.bind( self, 'wSaveJob'                , 'EVT_BUTTON',)
#         wxtools.bind( self, 'wSubmitJob'              , 'EVT_BUTTON')
#         wxtools.bind( self, 'wRetrieveJobs'           , 'EVT_BUTTON')
#         wxtools.bind( self, 'wLocalFileLocation'      , 'EVT_SET_FOCUS')
#         wxtools.bind( self, 'wLocalFileLocation'      , 'EVT_TEXT')
#         wxtools.bind( self, 'wLocalFileLocation'      , 'EVT_TEXT_ENTER')
#         wxtools.bind( self, 'wLocalFileLocation'      , 'EVT_CHAR_HOOK')
#         wxtools.bind( self, 'wRemoteLocation'         , 'EVT_COMBOBOX')
#         wxtools.bind( self, 'wJobsRetrieved'          , 'EVT_SET_FOCUS')
#         wxtools.bind( self, 'wRefresh'                , 'EVT_BUTTON')
#         wxtools.bind( self, 'wRetrieveJobs2'          , 'EVT_BUTTON')
#         wxtools.bind( self, 'wDeleteJob'              , 'EVT_BUTTON')
        self.Show()
        #set control data
        self.initialize_data()
        self.dump('At end of Launcher Frame constructor')
        if self.config.attributes.get('automatic_updates',False):
            self.check_for_updates(quiet=True)

        # ico = wx.Icon('Launcher-icon.ico', wx.BITMAP_TYPE_ICO)
        # self.SetIcon(ico)
        # no effect?
        del self.is_initializing

    ### the widgets ###
    def initialize_widgets(self):        
        self.CreateStatusBar()
        self.panel = wx.Panel(self)
        self.main_sizer = wx.BoxSizer(wx.VERTICAL)
        self.panel.SetSizer(self.main_sizer)
        
        self.wNotebook = wx.Notebook(self.panel, wx.ID_ANY)
        self.main_sizer.Add(self.wNotebook, 1, wx.EXPAND, 0)
        
        self.wNotebookPageResources = wxtools.add_notebook_page(self.wNotebook, 'Resources')
        self.wNotebookPageScript    = wxtools.add_notebook_page(self.wNotebook, "Job script")
        self.wNotebookPageRetrieve  = wxtools.add_notebook_page(self.wNotebook, "Retrieve")
   
        #self.wNotebookPageResources
        sizer_1 = wxtools.create_staticbox(self.wNotebookPageResources, "Request resources from:")
        sizer_2a= wxtools.create_staticbox(self.wNotebookPageResources, "Request nodes and cores per node:")
        sizer_2b= wxtools.create_staticbox(self.wNotebookPageResources, "Request cores and memory per core:")
        sizer_2 = wx.BoxSizer(wx.HORIZONTAL)
        sizer_2.Add(sizer_2a,1,wx.EXPAND,0)
        sizer_2.Add(sizer_2b,1,wx.EXPAND,0)
        #sizer_3 = wxtools.create_staticbox(self.wNotebookPageResources, "Node policies", orient=wx.HORIZONTAL)
        sizer_4 = wxtools.create_staticbox(self.wNotebookPageResources, "Request compute time:")
        sizer_5 = wxtools.create_staticbox(self.wNotebookPageResources, "Notify:")
        sizer_6 = wxtools.create_staticbox(self.wNotebookPageResources, "Job name:")
        sizer_7 = wxtools.create_staticbox(self.wNotebookPageResources, "Local file location:")
        sizer_8 = wxtools.create_staticbox(self.wNotebookPageResources, "Remote file location:",orient=wx.VERTICAL)
        sizer_9 = wxtools.create_staticbox(self.wNotebookPageResources, "Job management:",orient=wx.HORIZONTAL)
        self.wNotebookPageResources.SetSizer( wxtools.grid_layout( [ [sizer_1,1,wx.EXPAND]
                                                           , [sizer_2,1,wx.EXPAND]
                                                           , [sizer_4,1,wx.EXPAND]
                                                           , [sizer_5,1,wx.EXPAND]
                                                           , [sizer_6,1,wx.EXPAND]
                                                           , [sizer_7,1,wx.EXPAND]
                                                           , [sizer_8,1,wx.EXPAND]
                                                           , [sizer_9,1,wx.EXPAND] ]
                                                         , ncols=1
                                                         , growable_cols=[0] ) )
        #sizer_1
        self.wCluster = wx.ComboBox(self.wNotebookPageResources, wx.ID_ANY
                                   , style=wx.CB_DROPDOWN|wx.CB_READONLY|wx.CB_SIMPLE|wx.EXPAND
                                   )
        self.wCluster.SetToolTipString("Select the cluster to which you want to submit a job.")
        self.current_node_set = None
        
        self.wNodeSet = wx.ComboBox(self.wNotebookPageResources, wx.ID_ANY
                                   , style=wx.CB_DROPDOWN|wx.CB_READONLY|wx.CB_SIMPLE|wx.EXPAND
                                   )
        self.wNodeSet.SetToolTipString("Select the pool of nodes from which resources must be requested.")
        sizer_1.Add(wxtools.grid_layout([self.wCluster,[self.wNodeSet,1,wx.EXPAND]],growable_cols=[1]),1,wx.EXPAND,0 )        
       
        #sizer_2a and sizer_2b
        txt2a0                       = wx.StaticText(self.wNotebookPageResources,label='Number of nodes:')
        self.wNNodesRequested        = xx.SpinCtrl  (self.wNotebookPageResources )
        txt2a2                       = wx.StaticText(self.wNotebookPageResources,label='Cores per node:') 
        self.wNCoresPerNodeRequested = xx.SpinCtrl  (self.wNotebookPageResources ) 
        txt2a4                       = wx.StaticText(self.wNotebookPageResources,label='Memory per node [GB]:') 
        self.wGbPerNodeGranted       = wx.TextCtrl  (self.wNotebookPageResources,style=wx.TE_READONLY|wx.TE_RIGHT) 
        txt2a6                       = wx.StaticText(self.wNotebookPageResources,label='Enforce #nodes') 
        self.wEnforceNNodes          = wx.CheckBox  (self.wNotebookPageResources)

        txt2b0                       = wx.StaticText(self.wNotebookPageResources,label='Number of cores:')
        self.wNCoresRequested        = xx.SpinCtrl  (self.wNotebookPageResources )
        txt2b2                       = wx.StaticText(self.wNotebookPageResources,label='Memory per core [GB]:')
        self.wGbPerCoreRequested     = xx.SpinCtrl  (self.wNotebookPageResources,converter=float,digits=3) 
        txt2b4                       = wx.StaticText(self.wNotebookPageResources,label='Memory total [GB]:')
        self.wGbTotalGranted         = wx.TextCtrl  (self.wNotebookPageResources,style=wx.TE_READONLY|wx.TE_RIGHT)
        
        wxtools.set_tool_tip_string([txt2a0,self.wNNodesRequested]
                           ,tip="Number of nodes your job will use."
                           )
        wxtools.set_tool_tip_string([txt2a2,self.wNCoresPerNodeRequested]
                           ,tip="Number of cores per node your job will use."
                           )
        wxtools.set_tool_tip_string([txt2a4,self.wGbPerNodeGranted]
                           ,tip="Available memory per node (after substraction of the memory spaced occupied by the operating system)."
                           )
        wxtools.set_tool_tip_string([txt2a6,self.wEnforceNNodes]
                           ,tip="Prevent cores not requested from being used by another job."
                           )
        wxtools.set_tool_tip_string([txt2b0,self.wNCoresRequested]
                           ,tip="Enter the total number of cores requested for your job. "\
                                "After pressing enter, this will show the number of cores granted"
                           )
        wxtools.set_tool_tip_string([txt2b2,self.wGbPerCoreRequested]
                           ,tip="Enter the memory [Gb] per core you request for your job. "\
                                "After pressing Enter, this will show the average memory per core available to your job."
                           )
        wxtools.set_tool_tip_string([txt2b4,self.wGbTotalGranted]
                           ,tip="Total amount of memory [Gb] available to your job."
                           )    
    
        txt2a4                .SetForegroundColour(wx.TheColourDatabase.Find('GREY'))
        self.wGbPerNodeGranted.SetForegroundColour(wx.TheColourDatabase.Find('GREY'))
        self.wEnforceNNodes   .SetValue(True)
        txt2b4                .SetForegroundColour(wx.TheColourDatabase.Find('GREY'))
        self.wGbTotalGranted  .SetForegroundColour(wx.TheColourDatabase.Find('GREY'))

        lst2a = [txt2a0, self.wNNodesRequested
                ,txt2a2, self.wNCoresPerNodeRequested
                ,txt2a4, self.wGbPerNodeGranted
                ,txt2a6, self.wEnforceNNodes
                ]
        lst2b = [txt2b0, self.wNCoresRequested
                ,txt2b2, self.wGbPerCoreRequested
                ,txt2b4, self.wGbTotalGranted
                ]

        for i in lst2a[1:6:2]:
            h =i.GetSize()[1]
            i.SetMinSize(wx.Size(100,h))
        for i in lst2b[1:6:2]:
            h =i.GetSize()[1]
            i.SetMinSize(wx.Size(100,h))

        sizer_2a.Add(wxtools.grid_layout(lst2a,ncols=2),0,wx.EXPAND,0 )
        sizer_2b.Add(wxtools.grid_layout(lst2b,ncols=2),0,wx.EXPAND,0 )   
        #sizer_4
        self.wWalltime     = wx.SpinCtrl(self.wNotebookPageResources) 
        self.wWalltimeUnit = wx.ComboBox(self.wNotebookPageResources)
        
        lst = [ [wx.StaticText(self.wNotebookPageResources,label='wall time:'),0,wx.ALIGN_CENTER_VERTICAL]
              , [self.wWalltime,0,wx.ALIGN_CENTER_VERTICAL]
              , [self.wWalltimeUnit,0,wx.ALIGN_CENTER_VERTICAL]
              ]
        wxtools.set_tool_tip_string([lst[0][0],self.wWalltime], tip="Expected duration of your job (upper bound).")
        wxtools.set_tool_tip_string(self.wWalltimeUnit, tip="Unit in which the walltime is expressed.")
        
        sizer_4.Add(wxtools.grid_layout(lst),0,wx.EXPAND,0 )
        
        #sizer_5
        sizer_5a = wx.BoxSizer(wx.HORIZONTAL)
        self.wNotifyAddress = wx.TextCtrl(self.wNotebookPageResources,style=wx.EXPAND|wx.TE_PROCESS_ENTER|wx.TE_PROCESS_TAB)
        sizer_5a.Add(self.wNotifyAddress,1,wx.EXPAND,0)
        sizer_5b = wx.BoxSizer(wx.HORIZONTAL)
        self.wNotifyBegin = wx.CheckBox(self.wNotebookPageResources,label='notify on begin')
        self.wNotifyEnd   = wx.CheckBox(self.wNotebookPageResources,label='notify on end')
        self.wNotifyAbort = wx.CheckBox(self.wNotebookPageResources,label='notify on abort')
        sizer_5b.Add(self.wNotifyBegin,1,wx.EXPAND,0)
        sizer_5b.Add(self.wNotifyEnd  ,1,wx.EXPAND,0)
        sizer_5b.Add(self.wNotifyAbort,1,wx.EXPAND,0)
        sizer_5.Add(sizer_5a,1,wx.EXPAND,0)
        sizer_5.Add(sizer_5b,1,wx.EXPAND,0)
        
        if not LauncherM.is_valid_mail_address(self.wNotifyAddress.GetValue()):
            self.wNotifyAddress.SetForegroundColour(wx.TheColourDatabase.Find('GREY'))
        
        #sizer_6
        self.wJobName = wx.TextCtrl(self.wNotebookPageResources,style=wx.EXPAND|wx.TE_PROCESS_ENTER)
        wxtools.set_tool_tip_string(self.wJobName
                           ,tip="The job name is used as the name of the parent folder where your job script "\
                                "(pbs.sh) is stored in the local and remote file location. Typically also input "\
                                "and output file are stored here.\n"\
                                "If you do not provide a job name Launcher will propose a random name."
                           )
        self.wReloadJob  = wx.Button(self.wNotebookPageResources,label="Reload")
        wxtools.set_tool_tip_string(self.wReloadJob
                           ,tip="Reload existing job script (pbs.sh) from the local file location. "\
                                "Recent changes are discarded."
                           )
        sizer_6.Add(wxtools.grid_layout([[self.wJobName,1,wx.EXPAND]
                                , self.wReloadJob
                                ],growable_cols=[0]),1,wx.EXPAND,0 )        
        
        #sizer_7
        self.wLocalFileLocation = wx.TextCtrl(self.wNotebookPageResources)
        wxtools.set_tool_tip_string(self.wLocalFileLocation
                           ,tip="Local folder where your job script/input/output is stored. The parent folder "\
                                "is based on the job name."
                           )
        sizer_7.Add(self.wLocalFileLocation,1,wx.EXPAND,0)

        #sizer_8
        self.wUserId = wx.TextCtrl(self.wNotebookPageResources,style=wx.TE_PROCESS_ENTER)
        self.wRemoteLocation = wx.ComboBox(self.wNotebookPageResources,choices=['$VSC_SCRATCH','$VSC_DATA'])
        self.wRemoteSubfolder= wx.TextCtrl(self.wNotebookPageResources)
        lst1 = [ wx.StaticText(self.wNotebookPageResources,label="user id:")
               , wx.StaticText(self.wNotebookPageResources,label="location:")
               , wx.StaticText(self.wNotebookPageResources,label="subfolder:")
               ,[self.wUserId   ,0,wx.EXPAND]
               ,[self.wRemoteLocation ,0,wx.EXPAND]
               ,[self.wRemoteSubfolder,1,wx.EXPAND]                
               ]
        self.wRemoteFileLocation = wx.TextCtrl(self.wNotebookPageResources,style=wx.TE_READONLY)
        self.wRemoteFileLocation.SetForegroundColour(wx.TheColourDatabase.Find('GREY'))
        lst2 = [ [wxtools.grid_layout(lst1, 3, growable_cols=[2]),1,wx.EXPAND]
               , [self.wRemoteFileLocation,1,wx.EXPAND]
               ]
        wxtools.set_tool_tip_string([lst1[0],self.wUserId]
                           , tip="Your VSC account (vscXXXXX)."
                           )
        wxtools.set_tool_tip_string([lst1[1],self.wRemoteLocation]
                           , tip="Scratch disk space is the preferred location to run your job."
                           )
        wxtools.set_tool_tip_string([lst1[2],self.wRemoteSubfolder]
                           , tip="Remote location of your job folder (relative to <location>)."
                           )
        wxtools.set_tool_tip_string(self.wRemoteFileLocation
                           , tip="Remote location of your job files (<location>/<subfolder>/<jobname>)."
                           )
        sizer_8.Add( wxtools.grid_layout(lst2, 1, growable_cols=[0]),1,wx.EXPAND,0)
        
        #sizer_9
        self.wSaveJob   = wx.Button(self.wNotebookPageResources,label="Save")
        self.wSubmitJob = wx.Button(self.wNotebookPageResources,label="Submit")
        self.wRetrieveJobs  = wx.Button(self.wNotebookPageResources,label="Retrieve results")
        self.wCopyToLocalDisk = wx.CheckBox(self.wNotebookPageResources,label="Copy to local disk")
        self.wCopyToVSCDATA   = wx.CheckBox(self.wNotebookPageResources,label="Copy to $VSC_DATA")
        self.wCopyToLocalDisk.SetValue(True)
        self.wCopyToVSCDATA  .SetValue(False)

        wxtools.set_tool_tip_string(self.wSaveJob
                           , tip="Save the job script in the local file location (<local_file_location>/<jobname>/pbs.sh)."\
                           )
        wxtools.set_tool_tip_string(self.wSubmitJob
                           , tip="Save the job script in the local file location, copy the local job folder to "\
                                 "the remote file location, and submit the job."
                           )
        wxtools.set_tool_tip_string(self.wRetrieveJobs
                           , tip="If the job is finished copy the remote job folder back to the local file location and/or "\
                                 "the $VSC_DATA disk space."
                           )
        wxtools.set_tool_tip_string(self.wCopyToLocalDisk
                           , tip="Specify that the remote job folder must be copied back to the local file location."
                           )
        wxtools.set_tool_tip_string(self.wRetrieveJobs
                           , tip="Specify that the remote job folder (if on $VSC_SCRATCH) must be copied back to the $VSC_DATA."
                           )

        sizer_9a = wx.BoxSizer(orient=wx.VERTICAL)
        sizer_9a.Add(self.wRetrieveJobs   ,1,wx.EXPAND,0)
        sizer_9a.Add(self.wCopyToLocalDisk,1,wx.EXPAND,0)
        sizer_9a.Add(self.wCopyToVSCDATA  ,1,wx.EXPAND,0)
        sizer_9.Add(self.wSaveJob   ,1,wx.ALIGN_TOP,0)
        sizer_9.Add(self.wSubmitJob ,1,wx.ALIGN_TOP,0)
        sizer_9.Add(sizer_9a        ,1,wx.EXPAND,0)

        #self.wNotebookPageScript
        sizer_0 = wxtools.create_staticbox(self.wNotebookPageScript, "Job script")     
        self.wNotebookPageScript.SetSizer( wxtools.grid_layout( [ [sizer_0,1,wx.EXPAND] ]
                                                  , ncols=1
                                                  , growable_rows=[0]
                                                  , growable_cols=[0] ) )
        #sizer_0
        self.wScript = PbsEditor(self.wNotebookPageScript)

        sizer_1 = wx.BoxSizer(wx.HORIZONTAL)
        sizer_1.Add(wx.StaticText(self.wNotebookPageScript,label="Select module to load:"),0,wx.ALIGN_CENTER_VERTICAL,1)
        self.wSelectModule = wx.ComboBox(self.wNotebookPageScript,style=wx.CB_DROPDOWN|wx.CB_READONLY|wx.CB_SIMPLE)
        sizer_1.Add(self.wSelectModule,1,wx.EXPAND,0)
        sizer_0.Add( wxtools.grid_layout( [ [self.wScript,2,wx.EXPAND], [sizer_1,0,wx.EXPAND] ]
                                , ncols=1
                                , growable_rows=[0]
                                , growable_cols=[0] )
                   , 1, wx.EXPAND, 0 )     
    
        #self.wNotebookPageRetrieve
        self.fixed_pitch_font = wx.Font(10,wx.FONTFAMILY_MODERN,wx.FONTSTYLE_NORMAL,wx.FONTWEIGHT_NORMAL)
        
        sizer_1 = wxtools.create_staticbox(self.wNotebookPageRetrieve, "Retrieved jobs")
        self.wJobsRetrieved = wx.TextCtrl(self.wNotebookPageRetrieve, style=wx.TE_MULTILINE|wx.EXPAND|wx.TE_MULTILINE|wx.HSCROLL|wx.TE_DONTWRAP|wx.TE_READONLY)
        self.wJobsRetrieved.SetDefaultStyle(wx.TextAttr(font=self.fixed_pitch_font))
        sizer_1.Add(self.wJobsRetrieved,1,wx.EXPAND,0) 
        sizer_2 = wxtools.create_staticbox(self.wNotebookPageRetrieve, "Submitted jobs")
        self.wJobsSubmitted = wx.TextCtrl(self.wNotebookPageRetrieve, style=wx.TE_MULTILINE|wx.EXPAND|wx.TE_MULTILINE|wx.HSCROLL|wx.TE_DONTWRAP|wx.TE_READONLY)
        self.wJobsSubmitted.SetDefaultStyle(wx.TextAttr(font=self.fixed_pitch_font))
        sizer_2.Add(self.wJobsSubmitted,1,wx.EXPAND,0) 
        self.wRetrieveJobs2 = wx.Button(self.wNotebookPageRetrieve,label="Retrieve results of submitted jobs")
        
        self.wRefresh = wx.Button(self.wNotebookPageRetrieve,label="Refresh job status")
        self.wDeleteJob = wx.Button(self.wNotebookPageRetrieve,label="Delete selected job")
        #self.wAddJob = wx.Button(self.wNotebookPageRetrieve,label="Add a job to retrieve")
        sizer_3 = wx.BoxSizer(wx.HORIZONTAL)
        sizer_3.Add(self.wRefresh      ,1,wx.EXPAND,0)
        sizer_3.Add(self.wRetrieveJobs2,1,wx.EXPAND,0)
        sizer_3.Add(self.wDeleteJob    ,1,wx.EXPAND,0)
        #sizer_3.Add(self.wAddJob       ,1,wx.EXPAND,0)
        self.wNotebookPageRetrieve.SetSizer( wxtools.grid_layout( [ [sizer_1,1,wx.EXPAND]
                                                          , [sizer_2,1,wx.EXPAND]
                                                          , [sizer_3,1,wx.EXPAND]
                                                          ]
                                                          , ncols=1
                                                          , growable_rows=[0,1]
                                                          , growable_cols=[0] ) )
        # Setup the Menu
        menu_bar = wx.MenuBar()
        self.SetMenuBar(menu_bar)
        self.Bind(wx.EVT_MENU, self.on_EVT_MENU)
        self.menu_actions = {}
        # Tools Menu
        menu_ssh = wx.Menu()
        menu_bar.Append(menu_ssh, "&SSH")
        menu_ssh.Append(LauncherVC.ID_MENU_SSH_PREFERENCES     ,"Preferences\tCtrl+P")
        menu_ssh.Append(LauncherVC.ID_MENU_SSH_CONNECT         ,"Retry to connect\tCtrl+R")
        menu_ssh.Append(LauncherVC.ID_MENU_LOGIN_NODE          ,"Select login node\tCtrl+L")
        menu_ssh.Append(LauncherVC.ID_MENU_CLUSTER_MODULE_ADDED,"Check for new cluster modules\tCtrl+M")

        menu_launcher = wx.Menu()
        menu_bar.Append(menu_launcher, "&Launcher")
        menu_launcher.Append(LauncherVC.ID_MENU_CHECK_FOR_UPDATE,"Check for updates")

    def set_ssh_preferences(self):
        with log.LogItem('Viewing/setting SSH preferences'):
            ok = False
            while not ok:
                self.dlg = sshtools.SshPreferencesDialog(self)
                answer = self.dlg.ShowModal()
                if answer==wx.ID_OK:
                    try:
                        keep_ssh_preferences = self.dlg.update_preferences()
                    except:
                        pass
                    else:
                        ok = True
                        if keep_ssh_preferences:
                            with log.LogItem("Storing SSH preferences in config file:"):
                                self.config.attributes['SSH_PREFERENCES'] = sshtools.SSH_PREFERENCES
                else:
                    ok = True
                del self.dlg
        
    def initialize_data(self):
        self.model = LauncherM.LauncherM()

        self.config = LauncherM.Config(must_load=self.load_config, must_save=self.save_config)
        
        #set the window size
        frame_size = self.config.attributes.get("frame_size",(500,500))
        self.SetSize(frame_size)
        
        self.walltime_units    = ['seconds','minutes','hours','days ']
        self.walltime_unit_max = [ 60*60   , 60*24   , 24*7  , 7    ]
        self.walltime_unit_sec = [ 1       , 60      , 3600  , 86400]
        self.wWalltimeUnit.SetItems(self.walltime_units)
        i_unit = self.walltime_units.index(self.config.attributes['walltime_unit'])
        self.wWalltimeUnit.Select(i_unit)
        self.wWalltime.SetRange(1,self.walltime_unit_max[i_unit]) 
        self.wWalltime.SetValue( self.config.attributes['walltime_value'] )
        self.walltime_seconds = self.wWalltime.GetValue()*self.walltime_unit_sec[i_unit]

        self.wNotifyAddress.ChangeValue(self.config.attributes['mail_to'])

        self.local_parent_folder = self.config.attributes['local_files']
        self.wLocalFileLocation.ChangeValue(self.local_parent_folder)

        self.wUserId.ChangeValue(self.config.attributes['user_id'])
        self.wRemoteSubfolder.ChangeValue(self.config.attributes['remote_subfolder'])

        my_ssh_preferences = self.config.attributes.get("SSH_PREFERENCES",None)
        if my_ssh_preferences:
            sshtools.SSH_PREFERENCES = my_ssh_preferences
            
        #inject 
        for k,w in self.__dict__.iteritems(): #for each widget w  
            if k[0]=='w' and k[1].isupper():
                if isinstance(w,(wx.TextCtrl,xx.SpinCtrl)):
                    #create a Value object 
                    v = Value(w.GetValue())
                    #inject it in the model and in the widget
                    setattr( self.model, 'm_'+k, v )
                    #inject it in the model and in the widget
                    w.model_value = v
                    #inject transfer methods in the widget
                    w.viewer_to_model = types.MethodType( viewer_to_model, w )
                    w.model_to_viewer = types.MethodType( model_to_viewer, w )
                elif isinstance(w,wx.ComboBox):
                    #create a Value object 
                    v = Value(w.GetItems())
                    #inject it in the model and in the widget
                    setattr( self.model, 'm_'+k, v )
                    #inject it in the model and in the widget
                    w.model_value = v
                    #inject transfer methods in the widget
                    w.viewer_to_model = types.MethodType( viewer_to_model, w )
                    w.model_to_viewer = types.MethodType( model_to_viewer, w )
                    
        self.model.initialize_value_objects()

    ### event handlerss ###        
    def on_EVT_MENU(self, event):
        """Handle menu clicks"""
        event_id = event.GetId()
        if event_id==LauncherVC.ID_MENU_SSH_PREFERENCES:
            self.set_ssh_preferences()
        if event_id==LauncherVC.ID_MENU_SSH_CONNECT:
            sshtools.Client(self.get_user_id(),self.login_node,force=True)
            # the object is not stored because it is not used 
        if event_id==LauncherVC.ID_MENU_LOGIN_NODE:
            self.select_login_node()
        if event_id==LauncherVC.ID_MENU_CLUSTER_MODULE_ADDED:
            self.update_cluster_list()
            msg = "These cluster modules are installed in folder\n"+self.model.clusters.clusters_folder+":\n"
            for m in self.wCluster.GetItems():
                msg += "\n  - "+m+".py"
            answer = wx.MessageBox(msg, 'Cluster modules',wx.OK | wx.ICON_INFORMATION)
        if event_id==LauncherVC.ID_MENU_CHECK_FOR_UPDATE:
            self.check_for_updates()
        else:
            raise Exception("Unknown menu event id:"+str(event_id))
             
    def on_EVT_CLOSE(self,event=None):
        self.save_job()
        self.log_event(event)
        self.dump('Before closing Launcher Frame')
        with log.LogItem('Launcher closing'):
            self.config.attributes['frame_size'] = self.GetSize()
            print('    Saving config file.')
            self.Destroy()

    def wCluster_EVT_COMBOBOX(self,event):
        self.log_event(event)
        self.model.change_cluster(self.wCluster.GetValue())

    def wNodeSet_EVT_COMBOBOX(self,event):
        self.log_event(event)
        self.model.change_node_set(self.wNodeSet.GetSelection())

    def wNotebook_EVT_NOTEBOOK_PAGE_CHANGING(self,event):
        #see http://wiki.wxpython.org/Notebooks        
        old_page = event.GetOldSelection()
        new_page = event.GetSelection()
        selected = self.wNotebook.GetSelection()
        msg='wNotebook_EVT_NOTEBOOK_PAGE_CHANGING():  old_page:%d, new_page:%d, selected:%d' % (old_page, new_page, selected)
        self.log_event(event,msg=msg)
        if old_page==0:
            self.model.update_script_from_resources()
        elif old_page==1:
            self.model.update_resources_from_script()
        event.Skip()

    def wNotebook_EVT_NOTEBOOK_PAGE_CHANGED(self,event):        
        old_page = event.GetOldSelection()
        new_page = event.GetSelection()
        selected = self.wNotebook.GetSelection()
        msg='wNotebook_EVT_NOTEBOOK_PAGE_CHANGED():  old_page:%d, new_page:%d, selected:%d' % (old_page, new_page, selected)
        self.log_event(event,msg=msg)
        if new_page==2:
            self.wJobsRetrieved_EVT_SET_FOCUS("wNotebook_EVT_NOTEBOOK_PAGE_CHANGED() calling wJobsRetrieved_EVT_SET_FOCUS()")
        event.Skip()

    def wNNodesRequested_EVT_SPINCTRL(self,event):
        self.log_event(event)
        self.model.change_n_nodes_requested(self.wNNodesRequested.GetValue())

    def wNCoresPerNodeRequested_EVT_SPINCTRL(self,event):
        self.log_event(event)
        self.model.change_n_cores_per_node_requested(self.wNCoresPerNodeRequested.GetValue())

    def wNCoresRequested_EVT_SPINCTRL(self,event):
        self.log_event(event)
        self.model.change_n_cores_requested(self.wNCoresRequested.GetValue())
        
    def wGbPerCoreRequested_EVT_SPINCTRL(self,event):
        self.log_event(event)
        self.model.change_gb_per_core_requested(self.wGbPerCoreRequested.GetValue())
        
#     def update_view(self,page):
#         current_page = self.wNotebook.GetSelection()
#         if current_page==0:
            
#todo split over vc and m       
    def save_job(self,mode='w'):
        """
        mode = 'w'  : ask permission for overwrite
               'w+' overwrite
        """
        done = False
        while not done:
            try: 
                self.model.save_job(mode)
                done = True
            except LauncherM.AskPermissionToOverwrite:
                if self.ask_permission_to_overwrite():
                    mode = 'w+'
        
    
    def ask_permission_to_overwrite(self):
        old_job_name = self.wJobName.GetValue()
        msg="This folder already contains a job script, are your sure to overwrite it?\n"\
            "- Press OK to overwrite this job script\n"\
            "- Change the job name below and press OK to save the script under a different job name\n"\
            "- Press Cancel to not save the job script to disk."
        self.dlg = wx.TextEntryDialog(self,msg,caption="Save your job script",defaultValue=old_job_name)
        res = self.dlg.ShowModal()
        if res==wx.ID_OK:
            new_job_name = self.dlg.GetValue()
            del self.dlg
            if new_job_name==old_job_name:
                #update and save the job script (overwrite)
                if self.is_resources_modified:
                    self.update_script_from_resources()
                else:
                    self.update_resources_from_script()
                return True #permission given to overwrite
            else:
                #a new job name was specified
                self.wJobName.ChangeValue(new_job_name)
                self.job_name_has_changed(load=False) 
                if os.path.exists(os.path.join(self.wLocalFileLocation.GetValue(),'pbs.sh')):
                    #we will still be overwriting, continue to ask if that is ok
                    return self.ask_permission_to_overwrite()
        else:
            return False
           
            
            
    
    def dump(self,msg=''):
        """
        write all control values and data values 
        """
        with log.LogItem("Dump of all control values: "+msg):
            for k,v in self.__dict__.iteritems():
                if k[0]=='w' and k[1].isupper(): # a control
                    try:
                        value = v.GetValue()
                        if not value:
                            if isinstance(v, wx.CheckBox):
                                value='False'
                            if isinstance(v, wx.ComboBox):
                                value=str(v.GetItems())
                            else:
                                value = "'?'"
                        print()
                        print_line(title=k)
                        print(Indent(value,indent=4))
                    except AttributeError:
                        pass
                    v.SetName(k)
                else:                           # a data member
                    print()
                    print_line(title=k)
                    print(Indent(v,indent=4))
            print_line()

def print_line(title="",line_length=80,n_white=1,line_char='-',indent=4):
    if title:
        line = (indent-n_white)*line_char + (n_white*' ') + title + (n_white*' ')
        l = len(line)
        line += (line_length-l)*line_char
    else:
        line = line_length*line_char
    print(line)
            