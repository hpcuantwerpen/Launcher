from __future__ import print_function

import os,re,errno,datetime,posixpath,sys,pprint
import cfg,sshtools,constants
from collections import OrderedDict
from script import Script,my_makedirs
from indent import Indent
from log import log_exception,LogItem
import clusters

######################
### some constants ###
######################

# return codes for finished job retrieval (FJR):
FJR_SUCCESS      =0
FJR_INEXISTING   =1
FJR_NOT_FINISHED =2
FJR_NO_CONNECTION=3
FJR_ERROR        =4

################################################################################
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
def is_valid_mail_address(address):
    s = address.lower()
    pattern=re.compile(r'\b[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,4}\b')
    match = pattern.match(s)
    return s if match else False

################################################################################
def add_trailing_separator(path):
    path_sep = path
    if not path_sep.endswith(os.sep):
        path_sep+=os.sep
    return path_sep

################################################################################
def linux(path):
    if os.sep!=posixpath.sep:
        linux_path = path.replace(os.sep,posixpath.sep)
        return linux_path
    else:
        return path

################################################################################
walltime_units = { 'seconds':     1
                 , 'minutes':    60
                 , 'hours'  :  3600
                 , 'days'   : 86400 
                 }
  
################################################################################
class Launcher(object): 
################################################################################
    """
    A wrapper around script.Script that
      . acts as an in between for the gui and Script
      . store default values in a cfg.Config objecte
      . compute local and remote file paths
      . job management
      . ssh connection
    """
    is_testing = False
    verbose    = False

    def change_cluster(self,value=None,default=None,choices=[],inject_in=None):
        self.config.create('cluster',value=value,default=default,choices=choices            , inject_in=inject_in)
        cluster = self.config['cluster'].get()
        self.change_nodeset(               choices=clusters.decorated_nodeset_names(cluster), inject_in=inject_in)
        self.config.create('walltime_seconds', default=3600, choices=[1,clusters.walltime_limit[cluster]]
                                             , choices_is_range=True                        , inject_in=inject_in)
        self.config.create('walltime_unit'   , default='hours', choices=list(walltime_units), inject_in=inject_in)
        self.config.create('username'        , default=None                                 , inject_in=inject_in)
        self.config.create('login_node'      , choices=clusters.login_nodes[cluster]        , inject_in=inject_in)
        self.config.create('module_lists'    , default={}                                   , inject_in=inject_in)
        try:
            modules = self.get_modules()              
        except Exception as e:
            log_exception(e)
        else:
            self.config['module_lists'].get()[cluster] = modules
    def change_nodeset(self,value=None,default=None,choices=[],inject_in=None):
        self.config.create('nodeset',value=value,default=default,choices=choices            , inject_in=inject_in)
        cluster = self.config['cluster'].get()
        nodeset_name = self.config['nodeset'].get()
        nodeset_index= self.config['nodeset'].choices.index(nodeset_name)
        nodeset = clusters.nodesets[cluster][nodeset_index]
        self.change_nodes(choices=[1,nodeset.n_nodes]         , choices_is_range=True       , inject_in=inject_in)
        self.change_ppn  (choices=[1,nodeset.n_cores_per_node], choices_is_range=True       , inject_in=inject_in)
        self.change_gbpn (value=nodeset.gb_per_node                                         , inject_in=inject_in)
    
        
    def change_nodes(self,value=None,default=None,choices=[],choices_is_range=False,inject_in=None):
        self.config.create('nodes',value=value,default=default
                          , choices=choices, choices_is_range=choices_is_range, inject_in=inject_in)
    
    def change_ppn(self,value=None,default=None,choices=[],choices_is_range=False,inject_in=None):
        self.config.create('ppn'  ,value=value,default=choices[-1]
                          , choices=choices, choices_is_range=choices_is_range, inject_in=inject_in)
    
    def change_gbpn(self,value=None,default=None,choices=[],choices_is_range=False,inject_in=None):
        self.config.create('gbpn',value=value,default=default
                          , choices=choices, choices_is_range=choices_is_range, inject_in=inject_in)
    
    def __init__(self,reset=False):        
        cfg.Config.verbose = True
        self.is_resources_modified = -1
        #load the config file and copy all settings to member variables
        path = os.path.join(constants.launcher_home(),'Launcher.cfg')
        self.config = cfg.Config(path=path)
        if reset:
            self.config.reset()
        self.config.inject_all(self)

        #Create ConfigValues, in case they did not exist before
        self.change_cluster(choices=clusters.cluster_names, inject_in=self)        

        self.config.create('walltime',default='1:00:00',inject_in=self)
        
        self.config.create('submitted_jobs', default=OrderedDict(), inject_in=self)
        #   OrderedDict to assure that if all submitted jobs are retrieved, they are retrieved
        #   in the order they were submitted.

        self.config.create('notify_M', default='your.email@address.here', inject_in=self)
        self.config.create('notify_m', default='e'                      , inject_in=self)

        self.config.create('enforce_n_nodes'   , choices=[True,False], inject_in=self)
        self.config.create('automatic_requests', choices=[True,False], inject_in=self)
            
        default = os.path.join(constants.launcher_home(),'jobs')
        self.config.create('local_file_location',default=default,inject_in=self)
        self.config.create('remote_file_location',choices=["$VSC_SCRATCH","$VSC_DATA"],inject_in=self)
        self.config.create('subfolder',default='',inject_in=self)
        self.config.create('retrieve_copy_to_desktop' ,choices=[True,False],inject_in=self)
        self.config.create('retrieve_copy_to_VSC_DATA',choices=[False,True],inject_in=self)
        
        if Launcher.is_testing:
            with LogItem('Config values'):
                for k,v in self.config.values.iteritems():
                    print('    '+k+' : '+str(v.value))

        Script.auto_unhide = True
        self.script = Script(config=self.config)
        self.script.set_unsaved_changes(False)
    
    def load_job(self,parent_folder,jobname):
        """
        """
        filepath = os.path.join(parent_folder,jobname,'pbs.sh')
        #create a new script, if there was already a script it will be garbage collected
        self.script.read(filepath=filepath)
        self.script.set_unsaved_changes(False)
            
    def save_job(self,parent_folder,jobname):
        """
        """
        if not self.script.unsaved_changes():
            return True
        try:
            self.script['jobname'] = jobname
            filepath = os.path.join(parent_folder,jobname,'pbs.sh')
            self.script.write(filepath,warn_before_overwrite=False,create_directories=True)
        except:
            return False
        return True
    
    def set_status_text(self,msg):
        with LogItem('Status bar text:'):
            print( Indent(msg,4))
            try:
                self.gui_set_status_text(msg)
            except AttributeError:
                pass #Launcher is running without a gui
            except Exception as e:
                log_exception(e, msg_after='Unexpected error')
                
    def submit_job(self,local_parent_folder,remote_parent_folder,jobname):
        if not self.save_job(local_parent_folder,jobname):
#             self.set_status_text("Job not submitted: failed to save in local folder.")
            return
        self.copy_to_remote_location(local_parent_folder,remote_parent_folder,jobname,submit=True)
        
    def copy_to_remote_location(self,local_parent_folder,remote_parent_folder,jobname,submit=False):
        self.set_status_text('Preparing to copy ... ')
        local_folder  = os.path.join( local_parent_folder,jobname)
        remote_folder = posixpath.join(remote_parent_folder,jobname)
        try:
            ssh = sshtools.Client(self.get_username(),self.get_login_node())
            stdout,stderr = ssh.execute('echo '+remote_folder)
            remote_folder = stdout[0].strip()
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
                    remote = posixpath.join(remote_folder,sf[1:])
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
                for f in files:
                    if  (not f.startswith('.')          ) \
                    and (not f.startswith(jobname+".o")) \
                    and (not f.startswith(jobname+".e")):
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
                if self.get_retrieve_copy_to_VSC_DATA():
                    vsc_data_folder='$VSC_DATA'
                    folders_loc = local_folder .split(os.sep)
                    folders_rmt = remote_folder.split(posixpath.sep)
                    i=-1
                    while folders_loc[i]==folders_rmt[i]:
                        i-=1
                    while i<-1:
                        i+=1
                        vsc_data_folder = posixpath.join(vsc_data_folder,folders_rmt[i])
                else:
                    vsc_data_folder = ''
                self.get_submitted_jobs()[job_id] = {'cluster'            : self.get_cluster()
                                                    ,'username'           : self.get_username()
                                                    ,'job_folder_local'   : local_folder
                                                    ,'job_folder_remote'  : remote_folder
                                                    ,'job_folder_VSC_DATA': vsc_data_folder
                                                    ,'jobname'            : jobname
                                                    ,'status'             : 'submitted'
                                                    }
            self.set_status_text(msg)
                 
        ssh.close()
    
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
        jobname      = job_data['jobname']
        try:
            ssh = sshtools.Client(self.get_username(),self.get_login_node())
            ssh_ftp = ssh.open_sftp()
        except Exception as e:
            log_exception(e)
            self.set_status_text("Failed to retrieve job {} : '{}': No connection established.".format(job_id,jobname))
            job_data['status'] = 'not retrieved due to no connection (Paramiko/SSH).'
            return FJR_NO_CONNECTION
        if self.get_retrieve_copy_to_VSC_DATA():
            pass
        try:
            cmd = 'cd '+remote_folder
            out,err = ssh.execute(cmd)
            if err:
                ssh.close()
                self.set_status_text("Failed to retrieve job {} : '{}': remote folder '{}' not found.".format(job_id,jobname,remote_folder))
                job_data['status'] = 'not retrieved due to inexisting remote folder.'
                return FJR_INEXISTING
            
            cmd += " && tree -fi"
            out,err = ssh.execute(cmd+" --noreport")
            remote_files_and_folders = make_tree_output_relative(out)
            o = jobname+'.o'+job_id
            if not o in remote_files_and_folders:
                ssh.close()
                self.set_status_text("Failed to retrieve job {} : '{}': job is not finished (or file '{}.o{}' is missing)".format(job_id,jobname,jobname,job_id))
                job_data['status'] = 'job not retrieved since probably unfinished.'
                return FJR_NOT_FINISHED
            
            cmd += "d --noreport"
            out,err = ssh.execute(cmd)
            remote_folders = make_tree_output_relative(out)
            for f in remote_folders:
                if self.get_retrieve_copy_to_desktop():
                    lf = os.path.join(local_folder,f)
                    self.set_status_text("Creating local folder '{}'.".format(lf))
                    my_makedirs(lf)
                if self.get_retrieve_copy_to_VSC_DATA():
                    lf = os.path.join('$VSC_DATA',f)
                    self.set_status_text("Creating $VSC_DATA folder '{}'.".format(lf))
                    cmd = "mkdir -p "+lf
                    out,err = ssh.execute(cmd)
                    
            for f in remote_files_and_folders:
                if not f in remote_folders:
                    rf = os.path.join(remote_folder,f)
                    if self.get_retrieve_copy_to_desktop():
                        lf = os.path.join(local_folder,f)
                        self.set_status_text("Copying remote file '{}' to '{}'.".format(rf,lf))
                        ssh_ftp.get(rf,lf) 
                    if self.get_retrieve_copy_to_VSC_DATA():
                        lf = os.path.join(self.vsc_data_folder,f)
                        self.set_status_text("Copying remote file '{}' to '{}'.".format(rf,lf))
                        cmd = "cp {} {}".format(rf,lf)
                        out,err = ssh.execute(cmd)
                    
            ssh.close()
            self.set_status_text("Successfully retrieved job {}. Local folder is {}".format(job_id,local_folder))
            job_data['status'] = 'successfully retrieved.'
            return FJR_SUCCESS
        
        except Exception as e:
            log_exception(e)
            self.set_status_text("Failed to retrieve job {}. Local folder is {}".format(job_id,local_folder))
            ssh.close()
            return FJR_ERROR
        
    def retrieve_finished_jobs(self):
        self.set_status_text('Starting to retrieve finished jobs ...')
        self.retrieved_jobs = {}
        summary = [0,0,0,0,0]
        for job_id,job_data in self.get_submitted_jobs().iteritems():
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
            del self.get_submitted_jobs()[key] # can't do this on the fly

        tot = sum(summary)
        summary.append(tot)
        summary.append(summary[5]-summary[0])
        msg='Retrieved={}, removed={}, not finished={}, no connection={}, error={}, total={}, remaining={}'.format(*summary)
        self.set_status_text(msg)
        
        with LogItem("Retrieved jobs:"):
            print(Indent(self.retrieved_jobs,indent=4))

    def show_retrieved_jobs(self):
        if not hasattr(self,'retrieved_jobs'):
            self.retrieved_jobs = {}
        self.show_jobs(self.wJobsRetrieved,self.retrieved_jobs)
    
    def show_submitted_jobs(self):
        self.show_jobs(self.wJobsSubmitted,self.get_submitted_jobs(),none="All finished jobs are retrieved.\n")        
        ssh = sshtools.Client(self.get_username(),self.get_login_node())
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
            
    def get_modules(self):
        username = self.get_username()
        if not username:
            username = sshtools.Client.username
        ssh = sshtools.Client(username,self.get_login_node())
        stdout, stderr = ssh.execute('module avail -t')
        lines = stderr # yes, module avail writes to stderr
        del stdout
        ssh.close()
        
        lsts = {}
        for line in lines:
            line = line.strip()
            if line:
                if line.startswith('/'):
                    lsts[line] = []
                    lst = lsts[line]
                else:
                    lst.append(line)
        
        cluster = self.get_cluster().lower()
        module_list = []
        for k,v in lsts.iteritems():
            if cluster in k.lower():
                module_list.append('### '+k+' ###')
                module_list.extend(v) 
        return module_list
    
################################################################################
re_walltime = re.compile(r'(\d+):(\d\d):(\d\d)')

def str_to_walltime_seconds(walltime):
    m = re_walltime.match(walltime)
    if not m:
        raise BadWalltimeFormat(walltime)
    groups = m.groups()
    hh = int(groups[0])
    mm = int(groups[1])
    ss = int(groups[2])
    seconds = ss + 60*mm + 3600*hh
    return seconds

################################################################################
class BadWalltimeFormat(Exception):
    pass

################################################################################
def walltime_seconds_to_str(walltime_seconds):
    hh = int(walltime_seconds/3600)
    vv = walltime_seconds%3600 #remaining seconds after subtracting the full hours hh
    mm = int(vv/60)
    ss = int(vv%60)            #remaining seconds after also subtracting the full minutes mm
    s = str(hh)
    if hh>9:
        s = s.rjust(2,'0')
    s+=':'+str(mm).rjust(2,'0')+':'+str(ss).rjust(2,'0')
    return s

################################################################################
def abe_to_abort(abe):
    return 'a' in abe
def abort_to_abe(abort,abe):
    if abort:
        if not 'a' in abe:
            abe+='a'
    else:
        abe = abe.replace('a','')            
################################################################################
def abe_to_begin(abe):
    return 'b' in abe
def begin_to_abe(begin,abe):
    if begin:
        if not 'b' in abe:
            abe+=''
    else:
        abe = abe.replace('b','')            
################################################################################
def abe_to_end(abe):
    return 'e' in abe
def end_to_abe(end,abe):
    if end:
        if not 'e' in abe:
            abe+='e'
    else:
        abe = abe.replace('e','')            
################################################################################
class UnknownCluster(Exception):
    pass
################################################################################
class UnknownNodeset(Exception):
    pass
################################################################################
class AskPermissionToOverwrite(Exception):
    pass
################################################################################
class MissingJobName(Exception):
    pass
################################################################################
### test code ###
################################################################################
import unittest

class TestLauncher(unittest.TestCase):
    def setUp(self):
        unittest.TestCase.setUp(self)
        sshtools.Client.username = 'vsc20170'
        sshtools.Client.ssh_key  = 'id_rsa_npw'
        Launcher.verbose = True
        
    def testCtor(self):
        launcher = Launcher()
        self.assertEqual(launcher.get_cluster(), 'Hopper')
        self.assertEqual(launcher.get_login_node(), 'login.hpc.uantwerpen.be')
        print('0 TestLauncher.testCtor()\n',Indent(launcher.script.script_lines,2))

    def testScriptGeneration(self):
        launcher = Launcher()
        launcher.script['walltime'] = walltime_seconds_to_str(9001)
        print('0 TestLauncher.testScriptGeneration()\n',Indent(launcher.script.script_lines,2))
        
if __name__=='__main__':

    Launcher.is_testing = True
    unittest.main()
    