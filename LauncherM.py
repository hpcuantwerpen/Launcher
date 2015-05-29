from __future__ import print_function

import os,sys,pickle,random,shutil,time,re
import log
from indent import Indent
import clusters

from Value import Value
from __builtin__ import file

def __LAUNCHER_HOME__():
    env_home = "HOMEPATH" if sys.platform=="win32" else "HOME"
    user_home = os.environ[env_home]
    if not user_home or not os.path.exists(user_home):
        msg = "Error: environmaent variable '${}' not found.".format(env_home)
        raise EnvironmentError(msg)
    home = os.path.join(user_home,'Launcher')
    if not os.path.exists(home):
        os.mkdir(home)
    return home

__LAUNCHER_HOME__ = __LAUNCHER_HOME__()
__VERSION__ = [1,0]    

def git_version():
    f = open(os.path.join(__LAUNCHER_HOME__,'Launcher/git_commit_id.txt'))
    git_commit_id = f.readlines()[0].strip()
    return git_commit_id

# MVC - Modeler/Viewer/Controller
class LauncherM(object): # this is the Modeller
    
    def __init__(self,load_config=False,save_config=False):
        self.config = Config(must_load=load_config,must_save=save_config)
        self.is_resources_modified = False
        
    def initialize_value_objects(self):
        prev_cluster = self.config.attributes['cluster']
        try:
            i = clusters.cluster_list.index(prev_cluster)
        except ValueError:
            i=0
        self.m_wCluster.set(clusters.cluster_list[i])
    
        
    def __del__(self):
        self.config.save()
        
    def change_cluster(self,new_cluster):
        self.cluster = new_cluster
        self.login_node = clusters.login_nodes[self.cluster][0]
        
        with log.LogItem('[M] Accessing cluster:'):
            print('    cluster    = '+self.cluster)
            print('    login node = '+self.login_node)
        
        self.set_node_set_items(clusters.node_set_names(self.cluster))
        self.wSelectModule.SetItems(self.get_module_avail())
        self.is_resources_modified = self.increment(self.is_resources_modified)

    def change_node_set(self,selection):
        assert isinstance(selection,int)
        #remove extras required by previous node set
        if self.current_node_set and hasattr(self,'script'):
            self.current_node_set.script_extras(self.script,remove=True)
        #set current node set
        self.current_node_set = clusters.node_sets[self.cluster][selection]

        #add extras required by new node set
        if self.current_node_set and hasattr(self,'script'):
            self.current_node_set.script_extras(self.script)

        self.is_resources_modified = self.increment(self.is_resources_modified)

    def change_n_nodes_requested(self,value):
        self.n_nodes_requested = value
        self.request_nodes_and_cores_per_node()

    def change_n_cores_per_node_requested(self,value):
        self.n_cores_per_node_requested = value
        self.request_nodes_and_cores_per_node()

    def request_nodes_and_cores_per_node(self):
        n_cores,gb_per_core = self.current_node_set.request_nodes_cores(self.wNNodesRequested.GetValue(),self.wNCoresPerNodeRequested.GetValue())
        self.n_cores_requested  = n_cores
        self.gb_per_core_requested = gb_per_core
        self.gb_total_granted = gb_per_core*n_cores
        self.is_resources_modified = self.increment(self.is_resources_modified)
 
    def change_n_cores_requested(self,value):
        self.n_cores_requested = value
        request_cores_and_memory_per_core()
        
    def change_gb_per_core_requested(self,value):
        self.gb_per_core_requested = value
        request_cores_and_memory_per_core()

    def request_cores_and_memory_per_core(self):
        ### todo
        self.is_resources_modified = self.increment(self.is_resources_modified)

    def set_status_text(self,text):
        self.status = text    

    def save_job(self,mode):
        """
        ask > ask before overwrite.
        """
        folder = self.m_wLocalFileLocation.get()
        pbs_sh = os.path.join(folder,'pbs.sh')
        #is there anything to save?
        if not (self.is_resources_modified or self.is_script_modified):
            if not self.m_wJobName.get():
                self.set_status_text("There is nothing to save.")
                return True
            if os.path.exists(pbs_sh):
                self.set_status_text("Job script is already up to date.")
                return True
            
        file_exists = os.path.exists(pbs_sh)
        if mode=='w' and file_exists:
            raise AskPermissionToOverwrite

        if not self.wJobName.GetValue():
            raise MissingJobName
        
        #actual save or overwrite
        try:
            my_makedirs(folder)
            self.wScript.SaveFile(pbs_sh)
        except Exception as e:
            log_exception(e)
            return False
        else:
            self.status_text = "Job script saved {} to '{}'.".format('(overwritten)' if file_exists else '', pbs_sh)
            self.is_script_modified = False
            return True
        return False

class AskPermissionToOverwrite(Exception):
    pass

class MissingJobName(Exception):
    pass

class Config(object):
    """
    Launcher configuration
    """
    def __init__(self,must_load=False,must_save=False):
        """
        Some initialization options for (unit)testing. The default value correspond
        to production runs (not testing) 
        must_load  = False: do not load the config file
        must_save  = False: do not save the config file
        is_testing = True : do not call ShowModal() on dialogs
        """

        self.must_save = must_save
        
        self.attributes = {}
        if must_load:
            cfg_file = os.path.join(__LAUNCHER_HOME__,'Launcher.config')
            try:
                self.attributes = pickle.load(open(cfg_file))
                self.cfg_file = cfg_file
            except Exception as e:
                log.log_exception(e)

        default_attributes = { 'cluster' : 'some_cluster'
                             , 'mail_to' : 'your mail address goes here'
                             , 'walltime_unit' : 'hours'
                             , 'walltime_value' : 1
                             , 'local_files' : os.path.join(__LAUNCHER_HOME__,'jobs')
                             , 'user_id' : 'your_user_id'
                             , 'remote_subfolder' : '.'
                             , 'module_lists':{}
                             , 'submitted_jobs':{}
                             }
        for k,v in default_attributes.iteritems():
            if not k in self.attributes:
                self.attributes[k] = v

    def save(self):
        if self.must_save:
            pickle.dump(self.attributes,open(self.cfg_file,'w+'))

    def log_file(self):
        return os.path.join(__LAUNCHER_HOME__,'Launcher.log')

    def start_log(self,wx_version,paramiko_version):
        old_log = None
        f_log = self.log_file()
        if os.path.exists(f_log):
            old_log = f_log+str(random.random())[1:]
            shutil.copy(f_log,old_log)
    
        with log.LogItem('launch.py'):
            print('    begin of log.')
        
        with log.LogItem("Version info of used components"):
            print("    Python version:")
            print(Indent(sys.version,8))
            print("    Python executable:")
            print(Indent(sys.executable,8))
            print("    wxPython version:")
            print(Indent(wx_version,8))
            print("    paramiko version:")
            print(Indent(paramiko_version,8))
            print("    Launcher version:")
            global __VERSION__
            __VERSION__.append(git_version()) 
            s="v{}.{}.{}".format(*__VERSION__)
            print(Indent(s,8))
            print("    Platform:")
            print(Indent(sys.platform,8))
        
        with log.LogItem("cleaning log files"):
            if old_log:
                print("    Renaming previous log file 'Launcher.log' to '{}'".format(old_log))
            remove_logs_older_than = 14 
            removed=0
            if remove_logs_older_than:
                now = time.time()
                threshold = now - 60*60*24*remove_logs_older_than # Number of seconds in two days
                for folder,sub_folders,files in os.walk(__LAUNCHER_HOME__):
                    if folder==__LAUNCHER_HOME__:
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

    def stop_log(self):
        pass

def is_valid_mail_address(address):
    s = address.lower()
    pattern=re.compile(r'\b[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,4}\b')
    match = pattern.match(s)
    return s if match else False

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

    ### test code ###
if __name__=='__main__':
    with LogItem('begin','end'):
        print('in between')