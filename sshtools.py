from __future__ import print_function

import paramiko,wx

import datetime,re,copy,os

import wxtools
import log
from indent   import Indent
from settings import Settings

SSH_DESCRIPTORS = {"SSH_WORK_OFFLINE": "Work offline"
                  ,"SSH_KEEP_CLIENT" : "Keep SSH client alive"
                  ,"SSH_TIMEOUT"     : "Timeout [s] for connecting"
                  ,"SSH_WAIT"        : "Wait [s] before retry"
                  ,"SSH_VERBOSE"     : "Verbose"
                  ,"SSH_KEY"         : "Private key:"
                  ,"SSH_KEEP_PREFS"  : "Store preferences"
                  }

################################################################################
### Model part of MVC                                                        ###
################################################################################


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
class InexistingKey(Exception):
    pass
class InvalidUsername(Exception):
    pass
class MissingLoginNode(Exception):
    pass
class PassphraseNeeded(Exception):
    pass
################################################################################
class Client(Settings):
################################################################################

    @classmethod
    def full_reset(cls,defaults=None):
        #reset the ssh_preferences
        cls.reset(defaults=defaults)
        cls.username         = None
        cls.pwd              = None
        cls.login_node       = None
        cls.paramiko_client  = None
        cls.last_try         = datetime.datetime(2014,12,31)
        cls.last_try_success = False
        cls.frame            = None
        cls.key              = None
    
    def __init__(self,username=None,login_node=None,pwd=None,force=False,verbose=True,key=None):
        if Client.ssh_work_offline:
            self  .paramiko_client = None
            Client.paramiko_client = None
            return
        if (Client.username!=username) or (Client.login_node!=login_node) or (not Client.ssh_keep_client):
            #We need a new Client object, close the old one first
            if Client.paramiko_client:
                Client.paramiko_client.close()
                Client.paramiko_client = None
        if Client.paramiko_client and Client.ssh_keep_client:
            print("\nReusing Paramiko/SSH client (ssh_keep_client==True).")
            self.paramiko_client = Client.paramiko_client
        else:
            #create a new client
            if not is_valid_username(username):
                if not is_valid_username(Client.username):
                    raise InvalidUsername
            else:
                Client.username = username
            if login_node:
                Client.login_node = login_node
            if not Client.login_node:
                raise MissingLoginNode
            if pwd:
                Client.pwd = pwd
            if not key is None:
                key = str(key)
                if not os.path.exists(key):
                    raise InexistingKey
                else:
                    Client.ssh_key = key
            self.paramiko_client = self._connect(force=force,verbose=verbose)
                
            if Client.ssh_keep_client:
                Client.paramiko_client = self.paramiko_client
       
    def close(self):
        if not Client.ssh_keep_client:
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
        if Client.ssh_verbose or err:
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
        
        if not (force or last_try_success or ( not last_try_success and delta.total_seconds() > Client.ssh_wait ) ):
            msg ="The ssh_wait setting prevented reattempt to connect. Next attempt only possible after {}s.".format(Client.ssh_wait)
            self.set_status_text(msg)
            return None
        
        #retry to make ssh connection    
        msg ="Creating and opening Paramiko/SSH client (ssh_keep_client=={},force_reconnect=={}).".format(Client.ssh_keep_client,force)
        self.set_status_text(msg)
        
        try:
            ssh = paramiko.SSHClient()
            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            if not Client.ssh_key:
                ssh.load_system_host_keys()
                ssh.connect(Client.login_node, username=Client.username,timeout=Client.ssh_timeout)
            else:
                if not Client.pwd:
                    ssh.connect( Client.login_node, username=Client.username
                               , key_filename=Client.ssh_key
                               , timeout=Client.ssh_timeout
                               )
                else:
                    ssh.connect( Client.login_node, username=Client.username
                               , key_filename=Client.ssh_key, password=Client.pwd
                               , timeout=Client.ssh_timeout
                               )
        except paramiko.SSHException as e1:
            #log.log_exception(e1)
            if Client.ssh_key and Client.pwd is None:
                try:
                    ssh.connect( Client.login_node, username=Client.username
                               , key_filename=Client.ssh_key, password='test'
                               , timeout=Client.ssh_timeout
                               )
                except paramiko.ssh_exception.AuthenticationException as e2:
                    #log.log_exception(e2)
                    self.set_status_text('Connection FAILED ({})'.format(str(type(e2))))
                    raise PassphraseNeeded
                except:
                    self.set_status_text('Connection FAILED ({})'.format(str(type(e1))))
                    raise e1
            else:
                self.set_status_text('Connection FAILED ({})'.format(str(type(e1))))
                raise e1
                
        except Exception as e:
            self.set_status_text('Connection FAILED ({})'.format(str(type(e))))
            raise
        else:
            Client.last_try_success = True
                        
        msg = "Paramiko/SSH connection established: {}@{}".format(str(Client.username),Client.login_node) 
        self.set_status_text(msg)
        return ssh

    def set_status_text(self,msg,colour=wx.BLACK):
        pass #todo

################################################################################
# Client default ssh settings:
Client.full_reset(defaults = {'ssh_work_offline' : False
                             ,'ssh_keep_client'  : False
                             ,'ssh_timeout'      : 15   #number of seconds that paramiko.sshclient.connect attempts to connect.
                             ,'ssh_wait'         : 30
                             ,'ssh_verbose'      : True #the minimum number of sceconds between two successive attempts 
                                                        #to make an ssh connection if the first attempt was unsuccesful.
                             ,'ssh_key'          : ""
                             ,'ssh_keep_prefs'   : True
                             }
                 )

################################################################################
### test code                                                                ###
################################################################################
import unittest,traceback,sys

class TestClient(unittest.TestCase):
    def setUp(self):
        unittest.TestCase.setUp(self)
        print()
    def test0(self):
        Client.full_reset()
        try:
            ssh = Client()
        except Exception as e:
            assert isinstance(e,InvalidUsername)

    def test1(self):
        Client.full_reset()
        try:
            ssh = Client(username='vsc20170')
        except Exception as e:
            assert isinstance(e,MissingLoginNode)
                
    def test2(self):
        #using inexistent key
        Client.full_reset()
        try:
            Client(username='vsc20170', login_node='login.hpc.uantwerpen.be', key='i_m_not_there')
        except Exception as e:
            assert isinstance(e,InexistingKey)

    def test3(self):
        #using a valid key without a pass phrase
        Client.full_reset()
        ssh = Client(username='vsc20170',login_node='login.hpc.uantwerpen.be',key='id_rsa_npw')
        self.assertTrue(ssh.connected())
        ssh.execute('ls -l da*')
        del ssh

    def test4(self):
        #using a valid key and a pass phrase when none is needed
        #the pass phrase is ignored...
        Client.full_reset()
        try:
            ssh = Client(username='vsc20170',login_node='login.hpc.uantwerpen.be',key='id_rsa_npw',pwd='no_pwd_needed')
        except Exception as e:
            raise
        else: 
            self.assertTrue(ssh.connected())
            ssh.execute('ls -l da*')
            del ssh

    def test5(self):
        #using pass phrase protected key but pass phrase not provided 
        Client.full_reset()
        Client.pwd = None
        try:
            Client(username='vsc20170',login_node='login.hpc.uantwerpen.be',key='id_rsa_eendjes')
        except Exception as e:
            #traceback.print_exc(file=sys.stdout)
            assert isinstance(e,PassphraseNeeded)

    def test6(self):
        #using an invalid pass phrase protectd key with the pass phrase  
        try:
            ssh = Client(username='vsc20170',login_node='login.hpc.uantwerpen.be',key='id_rsa_eendjes',pwd='eendjes')
        except Exception as e:
            assert isinstance(e,paramiko.AuthenticationException)
        else:
            ssh.execute('ls -l da*')
            del ssh

    def test7(self):
        #using a valid pass phrase protectd key with the pass phrase  
        try:
            ssh = Client(username='vsc20170',login_node='login.hpc.uantwerpen.be',key='id_rsa',pwd='Thezoo12')
        except Exception as e:
            assert isinstance(e,paramiko.AuthenticationException)
        else:
            ssh.execute('ls -l da*')
            del ssh
            

if __name__=='__main__':
    Client.ssh_work_offline = False
    unittest.main()
    