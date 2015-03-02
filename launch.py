from __future__ import print_function
import shutil
import math,re,os,pickle,random,pprint,errno,sys,cStringIO,datetime,argparse,subprocess

import paramiko
import pbs

import wx
from BashEditor import PbsEditor

"""
    todo suggesties door franky
    - [DONE] $VSC_HOME weglaten 
    - [DONE] move results from VSC_SCRATCH to VSC_DATA
    todo
    - improve the log
    - [DONE] don't show the log pane by default. instead make it accessible 
      via menu
    - test on windows
    - test on linux ubuntu
    - document installation procedure
"""
######################
### some constants ###
######################
SSH_TIMEOUT = 15
#   Number of seconds that paramiko.SSHClient.connect attempts to
#   to connect.
SSH_WAIT = 30
#   the minimum number of sceconds between two successive attempts 
#   to make an ssh connection if the first attempt was unsuccesful.

# return codes for finished job retrieval (FJR):
FJR_SUCCESS      =0
FJR_INEXISTING   =1
FJR_NOT_FINISHED =2
FJR_NO_CONNECTION=3
FJR_ERROR        =4


#####################################
### some wx convenience functions ###
#####################################
def add_wNotebook_page(wNotebook,title):
    """
    Add a notebook page to a wx.Notebook
    Return the notebook page object (wx.Panel)
    """
    page = wx.Panel(wNotebook, wx.ID_ANY)
    wNotebook.AddPage(page, title)
    return page

def add_staticbox(parent,label='',orient=wx.VERTICAL):
    """
    return a wx.StaticBoxSizer containing a wx.StaticBox 
    """
    staticbox = wx.StaticBox(parent, wx.ID_ANY, label=label)
    staticbox.Lower()
    sizer = wx.StaticBoxSizer(staticbox, orient=orient)
    return sizer

def grid_layout(items,ncols=0,gap=0,growable_rows=[],growable_cols=[]):
    """
    return a wx.FlexGridSizer with ncols colums and enough rows to 
    contain all items in the items list. Growable rows and columns 
    can be set.
    ncols=0 corresponds to all items on a single row. To put all 
    items on a single column specify ncols=1.
    """
    nitems = len(items)
    ncols_ = ncols if ncols>0 else nitems 
    nrows_ = int(math.ceil(float(nitems)/ncols_))
    szr = wx.FlexGridSizer(nrows_,ncols_,gap,gap)
    for r in growable_rows:
        if r<nrows_:
            szr.AddGrowableRow(r)
    for c in growable_cols:
        if c<ncols_:
            szr.AddGrowableCol(c)
    for item in items:
        if isinstance(item, list):
            szr.Add(*item)
        else:
            szr.Add(item,0,0)
    return szr

def is_valid_mail_address(address):
    s = address.lower()
    pattern=re.compile(r'\b[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,4}\b')
    match = pattern.match(s)
    return s if match else False

class ContainsString(object):
    """
    Functor that tests if the argument contains a string
    """
    def __init__(self,look_for):
        self.look_for = look_for
    def __call__(self,string):
        return self.look_for in string

def my_makedirs(path):
    """
    http://stackoverflow.com/questions/273192/check-if-a-directory-exists-and-create-it-if-necessary
    """
    try:
        os.makedirs(path)
    except OSError as exception:
        if exception.errno != errno.EEXIST:
            raise

def make_tree_output_relative(arg):
    """
    helper function that transforms the output of the linux cmd 
        > tree -fi [-d] --noreport
    (which is a list of lines containing paths to files and/or folders starting with './'
    and ending with '\n')
    by
        - removing the home folder '.'
        - removing './' at the beginning of each path
        - removing the trailing newline
    """
    clean = []
    for a in arg:
        if a == ".\n":
            continue
        clean.append(a[2:-1])        
    return clean

################################################################################
### Launcher classes                                                         ###
################################################################################
class Config(object):
    """
    Launcher configuration
    """
    def __init__(self):
        home = os.environ['HOME']
        if not home or not os.path.exists(home):
            msg = "Error: environmaent variable '$HOME' not found."
            raise EnvironmentError(msg)
        home = os.path.join(home,'Launcher')
        if not os.path.exists(home):
            os.mkdir(home)
        self.launcher_home =  home
        cfg_file = os.path.join(home,'Launcher.config')

        try:
            self.attributes = pickle.load(open(cfg_file))
        except:
            self.attributes = {}
        default_attributes = { 'cluster' : 'Hopper'
                             , 'mail_to' : 'your mail address goes here'
                             , 'walltime_unit' : 'hours'
                             , 'walltime_value' : 1
                             , 'local_files' : os.path.join(home,'jobs')
                             , 'user_id' : 'your_user_id'
                             , 'remote_subfolder' : '.'
                             , 'module_lists':{}
                             , 'submitted_jobs':{}
                             }
#         if 'submitted' in self.attributes:
#             del self.attributes['submitted']
        self.add_default_attributes(default_attributes)
        self.home = home
        self.cfg_file = cfg_file
        
    def add_default_attributes(self,default_attributes):
        for k,v in default_attributes.iteritems():
            if not k in self.attributes:
                self.attributes[k] = v

    def save(self):
        pickle.dump(self.attributes,open(self.cfg_file,'w+'))

class Launcher(wx.Frame):
    """
    the Launcher gui
    """    
    def __init__(self, parent, title):
        super(Launcher,self).__init__(parent,title=title,size=wx.Size(600,670))

        self.config = Config()
        self.open_log_file()
        
        self.init_ui()
        self.set_names()
        
        self.Bind(wx.EVT_CLOSE, self.close)
        self.bind('wCluster'                , 'EVT_COMBOBOX')
        self.bind('wNodeSet'                , 'EVT_COMBOBOX')
        self.bind('wNNodesRequested'        , 'EVT_SPINCTRL')
        self.bind('wNCoresPerNodeRequested' , 'EVT_SPINCTRL')
        self.bind('wNCoresRequested'        , 'EVT_SPINCTRL')
        self.bind('wGbPerCoreRequested'     , 'EVT_SPINCTRLDOUBLE')
        self.bind('wNodeSet'                , 'EVT_COMBOBOX')
        self.bind('wSelectModule'           , 'EVT_COMBOBOX')
        self.bind('wScript'                 , 'EVT_SET_FOCUS')
        self.bind('wScript'                 , 'EVT_KILL_FOCUS')
        self.bind('wWalltime'               , 'EVT_SPINCTRL')
        self.bind('wWalltimeUnit'           , 'EVT_COMBOBOX')
        self.bind('wNotifyAbort'            , 'EVT_CHECKBOX')
        self.bind('wNotifyBegin'            , 'EVT_CHECKBOX')
        self.bind('wNotifyEnd'              , 'EVT_CHECKBOX')
        self.bind('wNotifyAddress'          , 'EVT_TEXT_ENTER')
        self.bind('wJobName'                , 'EVT_TEXT')
        self.bind('wRemoteSubfolder'        , 'EVT_TEXT')
        self.bind('wUserId'                 , 'EVT_TEXT_ENTER')
        #self.bind('wLoadJob'                , 'EVT_BUTTON')
        self.bind('wSaveJob'                , 'EVT_BUTTON',)
        self.bind('wSubmitJob'              , 'EVT_BUTTON')
        self.bind('wRetrieveJobs'           , 'EVT_BUTTON')
        self.bind('wLocalFileLocation'      , 'EVT_SET_FOCUS')
        self.bind('wLocalFileLocation'      , 'EVT_TEXT')
        self.bind('wLocalFileLocation'      , 'EVT_TEXT_ENTER')
        self.bind('wLocalFileLocation'      , 'EVT_CHAR_HOOK')
        self.bind('wRemoteLocation'         , 'EVT_COMBOBOX')
        self.bind('wJobsRetrieved'          , 'EVT_SET_FOCUS')
        self.bind('wRefresh'                , 'EVT_BUTTON')
        self.bind('wRetrieveJobs2'          , 'EVT_BUTTON')
        self.bind('wDeleteJob'              , 'EVT_BUTTON')
        self.Show()
        self.InitData()
    
    def bind(self,object_name,event_name):
        """
        Basically a wrapper of wx.Object.Bind
        helper function that binds an event handler (a Launcher method) to a
        object.
        - the object is self.<object_name> 
        - the event is <event_name>
        - the event handler is self.<object_name>_<event_name>
        """
        s = "self.{}.Bind(wx.{},self.{}_{})".format(object_name,event_name,object_name,event_name)
        eval(s)
        
    def set_names(self):
        """
        loop over self's attributes and if the attribute name matches the pattern
        for a wx object (starts with 'w' followed by a capital, e.g. wNotebook)
        sets the attribute's name. These names are used for logging events 
        """
        for k,v in self.__dict__.iteritems():
            if k[0]=='w' and k[1].isupper():
                v.SetName(k)
            
    def log_event(self,event):
        """
        log information about the event
        """
        if isinstance(event,(str,unicode)):
            print(event)
        else:
            print("\nEvent.type ",type(event))
            print(  "     .object.name :",event.GetEventObject().GetName())
            print(  "     .object.class:",event.GetEventObject().GetClassName())
        self.SetStatusText("")
    
    ### event handlerss ###
    def wCluster_EVT_COMBOBOX(self,event):
        self.log_event(event)
        self.cluster_has_changed()
        self.is_resources_modified = True
    
    def wNodeSet_EVT_COMBOBOX(self,event):
        self.log_event(event)
        self.node_set_has_changed()
        self.is_resources_modified = True

    def wNNodesRequested_EVT_SPINCTRL(self,event):
        self.log_event(event)
        self.request_nodes_cores()
        self.is_resources_modified = True
        
    def wNCoresPerNodeRequested_EVT_SPINCTRL(self,event):
        self.log_event(event)
        self.request_nodes_cores()
        self.is_resources_modified = True
        
    def wNCoresRequested_EVT_SPINCTRL(self,event):
        self.log_event(event)
        self.request_cores_memory()
        self.is_resources_modified = True
        
    def wGbPerCoreRequested_EVT_SPINCTRLDOUBLE(self,event):
        self.log_event(event)
        self.request_cores_memory()
        self.is_resources_modified = True
 
    def wSelectModule_EVT_COMBOBOX(self,event):
        self.log_event(event)
        self.add_module()
        
    def wScript_EVT_SET_FOCUS(self,event):
        self.log_event(event)
        self.update_script_from_resources()
        event.Skip()
        
    def wScript_EVT_KILL_FOCUS(self,event):
        self.log_event(event)
#         print '  Script is modified?',self.wScript.IsModified()
        self.update_resources_from_script(event)
        
    def wWalltime_EVT_SPINCTRL(self,event):
        self.log_event(event)
        self.walltime_has_changed()
        self.is_resources_modified = True
        
    def wWalltimeUnit_EVT_COMBOBOX(self,event):
        self.log_event(event)
        self.walltime_unit_has_changed(event)
        
    def wNotifyAbort_EVT_CHECKBOX(self,event):
        self.log_event(event)
        self.notify_address_has_changed(event)
        self.is_resources_modified = True
        
    def wNotifyBegin_EVT_CHECKBOX(self,event):
        self.log_event(event)
        self.notify_address_has_changed(event)
        self.is_resources_modified = True
        
    def wNotifyEnd_EVT_CHECKBOX(self,event):
        self.log_event(event)
        self.notify_address_has_changed(event)
        self.is_resources_modified = True
        
    def wNotifyAddress_EVT_TEXT_ENTER(self,event):
        self.log_event(event)
        self.notify_address_has_changed(event)
        self.is_resources_modified = True
        
    def wJobName_EVT_TEXT(self,event):
        self.log_event(event)
        self.job_name_has_changed()
        self.is_resources_modified = True
        
    def wRemoteSubfolder_EVT_TEXT(self,event):
        self.log_event(event)
        self.remote_subfolder_has_changed()
        
    def wUserId_EVT_TEXT_ENTER(self,event):
        self.log_event(event)
        self.user_id_has_changed()
                
    def wSaveJob_EVT_BUTTON(self,event):
        self.log_event(event)
        self.save_job()
        
    def wSubmitJob_EVT_BUTTON(self,event):
        self.log_event(event)
        self.submit_job()
        
    def wRetrieveJobs_EVT_BUTTON(self,event):
        self.log_event(event)
        self.retrieve_finished_jobs()

    def wRefresh_EVT_BUTTON(self,event):
        self.log_event(event)
        self.show_submitted_jobs()

    def wRetrieveJobs2_EVT_BUTTON(self,event):
        self.log_event(event)
        self.retrieve_finished_jobs()
        self.wJobsRetrieved_EVT_SET_FOCUS("wRetrieveJobs2_EVT_BUTTON calling wJobsRetrieved_EVT_SET_FOCUS")
        
    def wDeleteJob_EVT_BUTTON(self,event):
        self.log_event(event)
        selected_text = self.wJobsSubmitted.GetStringSelection()
        re_job_id = re.compile(r'job_id=(\d+)')
        n_selected = 0
        for m in re_job_id.finditer(selected_text):
            n_selected+=1
            job_id = m.group(1)
            title = "Delete job {}?".format(job_id)
            answer = wx.MessageBox('Delete this job from the retrieve list?',title,wx.YES|wx.NO | wx.ICON_QUESTION)
            if answer==wx.YES:
                del self.config.attributes['submitted_jobs'][job_id]
                self.set_status_text("\nDeleted job {} from list of retrieved jobs.")
        if n_selected==0:
            msg = "To delete one or more jobs from the retrieve list, you must select at least the entire first line of the job(s)."
            answer = wx.MessageBox(msg, 'No job was selected.',wx.OK | wx.ICON_INFORMATION)
        self.show_submitted_jobs()
            
    def wJobsRetrieved_EVT_SET_FOCUS(self,event):
        self.log_event(event)
        self.show_retrieved_jobs()
        self.show_submitted_jobs()

    def wLocalFileLocation_EVT_SET_FOCUS(self,event):
        self.log_event(event)
        self.local_parent_folder_set_focus()
        
    def wLocalFileLocation_EVT_TEXT(self,event):
        self.log_event(event)
        self.local_parent_folder_text(event)
        
    def wLocalFileLocation_EVT_TEXT_ENTER(self,event):
        self.log_event(event)
        self.local_parent_folder_has_changed()
        
    def wLocalFileLocation_EVT_CHAR_HOOK(self,event):
        self.log_event(event)
        self.local_parent_folder_key_up(event)

    def wRemoteLocation_EVT_COMBOBOX(self,event):
        self.log_event(event)
        self.remote_location_has_changed()

    def close(self,event=None):
        self.log_event(event)
        print('\nlauncher closing - '+str(datetime.datetime.now()))
        print('\nsaving config file.')
        self.config.save()
        self.Destroy()
        print('\nlaunch-2.py - end of log')
            
    ### the widgets ###
    def init_ui(self):        
        self.CreateStatusBar()
        self.panel = wx.Panel(self)
        self.main_sizer = wx.BoxSizer(wx.VERTICAL)
        self.panel.SetSizer(self.main_sizer)
        
        self.wNotebook = wx.Notebook(self.panel, wx.ID_ANY)
        self.main_sizer.Add(self.wNotebook, 1, wx.EXPAND, 0)
        
        self.wNotebookPageResources = add_wNotebook_page(self.wNotebook, 'Resources')
        self.wNotebookPageScript    = add_wNotebook_page(self.wNotebook, "PBS script")
        self.wNotebookPageRetrieve  = add_wNotebook_page(self.wNotebook, "Retrieve")
   
        #self.wNotebookPageResources
        sizer_1 = add_staticbox(self.wNotebookPageResources, "Requests resources from")
        sizer_2a= add_staticbox(self.wNotebookPageResources, "Requests nodes and cores per node")
        sizer_2b= add_staticbox(self.wNotebookPageResources, "Requests cores and memory per core")
        sizer_2 = wx.BoxSizer(wx.HORIZONTAL)
        sizer_2.Add(sizer_2a,1,wx.EXPAND,0)
        sizer_2.Add(sizer_2b,1,wx.EXPAND,0)
        sizer_4 = add_staticbox(self.wNotebookPageResources, "Request compute time")
        sizer_5 = add_staticbox(self.wNotebookPageResources, "Notify")
        sizer_6 = add_staticbox(self.wNotebookPageResources, "Job name (=used as the parent folder)")
        sizer_7 = add_staticbox(self.wNotebookPageResources, "Local file location")
        sizer_8 = add_staticbox(self.wNotebookPageResources, "Remote file location",orient=wx.VERTICAL)
        sizer_9 = add_staticbox(self.wNotebookPageResources, "Job management",orient=wx.HORIZONTAL)
        self.wNotebookPageResources.SetSizer( grid_layout( [ [sizer_1,1,wx.EXPAND]
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
        cluster = self.config.attributes['cluster']
        self.wCluster = wx.ComboBox(self.wNotebookPageResources, wx.ID_ANY
                                   , choices=pbs.clusters
                                   , value=cluster
                                   , style=wx.CB_DROPDOWN|wx.CB_READONLY|wx.CB_SIMPLE
                                   )
        self.wNodeSet = wx.ComboBox(self.wNotebookPageResources, wx.ID_ANY, choices=["thin_nodes (64GB)", "fat_nodes (256GB)"]
                                   , style=wx.CB_DROPDOWN|wx.CB_READONLY|wx.CB_SIMPLE|wx.EXPAND
                                   )
        #h = self.wNodeSet.GetSize()[1]
        sizer_1.Add(grid_layout([self.wCluster,[self.wNodeSet,1,wx.EXPAND]],growable_cols=[1]),1,wx.EXPAND,0 )        
        #sizer_2
        lst3 = []
        lst3.append( wx.StaticText    (self.wNotebookPageResources,label='Number of cores:') )
        lst3.append( wx.SpinCtrl      (self.wNotebookPageResources ) )
        lst3.append( wx.StaticText    (self.wNotebookPageResources,label='Memory per core [GB]:') )
        lst3.append( wx.SpinCtrlDouble(self.wNotebookPageResources ) )
        lst3.append( wx.StaticText    (self.wNotebookPageResources,label='Memory total [GB]:') )
        lst3.append( wx.TextCtrl      (self.wNotebookPageResources,style=wx.TE_READONLY|wx.TE_RIGHT) )
        self.wNCoresRequested    = lst3[1]
        self.wGbPerCoreRequested = lst3[3]
        self.wGbTotalGranted     = lst3[5]
        lst3[4]               .SetForegroundColour(wx.TheColourDatabase.Find('GREY'))
        self.wGbTotalGranted  .SetForegroundColour(wx.TheColourDatabase.Find('GREY'))
        h = lst3[0].GetSize()[1]
        w = 0
        for i in lst3[0::2]:
            w = max(w,i.GetSize()[0])
        #sizer_3
        lst4 = []
        lst4.append( wx.StaticText (self.wNotebookPageResources,label='Number of nodes:') )
        lst4.append( wx.SpinCtrl   (self.wNotebookPageResources ) )
        lst4.append( wx.StaticText(self.wNotebookPageResources,label='Cores per node:') )
        lst4.append( wx.SpinCtrl  (self.wNotebookPageResources ) )
        lst4.append( wx.StaticText    (self.wNotebookPageResources,label='Memory per node [GB]:') )
        lst4.append( wx.TextCtrl      (self.wNotebookPageResources,style=wx.TE_READONLY|wx.TE_RIGHT) )
        self.wNNodesRequested        = lst4[1]
        self.wNCoresPerNodeRequested = lst4[3]
        self.wGbPerNodeGranted       = lst4[5]
        lst4[4]               .SetForegroundColour(wx.TheColourDatabase.Find('GREY'))
        self.wGbPerNodeGranted.SetForegroundColour(wx.TheColourDatabase.Find('GREY'))
        for i in lst4[0::2]:
            w = max(w,i.GetSize()[0])
        for i in lst3[0::2]:
            i.SetMinSize(wx.Size(w,h))
        for i in lst4[0::2]:
            i.SetMinSize(wx.Size(w,h))
        #(interchange position of 3 and 4) 
        sizer_2b.Add(grid_layout(lst3,ncols=2),0,wx.EXPAND,0 )
        sizer_2a.Add(grid_layout(lst4,ncols=2),0,wx.EXPAND,0 )   
        #sizer_4
        self.walltime_units    = ['seconds','minutes','hours','days ']
        self.walltime_unit_max = [ 60*60   , 60*24   , 24*7  , 7    ]
        self.walltime_unit_sec = [ 1       , 60      , 3600  , 86400]
        self.wWalltime     = wx.SpinCtrl(self.wNotebookPageResources) 
        self.wWalltimeUnit = wx.ComboBox(self.wNotebookPageResources,choices=self.walltime_units)
        i_unit = self.walltime_units.index(self.config.attributes['walltime_unit'])
        self.wWalltimeUnit.Select(i_unit)
        self.wWalltime.SetRange(1,self.walltime_unit_max[i_unit]) 
        self.wWalltime.Value = self.config.attributes['walltime_value']
        self.walltime_seconds = self.wWalltime.Value*self.walltime_unit_sec[i_unit]
        
        lst = [ wx.StaticText(self.wNotebookPageResources,label='wall time:') ]
        lst.append( self.wWalltime )
        lst.append( self.wWalltimeUnit )
        lst[0].SetMinSize(wx.Size(w,h))
        sizer_4.Add(grid_layout(lst),0,wx.EXPAND,0 )
        #sizer_5
        sizer_5a = wx.BoxSizer(wx.HORIZONTAL)
        self.wNotifyAddress      = wx.TextCtrl(self.wNotebookPageResources,style=wx.EXPAND|wx.TE_PROCESS_ENTER|wx.TE_PROCESS_TAB,value=self.config.attributes['mail_to'])
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
        
#         lst = [[self.wNotifyAddress,1,wx.EXPAND], self.wNotifyBegin, self.wNotifyEnd, self.wNotifyAbort]
#         sizer_5.Add(grid_layout(lst,ncols=1,growable_cols=[0]),1,wx.EXPAND,0 )
        if not is_valid_mail_address(self.wNotifyAddress.Value):
            self.wNotifyAddress.SetForegroundColour(wx.TheColourDatabase.Find('GREY'))
        #sizer_6
        self.wJobName = wx.TextCtrl(self.wNotebookPageResources,style=wx.EXPAND|wx.TE_PROCESS_ENTER|wx.TE_PROCESS_TAB)
        sizer_6.Add(self.wJobName,1,wx.EXPAND,0)
        #sizer_7
        self.local_parent_folder = self.config.attributes['local_files']
        self.wLocalFileLocation = wx.TextCtrl(self.wNotebookPageResources,value=self.local_parent_folder)
        sizer_7.Add(self.wLocalFileLocation,1,wx.EXPAND,0)
        #sizer_8
        self.wUserId   = wx.TextCtrl(self.wNotebookPageResources,value=self.config.attributes['user_id'],style=wx.TE_PROCESS_ENTER)
        self.wRemoteLocation = wx.ComboBox(self.wNotebookPageResources,choices=['$VSC_SCRATCH','$VSC_DATA'])
        self.wRemoteSubfolder= wx.TextCtrl(self.wNotebookPageResources,value=self.config.attributes['remote_subfolder'])
        lst1 = [ wx.StaticText(self.wNotebookPageResources,label="user id:")
               , wx.StaticText(self.wNotebookPageResources,label="location:")
               , wx.StaticText(self.wNotebookPageResources,label="subfolder:")
               ,[self.wUserId   ,0,wx.EXPAND]
               ,[self.wRemoteLocation ,0,wx.EXPAND]
               ,[self.wRemoteSubfolder,1,wx.EXPAND]                
               ]
        self.wRemoteFileLocation = wx.TextCtrl(self.wNotebookPageResources,style=wx.TE_READONLY)
        self.wRemoteFileLocation.SetForegroundColour(wx.TheColourDatabase.Find('GREY'))
        lst2 = [ [grid_layout(lst1, 3, growable_cols=[2]),1,wx.EXPAND]
               , [self.wRemoteFileLocation,1,wx.EXPAND]
               ]
        sizer_8.Add( grid_layout(lst2, 1, growable_cols=[0]),1,wx.EXPAND,0)
        #sizer_9
#         self.wLoadJob   = wx.Button(self.wNotebookPageResources,label="Load")
#         sizer_9.Add(self.wLoadJob   ,1,wx.EXPAND,0)
        self.wSaveJob   = wx.Button(self.wNotebookPageResources,label="Save (to local disk)")
        self.wSubmitJob = wx.Button(self.wNotebookPageResources,label="Submit")
        self.wRetrieveJobs  = wx.Button(self.wNotebookPageResources,label="Retrieve results")
        self.wCopyToLocalDisk = wx.CheckBox(self.wNotebookPageResources,label="Copy to local disk")
        self.wCopyToVSCDATA   = wx.CheckBox(self.wNotebookPageResources,label="Copy to $VSC_DATA")
        self.wCopyToLocalDisk.SetValue(True)
        self.wCopyToVSCDATA  .SetValue(False)
        sizer_9a = wx.BoxSizer(orient=wx.VERTICAL)
        sizer_9a.Add(self.wRetrieveJobs   ,1,wx.EXPAND,0)
        sizer_9a.Add(self.wCopyToLocalDisk,1,wx.EXPAND,0)
        sizer_9a.Add(self.wCopyToVSCDATA  ,1,wx.EXPAND,0)
        sizer_9.Add(self.wSaveJob   ,1,wx.ALIGN_TOP,0)
        sizer_9.Add(self.wSubmitJob ,1,wx.ALIGN_TOP,0)
        sizer_9.Add(sizer_9a        ,1,wx.EXPAND,0)

        #self.wNotebookPageScript
        sizer_0 = add_staticbox(self.wNotebookPageScript, "Job script")     
        self.wNotebookPageScript.SetSizer( grid_layout( [ [sizer_0,1,wx.EXPAND] ]
                                                  , ncols=1
                                                  , growable_rows=[0]
                                                  , growable_cols=[0] ) )
        #sizer_0
        self.wScript = PbsEditor(self.wNotebookPageScript)

        sizer_1 = wx.BoxSizer(wx.HORIZONTAL)
        sizer_1.Add(wx.StaticText(self.wNotebookPageScript,label="Select module:"),0,wx.ALIGN_CENTER_VERTICAL,1)
        self.wSelectModule = wx.ComboBox(self.wNotebookPageScript,style=wx.CB_DROPDOWN|wx.CB_READONLY|wx.CB_SIMPLE)
        sizer_1.Add(self.wSelectModule,1,wx.EXPAND,0)
        sizer_0.Add( grid_layout( [ [self.wScript,2,wx.EXPAND], [sizer_1,0,wx.EXPAND] ]
                                , ncols=1
                                , growable_rows=[0]
                                , growable_cols=[0] )
                   , 1, wx.EXPAND, 0 )     
    
        #self.wNotebookPageRetrieve
        self.fixed_pitch_font = wx.Font(10,wx.FONTFAMILY_MODERN,wx.FONTSTYLE_NORMAL,wx.FONTWEIGHT_NORMAL)
        sizer_1 = add_staticbox(self.wNotebookPageRetrieve, "Retrieved jobs")
        self.wJobsRetrieved = wx.TextCtrl(self.wNotebookPageRetrieve, style=wx.TE_MULTILINE|wx.EXPAND|wx.TE_MULTILINE|wx.HSCROLL|wx.TE_DONTWRAP|wx.TE_READONLY)
        # todo:
        #   wx.HSCROLL|wx.TE_DONTWRAP don't seem to work on OSX. suggest to use StyledTextCtrl which
        #   might als be interesting for wScript
        self.wJobsRetrieved.SetDefaultStyle(wx.TextAttr(font=self.fixed_pitch_font))
        sizer_1.Add(self.wJobsRetrieved,1,wx.EXPAND,0) 
        sizer_2 = add_staticbox(self.wNotebookPageRetrieve, "Submitted jobs")
        
        self.wJobsSubmitted = wx.TextCtrl(self.wNotebookPageRetrieve, style=wx.TE_MULTILINE|wx.EXPAND|wx.TE_MULTILINE|wx.HSCROLL|wx.TE_DONTWRAP|wx.TE_READONLY)
        self.wJobsSubmitted.SetDefaultStyle(wx.TextAttr(font=self.fixed_pitch_font))
        sizer_2.Add(self.wJobsSubmitted,1,wx.EXPAND,0) 
        self.wRetrieveJobs2 = wx.Button(self.wNotebookPageRetrieve,label="Retrieve results of submitted jobs")
        
        self.wRefresh = wx.Button(self.wNotebookPageRetrieve,label="Refresh job status")
        self.wDeleteJob = wx.Button(self.wNotebookPageRetrieve,label="Delete selected job")
        self.wAddJob = wx.Button(self.wNotebookPageRetrieve,label="Add a job to retrieve")
        sizer_3 = wx.BoxSizer(wx.HORIZONTAL)
        sizer_3.Add(self.wRefresh      ,1,wx.EXPAND,0)
        sizer_3.Add(self.wRetrieveJobs2,1,wx.EXPAND,0)
        sizer_3.Add(self.wDeleteJob    ,1,wx.EXPAND,0)
        sizer_3.Add(self.wAddJob       ,1,wx.EXPAND,0)
        self.wNotebookPageRetrieve.SetSizer( grid_layout( [ [sizer_1,1,wx.EXPAND]
                                                          , [sizer_2,1,wx.EXPAND]
                                                          , [sizer_3,1,wx.EXPAND]
                                                          ]
                                                          , ncols=1
                                                          , growable_rows=[0,1]
                                                          , growable_cols=[0] ) )
             
#         # Setup the Menu
#         menu_bar = wx.MenuBar()
#         # Tools Menu
#         tools_menu = wx.Menu()
#         tools_menu.Append(ID_TOOLS_MENU_SHOW_LOG, "Open Log\tCtrl+L")
#         menu_bar.Append(tools_menu, "&Tools")
#         self.SetMenuBar(menu_bar)
#         # Event Handlers
#         self.Bind(wx.EVT_MENU, self.Evt_Menu)
# 
#     def Evt_Menu(self, event):
#         """Handle menu clicks"""
#         evt_id = event.GetId()
#         actions = { ID_TOOLS_MENU_SHOW_LOG  : self.open_log_pane
#                   }
#         action = actions.get(evt_id, None)
#         if action:
#             action()
#             

    def InitData(self):
        items=[]
        for item in pbs.node_sets.iterkeys():
            items.append(item)
        self.wCluster.SetItems(items)
        self.is_script_up_to_date = False
        self.set_cluster(self.config.attributes['cluster'])
        value=self.get_remote_file_location()
        self.wRemoteFileLocation.SetValue(value)
                
        n_waiting = len(self.config.attributes['submitted_jobs'])
        if n_waiting>0:
            msg = "There are {} jobs waiting to be retrieved. Start retrieving finished jobs now?".format(n_waiting)
            answer = wx.MessageBox(msg, 'Retrieve finished jobs?',wx.YES|wx.NO | wx.ICON_QUESTION)
            if answer==wx.YES:
                self.retrieve_finished_jobs()
        else:
            print("\nThere are no jobs waiting to be retrieved.")
    
    def set_default_pbs_parameters(self):
        self.wNNodesRequested.SetValue(1)
        node_set = self.get_node_set()
        self.wNCoresPerNodeRequested.SetValue(node_set.n_cores_per_node)
        self.request_nodes_cores()
        self.wWalltime.SetValue(1)
        self.wWalltimeUnit.Select(self.walltime_units.index('hours'))
        self.wNotifyBegin.SetValue(False)
        self.wNotifyEnd  .SetValue(False)
        self.wNotifyAbort.SetValue(False)

    ### methods used by the event handlers ###
    def open_log_file(self):
        self.log = os.path.join(self.config.launcher_home,'Launcher.log')
        old_log = None
        if os.path.exists(self.log):
            old_log = self.log+str(random.random())[1:]
            shutil.copy(self.log,old_log)

        print('launch-2.py - begin of log - '+str(datetime.datetime.now()))
        print("\nVersion info of used components")
        print("===============================")
        print(sys.version)
        print("wxPython version:",wx.version())
        print("paramiko version:",paramiko.__version__)
        print("Launcher version:",subprocess.check_output(["git","describe","HEAD"]))
        
        
        if old_log:
            print("\nRenaming previous log file 'Launcher.log' to\n    '{}'.".format(old_log))

    def remote_location_has_changed(self):
        if self.wRemoteLocation.GetValue()=="$VSC_DATA":
            self.wCopyToVSCDATA.SetValue(False)
            self.wCopyToVSCDATA.Disable()
        else:
            self.wCopyToVSCDATA.Enable()
        self.get_remote_file_location()
        self.remote_subfolder_has_changed()
        
    def remote_subfolder_has_changed(self):
        v = self.remote_location
        
        remote_subfolder = self.wRemoteSubfolder.Value
        if not remote_subfolder in ['','.','./']:
            v = os.path.join(v,self.wRemoteSubfolder.Value)
            self.remote_location_subfolder = v
    
        jobname = self.wJobName.Value
        if jobname:
            v = os.path.join(v,jobname)
        self.wRemoteFileLocation.SetValue(v)
    
    def job_name_has_changed(self):
        if self.wJobName.Value:
            self.wLocalFileLocation.Value = os.path.join(self.local_parent_folder,self.wJobName.Value)
            self.wLocalFileLocation.SetForegroundColour(wx.BLUE)
            if not hasattr(self,'remote_location_subfolder'):
                self.remote_location_subfolder = self.get_remote_file_location()
            self.wRemoteFileLocation.Value = os.path.join(self.remote_location_subfolder,self.wJobName.Value)
            f_script = os.path.join(self.wLocalFileLocation.Value,'pbs.sh')
            if os.path.exists(f_script):
                msg = "Found script '{}'.\nDo you want to load it?".format(f_script)
                answer = wx.MessageBox(msg, 'Script found',wx.YES|wx.NO | wx.ICON_QUESTION)
                if answer==wx.NO:
                    self.set_status_text("The script was not loaded. It will be overwritten when pressing the 'Save' button.")
                    self.is_script_up_to_date = False
                elif answer==wx.YES:
                    self.load_job()
                    self.set_status_text("Script '{}' is loaded.".format(f_script))
        else:
            self.wLocalFileLocation.Value = self.local_parent_folder
            self.wLocalFileLocation.SetForegroundColour(wx.BLACK)
            self.SetStatusText('')
        
    def local_parent_folder_text(self,event):
        #print 'local_parent_folder_text',event.GetString()
        #todo???
        pass
    
    def local_parent_folder_set_focus(self):
        self.wLocalFileLocation.SetForegroundColour(wx.BLACK)
        self.wLocalFileLocation.Value = self.local_parent_folder
        self.set_status_text('Modify the local location. Do NOT add the Job name.')
        
    def local_parent_folder_has_changed(self):
        if self.wLocalFileLocation.Value:
            v = self.wLocalFileLocation.Value
            vsplit = os.path.split(v)
            if vsplit[-1]==self.wJobName.Value:
                msg = "The parent folder is equal to the job name.\n:Is that OK?"
                answer = wx.MessageBox(msg, 'Warning',wx.NO|wx.YES | wx.ICON_WARNING)
                if  answer==wx.NO:
                    v = os.path.join(*vsplit[:-1])
        else:
            v = self.local_parent_folder
        self.local_parent_folder = v
        if self.wJobName.Value:
            self.wLocalFileLocation.Value = os.path.join(self.local_parent_folder,self.wJobName.Value)
            self.wLocalFileLocation.SetForegroundColour(wx.BLUE)
        else:
            self.wLocalFileLocation.SetForegroundColour(wx.BLACK)
        my_makedirs(self.local_parent_folder)

    def local_parent_folder_key_up(self,event):
        key_code = event.GetKeyCode()
        if key_code == wx.WXK_ESCAPE:
            #print 'escape'
            self.wLocalFileLocation.Value=self.local_parent_folder
            self.local_parent_folder_has_changed()
        elif key_code == wx.WXK_RETURN:
            #print 'enter'
            self.local_parent_folder_has_changed()
        event.Skip()
                               
    def set_cluster(self,cluster):
        if isinstance(cluster,str):
            i = self.wCluster.GetItems().index(cluster)
        else:
            i = cluster
        self.wCluster.Select(i)
        self.cluster_has_changed()
                
    def get_cluster(self):
        return self.wCluster.Value
    
    def cluster_has_changed(self):
        self.set_node_set_items(pbs.node_sets[self.get_cluster()])        
        self.wSelectModule.SetItems(self.get_module_avail())
        self.is_resources_modified = True
            
    def set_node_set_items(self,node_sets):
        if 'node_set' in self.config.attributes:
            config_node_set = self.config.attributes['node_set']
        else:
            config_node_set =''
        select_node_set = 0
        node_set_items=[]
        for node_set in node_sets:
            if node_set.name==config_node_set:
                select_node_set = len(node_set_items)
            node_set_items.append(node_set.name+' ({}Gb/node, {} cores/node)'.format(node_set.gb_per_node,node_set.n_cores_per_node))
        self.wNodeSet.SetItems(node_set_items)
        self.set_node_set(select_node_set)

    def set_node_set(self,node_set):    
        if isinstance(node_set,str):
            i = self.wCluster.GetItems().index(node_set.split()[0])
        else: # type is int 
            i = node_set
        self.wNodeSet.Select(i)
        node_set = self.get_node_set()
        self.wNNodesRequested.SetRange(1,node_set.n_nodes)
        self.wNNodesRequested.Value = 1
        self.wNCoresPerNodeRequested.SetRange(1,node_set.n_cores_per_node)
        self.wNCoresPerNodeRequested.Value = node_set.n_cores_per_node
        self.wGbPerNodeGranted      .Value = str(node_set.gb_per_node)
        self.wGbPerCoreRequested    .SetRange(0,node_set.gb_per_node)
        self.wNCoresRequested       .SetRange(1,node_set.n_cores_per_node*node_set.n_nodes)
        self.request_nodes_cores()
        
    def get_node_set(self):
        node_sets = pbs.node_sets[self.get_cluster()]
        i=self.wNodeSet.Selection
        return node_sets[i]

    def node_set_has_changed(self):
        #todo
        self.is_resources_modified = True
            
    def request_nodes_cores(self):
        node_set = self.get_node_set()
        n_cores,gb_per_core = node_set.request_nodes_cores(self.wNNodesRequested.Value,self.wNCoresPerNodeRequested.Value)
        self.wNCoresRequested   .Value = n_cores
        self.wGbPerCoreRequested.Value = gb_per_core
        self.wGbTotalGranted    .Value = str(gb_per_core*n_cores)

        self.wNNodesRequested       .SetForegroundColour(wx.BLUE)
        self.wNCoresPerNodeRequested.SetForegroundColour(wx.BLUE)        
        self.wNCoresRequested       .SetForegroundColour(wx.BLACK)
        self.wGbPerCoreRequested    .SetForegroundColour(wx.BLACK)
            
    def request_cores_memory(self):
        node_set = self.get_node_set()
        n_nodes, n_cores, n_cores_per_node, gb_per_core, gb = node_set.request_cores_memory(self.wNCoresRequested.Value,self.wGbPerCoreRequested.Value)
        self.wNNodesRequested       .Value = n_nodes
        self.wNCoresRequested       .Value = n_cores
        self.wNCoresPerNodeRequested.Value = n_cores_per_node
        self.wGbPerCoreRequested    .Value = gb_per_core
        self.wGbTotalGranted        .Value = str(gb)

        self.wNNodesRequested       .SetForegroundColour(wx.BLACK)
        self.wNCoresPerNodeRequested.SetForegroundColour(wx.BLACK)        
        self.wNCoresRequested       .SetForegroundColour(wx.BLUE)
        self.wGbPerCoreRequested    .SetForegroundColour(wx.BLUE)
        
    def get_remote_file_location(self):
        ssh = self.open_ssh_connection()
        if not ssh is None:
            try:
                stdout, stderr = self.ssh_command(ssh, 'echo '+self.wRemoteLocation.Value, verbose=True)
                location = stdout.readlines()[0].strip()
                stdout, stderr = self.ssh_command(ssh, 'echo $VSC_DATA', verbose=True)
                self.vsc_data_folder = stdout.readlines()[0].strip()
                stdout, stderr = self.ssh_command(ssh, 'echo $VSC_SCRATCH', verbose=True)
                del stderr
                self.vsc_scratch_folder = stdout.readlines()[0].strip()
                location = self.vsc_scratch_folder 
                if self.wRemoteLocation.Value=='$VSC_DATA':
                    location = self.vsc_data_folder 
                ssh.close()
            except:
                location = self.wRemoteLocation.Value
        else:
            location = self.wRemoteLocation.Value

        self.remote_location = location
        if not self.wRemoteSubfolder.Value in ['.','./']:
            location = os.path.join(location,self.wRemoteSubfolder.Value) 
        return location
        
    def get_module_avail(self):
        ssh = self.open_ssh_connection()
        if not ssh is None:
            stdout, stderr = self.ssh_command(ssh, 'module avail', verbose=True)
            lines = stderr # yes, module avail writes to stderr
            del stdout
            ssh.close()
            
            lsts = []
            for line in lines:
                line = line.strip()
                if line:
                    if line.startswith('--'):
                        line = line.replace('-','')
                        line = '--'+line+'--'
                        lsts.append([])
                        lst = lsts[-1]
                        lst.append(line)
                    elif '  in line':
                        ll = line.split()
                        lst.extend(ll)
                    else:
                        lst.append(line)
            module_list=[]
            for lst in lsts:
                module_list.extend( sorted( lst, key=lambda s: s.lower() ) )
            self.config.attributes['module_lists'][self.get_cluster()] = module_list
        else:
            cluster = self.get_cluster()
            if cluster in self.config.attributes['module_lists']:
                module_list = self.config.attributes['module_lists'][cluster]
            else:
                module_list = ['-- for launcher tests only --','mo1','mo2']
        
        return module_list
    
    def update_resources_from_script(self,event=None):
        lines = self.wScript.Value.splitlines(True)
        self.script.parse(lines)
        self.set_default_pbs_parameters()
        self.update_resources()
        
    def update_script_from_resources(self):       
        if not self.is_resources_modified:
            return
        print('  resources modified, updating script')
        #make sure all values are transferred to self.script.values
        if not hasattr(self, 'script'):
            self.script = pbs.Script()
        if not 'n_nodes' in self.script.values: 
            self.script.add_pbs_option('-l','nodes={}:ppn={}'.format(self.wNNodesRequested.Value,self.wNCoresPerNodeRequested.Value))
        else:
            self.script.values['n_nodes'         ] = self.wNNodesRequested       .Value
            self.script.values['n_cores_per_node'] = self.wNCoresPerNodeRequested.Value
        
        if not 'walltime_seconds' in self.script.values:
            self.script.add_pbs_option('-l', 'walltime='+pbs.walltime_seconds_to_str(self.walltime_seconds))
        else:
            self.script.values['walltime_seconds'] = self.walltime_seconds
            
        if self.wNotifyAddress.Value: 
            abe = ''
            if self.wNotifyAbort.IsChecked():
                abe+='a'
            if self.wNotifyBegin.IsChecked():
                abe+='b'
            if self.wNotifyEnd.IsChecked():
                abe+='e'
            if abe: 
                if not 'notify_address' in self.script.values:
                    self.script.add_pbs_option('-M',self.wNotifyAddress.Value)
                    self.script.add_pbs_option('-m',abe)
                else:
                    self.script.values['notify_address'] = self.wNotifyAddress.Value
                    self.script.values['notify_abe'    ] = abe
                
        if self.wJobName.Value:
            if not 'job_name' in self.script.values:
                self.script.add_pbs_option('-N',self.wJobName.Value)
            else:
                self.script.values['job_name'] = self.wJobName.Value
                
        lines = self.script.compose()
        self.format_script(lines)
        
    def format_script(self,lines):
        self.wScript.ClearAll()
        for line in lines:
            self.wScript.AddText(line)
        #self.wScript.Colourise()
        self.is_script_modified = False

    def update_resources(self):
        if not hasattr(self,'script'):
            return
        if not hasattr(self.script,'values'):
            return
        #print 'update_resources()'
        new_request = False
        new_request|= self.update_value(self.wNNodesRequested       ,'n_nodes')
        new_request|= self.update_value(self.wNCoresPerNodeRequested,'n_cores_per_node')
        if new_request:
            self.request_nodes_cores()
        
        v = self.script.values.get('walltime_seconds')
        if v:
            self.set_walltime(v)

        self.update_value(self.wNotifyAddress,'notify_address')
        self.update_value(self.wNotifyAbort  ,'notify_abe',ContainsString('a'))
        self.update_value(self.wNotifyBegin  ,'notify_abe',ContainsString('b'))
        self.update_value(self.wNotifyEnd    ,'notify_abe',ContainsString('e'))

        self.update_value(self.wJobName      ,'job_name')

    def update_value(self,ctrl,varname,function=None):
        if varname in self.script.values:
            value = self.script.values[varname]
            if not function is None:
                value = function(value)
            if ctrl.Value == value:
                return False
            else:
                ctrl.Value = value
                return True
        else:
            return False
        
    def set_walltime(self,seconds):
        for i_unit in range(1,len(self.walltime_units)):
            us=self.walltime_unit_sec[i_unit]
            rem = seconds%us
            if rem:
                i_unit-=1
                break
        self.wWalltimeUnit.Select(i_unit)
        us = self.walltime_unit_sec[i_unit]
        n  = seconds/us
        self.wWalltime.Value = n        
        self.walltime_has_changed()
           
    def walltime_has_changed(self):
        self.walltime_seconds = self.wWalltime.Value * self.walltime_unit_sec[self.wWalltimeUnit.Selection]
 
    def walltime_unit_has_changed(self,event=None):
        unit_sec = self.walltime_unit_sec[self.wWalltimeUnit.Selection]
        if self.walltime_seconds%unit_sec:
            self.wWalltime.Value = (self.walltime_seconds/unit_sec + 1)
            self.walltime_seconds = self.wWalltime.Value*unit_sec
        else:
            self.wWalltime.Value = self.walltime_seconds/unit_sec 
            self.is_resources_modified = True

    def notify_address_has_changed(self,event=None):
        #print "event handler 'notify_address_has_changed (event={})'".format(event)

        pattern=re.compile(r'\b[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,4}\b')
        s = self.wNotifyAddress.Value.lower()
        match = pattern.match(s)
        if not match:            
            self.wNotifyAddress.SetForegroundColour(wx.RED)
            #todo -> dialog
            self.wNotifyAddress.Value = 'Enter a valid e-mail address'
        else:
            self.wNotifyAddress.SetForegroundColour(wx.BLACK)
            self.wNotifyAddress.Value = s
            self.is_script_up_to_date = False
        if not event is None:
            event.Skip()

    def add_module(self):
        module = self.wSelectModule.Value
        if module.startswith("--"):
            self.set_status_text('No module selected.')
        else:
            self.wScript.AddText("\nmodule load "+self.wSelectModule.Value)
                            
    def user_id_has_changed(self):
        self.config.attributes['user_id'] = self.wUserId.GetValue()
        self.wRemoteFileLocation.Value = self.get_remote_file_location()
    
    def load_job(self):
        self.is_resources_modified = False
        s = self.wLocalFileLocation.Value
        if os.path.isdir(s):
            s = os.path.join(s,'pbs.sh')
        if os.path.isfile(s):
            fname = s
        else:
            #todo dialog  
            print("error: script '{}' not found. See 'Local file location'.".format(s))
            return
        self.wScript.LoadFile(fname)
        lines = self.wScript.GetValue().splitlines(True)
        self.format_script(lines)
        self.script = pbs.Script(lines)
        self.update_resources()
        
    def transfer(self,values,name,attr):
        if name in values:
            if attr[0]=='w' and attr[1].isupper():
                getattr(self,attr).SetValue( values[name] )
            else:
                setattr(self,attr,values[name])
        
#PBS -l advres=fat-reservation.31           
    def check_job_name_present(self):
        if not self.wJobName.Value:
            random_job_name = 'job-'+str(random.random())[2:]
            msg = "No Job name specified.\n:Press\n  - 'Cancel' to specify a Job name, then retry, or\n  - 'OK' to use '{}' (random).".format(random_job_name)
            answer = wx.MessageBox(msg, 'Warning',wx.CANCEL|wx.OK | wx.ICON_WARNING)
            if answer==wx.CANCEL:
                self.SetStatusText('You can specify a job name now.')
                return False
            elif answer==wx.OK:
                self.wJobName.SetValue(random_job_name)
                return True
        return True
        
    def save_job(self):
        if not self.check_job_name_present():
            #todo dialog
            self.set_status_text("Job script not saved: job name missing.")
            return False

        self.update_script_from_resources()
        self.set_status_text("Saving job script to '{}' ...".format(self.wJobName.Value))
        my_makedirs(self.wLocalFileLocation.Value)
        fname = os.path.join(self.wLocalFileLocation.Value,'pbs.sh')
        self.wScript.SaveFile(fname)
        self.set_status_text("Job script saved to '{}'. Add input files to local location.".format(fname))
        return True
    
    def ssh_command(self,ssh,cmd,verbose=False):
        """
        Run a remote command cmd through ssh
        Return stdout and stderr (list of strings (=lines) containing eol)
        """
        stdin, stdout, stderr = ssh.exec_command(cmd)
        out = stdout.readlines()
        err = stderr.readlines()
        del stdin
        if verbose or err:
            print("Executing ssh command:")
            print("  > ",cmd )
            print("  > stdout:" )
            pprint.pprint(out )
            print("  > stderr:" )
            pprint.pprint(err )
            print("  > ssh command done." )
        return out,err

    def copy_to_remote_location(self,submit=False):
        self.set_status_text('copying ... ')
        local_folder = self.wLocalFileLocation.Value
        remote_folder = self.wRemoteFileLocation.Value
        try:
            ssh = self.open_ssh_connection()
            ssh_ftp = ssh.open_sftp()
        except:
            msg = 'FJR_ERROR: copy_to_remote_location : no connection.'
            print(msg)
            self.set_status_text(msg)
            return
        verbose_ssh=True
        cmd = "mkdir -p "+remote_folder
        out,err = self.ssh_command(ssh,cmd,verbose=verbose_ssh)
        l=len(local_folder)
        try:
            print("Copying contents of local folder '{}' to remote folder '{}'.".format(local_folder,remote_folder))
            for folder, subfolders, files in os.walk(local_folder):
                self.set_status_text("  Entering folder '{}'".format(folder))
                # corresponding folder on remote site
                sf=folder[l:]
                if sf:
                    remote = os.path.join(remote_folder,sf[1:])
                else:
                    remote = remote_folder
                #create the necessary subfolders on the remote side
                for subfolder in subfolders:
                    dst = os.path.join(remote,subfolder)
                    sf = os.path.basename(subfolder)
                    if not sf.startswith('.'):
                        self.set_status_text("  Creating folder '{}'".format(dst))
                        cmd = "mkdir -p "+dst
                        out,err = self.ssh_command(ssh,cmd,verbose=verbose_ssh)
                for f in files:
                    if not f.startswith('.'):
                        self.set_status_text("  Copying file '{}/{}' to '{}'".format(folder,f,remote))
                        src = os.path.join(folder,file)
                        dst = os.path.join(remote,file)
                        ssh_ftp.put(src,dst)
        except:
            msg = "Failed copying local folder ('{}') contents to remote folder '{}'.".format(local_folder,remote_folder)
        else:
            msg = "Successfully copied local folder ('{}') contents to remote folder '{}'.".format(local_folder,remote_folder)
        self.set_status_text(msg)
        
        if submit:
            msg = "Submitting job ..."  
            self.set_status_text(msg)
            cmd = 'cd '+remote_folder+' && qsub pbs.sh'      
            out,err = self.ssh_command(ssh,cmd,verbose=verbose_ssh)
            if err:
                msg = 'Failed to submit job.'
            else:
                job_id = out[0].split('.')[0]
                msg = 'Submitted job : job_id = '+job_id
                self.config.attributes['submitted_jobs'][job_id] = {'cluster'      : self.get_cluster()
                                                              ,'user_id'      : self.wUserId.GetValue() 
                                                              ,'remote_folder': remote_folder
                                                              ,'local_folder' : local_folder
                                                              ,'job_name'     : self.wJobName.GetValue()
                                                              ,'status'       : 'submitted'
                                                              }
            self.set_status_text(msg)
                
        ssh.close()
    
    def set_status_text(self,msg):
        print('\nStatus bar text: '+msg)
        self.SetStatusText(msg)
    
    def submit_job(self):
        if not self.save_job():
            self.set_status_text("Job not submitted: failed to save in local folder.")
            return
        self.copy_to_remote_location(submit=True)
            
    def retrieve_finished_job(self,job_id,job_data):
        """
        Copy the output back to the local folder and/or to $VSC_DATA
        return
            FJR_SUCCESS       = success
            FJR_INEXISTING    = remote directory does not exist
            FJR_NOT_FINISHED  = the job is not finished yet
            FJR_NO_CONNECTION = no ssh connection established
            FJR_ERROR         = an exception was raised during the copy process
        we need to find a way of knowing all files and folders
        . linux tree cmd
          this gives a list of all folders (full name), one per line, 
            tree -fid 
          this gives a list of folders and files (full name)
            tree -fi
        """
#         cluster      = job_data['cluster']
#         user_id      = job_data['user_id']
        local_folder = job_data['local_folder']
        remote_folder= job_data['remote_folder']
        job_name     = job_data['job_name']
        try:
            ssh = self.open_ssh_connection()
            ssh_ftp = ssh.open_sftp()
        except:
            self.set_status_text("Failed to retrieve job {} : '{}': No connection established.".format(job_id,job_name))
            job_data['status'] = 'not retrieved due to no connection (ssh).'
            return FJR_NO_CONNECTION
        
        try:
            cmd = 'cd '+remote_folder
            ssh_verbose=True
            out,err = self.ssh_command(ssh, cmd, ssh_verbose)
            if err:
                ssh.close()
                self.set_status_text("Failed to retrieve job {} : '{}': remote folder '{}' not found.".format(job_id,job_name,remote_folder))
                job_data['status'] = 'not retrieved due to inexisting remote folder.'
                return FJR_INEXISTING
            
            cmd += " && tree -fi"
            out,err = self.ssh_command(ssh, cmd+" --noreport", ssh_verbose)
            remote_files_and_folders = make_tree_output_relative(out)
            o = job_name+'.o'+job_id
            if not o in remote_files_and_folders:
                ssh.close()
                self.set_status_text("Failed to retrieve job {} : '{}': job is not finished (or file '{}.o{}' is missing)".format(job_id,job_name,job_name,job_id))
                job_data['status'] = 'not retrieved due to unfinished job.'
                return FJR_NOT_FINISHED
            
            cmd += "d --noreport"
            out,err = self.ssh_command(ssh, cmd, ssh_verbose)
            remote_folders = make_tree_output_relative(out)
            for f in remote_folders:
                if self.wCopyToLocalDisk.IsChecked():
                    lf = os.path.join(local_folder,f)
                    self.set_status_text("Creating local folder '{}'.".format(lf))
                    my_makedirs(lf)
                if self.wCopyToVSCDATA.IsChecked():
                    lf = os.path.join(self.vsc_data_folder,f)
                    self.set_status_text("Creating $VSC_DATA folder '{}'.".format(lf))
                    cmd = "mkdir -p "+lf
                    out,err = self.ssh_command(ssh, cmd, ssh_verbose)
                    
            for f in remote_files_and_folders:
                if not f in remote_folders:
                    rf = os.path.join(remote_folder,f)
                    if self.wCopyToLocalDisk.IsChecked():
                        lf = os.path.join(local_folder,f)
                        self.set_status_text("Copying remote file '{}' to '{}'.".format(rf,lf))
                        ssh_ftp.get(rf,lf) 
                    if self.wCopyToVSCDATA.IsChecked():
                        lf = os.path.join(self.vsc_data_folder,f)
                        self.set_status_text("Copying remote file '{}' to '{}'.".format(rf,lf))
                        cmd = "cp {} {}".format(rf,lf)
                        out,err = self.ssh_command(ssh, cmd, ssh_verbose)
                    
            ssh.close()
            self.set_status_text("Successfully retrieved job {}. Local folder is {}".format(job_id,local_folder))
            job_data['status'] = 'successfully retrieved.'
            return FJR_SUCCESS
        
        except Exception as e:
            print(e)
            self.set_status_text("Failed to retrieve job {}. Local folder is {}".format(job_id,local_folder))
            ssh.close()
            return FJR_ERROR
        
    def retrieve_finished_jobs(self):
        
        if not self.wCopyToLocalDisk.IsChecked() and not self.wCopyToVSCDATA.IsChecked():
            msg = "Nothing to do.\nCheck at least one of the 'Copy to ...' checkboxes to specify a destination for the results."
            wx.MessageBox(msg, 'Warning',wx.OK | wx.ICON_WARNING)
            return
        
        self.set_status_text('Starting to retrieve finished jobs ...')
        self.retrieved_jobs = {}
        summary = [0,0,0,0,0]
        for job_id,job_data in self.config.attributes['submitted_jobs'].iteritems():
            ret = self.retrieve_finished_job(job_id,job_data)
            if ret == FJR_SUCCESS:
                self.retrieved_jobs[job_id] = job_data
            elif ret == FJR_INEXISTING:
                del self.config.submitted[job_id]
                print("Removed job {}: remote directory no longer exists.".format(job_id))
                pprint.pprint(job_data)
                print
            summary[ret]+=1
        for key in self.retrieved_jobs:
            del self.config.attributes['submitted_jobs'][key] # can't do this on the fly

        tot = sum(summary)
        summary.append(tot)
        summary.append(summary[5]-summary[0])
        msg='Retrieved={}, removed={}, not finished={}, no connection={}, error={}, total={}, remaining={}'.format(*summary)
        self.set_status_text(msg)
        print("Retrieved jobs:")
        pprint.pprint(self.retrieved_jobs)
    
    def show_retrieved_jobs(self):
        if not hasattr(self,'retrieved_jobs'):
            self.retrieved_jobs = {}
        self.show_jobs(self.wJobsRetrieved,self.retrieved_jobs)
    
    def open_ssh_connection(self):
        """
        Try to open an ssh connection
          - if the previous attempt was succesful
          - or if the previous attempt was not succesful but long enough (SSH_WAIT) ago
        Return the ssh client or None.
        """
        last_try = getattr(self,"last_try",datetime.datetime(2014,12,31))
        last_try_success = getattr(self,"last_try_success",False)
        delta = datetime.datetime.now() - last_try
        
        if last_try_success or ( not last_try_success and delta.total_seconds() > SSH_WAIT ):
            #retry to make ssh connection
            self.set_status_text("Trying to make ssh connection ...")
            cluster = self.get_cluster()
            user_id = self.wUserId.Value
            user_at_login = "{}@{}".format(user_id,pbs.logins[cluster])
    
            try:
                ssh = paramiko.SSHClient()
                ssh.load_system_host_keys()
                # Create the ProgressDialog
                msg = "ssh "+user_at_login+"\n\nPlease be patient for {} seconds :-).".format(str(SSH_TIMEOUT))
                dialog = wx.ProgressDialog("Attempting to connect ...",msg,maximum=100,parent=self,style=wx.PD_APP_MODAL)
                #dialog.Update(0)
                ssh.connect(pbs.logins[cluster], username=user_id,timeout=SSH_TIMEOUT)
            except:
                if dialog:
                    dialog.Destroy()                
                self.set_status_text("Unable to connect via ssh to "+user_at_login)
                self.last_try_success = False
                ssh = None
                msg ="Unable to connect via ssh to "+user_at_login
                msg+="\nPossible causes are:"
                msg+="\n  - no internet connection."
                msg+="\n  - you are not connected to your home institution (VPN connection needed?)."
                wx.MessageBox(msg, 'No ssh connection.',wx.OK | wx.ICON_INFORMATION)
            else:
                if dialog:
                    dialog.Destroy()                
                self.set_status_text("Connected via ssh to "+user_at_login)
                self.last_try_success = True
                
            self.last_try = datetime.datetime.now()
        else:
            # don't retry to make ssh connection
            ssh = False
        
        return ssh
        
    def show_submitted_jobs(self):
        self.show_jobs(self.wJobsSubmitted,self.config.attributes['submitted_jobs'])        
        last_try = getattr(self,"last_try",datetime.datetime(2014,12,31))
        last_try_success = getattr(self,"last_try_success",False)
        delta = datetime.datetime.now() - last_try
        if delta.total_seconds() > 30 and not last_try_success:
            ssh = self.open_ssh_connection()
        else:
            ssh = False
        if ssh:
            cmd = "qstat -u "+self.wUserId.GetValue()
            out,err = self.ssh_command(ssh,cmd)
            ssh.close()
            for line in out:
                self.wJobsSubmitted.AppendText(line)
            del err
        else:
            msg = 'No info on status of submitted jobs (no ssh connection).'
            self.set_status_text(msg)
            
    def show_jobs(self,textctrl,jobs):
        """
        show information on a set of jobs in textctrl
        """
        textctrl.SetValue('')
        if jobs:
            for k,v in jobs.iteritems():
                textctrl.AppendText("job_id={}\n".format(k))
                for kk,vv in v.iteritems():
                    textctrl.AppendText("   {:<13} : '{}'\n".format(kk,vv))
                textctrl.AppendText("\n")
        else:
            textctrl.SetValue('-- none --')

            
class RedirectStdStreams(object):
    def __init__(self, stdout=None, stderr=None):
        self._stdout = stdout or sys.stdout
        self._stderr = stderr or sys.stderr

    def __enter__(self):
        self.old_stdout, self.old_stderr = sys.stdout, sys.stderr
        self.old_stdout.flush(); self.old_stderr.flush()
        sys.stdout, sys.stderr = self._stdout, self._stderr

    def __exit__(self, exc_type, exc_value, traceback):
        self._stdout.flush(); self._stderr.flush()
        sys.stdout = self.old_stdout
        sys.stderr = self.old_stderr

def run_launcher():
    app = wx.App()
    frame = Launcher(None,"launch-4")
    log = frame.log
    app.SetTopWindow(frame)
    app.MainLoop()
    return log # to save the log file

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-p","--printlog"
                       , help="Print the log instead of redirecting it to the log file."
                       , action="store_true"
                       )
    args = parser.parse_args()
    
    if not args.printlog:
        s_out_err = cStringIO.StringIO()
        with RedirectStdStreams(stdout=s_out_err,stderr=s_out_err):
            log = run_launcher()
            open(log,"w+").write(s_out_err.getvalue())
    else:
        run_launcher()