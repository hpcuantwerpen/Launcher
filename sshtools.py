from __future__ import print_function
import paramiko,wx
import datetime,re,pprint

SSH_KEEP_CLIENT = True
SSH_TIMEOUT = 15
#   Number of seconds that paramiko.SSHClient.connect attempts to
#   to connect.
SSH_WAIT = 30
#   the minimum number of sceconds between two successive attempts 
#   to make an ssh connection if the first attempt was unsuccesful.
SSH_VERBOSE = True
_regexp_userid = re.compile(r'vsc\d{5}')

class Client(object):
    user_id          = None
    login_node       = None
    paramiko_client  = None
    last_try         = datetime.datetime(2014,12,31)
    last_try_success = False
    frame            = None
    
    def __init__(self,user_id,login_node):
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
            m = _regexp_userid.match(user_id)
            if not m:
                msg ="Invalid user_id: "+user_id
                wx.MessageBox(msg, 'Error',wx.OK | wx.ICON_WARNING)
                return
    
            ssh = None
            pwd = None
            while True:
                try:
                    if not ssh:
                        ssh = paramiko.SSHClient()
                        ssh.load_system_host_keys()
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
    
        else:
            # don't retry to make ssh connection
            ssh = None

        msg = "Paramiko/SSH connection established: {}@{}".format(Client.user_id,Client.login_node) if ssh else \
              "Paramiko/SSH connection NOT established."
        self.frame_set_status(msg)
        return ssh
    
    def frame_set_status(self,msg,colour=wx.BLACK):
        if Client.frame:
            Client.frame.set_status_text(msg,colour)