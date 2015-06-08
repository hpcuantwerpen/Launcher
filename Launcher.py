from __future__ import print_function

import os,re,errno
import log,cfg,sshtools,constants,transactions,pbs
from indent import Indent
import clusters


################################################################################
class Launcher(transactions.TransactionManager): 
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
        
        default = clusters.decorated_node_set_names(self.get_cluster())[0]
        self.config.create('selected_node_set_name', default=default, inject_in=self)
        
        self.config.create('module_lists', default={}, inject_in=self)

        self.config.create('notify', default={'address':'','a':False,'b':False,'e':False}, inject_in=self)

        self.config.create('enforce_n_nodes', default=True, inject_in=self)
        
        #create other variables        
        self.n_nodes_req          =  1
        self.n_cores_per_node_req =  1
        self.n_cores_req          =  1
        self.gb_per_core_req      = -1 # value to be retrieved from the nodeset
        
        self.wall_time_seconds = 3600

        if not self.get_cluster() in clusters.cluster_names:
            self.config['cluster'].reset() # this will also set the node set
        self.on_change_cluster()
        
        if Launcher.is_testing:
            with log.LogItem('Config values'):
                for k,v in self.config.values.iteritems():
                    print('    '+k+' : '+str(v.value))
              
        self.is_resources_modified = 0 #start counting changes
        
    def __del__(self): #destructor
        self.config.save()
        
    def on_change_cluster(self,new_cluster=None):
        if not new_cluster is None:
            assert new_cluster in clusters.cluster_names, "Unknown cluster: '{}' ".format(new_cluster)
            self.set_cluster(new_cluster)
            cluster = new_cluster
        else:
            cluster = self.get_cluster()

        self.node_set_names = clusters.decorated_node_set_names(cluster)
        if not self.get_selected_node_set_name() in self.node_set_names:
            self.config['selected_node_set_name'].reset()
        self.on_change_selected_node_set_name()
                   
        self.modules = self.get_modules()
        
        self.is_resources_modified = self.increment(self.is_resources_modified)
        
        with log.LogItem('[M] Accessing cluster:'):
            print('    cluster    = '+cluster)
            print('    login node = '+self.get_login_node())
            print('    node set   = '+self.get_selected_node_set_name())
            print('    modules    = '+str(self.modules))

    def on_change_selected_node_set_name(self,new_selected_node_set_name=None):        
        if not new_selected_node_set_name is None:
            assert new_selected_node_set_name in self.node_set_names, \
                "Node set '{}' does not belong to selected cluster '{}'.".format(new_selected_node_set_name,self.get_cluster())
            self.set_selected_node_set_name(new_selected_node_set_name)
            selected = new_selected_node_set_name
        else:
            selected = self.get_selected_node_set_name()
            
        #remove extras required by previous node set
        if getattr(self, 'selected_node_set',None) and hasattr(self,'script'):
            self.selected_node_set.script_extras(self.script,remove=True)
        
        #set current node set
        #print(self.node_set_names)
        self.selected_node_set = clusters.node_sets[self.get_cluster()][self.node_set_names.index(selected)]

        #add extras required by new node set
        if self.selected_node_set and hasattr(self,'script'):
            self.selected_node_set.script_extras(self.script)
        
        self.n_cores_per_node_req = self.selected_node_set.n_cores_per_node
        self.n_cores_per_node_max = self.selected_node_set.n_cores_per_node        
        self.request_nodes_and_cores_per_node()

    def n_nodes_max(self):
        return self.selected_node_set.n_nodes

    def n_cores_per_node_max(self):
        return self.selected_node_set.n_cores_per_node
    
    def n_cores_max(self):
        return self.selected_node_set.n_cores_per_node * self.selected_node_set.n_nodes
    
    def gb_per_core_max(self):
        return self.selected_node_set.gb_per_node
    
    def increment(self,value):
        if value<0:
            #do not count changes during initialization
            return value
        else:
            return value+1
        
    def change_n_nodes_req(self,value):
        self.n_nodes_req = value
        self.request_nodes_and_cores_per_node()

    def change_n_cores_per_node_req(self,value):
        self.n_cores_per_node_req = value
        self.request_nodes_and_cores_per_node()

    def request_nodes_and_cores_per_node(self):
        n_cores,gb_per_core = self.selected_node_set.request_nodes_cores(self.n_nodes_req,self.n_cores_per_node_req)
        self.n_cores_req  = n_cores
        self.gb_per_core_req = gb_per_core
        self.gb_total_granted = gb_per_core*n_cores
        self.is_resources_modified = self.increment(self.is_resources_modified)
 
    def change_n_cores_req(self,value):
        self.n_cores_req = value
        self.request_cores_and_memory_per_core()
        
    def change_gb_per_core_req(self,value):
        self.gb_per_core_req = value
        self.request_cores_and_memory_per_core()

    def request_cores_and_memory_per_core(self):
        n_nodes, n_cores, n_cores_per_node, gb_per_core, gb = self.selected_node_set.request_cores_memory(self.n_cores_req,self.gb_per_core_req)
        self.n_nodes_req          = n_nodes
        self.n_cores_req          = n_cores
        self.n_cores_per_node_req = n_cores_per_node
        self.gb_per_core_req      = gb_per_core
        self.gb_total_granted     = gb

    def set_status_text(self,text):
        pass #todo

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
            log.log_exception(e)
            return False
        else:
            self.status_text = "Job script saved {} to '{}'.".format('(overwritten)' if file_exists else '', pbs_sh)
            self.is_script_modified = False
            return True
        return False

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
#         module_list=[]
#         for lst in lsts:
#             module_list.extend( sorted( lst, key=lambda s: s.lower() ) )
        #store for reuse when there is no connection
        cluster = self.get_cluster().lower()
        module_list = []
        for k,v in lsts.iteritems():
            if cluster in k.lower():
                self.add_to_module_lists( (self.get_cluster(),module_list) )
        return module_list
    
    def update_script_from_resources(self):       
        with log.LogItem('update_script_from_resources()'):
            if not self.is_resources_modified:
                print('    Script is already up to date.')
                return False
            #make sure all values are transferred to self.script.values
            if not hasattr(self, 'script'):
                self.script = pbs.Script()
                self.selected_node_set.script_extras(self.script)
            if not 'n_nodes' in self.script.values: 
                self.script.add_pbs_option('-l','nodes={}:ppn={}'.format(self.n_nodes_req,self.n_cores_per_node_req))
            else:
                self.script.values['n_nodes'         ] = self.n_nodes_req
                self.script.values['n_cores_per_node'] = self.n_cores_per_node_req
            
            if not 'walltime_seconds' in self.script.values:
                self.script.add_pbs_option('-l', 'walltime='+pbs.walltime_seconds_to_str(self.walltime_seconds))
            else:
                self.script.values['walltime_seconds'] = self.walltime_seconds
             
            notify= self.get_notify()
            notify_address = is_valid_mail_address(notify['address'])
            if notify_address:
                notify['address'] = notify_address 
                abe = ''
                for c in 'abe':
                    if notify[c]:
                        abe+=c
                if abe: 
                    if not 'notify_address' in self.script.values:
                        self.script.add_pbs_option('-M',notify['address'])
                        self.script.add_pbs_option('-m',abe)
                    else:
                        self.script.values['notify_address'] = notify_address
                        self.script.values['notify_abe'    ] = abe
                        
            self.script.values['enforce_n_nodes'] = self.get_enforce_n_nodes()
            if self.get_enforce_n_nodes():
                self.script.add_pbs_option('-W','x=nmatchpolicy:exactnode')
    
            if self.wJobName.GetValue():
                if not 'job_name' in self.script.values:
                    self.script.add_pbs_option('-N',self.wJobName.GetValue())
                else:
                    self.script.values['job_name'] = self.wJobName.GetValue()
                    
            lines = self.script.compose()
            if Launcher.verbose:
                print(Indent(l,'### begin script ###'))
                for l in lines:
                    print(Indent(l,6))
                print(Indent(l,'### end script ###'))
            print("    Script updated.")
            self.is_script_modified = True
            self.is_resources_modified = 0
            return True

################################################################################
class AskPermissionToOverwrite(Exception):
    pass
################################################################################
class MissingJobName(Exception):
    pass
################################################################################
def is_valid_mail_address(address):
    s = address.lower()
    pattern=re.compile(r'\b[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,4}\b')
    match = pattern.match(s)
    return s if match else False

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
        
    def testCtor(self):
        launcher = Launcher()
        self.assertEqual(launcher.get_cluster(), 'Hopper')
        self.assertEqual(len(launcher.node_set_names), 2)
        self.assertTrue(launcher.selected_node_set.name.startswith('Hopper-thin-nodes'), 2)
        self.assertEqual(launcher.get_login_node(), 'login.hpc.uantwerpen.be')
        self.assertEqual(launcher.n_nodes_req, 1)
        self.assertEqual(launcher.n_cores_req, 20)
        self.assertEqual(launcher.n_cores_per_node_req, 20)
        self.assertEqual(launcher.gb_per_core_req, launcher.gb_per_core_max()/20)

if __name__=='__main__':

    Launcher.is_testing = True
    unittest.main()
    