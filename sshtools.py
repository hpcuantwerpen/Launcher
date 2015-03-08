from __future__ import print_function
import paramiko,wx
import datetime,re,pprint
import wxtools

SSH_DEFAULTS = {"SSH_WORK_OFFLINE": True
               ,"SSH_KEEP_CLIENT" : False
               ,"SSH_TIMEOUT"     : 15
               ,"SSH_WAIT"        : 30
               ,"SSH_VERBOSE"     : True
               ,"SSH_KEY"         : "''"
               }
for k,v in SSH_DEFAULTS.iteritems():
    exec("{} = {}".format(k,v))
        
ID_BUTTON_SSH_RESET = wx.NewId()

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

class Client(object):
    user_id          = None
    login_node       = None
    paramiko_client  = None
    last_try         = datetime.datetime(2014,12,31)
    last_try_success = False
    frame            = None
    
    def __init__(self,user_id,login_node):
        if SSH_WORK_OFFLINE:
            self  .paramiko_client = None
            Client.paramiko_client = None
            return
        if (Client.user_id!=user_id) or (Client.login_node!=login_node) or (not SSH_KEEP_CLIENT):
            #Close old client
            if Client.paramiko_client:
                Client.paramiko_client.close()
                Client.paramiko_client = None
        if Client.paramiko_client and SSH_KEEP_CLIENT:
            print("\nReusing Paramiko/SSH client (SSH_KEEP_CLIENT==True).")
            self.paramiko_client = Client.paramiko_client
        else:
            #get new client
            Client.user_id = user_id
            Client.login_node = login_node            
            self.paramiko_client = self._connect(user_id,login_node)
            if SSH_KEEP_CLIENT:
                Client.paramiko_client = self.paramiko_client
       
    def close(self):
        if not SSH_KEEP_CLIENT:
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
        if SSH_VERBOSE or err:
            print("\n<<< Executing Paramiko/SSH command:")
            print("  > ",cmd )
            print("  > stdout:" )
            pprint.pprint(out )
            print("  > stderr:" )
            pprint.pprint(err )
            print(">>> Paramiko/SSH command done." )
        return out,err

    def open_sftp(self):
        return self.paramiko_client.open_sftp()
    
    def connected(self):
        return (not self.paramiko_client is None)

    def _connect(self,user_id,login_node):
        """
        Try to open an ssh connection
          - if the previous attempt was succesful
          - or if the previous attempt was not succesful but long enough (SSH_WAIT) ago
        Return the ssh client or None.
        """  
        last_try = Client.last_try
        last_try_success = Client.last_try_success
        delta = datetime.datetime.now() - last_try
        
        if last_try_success or ( not last_try_success and delta.total_seconds() > SSH_WAIT ):
            #retry to make ssh connection    
            msg ="Creating and opening Paramiko/SSH client (SSH_KEEP_CLIENT=={}).".format(("True" if SSH_KEEP_CLIENT else "False"))
            self.frame_set_status(msg)
            ssh = None
            pwd = None
            while True:
                try:
                    if not ssh:
                        ssh = paramiko.SSHClient()
                        ssh.load_system_host_keys()
                        #ssh.load_system_keys("/home/Users/etijskens/.ssh/newkey")
                    if pwd is None:
                        ssh.connect(login_node, username=user_id)
    #                         ssh.connect(login_node, username=user_id,timeout=SSH_TIMEOUT)
                    else:
                        ssh.connect(login_node, username=user_id,timeout=SSH_TIMEOUT,password=pwd)
                
                except paramiko.ssh_exception.PasswordRequiredException as e:
                    print("Handled exception:",e)
                    dlg = wx.PasswordEntryDialog(self,"Enter pass phrase to unlock your key:")
                    res = dlg.ShowModal()
                    if res==wx.ID_OK:
                        pwd = dlg.GetValue()
                        del dlg
                    else:
                        del ssh
                        ssh = None
                        Client.last_try = datetime.datetime.now()
                        break
    
                except paramiko.ssh_exception.AuthenticationException as e:
                    #todo: test this
                    print("Handled exception:",e)
                    dlg = wx.PasswordEntryDialog(self,"Wrong password, retry ...\nEnter pass phrase to unlock your key:")
                    res = dlg.ShowModal()
                    if res==wx.ID_OK:
                        pwd = dlg.GetValue()
                        del dlg
                    else:
                        del ssh
                        ssh = None
                        Client.last_try = datetime.datetime.now()
                        break
                
                except Exception as e:
                    print("Handled exception (?):",e)
                    Client.last_try_success = False
                    ssh = None
                    msg ="Unable to connect via Paramiko/SSH to "+user_id+"@"+login_node
                    msg+="\nPossible causes are:"
                    msg+="\n  - no internet connection."
                    msg+="\n  - you are not connected to your home institution (VPN connection needed?)."
                    wx.MessageBox(msg, 'No Paramiko/SSH connection.',wx.OK | wx.ICON_INFORMATION)
                    Client.last_try = datetime.datetime.now()
                    break
                    
                else:
                    Client.last_try_success = True
                    break
    
            msg = "Paramiko/SSH connection established: {}@{}".format(Client.user_id,Client.login_node) if ssh else \
                  "Paramiko/SSH connection NOT established."
            self.frame_set_status(msg)
        else:
            # don't retry to make ssh connection
            ssh = None

        return ssh
    
    def frame_set_status(self,msg,colour=wx.BLACK):
        if Client.frame:
            Client.frame.set_status_text(msg,colour)

            
class SSHPreferencesDialog(wx.Dialog):
    def __init__(self, parent, title="SSH preferences"):
        super(SSHPreferencesDialog,self).__init__(parent, title=title)

        lst=[]
        tip="Check this box to avoid the timeout while trying to connect to the host."
        lst.extend(wxtools.pair(self, label="SSH_WORK_OFFLINE", value=SSH_WORK_OFFLINE,tip=tip,style0=wx.ALIGN_RIGHT))
        lst[-2]=[lst[-2],0,wx.ALIGN_RIGHT]
        tip="Connect once to the host and keep the connection alive during the entire session."
        lst.extend(wxtools.pair(self, label="SSH_KEEP_CLIENT", value=SSH_KEEP_CLIENT ,tip=tip))
        lst[-2]=[lst[-2],0,wx.ALIGN_RIGHT]
        tip="Verbose logging of SSH actions"
        lst.extend(wxtools.pair(self, label="SSH_VERBOSE", value=SSH_VERBOSE     ,tip=tip))
        lst[-2]=[lst[-2],0,wx.ALIGN_RIGHT]
        tip="Give up trying to connect to the host after this many seconds."
        lst.extend(wxtools.pair(self, label="SSH_TIMEOUT", value=SSH_TIMEOUT, value_range=(0,120, 1), tip=tip))
        lst[-2]=[lst[-2],0,wx.ALIGN_RIGHT]
        tip="Do not retry to connect before this many seconds"
        lst.extend(wxtools.pair(self, label="SSH_WAIT", value=SSH_WAIT, value_range=(0,360,10), tip=tip))
        lst[-2]=[lst[-2],0,wx.ALIGN_RIGHT]
        tip="Path and filename of your ssh key for accessing the VSC clusters. If empty, Paramiko tries to find it automatically (not always successful)."
        lst.extend(wxtools.pair(self, label="SSH_KEY", value=SSH_KEY, tip=tip))
        lst[-1]=[lst[-1],1,wx.EXPAND]
        lst[-2]=[lst[-2],0,wx.ALIGN_RIGHT]
        
        lst.append( wx.StaticText(self) )
        lst.append([wx.Button(self,ID_BUTTON_SSH_RESET,label="Reset to Defaults"),1,wx.EXPAND])

        lst.append([wx.Button(self, wx.ID_CANCEL),1,wx.EXPAND])
        lst.append([wx.Button(self, wx.ID_OK    ),1,wx.EXPAND])
        sizer =  wxtools.grid_layout(lst, ncols=2, growable_cols=[0,1])
        self.SetSizer(sizer)
        self.lst = lst
    
    def GetValue(self,item=None):
        value = None
        n = len(self.lst)
        if item is None:
            value = [w.GetValue() for w in self.lst[1:2:n]]
        else:
            for i in range(0,n,2):
                if self.lst[i].GetLabel()==item:
                    value = self.lst[i+1].GetValue()
        return value 
    
    def update_preferences(self):
        print("\nUpdating SSH preferences:")
        changed = self.update_preference("SSH_KEY")
        if changed:
            del Client.paramiko_client
            Client.paramiko_client = None
        changed |= self.update_preference("SSH_WORK_OFFLINE")    
        changed |= self.update_preference("SSH_KEEP_CLIENT")    
        changed |= self.update_preference("SSH_TIMEOUT")    
        changed |= self.update_preference("SSH_WAIT")    
        changed |= self.update_preference("SSH_VERBOSE")
        if not changed:
            print("  nothing changed.")
        
    def update_preference(self,varname):
        v = self.GetValue(varname)
        changed = eval("{}!={}".format(varname,v))
        if changed:
            exec("{} = {}".format(varname,v))
            print("SSH preferences updated : {} = {}".format(varname,v))
        return changed