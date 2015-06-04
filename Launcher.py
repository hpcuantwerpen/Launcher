from __future__ import print_function

import os,re
import log
import cfg
import clusters
import transactions
import sshtools
import constants

################################################################################
class LauncherM(transactions.TransactionManager): 
################################################################################
    """
    The Launcher Model part in the MVC approach. This contains only the logic, not the GUI part.
    """
    is_testing=False
    
    def __init__(self,reset=False,**kwargs):
        self.is_resources_modified = -1
        #load the config file and copy all settings to member variables
        path = os.path.join(constants.launcher_home(),'Launcher.cfg')
        self.config = cfg.Config(path=path)
        if reset:
            self.config.reset()
        self.config.inject_all(self)

        self.config.create('username', default='', inject_in=self)
        
        default = clusters.cluster_names[0]
        self.config.create('cluster', default=default, inject_in=self)
 
        default = clusters.login_nodes[self.get_cluster()][0]
        self.config.create('login_node', default=default, inject_in=self)
        
        default = clusters.decorated_node_set_names(self.get_cluster())[0]
        self.config.create('selected_node_set_name', default=default, inject_in=self)

        if not self.get_cluster() in clusters.cluster_names:
            self.config['cluster'].reset()
        self.on_change_cluster()
        
        if LauncherM.is_testing:
            print('config values')
            for k,v in self.config.values.iteritems():
                print(k+' : '+str(v.value))
              
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
            print('    modules    = ')
        

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
        print(self.node_set_names)
        self.selected_node_set = clusters.node_sets[self.get_cluster()][self.node_set_names.index(selected)]

        #add extras required by new node set
        if self.selected_node_set and hasattr(self,'script'):
            self.selected_node_set.script_extras(self.script)

        self.is_resources_modified = self.increment(self.is_resources_modified)

    def increment(self,value):
        if value<0:
            #do not count changes during initialization
            return value
        else:
            return value+1
        
    def set_n_nodes_requested(self,value):
        self.n_nodes_requested = value
        self.request_nodes_and_cores_per_node()

    def set_n_cores_per_node_requested(self,value):
        self.n_cores_per_node_requested = value
        self.request_nodes_and_cores_per_node()

    def request_nodes_and_cores_per_node(self):
        n_cores,gb_per_core = self.selected_node_set.request_nodes_cores(self.wNNodesRequested.GetValue(),self.wNCoresPerNodeRequested.GetValue())
        self.n_cores_requested  = n_cores
        self.gb_per_core_requested = gb_per_core
        self.gb_total_granted = gb_per_core*n_cores
        self.is_resources_modified = self.increment(self.is_resources_modified)
 
    def set_n_cores_requested(self,value):
        self.n_cores_requested = value
        self.request_cores_and_memory_per_core()
        
    def set_gb_per_core_requested(self,value):
        self.gb_per_core_requested = value
        self.request_cores_and_memory_per_core()

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
        ssh = sshtools.Client(self.get_username(),self.get_login_node())
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

class TestLauncherM(unittest.TestCase):
    def testCtor(self):
        launcher_m = LauncherM()
        self.assertEqual(launcher_m.get_cluster(), 'Hopper')
        self.assertEqual(len(launcher_m.node_set_names), 2)
        self.assertTrue(launcher_m.selected_node_set.name.startswith('Hopper-thin-nodes'), 2)
        self.assertEqual(launcher_m.get_login_node(), 'login.hpc.uantwerpen.be')
if __name__=='__main__':

    LauncherM.is_testing = True
    unittest.main()
    