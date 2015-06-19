from __future__ import print_function

import os,re,errno,datetime,posixpath
import log,cfg,sshtools,constants
from script import Script,Shebang
from indent import Indent
import clusters

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
class Launcher(object): 
################################################################################
    """
    The Launcher Model part in the MVC approach. This contains only the logic, not the GUI part.
    """
    is_testing = False
    verbose    = False
    
    def __init__(self,reset=False,**kwargs):
        cfg.Config.verbose = True
        self.is_resources_modified = -1
        #load the config file and copy all settings to member variables
        path = os.path.join(constants.launcher_home(),'Launcher.cfg')
        self.config = cfg.Config(path=path)
        if reset:
            self.config.reset()
        self.config.inject_all(self)

        #Create ConfigValues, in case they did not exist before
        self.config.create('username', default=None, inject_in=self)
        
        default = clusters.cluster_names[0]
        self.config.create('cluster', default=default, inject_in=self)
 
        default = clusters.login_nodes[self.get_cluster()][0]
        self.config.create('login_node', default=default, inject_in=self)
        
        default = clusters.decorated_nodeset_names(self.get_cluster())[0]
        self.config.create('selected_nodeset_name', default=default, inject_in=self)
        
        self.config.create('module_lists', default={}, inject_in=self)

        self.config.create('notify', default={'address':'','a':False,'b':False,'e':False}, inject_in=self)

        self.config.create('enforce_n_nodes'  , default=True, inject_in=self)
        self.config.create('automatic_requests', default=True, inject_in=self)
            
        default = os.path.join(constants.launcher_home(),'jobs')
        self.config.create('local_file_location',default=default,inject_in=self)
        self.config.create('remote_file_location',choices=["$VSC_SCRATCH","$VSC_DATA"],inject_in=self)
        self.config.create('subfolder',default='/',inject_in=self)
        self.config.create('retrieve_copy_to_local_file_location',choices=[True,False],inject_in=self)
        self.config.create('retrieve_copy_to_$VSC_DATA'          ,choices=[False,True],inject_in=self)
        
        self.set_default_values_non_config()

        if not self.get_cluster() in clusters.cluster_names:
            self.config['cluster'].reset() # this will also set the node set
        self.change_cluster()
        
        if Launcher.is_testing:
            with log.LogItem('Config values'):
                for k,v in self.config.values.iteritems():
                    print('    '+k+' : '+str(v.value))
              
        self.is_resources_modified = 0 #start counting changes
        self.is_script_modified = False
    
    def set_default_values_non_config(self):
        #create other variables than the config variables        
        self.n_nodes_req          =  1
        self.n_cores_per_node_req =  1
        self.n_cores_req          =  1
        self.gb_per_core_req      = -1 # value to be retrieved from the nodeset
        
        self.walltime_seconds = 3600
        self.jobname = ''
        
    def change_cluster(self,new_cluster=None):
        if self.get_cluster()==new_cluster:
            return #already ok
        if not new_cluster is None:
            if isinstance(new_cluster, int):
                new_cluster = clusters.cluster_names[new_cluster]
            if not new_cluster in clusters.cluster_names:
                raise UnknownCluster(new_cluster)
            self.set_cluster(new_cluster)
            cluster = new_cluster
        else:
            #get the cluster from self.config
            cluster = self.get_cluster()

        self.nodeset_names = clusters.decorated_nodeset_names(cluster)
        self.change_selected_nodeset_name()
                   
        self.modules = self.get_modules()
        
        self.is_resources_modified = self.increment(self.is_resources_modified)
        
        with log.LogItem('[M] Accessing cluster:'):
            print('    cluster    = '+cluster)
            print('    login node = '+self.get_login_node())
            print('    node set   = '+self.get_selected_nodeset_name())
            if Launcher.verbose:
                print('    modules    = ')
                print(Indent(self.modules,6))
            else:
                print("    modules    = [...]")
                
    def change_selected_nodeset_name(self,new_selected_nodeset_name=None):
        if self.get_selected_nodeset_name()==new_selected_nodeset_name:
            return #already ok        
        if new_selected_nodeset_name is None:
            selected = self.get_selected_nodeset_name()
            if not selected in self.nodeset_names:
                selected = self.nodeset_names[0]
        else:
            if not new_selected_nodeset_name in self.nodeset_names:
                raise UnknownNodeset("Nodeset '{}' not defined for cluster '{}.".format(new_selected_nodeset_name,self.get_cluster()))
            self.set_selected_nodeset_name(new_selected_nodeset_name)
            selected = new_selected_nodeset_name
            
        #remove extras required by previous node set
        if getattr(self, 'selected_nodeset',None) and hasattr(self,'script'):
            self.selected_nodeset.nodeset_features(self.script,remove=True)
        
        #set current node set
        #print(self.nodeset_names)
        self.selected_nodeset = clusters.nodesets[self.get_cluster()][self.nodeset_names.index(selected)]

        #add extras required by new node set
        if self.selected_nodeset and hasattr(self,'script'):
            self.selected_nodeset.nodeset_features(self.script)
        
        self.n_cores_per_node_req = self.selected_nodeset.n_cores_per_node
        self.n_cores_per_node_max = self.selected_nodeset.n_cores_per_node        
        self.request_nodes_and_cores_per_node()

    def n_nodes_max(self):
        return self.selected_nodeset.n_nodes

    def n_cores_per_node_max(self):
        return self.selected_nodeset.n_cores_per_node
    
    def n_cores_max(self):
        return self.selected_nodeset.n_cores_per_node * self.selected_nodeset.n_nodes
    
    def gb_per_core_max(self):
        return self.selected_nodeset.gb_per_node
    
    def increment(self,value):
        if value<0:
            #do not count changes during initialization
            return value
        else:
            return value+1
        
    def change_n_nodes_req(self,value):
        if self.n_nodes_req == value:
            return #already ok
        self.n_nodes_req = value
        self.request_nodes_and_cores_per_node()

    def change_n_cores_per_node_req(self,value):
        if self.n_cores_per_node_req == value:
            return #already ok
        self.n_cores_per_node_req = value
        self.request_nodes_and_cores_per_node()

    def request_nodes_and_cores_per_node(self):
        n_cores,gb_per_core = self.selected_nodeset.request_nodes_cores(self.n_nodes_req,self.n_cores_per_node_req)
        self.n_cores_req  = n_cores
        self.gb_per_core_req = gb_per_core
        self.gb_total_granted = gb_per_core*n_cores
        self.is_resources_modified = self.increment(self.is_resources_modified)
 
    def change_n_cores_req(self,value):
        if self.n_cores_req == value:
            return #already ok
        self.n_cores_req = value
        self.request_cores_and_memory_per_core()
        
    def change_gb_per_core_req(self,value):
        if self.gb_per_core_req == value:
            return #already ok
        self.gb_per_core_req = value
        self.request_cores_and_memory_per_core()

    def request_cores_and_memory_per_core(self):
        n_nodes, n_cores, n_cores_per_node, gb_per_core, gb = self.selected_nodeset.request_cores_memory(self.n_cores_req,self.gb_per_core_req)
        self.n_nodes_req          = n_nodes
        self.n_cores_req          = n_cores
        self.n_cores_per_node_req = n_cores_per_node
        self.gb_per_core_req      = gb_per_core
        self.gb_total_granted     = gb

    def set_status_text(self,text):
        pass #todo
    
    def local_job_folder(self):
        if not self.jobname:
            raise MissingJobName
        job_folder = self.get_subfolder()
        if not job_folder:
            job_folder = self.get_local_file_location()
        else:
            job_folder = os.path.join(self.get_local_file_location(), job_folder)
        job_folder = os.path.join(job_folder,self.jobname)
        return job_folder
    
    def remote_job_folder(self):
        """
        use posixpath.join(a) because the remote path is always on UNIX
        """
    
    def load_job(self):
        """
        """
        filename = os.path.join(self.local_job_folder(),'pbs.sh')
        #create a new script, if there was already a script it will be garbage collected
        self.script = Script(filename=filename)
        self.update_resources_from_script()
            
    def unsaved_changes(self):
        """ Are there any unsaved changes? 
        If nothing was changed to the default script options, False is returned
        If nothing was changed since the last save, False is returned
        """
        return (self.is_resources_modified or (hasattr(self,'script') and self.script.is_modified))

    def save_job(self):
        """
        """
        if not self.update_script_from_resources():
            #not really necessary - but clean anyway
            self.update_resources_from_script()
        filename = os.path.join(self.local_job_folder(),'pbs.sh')
        self.script.write(filename)

    def get_modules(self):
#         if hasattr(self,'is_initializing') and self.is_testing:
#             return ["-- none --"]
        try:
            ssh = sshtools.Client(self.get_username(),self.get_login_node())
        except Exception as e:
            log.log_exception(e)
            module_list = self.get_module_lists().get( self.get_cluster(), ['-- none found (no connection) --'] )
            return module_list
        
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
        
        #store for reuse when there is no connection
        cluster = self.get_cluster().lower()
        module_list = []
        for k,v in lsts.iteritems():
            if cluster in k.lower():
                module_list.append('### '+k+' ###')
                module_list.extend(v) 
        self.add_to_module_lists( (self.get_cluster(),module_list) )
        return module_list
    
    def update_script_from_resources(self,force=False):       
        with log.LogItem('Updating script from resources:'):
            is_new_script = False
            if not hasattr(self, 'script'):
                self.script = Script()
                is_new_script = True
                self.selected_nodeset.nodeset_features(self.script)
            if not force and not self.is_resources_modified and not is_new_script:
                print('    Script is already up to date.')
                return False
                
            self.script.add(Shebang())
            
            self.script.add('#PBS -l nodes={}:ppn={}'.format(self.n_nodes_req,self.n_cores_per_node_req))
            self.script.add('#PBS -l walltime={}'.format(walltime_seconds_to_str(self.walltime_seconds)))
             
            notify = self.get_notify()
            notify_address = is_valid_mail_address(notify['address'])
            if notify_address:
                notify['address'] = notify_address 
                abe = ''
                for c in 'abe':
                    if notify[c]:
                        abe+=c
                if abe: 
                    self.script.add('#PBS -M {}'.format(notify['address']))
                    self.script.add('#PBS -m {}'.format(abe              ))
                        
            if self.get_enforce_n_nodes():
                self.script.add   ('#PBS -W x=nmatchpolicy:exactnode')
            else:
                self.script.remove('#PBS -W x=nmatchpolicy:exactnode')
                
            if self.jobname:
                self.script.add('#PBS -N {}'.format(self.jobname))
            
            self.script.add("#La# generated_on = "+str(datetime.datetime.now()))
            self.script.add("#La#      cluster = "+self.get_cluster())
            self.script.add("#La#      nodeset = "+self.get_selected_nodeset_name())
            
            if is_new_script:
                self.script.add('#')
                self.script.add("#--- shell commands below ".ljust(80,'-'))
                self.script.add('cd $PBS_O_WORKDIR')
                
            lines = self.script.get_lines()
            if Launcher.verbose:
                print(Indent('### begin script ###',4))
                print(Indent(self.script.script_lines,6))
                print(Indent('### end   script ###',4))
            print("    Script updated.")
            self.is_script_modified = True
            self.is_resources_modified = 0
            
            return lines 
        
    def update_resources_from_script(self,lines=None):
        with log.LogItem('Updating resources from script:'):
            if lines:
                if not hasattr(self, 'script'):
                    self.script = Script()
                self.script.parse(lines)        
            
            if not hasattr(self, 'script'):
                print("    No input lines provided, and script is empty, nothing to do.")
                return
            
            try:
                cluster = self.script['cluster']
                self.change_cluster(cluster)
            except UnknownCluster as e:
                log.log_exception(e,msg_after="Using '{}' instead.".format(self.get_cluster()))
                
            try:
                nodeset = self.script['nodeset']
                self.change_selected_nodeset_name(nodeset)
            except UnknownNodeset as e:
                log.log_exception(e,msg_after="Using '{}' instead.".format(self.get_selected_nodeset_name()))
            self.selected_nodeset.nodeset_features(self.script)
                        
            new_request = False
            try:
                nodes = self.script['nodes']
            except KeyError:
                nodes = self.n_nodes_req
                pass
            else:
                if self.n_nodes_req != nodes:
                    self.n_nodes_req = nodes
                    new_request = True
            try:
                ppn = self.script['ppn']
            except KeyError:
                ppn = self.n_cores_per_node_req
                pass
            else:
                if self.n_cores_per_node_req != ppn:
                    self.n_cores_per_node_req = ppn
                    new_request = True
            if new_request:
                self.request_nodes_and_cores_per_node()
                #the new request may have changed requested nodes or ppn 
                if self.n_nodes_req!=nodes or self.n_cores_per_node_req!=ppn:
                    self.script.add('#PBS -l nodes={}:ppn={}'.format(self.n_nodes_req,self.n_cores_per_node_req))
            
            self.set_enforce_n_nodes( self.script.has_line('#PBS -W x=nmatchpolicy:exactnode') )
            
            try:
                walltime = self.script['walltime']
            except KeyError:
                pass
            else:
                seconds = str_to_walltime_seconds(walltime)
                self.walltime_seconds = seconds
    
            notify = self.get_notify()
            try:
                notify['address'] = self.script['-M']
            except KeyError:
                pass
            else:
                abe = self.script['-m']
                for c in 'abe':
                    notify[c] = False
                for c in abe:
                    notify[c] = True
            try:
                self.jobname = self.script['-N']
            except KeyError:
                pass

            if Launcher.verbose:
                print(Indent('### begin script ###',4))
                print(Indent(self.script.script_lines,6))
                print(Indent('### end   script ###',4))
                print(Indent('Resources:',4))
                for k,v in self.__dict__.iteritems():
                    if not callable(v):
                        print(Indent(k+' :\n\t'+str(v),6))
            print("    Resources updated.")
                
    def get_gb_total_granted(self):
        return round(self.gb_total_granted,3)
    
################################################################################
walltime_units = {'s':    1
                 ,'m':   60
                 ,'h': 3600
                 ,'d':86400 }

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

class BadWalltimeFormat(Exception):
    pass

################################################################################
def embrace(varname,value):
    return '{'+varname+'='+str(value)+'}'
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
        self.assertEqual(len(launcher.nodeset_names), 2)
        self.assertTrue(launcher.selected_nodeset.name.startswith('Hopper-thin-nodes'), 2)
        self.assertEqual(launcher.get_login_node(), 'login.hpc.uantwerpen.be')
        self.assertEqual(launcher.n_nodes_req, 1)
        self.assertEqual(launcher.n_cores_req, 20)
        self.assertEqual(launcher.n_cores_per_node_req, 20)
        self.assertEqual(launcher.gb_per_core_req, launcher.gb_per_core_max()/20)

    def testScriptGeneration(self):
        launcher = Launcher()
        launcher.update_script_from_resources(force=True)
        launcher.script['walltime'] = walltime_seconds_to_str(9001)
        launcher.update_resources_from_script()
        
        
        
        
if __name__=='__main__':

    Launcher.is_testing = True
    unittest.main()
    