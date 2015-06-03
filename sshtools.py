from __future__ import print_function
import paramiko,wx
import datetime,re,pprint,copy,os
import wxtools
from indent import Indent
import log


SSH_DEFAULTS    = {"SSH_WORK_OFFLINE": False
                  ,"SSH_KEEP_CLIENT" : False
                  ,"SSH_TIMEOUT"     : 15
                  ,"SSH_WAIT"        : 30
                  ,"SSH_VERBOSE"     : True
                  ,"SSH_KEY"         : ""
                  ,"SSH_KEEP_PREFS"  : True
                  }
SSH_DESCRIPTORS = {"SSH_WORK_OFFLINE": "Work offline"
                  ,"SSH_KEEP_CLIENT" : "Keep SSH client alive"
                  ,"SSH_TIMEOUT"     : "Timeout [s] for connecting"
                  ,"SSH_WAIT"        : "Wait [s] before retry"
                  ,"SSH_VERBOSE"     : "Verbose"
                  ,"SSH_KEY"         : "Private key:"
                  ,"SSH_KEEP_PREFS"  : "Store preferences"
                  }
################################################################################
### Model part of MVC 
def reset_ssh_preferences():
    if Client.ssh_preferences:
        print("sshtools.reset_Client.ssh_preferences()")
    Client.ssh_preferences = copy.copy(SSH_DEFAULTS)   
     
reset_ssh_preferences()

# SSH_WORK_OFFLINE = True
# SSH_KEEP_CLIENT = False
# SSH_TIMEOUT = 15
#   Number of seconds that paramiko.SSHClient.connect attempts to

#   to connect.
# SSH_WAIT = 30
#   the minimum number of sceconds between two successive attempts 
#   to make an ssh connection if the first attempt was unsuccesful.
# SSH_VERBOSE = True
# SSH_KEY=""

ID_BUTTON_SSH_RESET = wx.NewId()
################################################################################
_regexp_username = re.compile(r'vsc\d{5}')

def is_valid_username(username):
    if not isinstance(username,(str,unicode)):
        return False
    m = _regexp_username.match(username)
    if m:
        return True
    else:
        return False

################################################################################
class InvalidUsername(Exception):
    pass
################################################################################
class MissingLoginNode(Exception):
    pass
################################################################################
class Client(object):
################################################################################
    ssh_preferences  = None
    username         = None
    pwd              = None
    login_node       = None
    paramiko_client  = None
    last_try         = datetime.datetime(2014,12,31)
    last_try_success = False
    frame            = None
    
    def __init__(self,username=None,login_node=None,pwd=None,force=False,verbose=True):
        if Client.ssh_preferences["SSH_WORK_OFFLINE"]:
            self  .paramiko_client = None
            Client.paramiko_client = None
            return
        if (Client.username!=username) or (Client.login_node!=login_node) or (not Client.ssh_preferences["SSH_KEEP_CLIENT"]):
            #We need a new Client object, close the old one first
            if Client.paramiko_client:
                Client.paramiko_client.close()
                Client.paramiko_client = None
        if Client.paramiko_client and Client.ssh_preferences["SSH_KEEP_CLIENT"]:
            print("\nReusing Paramiko/SSH client (SSH_KEEP_CLIENT==True).")
            self.paramiko_client = Client.paramiko_client
        else:
            #create a new client
            if username:
                if not is_valid_username(username):
                    raise InvalidUsername
                Client.username = username
            if login_node:
                Client.login_node = login_node
            if not Client.login_node:
                raise MissingLoginNode
            if pwd:
                Client.pwd = pwd
                   
            self.paramiko_client = self._connect(force=force,verbose=verbose)
            if Client.ssh_preferences["SSH_KEEP_CLIENT"]:
                Client.paramiko_client = self.paramiko_client
       
    def close(self):
        if not Client.ssh_preferences["SSH_KEEP_CLIENT"]:
            self.paramiko_client.close()
            Client.paramiko_client = None
    
    def execute(self,cmd):
        """
        Run a remote command cmd through ssh
        Return stdout and stderr (list of strings (=lines) containing eol)
        """
        stdin, stdout, stderr = self.paramiko_client.exec_command(cmd)
        out = stdout.readlines()
        err = stderr.readlines()
        del stdin
        if Client.ssh_preferences["SSH_VERBOSE"] or err:
            with log.LogItem("Executing Paramiko/SSH command:"):
                print("  >",cmd )
                print("  > stdout:" )
                print( Indent(out,4) )
                print("  > stderr:" )
                print( Indent(err,4) )
        return out,err

    def open_sftp(self):
        return self.paramiko_client.open_sftp()
    
    def connected(self):
        return (not self.paramiko_client is None)

    def _connect(self,force=False,verbose=True):
        """
        Try to open an ssh connection
          - if the previous attempt was succesful
          - or if the previous attempt was not succesful but long enough (SSH_WAIT) ago
        Return the ssh client or None.
        """  
        last_try = Client.last_try
        last_try_success = Client.last_try_success
        delta = datetime.datetime.now() - last_try
        
        if not (force or last_try_success or ( not last_try_success and delta.total_seconds() > Client.ssh_preferences["SSH_WAIT"] ) ):
            msg ="SSH_WAIT prevented reattempt to connect. Next attempt only possible after {}s.".format(Client.ssh_preferences["SSH_WAIT"])
            self.frame_set_status(msg)
            return None
        
        #retry to make ssh connection    
        msg ="Creating and opening Paramiko/SSH client (SSH_KEEP_CLIENT=={},force_reconnect=={}).".format(Client.ssh_preferences["SSH_KEEP_CLIENT"],force)
        self.frame_set_status(msg)
        
                try:
                    ssh = paramiko.SSHClient()
                    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
                    if not Client.ssh_preferences["SSH_KEY"]:
                        ssh.load_system_host_keys()
                        ssh.connect(login_node, username=user_id,timeout=Client.ssh_preferences["SSH_TIMEOUT"])
                    else:
                        if not Client.pwd:
                            ssh.connect( login_node, username=user_id
                                       , key_filename=Client.ssh_preferences['SSH_KEY']
                                       , timeout=Client.ssh_preferences["SSH_TIMEOUT"]
                                       )
                        else:
                            ssh.connect( login_node, username=user_id
                                       , key_filename=Client.ssh_preferences['SSH_KEY'], password=Client.pwd
                                       , timeout=Client.ssh_preferences["SSH_TIMEOUT"]
                                       )
                                
                except paramiko.ssh_exception.PasswordRequiredException as e:
                    msg = "A pass phrase is needed to unlock your key. \nEnter pass phrase or press 'Cancel':"
                    Client.pwd = self.need_pw(msg)
                    if not Client.pwd:
                        del ssh
                        ssh = None
                        Client.last_try = datetime.datetime.now()
                        break
    
                except paramiko.ssh_exception.SSHException as e:
                    msg = "A pass phrase might be needed to unlock your key. "\
                          "In some cases paramiko does not recognise ssh keys "\
                          "protected with a pass phrase as valid keys. If you "\
                          "know you key is pass phrase protected, enter a pass "\
                          "phrase, or press 'Cancel' otherwise."
                    msg+= "\n\nException details:\n"+self.exception_to_str(e)
                    Client.pwd = self.need_pw(msg)
                    if not Client.pwd:
                        del ssh
                        ssh = None
                        Client.last_try = datetime.datetime.now()
                        break
    
                except paramiko.ssh_exception.AuthenticationException as e:
                    msg = "Wrong pass phrase ...\nRe-enter pass phrase or press 'Cancel':"
                    Client.pwd = self.need_pw(msg)
                    if not Client.pwd:
                        del ssh
                        ssh = None
                        Client.last_try = datetime.datetime.now()
                        break
                                        
                except Exception as e:
                    msg = "Unable to connect via Paramiko/SSH to "+str(user_id)+"@"+login_node
                    msg+= "\nUnhandled Exception:"
                    msg+= "\n\nException details:\n"+self.exception_to_str(e)
                    ssh = None
                    wx.MessageBox(msg, 'No Paramiko/SSH connection.',wx.OK | wx.ICON_INFORMATION)
                    Client.last_try = datetime.datetime.now()
                    break
                    
                else:
                    Client.last_try_success = True
                    break
            
            #end while not ssh
            
            msg = "Paramiko/SSH connection established: {}@{}".format(str(Client.user_id),Client.login_node) if ssh else \
                  "Paramiko/SSH connection NOT established."
            self.frame_set_status(msg)

        return ssh
    
    def exception_to_str(self,e):
        s = StringIO.StringIO()
        traceback.print_exc(limit=1,file=s)
        s = s.getvalue()
        return s
    
    def need_pw(self,msg):
        dlg = wx.PasswordEntryDialog(None,msg)
        res = dlg.ShowModal()
        pw = dlg.GetValue()
        dlg.Destroy()
        if res!=wx.ID_OK:
            pw = None
        return pw

    def frame_set_status(self,msg,colour=wx.BLACK):
        if Client.frame:
            Client.frame.set_status_text(msg,colour)
   
class InexistingKey(Exception):
    pass
         
class SshPreferencesDialog(wx.Dialog):
    def __init__(self, parent, title="SSH preferences"):
        super(SshPreferencesDialog,self).__init__(parent, title=title, style=wx.DEFAULT_DIALOG_STYLE | wx.RESIZE_BORDER)

        sizer = wx.BoxSizer(orient=wx.VERTICAL)
        
        lst=[] # all the widgets in the grid_layout
        self.key2ctrl={} # map preference key to ctrl
        
        swap=True
        ikey=-2 if swap else -1

        key="SSH_WORK_OFFLINE"
        tip="Do not attempt to connect to the host."
        lst.extend(wxtools.pair(self, label=SSH_DESCRIPTORS[key], value=Client.ssh_preferences[key],tip=tip,swap=swap))
        lst[-2]=[lst[-2],0,wx.ALIGN_RIGHT]
        self.key2ctrl[key] = lst[ikey][0] if isinstance(lst[ikey], list) else lst[ikey]

        key="SSH_KEEP_CLIENT"
        tip="Connect once to the host and keep the connection alive during the entire session."
        lst.extend(wxtools.pair(self, label=SSH_DESCRIPTORS[key], value=Client.ssh_preferences[key] ,tip=tip,swap=swap))
        lst[-2]=[lst[-2],0,wx.ALIGN_RIGHT]
        self.key2ctrl[key] = lst[ikey][0] if isinstance(lst[ikey], list) else lst[ikey]
        
        key="SSH_VERBOSE"
        tip="Verbose logging of SSH actions"
        lst.extend(wxtools.pair(self, label=SSH_DESCRIPTORS[key], value=Client.ssh_preferences[key],tip=tip,swap=swap))
        lst[-2]=[lst[-2],0,wx.ALIGN_RIGHT]
        self.key2ctrl[key] = lst[ikey][0] if isinstance(lst[ikey], list) else lst[ikey]
        
        key="SSH_TIMEOUT"
        tip="Give up trying to connect to the host after this many seconds."
        lst.extend(wxtools.pair(self, label=SSH_DESCRIPTORS[key], value=Client.ssh_preferences[key], value_range=(0,120, 1), tip=tip,swap=swap))
        self.key2ctrl[key] = lst[ikey][0] if isinstance(lst[ikey], list) else lst[ikey]

        key="SSH_WAIT"
        tip="Do not retry to connect before this many seconds"
        lst.extend(wxtools.pair(self, label=SSH_DESCRIPTORS[key], value=Client.ssh_preferences[key], value_range=(0,360,10), tip=tip,swap=swap))
        self.key2ctrl[key] = lst[ikey][0] if isinstance(lst[ikey], list) else lst[ikey]
        
        swap=False
        ikey=-2 if swap else -1

        key="SSH_KEY"
        tip="Path and filename of your ssh key for accessing the VSC clusters. If empty, Paramiko tries to find it automatically (not always successful)."
        lst.extend(wxtools.pair(self, label=SSH_DESCRIPTORS[key], value=str(Client.ssh_preferences[key]), tip=tip))
        #                                                    the str() converts unicode to str, which is what the TextCtrl expects
        lst[-1]=[lst[-1],1,wx.EXPAND]
        lst[-2]=[lst[-2],0,wx.ALIGN_RIGHT]
        self.key2ctrl[key] = lst[ikey][0] if isinstance(lst[ikey], list) else lst[ikey]
        
        if Client.ssh_preferences[key] and not os.path.exists(Client.ssh_preferences[key]):
            lst.append(wx.StaticText(self))
            error = wx.StaticText(self, label="Private key refers to inexisting file.")
            error.SetForegroundColour(wx.RED)
            lst.append([error,1,wx.EXPAND])
            
        swap=True
        ikey=-2 if swap else -1

        key="SSH_KEEP_PREFS"
        tip="Store the settings in the config file, as to make them effective also in your next Launcher setting."
        lst.extend(wxtools.pair(self, label=SSH_DESCRIPTORS[key], value=Client.ssh_preferences[key], tip=tip,swap=swap))
        lst[-1]=[lst[-1],1,wx.EXPAND]
        lst[-2]=[lst[-2],0,wx.ALIGN_RIGHT]
        self.key2ctrl[key] = lst[ikey][0] if isinstance(lst[ikey], list) else lst[ikey]

        sizer.Add( wxtools.grid_layout( lst, ncols=2, growable_cols=[1] ),flag=wx.EXPAND )
        
        sizer.Add( wx.StaticText(self) )
        
        tip="Restore default settings"
        self.wResetButton = wx.Button(self,ID_BUTTON_SSH_RESET,label=tip)

        sizer.Add( self.wResetButton,flag=wx.EXPAND )
        
        sizer.Add( wx.StaticText(self) )

        sizer.Add( self.CreateSeparatedButtonSizer(wx.CANCEL|wx.OK),flag=wx.EXPAND )
        self.SetSizer(sizer)

#         lst.append( wx.StaticText(self) )
#         lst.append( wx.StaticText(self) )
#         
#         lst.append( wx.StaticText(self) )
#         lst.append()
        self.wResetButton.Bind(wx.EVT_BUTTON,self.reset_preferences)
#         lst[-3][0].SetBackgroundStyle(wx.BG_STYLE_COLOUR)
#         lst[-3][0].SetBackgroundColour(wx.Colour(0,0,255))
                        
    def reset_preferences(self,event):
        reset_Client.ssh_preferences()
        #update the dialog
        for k,v in Client.ssh_preferences.iteritems():
            self.dct[k].SetValue(v)
                
    def update_preferences(self):
        some_value_has_changed = False
        for key,ctrl in self.key2ctrl.iteritems():
            ctrl_value = ctrl.GetValue()
            crnt_value = Client.ssh_preferences[key]
            value_has_changed = crnt_value!=ctrl_value
            if value_has_changed:
                print("changing Client.ssh_preferences[{}] from {} to {}.".format(key,crnt_value,ctrl_value))
                Client.ssh_preferences[key] = ctrl_value
                if key=="SSH_KEY":
                    if not os.path.exists(ctrl_value):
                        raise InexistingKey
                    #destroy current connection if a new key is provided.
                    del Client.paramiko_client
                    Client.paramiko_client = None
            some_value_has_changed |= value_has_changed
        if not some_value_has_changed:
            print("nothing changed.")
            
        return Client.ssh_preferences["SSH_KEEP_PREFS"]
