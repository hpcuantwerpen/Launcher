from __future__ import print_function
import sys,cStringIO,datetime,argparse,subprocess,traceback,time
import re,os,random,shutil,pprint,errno,pickle,inspect

import wx
import xx
import wx.lib.newevent
import sshtools
from  indent import Indent

from wxtools import *

from BashEditor import PbsEditor
import clusters
import pbs

wx.ToolTip.Enable(True)

######################
### some constants ###
######################

# return codes for finished job retrieval (FJR):
FJR_SUCCESS      =0
FJR_INEXISTING   =1
FJR_NOT_FINISHED =2
FJR_NO_CONNECTION=3
FJR_ERROR        =4

__VERSION__ = (0,5)

#####################################
### some wx convenience functions ###
#####################################



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
        """set the string to look for"""
        self.look_for = look_for
    def __call__(self,string):
        """test if string contains string self.look_for"""
        return self.look_for in string

def my_makedirs(path):
    """
    Check if a directory exists and create it if necessary
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

def log_exception(exception):
    print("\n### Exception raised: #############################################################")
    traceback.print_exc(file=sys.stdout)
    print( exception )
    print(  "###################################################################################")

def print_line(title="",line_length=80,n_white=1,line_char='-',indent=4):
    if title:
        line = (indent-n_white)*line_char + (n_white*' ') + title + (n_white*' ')
        l = len(line)
        line += (line_length-l)*line_char
    else:
        line = line_length*line_char
    print(line)
    
_timestamps = []
def print_item_header(title=""):
    global _start
    now = datetime.datetime.now()
    _timestamps.append(now)
    print()
    print(now)
    print("<<<",title)

def print_item_footer(footer=""):
    if footer:
        print(">>>",footer,(datetime.datetime.now()-_timestamps.pop()).total_seconds(),'s')
    else:
        print(">>>",(datetime.datetime.now()-_timestamps.pop()).total_seconds(),'s')
        
#----------------------#
# below only test code #
#----------------------#
################################################################################
### Launcher classes                                                         ###
################################################################################
class Config(object):
    """
    Launcher configuration
    """
    def __init__(self):
        env_home = "HOMEPATH" if sys.platform=="win32" else "HOME"
        user_home = os.environ[env_home]
        if not user_home or not os.path.exists(user_home):
            msg = "Error: environmaent variable '${}' not found.".format(env_home)
            raise EnvironmentError(msg)
        launcher_home = os.path.join(user_home,'Launcher')
        if not os.path.exists(launcher_home):
            os.mkdir(launcher_home)
        self.launcher_home = launcher_home
        
        cfg_file = os.path.join(launcher_home,'Launcher.config')
        try:
            self.attributes = pickle.load(open(cfg_file))
        except Exception as e:
            log_exception(e)
            self.attributes = {}
        self.cfg_file = cfg_file

        default_attributes = { 'cluster' : 'some_cluster'
                             , 'mail_to' : 'your mail address goes here'
                             , 'walltime_unit' : 'hours'
                             , 'walltime_value' : 1
                             , 'local_files' : os.path.join(launcher_home,'jobs')
                             , 'user_id' : 'your_user_id'
                             , 'remote_subfolder' : '.'
                             , 'module_lists':{}
                             , 'submitted_jobs':{}
                             }
#         if 'submitted' in self.attributes:
#             del self.attributes['submitted']
        self.add_default_attributes(default_attributes)
        
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
    regexp_userid = re.compile(r'vsc\d{5}')
    ID_MENU_SSH_PREFERENCES = wx.NewId()
    ID_MENU_SSH_CONNECT     = wx.NewId()
    ID_MENU_LOGIN_NODE      = wx.NewId()
    ID_MENU_CLUSTER_MODULE_ADDED = wx.NewId()
    
    def __init__(self, parent, title):
        """
        Create all the controls in the Launcher window and bind the necessary methods
        """
        self.config = Config()
        #set the window size
        frame_size = self.config.attributes.get("frame_size",(500,500))
   
        #allow sshtools to set the statusbar of the Launcher    
        sshtools.Client.frame = self
            
        self.open_log_file()        
        
#        self.validate_user_id(self.config.attributes.get("user_id","your_user_id"))

        super(Launcher,self).__init__(parent,title=title,size=frame_size)
        
        self.init_ui()
        self.set_names()
        
        self.Bind(wx.EVT_CLOSE, self.on_EVT_CLOSE)
        
        self.bind('wCluster'                , 'EVT_COMBOBOX')
        self.bind('wNodeSet'                , 'EVT_COMBOBOX')
        self.bind('wNotebook'               , 'EVT_NOTEBOOK_PAGE_CHANGING')
        self.bind('wNotebook'               , 'EVT_NOTEBOOK_PAGE_CHANGED')
        self.bind('wNNodesRequested'        , 'EVT_SPINCTRL')
        self.bind('wNCoresPerNodeRequested' , 'EVT_SPINCTRL')
        self.bind('wNCoresRequested'        , 'EVT_SPINCTRL')
        self.bind('wGbPerCoreRequested'     , 'EVT_SPINCTRL')            
        self.bind('wEnforceNNodes'          , 'EVT_CHECKBOX')
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
        #set control data
        self.InitData()
        self.dump()
    
    def bind(self,object_name,event_name):
        """
        Basically a wrapper of wx.Object.Bind
        helper function that binds an event handler (a Launcher method) to a
        object.
        - the object is self.<object_name> 
        - the event is <event_name>
        - the event handler is self.<object_name>_<event_name>
        """
        type_str = str( type(eval("self."+object_name)) )
        if type_str.startswith("<class 'xx."):
            s = "self.{}.GetTextCtrl().Bind(xx.{},self.{}_{})".format(object_name,event_name,object_name,event_name)
        else:
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
            
    def log_event(self,event,msg=None):
        """
        log information about the event
        """
        print_item_header("Launcher.log_event():")
        if isinstance(event,(str,unicode)):
            print(Indent(event,4))
        else:
            t = str(type(event))
            print(Indent("Event.type "+t,4))
            if "wx.lib.newevent._Event" in t:
                print("    custom event")
                print(Indent(event.__dict__,4))
            else:
                print("         .object.name : "+event.GetEventObject().GetName())
                print("         .object.class: "+event.GetEventObject().GetClassName())
        if msg: 
            print(msg)
        print_item_footer()
        self.SetStatusText("")
        
    def dump(self):
        """
        write all control values and data values 
        """
        print_item_header("Dump of all control values:")
        for k,v in self.__dict__.iteritems():
            if k[0]=='w' and k[1].isupper(): # a control
                try:
                    value = v.GetValue()
                    if not value:
                        if isinstance(v, wx.CheckBox):
                            value='False'
                        else:
                            value = "''"
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
        print_item_footer()
            
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
        
    def wGbPerCoreRequested_EVT_SPINCTRL(self,event):
        self.log_event(event)
        self.request_cores_memory()
        self.is_resources_modified = True

    def wEnforceNNodes_EVT_CHECKBOX(self,event):
        self.log_event(event)
        self.is_resources_modified = True

    def wNAccessPolicy_EVT_CHECKBOX(self,event):
        self.log_event(event)
        self.is_resources_modified = True
 
    def wSelectModule_EVT_COMBOBOX(self,event):
        self.log_event(event)
        self.add_module()
        
    def wNotebook_EVT_NOTEBOOK_PAGE_CHANGING(self,event):
        #see http://wiki.wxpython.org/Notebooks        
        old_page = event.GetOldSelection()
        new_page = event.GetSelection()
        selected = self.wNotebook.GetSelection()
        msg='wNotebook_EVT_NOTEBOOK_PAGE_CHANGING():  old_page:%d, new_page:%d, selected:%d' % (old_page, new_page, selected)
        self.log_event(event,msg=msg)
        if old_page==0:
            self.update_script_from_resources()
        elif old_page==1:
            self.update_resources_from_script()
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
        self.wNotebook.ChangeSelection(2)

    def wRefresh_EVT_BUTTON(self,event):
        self.log_event(event)
        self.show_submitted_jobs()

    def wRetrieveJobs2_EVT_BUTTON(self,event):
        self.log_event(event)
        self.retrieve_finished_jobs()
        self.wJobsRetrieved_EVT_SET_FOCUS("wRetrieveJobs2_EVT_BUTTON() calling wJobsRetrieved_EVT_SET_FOCUS()")
        
    def wDeleteJob_EVT_BUTTON(self,event):
        """ Delete a job from the list of jobs to be retrieved.
        """
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
            
    def wJobsRetrieved_EVT_SET_FOCUS(self,event=None):
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

    def on_EVT_CLOSE(self,event=None):
        self.log_event(event)
        print_item_header('Launcher closing')
        frame_size = self.GetSize()
        self.config.attributes['frame_size'] = frame_size
        print('    Saving config file.')
        self.config.save()
        self.Destroy()
        print_item_footer('end of log')
             
    ### the widgets ###
    def init_ui(self):        
        self.CreateStatusBar()
        self.panel = wx.Panel(self)
        self.main_sizer = wx.BoxSizer(wx.VERTICAL)
        self.panel.SetSizer(self.main_sizer)
        
        self.wNotebook = wx.Notebook(self.panel, wx.ID_ANY)
        self.main_sizer.Add(self.wNotebook, 1, wx.EXPAND, 0)
        
        self.wNotebookPageResources = add_notebook_page(self.wNotebook, 'Resources')
        self.wNotebookPageScript    = add_notebook_page(self.wNotebook, "PBS script")
        self.wNotebookPageRetrieve  = add_notebook_page(self.wNotebook, "Retrieve")
   
        #self.wNotebookPageResources
        sizer_1 = create_staticbox(self.wNotebookPageResources, "Requests resources from")
        sizer_2a= create_staticbox(self.wNotebookPageResources, "Requests nodes and cores per node")
        sizer_2b= create_staticbox(self.wNotebookPageResources, "Requests cores and memory per core")
        sizer_2 = wx.BoxSizer(wx.HORIZONTAL)
        sizer_2.Add(sizer_2a,1,wx.EXPAND,0)
        sizer_2.Add(sizer_2b,1,wx.EXPAND,0)
        #sizer_3 = create_staticbox(self.wNotebookPageResources, "Node policies", orient=wx.HORIZONTAL)
        sizer_4 = create_staticbox(self.wNotebookPageResources, "Request compute time")
        sizer_5 = create_staticbox(self.wNotebookPageResources, "Notify")
        sizer_6 = create_staticbox(self.wNotebookPageResources, "Job name")
        sizer_7 = create_staticbox(self.wNotebookPageResources, "Local file location")
        sizer_8 = create_staticbox(self.wNotebookPageResources, "Remote file location",orient=wx.VERTICAL)
        sizer_9 = create_staticbox(self.wNotebookPageResources, "Job management",orient=wx.HORIZONTAL)
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
        self.wCluster = wx.ComboBox(self.wNotebookPageResources, wx.ID_ANY
                                   , style=wx.CB_DROPDOWN|wx.CB_READONLY|wx.CB_SIMPLE|wx.EXPAND
                                   )
        self.wCluster.SetToolTipString("Select the cluster to which you want to submit a job.")
        self.current_node_set = None
        
        self.wNodeSet = wx.ComboBox(self.wNotebookPageResources, wx.ID_ANY
                                   , style=wx.CB_DROPDOWN|wx.CB_READONLY|wx.CB_SIMPLE|wx.EXPAND
                                   )
        self.wNodeSet.SetToolTipString("Select the pool of nodes from which resources must be requested.")
        sizer_1.Add(grid_layout([self.wCluster,[self.wNodeSet,1,wx.EXPAND]],growable_cols=[1]),1,wx.EXPAND,0 )        
       
        #sizer_2a and sizer_2b
        txt2a0                       = wx.StaticText(self.wNotebookPageResources,label='Number of nodes:')
        self.wNNodesRequested        = xx.SpinCtrl  (self.wNotebookPageResources )
        txt2a2                       = wx.StaticText(self.wNotebookPageResources,label='Cores per node:') 
        self.wNCoresPerNodeRequested = xx.SpinCtrl  (self.wNotebookPageResources ) 
        txt2a4                       = wx.StaticText(self.wNotebookPageResources,label='Memory per node [GB]:') 
        self.wGbPerNodeGranted       = wx.TextCtrl  (self.wNotebookPageResources,style=wx.TE_READONLY|wx.TE_RIGHT) 
        txt2a6                       = wx.StaticText(self.wNotebookPageResources,label='enforce #nodes') 
        self.wEnforceNNodes          = wx.CheckBox  (self.wNotebookPageResources)

        txt2b0                       = wx.StaticText(self.wNotebookPageResources,label='Number of cores:')
        self.wNCoresRequested        = xx.SpinCtrl  (self.wNotebookPageResources )
        txt2b2                       = wx.StaticText(self.wNotebookPageResources,label='Memory per core [GB]:')
        self.wGbPerCoreRequested     = xx.SpinCtrl  (self.wNotebookPageResources,converter=float ) 
        txt2b4                       = wx.StaticText(self.wNotebookPageResources,label='Memory total [GB]:')
        self.wGbTotalGranted         = wx.TextCtrl  (self.wNotebookPageResources,style=wx.TE_READONLY|wx.TE_RIGHT)
        
        set_tool_tip_string([txt2a0,self.wNNodesRequested]
                           ,tip="Number of nodes your job will use."
                           )
        set_tool_tip_string([txt2a2,self.wNCoresPerNodeRequested]
                           ,tip="Number of cores per node your job will use."
                           )
        set_tool_tip_string([txt2a4,self.wGbPerNodeGranted]
                           ,tip="Available memory per node (after substraction of the memory spaced occupied by the operating system)."
                           )
        set_tool_tip_string([txt2a6,self.wEnforceNNodes]
                           ,tip="Prevent cores not requested from being used by another job."
                           )
        set_tool_tip_string([txt2b0,self.wNCoresRequested]
                           ,tip="Enter the total number of cores requested for your job. "\
                                "After pressing enter, this will show the number of cores granted"
                           )
        set_tool_tip_string([txt2b2,self.wGbPerCoreRequested]
                           ,tip="Enter the memory [Gb] per core you request for your job. "\
                                "After pressing Enter, this will show the average memory per core available to your job."
                           )
        set_tool_tip_string([txt2b4,self.wGbTotalGranted]
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

        sizer_2a.Add(grid_layout(lst2a,ncols=2),0,wx.EXPAND,0 )
        sizer_2b.Add(grid_layout(lst2b,ncols=2),0,wx.EXPAND,0 )   
        #sizer_4
        self.walltime_units    = ['seconds','minutes','hours','days ']
        self.walltime_unit_max = [ 60*60   , 60*24   , 24*7  , 7    ]
        self.walltime_unit_sec = [ 1       , 60      , 3600  , 86400]
        self.wWalltime     = wx.SpinCtrl(self.wNotebookPageResources) 
        self.wWalltimeUnit = wx.ComboBox(self.wNotebookPageResources,choices=self.walltime_units)
        i_unit = self.walltime_units.index(self.config.attributes['walltime_unit'])
        self.wWalltimeUnit.Select(i_unit)
        self.wWalltime.SetRange(1,self.walltime_unit_max[i_unit]) 
        self.wWalltime.SetValue( self.config.attributes['walltime_value'] )
        self.walltime_seconds = self.wWalltime.GetValue()*self.walltime_unit_sec[i_unit]
        
        lst = [ [wx.StaticText(self.wNotebookPageResources,label='wall time:'),0,wx.ALIGN_CENTER_VERTICAL]
              , [self.wWalltime,0,wx.ALIGN_CENTER_VERTICAL]
              , [self.wWalltimeUnit,0,wx.ALIGN_CENTER_VERTICAL]
              ]
        set_tool_tip_string([lst[0][0],self.wWalltime], tip="Expected duration of your job (upper bound).")
        set_tool_tip_string(self.wWalltimeUnit, tip="Unit in which the walltime is expressed.")
        
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
        
        if not is_valid_mail_address(self.wNotifyAddress.GetValue()):
            self.wNotifyAddress.SetForegroundColour(wx.TheColourDatabase.Find('GREY'))
        
        #sizer_6
        self.wJobName = wx.TextCtrl(self.wNotebookPageResources,style=wx.EXPAND|wx.TE_PROCESS_ENTER|wx.TE_PROCESS_TAB)
        set_tool_tip_string(self.wJobName
                           ,tip="The job name is used as the name of the parent folder where your job script "\
                                "(pbs.sh) is stored in the local and remote file location. Typically also input "\
                                "and output file are stored here.\n"\
                                "If you do not provide a job name Launcher will propose a random name."
                           )
        sizer_6.Add(self.wJobName,1,wx.EXPAND,0)
        
        #sizer_7
        self.local_parent_folder = self.config.attributes['local_files']
        self.wLocalFileLocation = wx.TextCtrl(self.wNotebookPageResources,value=self.local_parent_folder)
        set_tool_tip_string(self.wLocalFileLocation
                           ,tip="Local folder where your job script/input/output is stored. The parent folder "\
                                "is based on the job name."
                           )
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
        set_tool_tip_string([lst1[0],self.wUserId]
                           , tip="Your VSC account (vscXXXXX)."
                           )
        set_tool_tip_string([lst1[1],self.wRemoteLocation]
                           , tip="Scratch disk space is the preferred location to run your job."
                           )
        set_tool_tip_string([lst1[2],self.wRemoteSubfolder]
                           , tip="Remote location of your job folder (relative to <location>)."
                           )
        set_tool_tip_string(self.wRemoteFileLocation
                           , tip="Remote location of your job files (<location>/<subfolder>/<jobname>)."
                           )
        sizer_8.Add( grid_layout(lst2, 1, growable_cols=[0]),1,wx.EXPAND,0)
        
        #sizer_9
        self.wSaveJob   = wx.Button(self.wNotebookPageResources,label="Save")
        self.wSubmitJob = wx.Button(self.wNotebookPageResources,label="Submit")
        self.wRetrieveJobs  = wx.Button(self.wNotebookPageResources,label="Retrieve results")
        self.wCopyToLocalDisk = wx.CheckBox(self.wNotebookPageResources,label="Copy to local disk")
        self.wCopyToVSCDATA   = wx.CheckBox(self.wNotebookPageResources,label="Copy to $VSC_DATA")
        self.wCopyToLocalDisk.SetValue(True)
        self.wCopyToVSCDATA  .SetValue(False)

        set_tool_tip_string(self.wSaveJob
                           , tip="Save the job script in the local file location (<local_file_location>/<jobname>/pbs.sh)."\
                           )
        set_tool_tip_string(self.wSubmitJob
                           , tip="Save the job script in the local file location, copy the local job folder to "\
                                 "the remote file location, and submit the job."
                           )
        set_tool_tip_string(self.wRetrieveJobs
                           , tip="If the job is finished copy the remote job folder back to the local file location and/or "\
                                 "the $VSC_DATA disk space."
                           )
        set_tool_tip_string(self.wCopyToLocalDisk
                           , tip="Specify that the remote job folder must be copied back to the local file location."
                           )
        set_tool_tip_string(self.wRetrieveJobs
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
        sizer_0 = create_staticbox(self.wNotebookPageScript, "Job script")     
        self.wNotebookPageScript.SetSizer( grid_layout( [ [sizer_0,1,wx.EXPAND] ]
                                                  , ncols=1
                                                  , growable_rows=[0]
                                                  , growable_cols=[0] ) )
        #sizer_0
        self.wScript = PbsEditor(self.wNotebookPageScript)

        sizer_1 = wx.BoxSizer(wx.HORIZONTAL)
        sizer_1.Add(wx.StaticText(self.wNotebookPageScript,label="Select module to load:"),0,wx.ALIGN_CENTER_VERTICAL,1)
        self.wSelectModule = wx.ComboBox(self.wNotebookPageScript,style=wx.CB_DROPDOWN|wx.CB_READONLY|wx.CB_SIMPLE)
        sizer_1.Add(self.wSelectModule,1,wx.EXPAND,0)
        sizer_0.Add( grid_layout( [ [self.wScript,2,wx.EXPAND], [sizer_1,0,wx.EXPAND] ]
                                , ncols=1
                                , growable_rows=[0]
                                , growable_cols=[0] )
                   , 1, wx.EXPAND, 0 )     
    
        #self.wNotebookPageRetrieve
        self.fixed_pitch_font = wx.Font(10,wx.FONTFAMILY_MODERN,wx.FONTSTYLE_NORMAL,wx.FONTWEIGHT_NORMAL)
        
        sizer_1 = create_staticbox(self.wNotebookPageRetrieve, "Retrieved jobs")
        self.wJobsRetrieved = wx.TextCtrl(self.wNotebookPageRetrieve, style=wx.TE_MULTILINE|wx.EXPAND|wx.TE_MULTILINE|wx.HSCROLL|wx.TE_DONTWRAP|wx.TE_READONLY)
        self.wJobsRetrieved.SetDefaultStyle(wx.TextAttr(font=self.fixed_pitch_font))
        sizer_1.Add(self.wJobsRetrieved,1,wx.EXPAND,0) 
        sizer_2 = create_staticbox(self.wNotebookPageRetrieve, "Submitted jobs")
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
             
        # Setup the Menu
        menu_bar = wx.MenuBar()
        self.SetMenuBar(menu_bar)
        self.Bind(wx.EVT_MENU, self.on_EVT_MENU)
        self.menu_actions = {}
        # Tools Menu
        menu_ssh = wx.Menu()
        menu_bar.Append(menu_ssh, "&SSH")
        menu_ssh.Append(Launcher.ID_MENU_SSH_PREFERENCES     ,"Preferences\tCtrl+P")
        menu_ssh.Append(Launcher.ID_MENU_SSH_CONNECT         ,"Retry to connect\tCtrl+R")
        menu_ssh.Append(Launcher.ID_MENU_LOGIN_NODE          ,"Select login node\tCtrl+L")
        menu_ssh.Append(Launcher.ID_MENU_CLUSTER_MODULE_ADDED,"Check for new cluster modules\tCtrl+M")

    def on_EVT_MENU(self, event):
        """Handle menu clicks"""
        event_id = event.GetId()
        if event_id==Launcher.ID_MENU_SSH_PREFERENCES:
            self.set_ssh_preferences()
        if event_id==Launcher.ID_MENU_SSH_CONNECT:
            sshtools.Client(self.get_user_id(),self.login_node,force=True)
            # the object is not stored because it is not used 
        if event_id==Launcher.ID_MENU_LOGIN_NODE:
            self.select_login_node()
        if event_id==Launcher.ID_MENU_CLUSTER_MODULE_ADDED:
            self.update_cluster_list()
            msg = "These cluster modules are installed in folder\n"+clusters.clusters_folder+":\n"
            for m in self.wCluster.GetItems():
                msg += "\n  - "+m+".py"
            wx.MessageBox(msg, 'Cluster modules',wx.OK | wx.ICON_INFORMATION)

        else:
            raise Exception("Unknown menu event id:"+str(event_id))
             
    def set_ssh_preferences(self):
        print("set_ssh_preferences(self)")
        dlg = sshtools.SSHPreferencesDialog(self)
        answer = dlg.ShowModal()
        if answer==wx.ID_OK:
            dlg.update_preferences()
        del dlg
        
    def select_login_node(self):
        dlg = SelectLoginNodeDialog(self)
        answer = dlg.ShowModal()
        if answer==wx.ID_OK:
            dlg.select()
        del dlg
        
    
    def InitData(self):        
        """
        Initializte the data elements of the controls of the Launcher window
        """
        #select a cluster
        cluster = self.config.attributes['cluster']
        try:
            i = clusters.cluster_list.index(cluster)
        except ValueError:
            i=0
        cluster = clusters.cluster_list[i]
        self.wCluster.SetItems(clusters.cluster_list)        
        self.set_cluster(cluster) 
        # also sets all elements that depend on the cluster, such as nodesets, ...

        value=self.get_remote_file_location()
        self.is_script_up_to_date = False
        self.wRemoteFileLocation.SetValue(value)
        
        #check if there are any jobs submitted by the user which are ready for copying back   
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
        self.wNCoresPerNodeRequested.SetValue(self.current_node_set.n_cores_per_node)
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

        print_item_header('launch.py')
        print('    begin of log.')
        print_item_footer()
        
        print_item_header("Version info of used components")
        print("    Python version:")
        print(Indent(sys.version,8))
        print("    wxPython version:")
        print(Indent(wx.version(),8))
        print("    paramiko version:")
        print(Indent(sshtools.paramiko.__version__,8))
        print("    Launcher version:")
        try:
            s=subprocess.check_output(["git","describe","HEAD"])
            if s[-1]=='\n':
                s = s[:-1]
        except:
            s="v{}.{}".format(*__VERSION__)
        print(Indent(s,8))
        print("    Platform:")
        print(Indent(sys.platform,8))
        print_item_footer()
        
        print_item_header("cleaning log files")
        if old_log:
            print("    Renaming previous log file 'Launcher.log' to '{}'".format(old_log))
            
        remove_logs_older_than = 14 
        removed=0
        if remove_logs_older_than:
            now = time.time()
            threshold = now - 60*60*24*remove_logs_older_than # Number of seconds in two days
            for folder,sub_folders,files in os.walk(self.config.launcher_home):
                if folder==self.config.launcher_home:
                    for fname in files:
                        if fname.startswith("Launcher.log."):
                            fpath = os.path.join(folder,fname)
                            ftime = os.path.getctime(fpath)
                            if ftime<threshold:
                                os.remove(fpath)
                                removed += 1
#                                 print('*',fname,ftime)
#                             else:
#                                 print(' ',fname,ftime) 
                    break
            print("    Removed {} log files older than {} days.".format(removed,remove_logs_older_than))
        print_item_footer()
            
    def remote_location_has_changed(self):
        if self.wRemoteLocation.GetValue()=="$VSC_DATA":
            #Copying job output is meaningless since it is already there
            self.wCopyToVSCDATA.SetValue(False)
            self.wCopyToVSCDATA.Disable()
        else:
            self.wCopyToVSCDATA.Enable()
        self.get_remote_file_location()
        self.remote_subfolder_has_changed()
        
    def remote_subfolder_has_changed(self):
        v = self.remote_location
        
        remote_subfolder = self.wRemoteSubfolder.GetValue()
        if not remote_subfolder in ['','.','./']:
            v = os.path.join(v,self.wRemoteSubfolder.GetValue())
            self.remote_location_subfolder = v
    
        jobname = self.wJobName.GetValue()
        if jobname:
            v = os.path.join(v,jobname)
        self.wRemoteFileLocation.SetValue(v)
    
    def job_name_has_changed(self):
        if self.wJobName.GetValue():
            self.wLocalFileLocation.SetValue( os.path.join(self.local_parent_folder,self.wJobName.GetValue()) )
            self.wLocalFileLocation.SetForegroundColour(wx.BLUE)
            if not hasattr(self,'remote_location_subfolder'):
                self.remote_location_subfolder = self.get_remote_file_location()
            self.wRemoteFileLocation.SetValue( os.path.join(self.remote_location_subfolder,self.wJobName.GetValue()) )
            f_script = os.path.join(self.wLocalFileLocation.GetValue(),'pbs.sh')
            if os.path.exists(f_script):
                msg = "Found script '{}'.\nDo you want to load it?".format(f_script)
                msg+= "\n(If you answer No, it will be overwritten if you continue to work in this local file location.)"
                answer = wx.MessageBox(msg, 'Script found',wx.YES|wx.NO | wx.ICON_QUESTION)
                if answer==wx.NO:
                    self.set_status_text("The script was not loaded. It will be overwritten when pressing the 'Save' button.")
                    self.is_script_up_to_date = False
                elif answer==wx.YES:
                    self.load_job_script()
                    self.set_status_text("Script '{}' is loaded.".format(f_script))
        else:
            self.wLocalFileLocation.SetValue( self.local_parent_folder )
            self.wLocalFileLocation.SetForegroundColour(wx.BLACK)
            self.SetStatusText('')
        
    def local_parent_folder_text(self,event):
        #print 'local_parent_folder_text',event.GetString()
        #todo???
        pass
    
    def local_parent_folder_set_focus(self):
        self.wLocalFileLocation.SetForegroundColour(wx.BLACK)
        self.wLocalFileLocation.SetValue( self.local_parent_folder )
        self.set_status_text('Modify the local location. Do NOT add the Job name.')
        
    def local_parent_folder_has_changed(self):
        if self.wLocalFileLocation.GetValue():
            v = self.wLocalFileLocation.GetValue()
            vsplit = os.path.split(v)
            if vsplit[-1]==self.wJobName.GetValue():
                msg = "The parent folder is equal to the job name.\n:Is that OK?"
                answer = wx.MessageBox(msg, 'Warning',wx.NO|wx.YES | wx.ICON_WARNING)
                if  answer==wx.NO:
                    v = os.path.join(*vsplit[:-1])
        else:
            v = self.local_parent_folder
        self.local_parent_folder = v
        if self.wJobName.GetValue():
            self.wLocalFileLocation.SetValue( os.path.join(self.local_parent_folder,self.wJobName.GetValue()) )
            self.wLocalFileLocation.SetForegroundColour(wx.BLUE)
        else:
            self.wLocalFileLocation.SetForegroundColour(wx.BLACK)
        my_makedirs(self.local_parent_folder)

    def local_parent_folder_key_up(self,event):
        key_code = event.GetKeyCode()
        if key_code == wx.WXK_ESCAPE:
            #print 'escape'
            self.wLocalFileLocation.SetValue(self.local_parent_folder)
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
        return self.wCluster.GetValue()
    
    def update_cluster_list(self):
        reload(clusters)
        self.wCluster.SetItems(clusters.cluster_list)        
        try:
            i = clusters.cluster_list.index(self.cluster)
        except ValueError:
            i = 0
        cluster = clusters.cluster_list[i]
        self.set_cluster(cluster)
    
    def get_user_id(self):
        user_id = self.wUserId.Value
        pattern=re.compile(r'vsc\d{5}')
        m = pattern.match(user_id)
        return None if (m is None) else user_id
    
    def cluster_has_changed(self):
        self.cluster = self.get_cluster()
        self.login_node = clusters.login_nodes[self.cluster][0]
        print_item_header('Accessing cluster:')
        print('    cluster    = '+self.cluster)
        print('    login node = '+self.login_node)
        print_item_footer()
        self.set_node_set_items(clusters.node_set_names(self.cluster))
        self.wSelectModule.SetItems(self.get_module_avail())
        self.is_resources_modified = True
         
    def set_node_set_items(self,node_sets):
        node_set_names = clusters.node_set_names(self.cluster)
        self.wNodeSet.SetItems(node_set_names)
        
        node_set = self.config.attributes.get('node_set','')
        if node_set:
            try: 
                i = node_set_names.index(node_set)
            except ValueError:
                i=0
        else:
            i=0
        self.set_node_set(i)

    def set_node_set(self,node_set):    
        if isinstance(node_set,str):
            i = self.wCluster.GetItems().index(node_set.split()[0])
        else: # type is int 
            i = node_set
        self.wNodeSet.Select(i)
        self.node_set_has_changed()
        self.wNNodesRequested.SetRange(1,self.current_node_set.n_nodes)
        self.wNCoresPerNodeRequested.SetRange(1,  self.current_node_set.n_cores_per_node)
        self.wNCoresPerNodeRequested.SetValue(    self.current_node_set.n_cores_per_node)
        self.wGbPerNodeGranted      .SetValue(str(self.current_node_set.gb_per_node))
        self.wGbPerCoreRequested    .SetRange(0,  self.current_node_set.gb_per_node)
        self.wNCoresRequested       .SetRange(1,  self.current_node_set.n_cores_per_node*self.current_node_set.n_nodes)
        self.request_nodes_cores()

    def node_set_has_changed(self):
        #remove extras required by previous node set
        if self.current_node_set and hasattr(self,'script'):
            self.current_node_set.script_extras(self.script,remove=True)
        #set current node set
        i=self.wNodeSet.GetSelection()
        self.current_node_set = clusters.node_sets[self.cluster][i]

        #add extras required by new node set
        if self.current_node_set and hasattr(self,'script'):
            self.current_node_set.script_extras(self.script)

        self.is_resources_modified = True
            
    def request_nodes_cores(self):
        n_cores,gb_per_core = self.current_node_set.request_nodes_cores(self.wNNodesRequested.GetValue(),self.wNCoresPerNodeRequested.GetValue())
        self.wNCoresRequested   .SetValue(n_cores)
        self.wGbPerCoreRequested.SetValue(gb_per_core)
        self.wGbTotalGranted    .SetValue(str(gb_per_core*n_cores))

        self.wNNodesRequested       .GetTextCtrl().SetForegroundColour(wx.BLUE)
        self.wNCoresPerNodeRequested.GetTextCtrl().SetForegroundColour(wx.BLUE)        
        self.wNCoresRequested       .GetTextCtrl().SetForegroundColour(wx.BLACK)
        self.wGbPerCoreRequested    .GetTextCtrl().SetForegroundColour(wx.BLACK)

    def request_cores_memory(self):
        n_nodes, n_cores, n_cores_per_node, gb_per_core, gb = self.current_node_set.request_cores_memory(self.wNCoresRequested.GetValue(),self.wGbPerCoreRequested.GetValue())
        self.wNNodesRequested       .SetValue(n_nodes)
        self.wNCoresRequested       .SetValue(n_cores)
        self.wNCoresPerNodeRequested.SetValue(n_cores_per_node)
        self.wGbPerCoreRequested    .SetValue(gb_per_core)
        self.wGbTotalGranted        .SetValue(str(gb))

        self.wNNodesRequested       .GetTextCtrl().SetForegroundColour(wx.BLACK)
        self.wNCoresPerNodeRequested.GetTextCtrl().SetForegroundColour(wx.BLACK)        
        self.wNCoresRequested       .GetTextCtrl().SetForegroundColour(wx.BLUE)
        self.wGbPerCoreRequested    .GetTextCtrl().SetForegroundColour(wx.BLUE)
        
    def get_remote_file_location(self):
        ssh = sshtools.Client(self.get_user_id(),self.login_node)
        if ssh.connected():
            #retrieve the values of the environment variables $VSC_DATA and $VSC_SCRATCH 
            try:
                stdout_dat, stderr = ssh.execute('echo $VSC_DATA')
                stdout_scr, stderr = ssh.execute('echo $VSC_SCRATCH')
                #location                = stdout_loc[0].strip()
                self.vsc_data_folder    = stdout_dat[0].strip()
                self.vsc_scratch_folder = stdout_scr[0].strip()
                del stderr
                location = self.vsc_scratch_folder 
                if self.wRemoteLocation.GetValue()=='$VSC_DATA':
                    location = self.vsc_data_folder 
            except Exception as e:
                #todo: need to investigate what went wrong
                print("!!! Something unexpected has happened...")
                log_exception(e)
                location = self.wRemoteLocation.GetValue()
            ssh.close()
        else:
            location = self.wRemoteLocation.GetValue()

        self.remote_location = location
        if not self.wRemoteSubfolder.GetValue() in ['.','./']:
            location = os.path.join(location,self.wRemoteSubfolder.GetValue()) 
        return location
        
    def get_module_avail(self):
        ssh = sshtools.Client(self.get_user_id(),self.login_node)
        if ssh.connected():
            stdout, stderr = ssh.execute('module avail')
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
        print_item_header('update_resources_from_script()')
        lines = self.wScript.GetText().splitlines(True)
        self.script.parse(lines)
        self.set_default_pbs_parameters()
        self.update_resources()
        print_item_footer()
        
    def update_script_from_resources(self):       
        if not self.is_resources_modified:
            return
        print_item_header('update_script_from_resources()')
        #make sure all values are transferred to self.script.values
        if not hasattr(self, 'script'):
            self.script = pbs.Script()
            self.current_node_set.script_extras(self.script)
        if not 'n_nodes' in self.script.values: 
            self.script.add_pbs_option('-l','nodes={}:ppn={}'.format(self.wNNodesRequested.GetValue()
                                                                    ,self.wNCoresPerNodeRequested.GetValue()))
        else:
            self.script.values['n_nodes'         ] = self.wNNodesRequested       .GetValue()
            self.script.values['n_cores_per_node'] = self.wNCoresPerNodeRequested.GetValue()
        
        if not 'walltime_seconds' in self.script.values:
            self.script.add_pbs_option('-l', 'walltime='+pbs.walltime_seconds_to_str(self.walltime_seconds))
        else:
            self.script.values['walltime_seconds'] = self.walltime_seconds
            
        if self.wNotifyAddress.GetValue(): 
            abe = ''
            if self.wNotifyAbort.IsChecked():
                abe+='a'
            if self.wNotifyBegin.IsChecked():
                abe+='b'
            if self.wNotifyEnd.IsChecked():
                abe+='e'
            if abe: 
                if not 'notify_address' in self.script.values:
                    self.script.add_pbs_option('-M',self.wNotifyAddress.GetValue())
                    self.script.add_pbs_option('-m',abe)
                else:
                    self.script.values['notify_address'] = self.wNotifyAddress.GetValue()
                    self.script.values['notify_abe'    ] = abe
                    
        self.script.values['enforce_n_nodes'] = self.wEnforceNNodes.IsChecked()
        if self.wEnforceNNodes.IsChecked():
            self.script.add_pbs_option('-W','x=nmatchpolicy:exactnode')

        if self.wJobName.GetValue():
            if not 'job_name' in self.script.values:
                self.script.add_pbs_option('-N',self.wJobName.GetValue())
            else:
                self.script.values['job_name'] = self.wJobName.GetValue()
                
        lines = self.script.compose()
        print("    Script updated.")
        self.format_script(lines)
        print_item_footer()
        
    def format_script(self,lines):
        self.wScript.ClearAll()
        for line in lines:
            self.wScript.AddText(line)
        #self.wScript.Colourise()
        self.is_script_modified = False
        print("    Script formatted.")

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
        self.update_value(self.wEnforceNNodes, '')
        
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
            if ctrl.GetValue() == value:
                return False
            else:
                ctrl.SetValue(value)
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
        self.wWalltime.SetValue(n)        
        self.walltime_has_changed()
           
    def walltime_has_changed(self):
        self.walltime_seconds = self.wWalltime.GetValue() * self.walltime_unit_sec[self.wWalltimeUnit.Selection]
 
    def walltime_unit_has_changed(self,event=None):
        unit_sec = self.walltime_unit_sec[self.wWalltimeUnit.Selection]
        if self.walltime_seconds%unit_sec:
            self.wWalltime.SetValue(self.walltime_seconds/unit_sec + 1)
            self.walltime_seconds = self.wWalltime.GetValue()*unit_sec
        else:
            self.wWalltime.SetValue( self.walltime_seconds/unit_sec ) 
            self.is_resources_modified = True

    def notify_address_has_changed(self,event=None):
        #print "event handler 'notify_address_has_changed (event={})'".format(event)

        pattern=re.compile(r'\b[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,4}\b')
        s = self.wNotifyAddress.GetValue().lower()
        match = pattern.match(s)
        if not match:            
            self.wNotifyAddress.SetForegroundColour(wx.RED)
            msg = "Enter a valid e-mail address."
            wx.MessageBox(msg, 'Invalid e-mail address',wx.OK | wx.ICON_WARNING)
        else:
            self.wNotifyAddress.SetForegroundColour(wx.BLACK)
            self.wNotifyAddress.SetValue(s)
            self.is_script_up_to_date = False
        if not event is None:
            event.Skip()

    def add_module(self):
        module = self.wSelectModule.GetValue()
        if module.startswith("--"):
            self.set_status_text('No module selected.')
        else:
            self.wScript.AddText("\nmodule load "+self.wSelectModule.GetValue())
                            
    def user_id_has_changed(self):
        user_id = self.validate_user_id(self.wUserId.GetValue())
        if user_id:
            self.wRemoteFileLocation.SetValue( self.get_remote_file_location() )
    
    def load_job_script(self):
        self.wScript.ClearAll()
        self.is_resources_modified = False
        s = self.wLocalFileLocation.GetValue()
        if os.path.isdir(s):
            s = os.path.join(s,'pbs.sh')
        if os.path.isfile(s):
            fname = s
        else:
            msg = "No 'pbs.sh' script found at location '{}'.".format(s)
            wx.MessageBox(msg, 'Error',wx.OK | wx.ICON_ERROR)
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
        
    def check_job_name_present(self):
        if not self.wJobName.GetValue():
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
            self.set_status_text("Job script not saved: job name missing.")
            return False
        self.update_script_from_resources()
        self.set_status_text("Saving job script to '{}' ...".format(self.wJobName.GetValue()))
        my_makedirs(self.wLocalFileLocation.GetValue())
        fname = os.path.join(self.wLocalFileLocation.GetValue(),'pbs.sh')
        self.wScript.SaveFile(fname)
        self.set_status_text("Job script saved to '{}'. Add input files to local location.".format(fname))
        return True
    
    def copy_to_remote_location(self,submit=False):
        self.set_status_text('Preparing to copy ... ')
        local_folder = self.wLocalFileLocation.GetValue()
        remote_folder = self.wRemoteFileLocation.GetValue()
        try:
            ssh = sshtools.Client(self.get_user_id(),self.login_node)
            ssh_ftp = ssh.open_sftp()
        except Exception as e:
            log_exception(e)
            msg = 'FJR_ERROR: copy_to_remote_location : no connection.'
            print(msg)
            self.set_status_text(msg)
            return
        self.set_status_text('Creating remote folder ... ')
        cmd = "mkdir -p "+remote_folder
        out,err = ssh.execute(cmd)
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
                        out,err = ssh.execute(cmd)
                job_name = self.wJobName.GetValue()
                for f in files:
                    if  (not f.startswith('.')          ) \
                    and (not f.startswith(job_name+".o")) \
                    and (not f.startswith(job_name+".e")):
                        self.set_status_text("  Copying file '{}/{}' to '{}'".format(folder,f,remote))
                        src = os.path.join(folder,f)
                        dst = os.path.join(remote,f)
                        ssh_ftp.put(src,dst)
                        if sys.platform in ("win32"):
                            cmd = "dos2unix "+dst
                            ssh.execute(cmd)
                        elif sys.platform in ("darwin"):
                            cmd = "dos2unix -c mac "+dst
                            ssh.execute(cmd)
        except Exception as e:
            log_exception(e)
            msg = "Failed copying local folder ('{}') contents to remote folder '{}'.".format(local_folder,remote_folder)
        else:
            msg = "Successfully copied local folder ('{}') contents to remote folder '{}'.".format(local_folder,remote_folder)
        self.set_status_text(msg)
        
        if submit:
            msg = "Submitting job ..."  
            self.set_status_text(msg)
            cmd = 'cd '+remote_folder+' && qsub pbs.sh'      
            out,err = ssh.execute(cmd)
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
    
    def set_status_text(self,msg,colour=wx.BLACK):
        print_item_header('Status bar text:')
        print( Indent(msg,4))
        self.SetStatusText(msg)
        #todo allow for colored messages? not working on macosx
        #self.GetStatusBar().SetForeGroundColour(colour)
        print_item_footer()
        
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
            FJR_NO_CONNECTION = no sshtools connection established
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
            ssh = sshtools.Client(self.get_user_id(),self.login_node)
            ssh_ftp = ssh.open_sftp()
        except Exception as e:
            log_exception(e)
            self.set_status_text("Failed to retrieve job {} : '{}': No connection established.".format(job_id,job_name))
            job_data['status'] = 'not retrieved due to no connection (Paramiko/SSH).'
            return FJR_NO_CONNECTION
        
        try:
            cmd = 'cd '+remote_folder
            out,err = ssh.execute(cmd)
            if err:
                ssh.close()
                self.set_status_text("Failed to retrieve job {} : '{}': remote folder '{}' not found.".format(job_id,job_name,remote_folder))
                job_data['status'] = 'not retrieved due to inexisting remote folder.'
                return FJR_INEXISTING
            
            cmd += " && tree -fi"
            out,err = ssh.execute(cmd+" --noreport")
            remote_files_and_folders = make_tree_output_relative(out)
            o = job_name+'.o'+job_id
            if not o in remote_files_and_folders:
                ssh.close()
                self.set_status_text("Failed to retrieve job {} : '{}': job is not finished (or file '{}.o{}' is missing)".format(job_id,job_name,job_name,job_id))
                job_data['status'] = 'not retrieved due to unfinished job.'
                return FJR_NOT_FINISHED
            
            cmd += "d --noreport"
            out,err = ssh.execute(cmd)
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
                    out,err = ssh.execute(cmd)
                    
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
                        out,err = ssh.execute(cmd)
                    
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
    
    def show_submitted_jobs(self):
        self.show_jobs(self.wJobsSubmitted,self.config.attributes['submitted_jobs'],none="All finished jobs are retrieved.\n")        
        ssh = sshtools.Client(self.get_user_id(),self.login_node)
        if ssh.connected():
            cmd = "qstat -u "+self.wUserId.GetValue()
            out,err = ssh.execute(cmd)
            ssh.close()
            for line in out:
                self.wJobsSubmitted.AppendText(line)
            del err
        else:
            msg = 'No info on status of submitted jobs (no Paramiko/SSH connection).'
            self.set_status_text(msg)
            
    def show_jobs(self,textctrl,jobs,none="-- none --"):
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
            textctrl.SetValue(none)

    def validate_user_id(self,user_id):
        msg ="Enter a valid VSC user_id (e.g. vscDDDDD)\nor press Cancel: "
        m = Launcher.regexp_userid.match(user_id)
        while not m:
            dlg = wx.TextEntryDialog(self,msg,caption="Invalid user_id",defaultValue=user_id)
            res = dlg.ShowModal()
            if res==wx.ID_OK:
                user_id = dlg.GetValue()
                del dlg
                m = Launcher.regexp_userid.match(user_id)
                if m:
                    print("\nStoring valid user_id '{}'".format(user_id))
                    self.config.attributes['user_id'] = user_id
                    self.wUserId.SetValue(user_id)                    
            else:
                return None
        return user_id
    
class SelectLoginNodeDialog(wx.Dialog):
    def __init__(self, parent, title="Select login node"):
        super(SelectLoginNodeDialog,self).__init__(parent, title=title)
    
        login_node = parent.login_node
        self. choices = wx.ComboBox( self, wx.ID_ANY
                                   , choices=clusters.login_nodes[parent.cluster], value=login_node
                                   , style=wx.CB_DROPDOWN|wx.CB_READONLY|wx.CB_SIMPLE|wx.EXPAND
                                   )
        buttons = self.CreateSeparatedButtonSizer(wx.CANCEL|wx.OK)
        self.SetSizer( grid_layout( [ [self.choices,1,wx.EXPAND]
                                    , wx.StaticText(self)
                                    , wx.StaticText(self)
                                    , wx.StaticText(self)
                                    , wx.StaticText(self)
                                    , wx.StaticText(self)
                                    , wx.StaticText(self)
                                    , wx.StaticText(self)
                                    , wx.StaticText(self)
                                    , [buttons,1,wx.EXPAND]
                                    ]
                                  , ncols=1, growable_cols=[0] ) )
    def select(self):
        login_node = clusters.login_nodes[self.GetParent().cluster][self.choices.GetSelection()]
        if self.GetParent().login_node == login_node:
            return 
        else:
            self.GetParent().login_node = login_node
            print_item_header('Selecting login node')
            print('    selected: '+login_node)
            print_item_footer()
            
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
    frame = Launcher(None,"Launcher v{}.{}".format(*__VERSION__))
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