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
        
        default = clusters.decorated_nodeset_names(self.get_cluster())[0]
        self.config.create('selected_nodeset_name', default=default, inject_in=self)
        
        self.config.create('module_lists', default={}, inject_in=self)

        self.config.create('notify', default={'address':'','a':False,'b':False,'e':False}, inject_in=self)

        self.config.create('enforce_n_nodes', default=True, inject_in=self)
        
        self.set_default_values_non_config()

        if not self.get_cluster() in clusters.cluster_names:
            self.config['cluster'].reset() # this will also set the node set
        self.change_cluster()
        
        if Launcher.is_testing:
            with log.LogItem('Config values'):
                for k,v in self.config.values.iteritems():
                    print('    '+k+' : '+str(v.value))
              
        self.is_resources_modified = 0 #start counting changes
    
    def set_default_values_non_config(self):
        #create other variables than the config variables        
        self.n_nodes_req          =  1
        self.n_cores_per_node_req =  1
        self.n_cores_req          =  1
        self.gb_per_core_req      = -1 # value to be retrieved from the nodeset
        
        self.walltime_seconds = 3600
        self.jobname = ''
        
    def __del__(self): #destructor
        self.config.save()
        
    def change_cluster(self,new_cluster=None):
        if not new_cluster is None:
            if not new_cluster in clusters.cluster_names:
                raise UnknownCluster(new_cluster)
            self.set_cluster(new_cluster)
            cluster = new_cluster
        else:
            cluster = self.get_cluster()

        self.nodeset_names = clusters.decorated_nodeset_names(cluster)
        if not self.get_selected_nodeset_name() in self.nodeset_names:
            self.config['selected_nodeset_name'].reset()
        self.on_change_selected_nodeset_name()
                   
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
        if not new_selected_nodeset_name is None:
            if not new_selected_nodeset_name in self.nodeset_names:
                raise UnknownNodeset("Nodeset '{}' not defined for cluster '{}.".format(new_selected_nodeset_name,self.get_cluster()))
            self.set_selected_nodeset_name(new_selected_nodeset_name)
            selected = new_selected_nodeset_name
        else:
            selected = self.get_selected_nodeset_name()
            
        #remove extras required by previous node set
        if getattr(self, 'selected_nodeset',None) and hasattr(self,'script'):
            self.selected_nodeset.script_extras(self.script,remove=True)
        
        #set current node set
        #print(self.nodeset_names)
        self.selected_nodeset = clusters.nodesets[self.get_cluster()][self.nodeset_names.index(selected)]

        #add extras required by new node set
        if self.selected_nodeset and hasattr(self,'script'):
            self.selected_nodeset.script_extras(self.script)
        
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
        self.n_nodes_req = value
        self.request_nodes_and_cores_per_node()

    def change_n_cores_per_node_req(self,value):
        self.n_cores_per_node_req = value
        self.request_nodes_and_cores_per_node()

    def request_nodes_and_cores_per_node(self):
        n_cores,gb_per_core = self.selected_nodeset.request_nodes_cores(self.n_nodes_req,self.n_cores_per_node_req)
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
        n_nodes, n_cores, n_cores_per_node, gb_per_core, gb = self.selected_nodeset.request_cores_memory(self.n_cores_req,self.gb_per_core_req)
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
            if not force and not self.is_resources_modified:
                print('    Script is already up to date.')
                return False
            #make sure all values are transferred to self.script.values
            if not hasattr(self, 'script'):
                self.script = pbs.Script()
                self.selected_nodeset.script_extras(self.script)
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
            
            if self.jobname:
                if not 'job_name' in self.script.values:
                    self.script.add_pbs_option('-N',self.wJobName.GetValue())
                else:
                    self.script.values['job_name'] = self.wJobName.GetValue()
            
            self.script.set_comments(cluster=self.get_cluster(),nodes=self.selected_nodeset.name)
            
            lines = self.script.compose(add_comments=True)
            if Launcher.verbose:
                print(Indent('### begin script ###',4))
                print(Indent(lines,6))
                print(Indent('### end   script ###',4))
            print("    Script updated.")
            self.is_script_modified = True
            self.is_resources_modified = 0
            return lines
        
    def update_resources_from_script(self,lines):
        with log.LogItem('Updating resources from script:'):
            if isinstance(lines,(str,unicode)):
                lines = lines.split('\n')
    
            if not hasattr(self, 'script'):
                self.script = pbs.Script()
            self.script.parse(lines)        
    
            try:
                cluster = self.script.get_cluster_from_comments()
                self.change_cluster(cluster)
            except UnknownCluster as e:
                log.log_exception(e,msg_after="Using '{}' instead.".format(self.get_cluster()))
                
            try:
                nodeset = self.script.get_nodeset_from_comments() 
                self.change_selected_nodeset_name(nodeset)
                self.selected_nodeset.script_extras(self.script)
            except UnknownNodeset as e:
                log.log_exception(e,msg_after="Using '{}' instead.".format(self.get_selected_nodeset_name()))
            
            if not hasattr(self.script,'values'):
                return #there is nothing to update
            
            new_request = False
            if self.n_nodes_req != self.script.values['n_nodes']:
                self.n_nodes_req = self.script.values['n_nodes']
                new_request = True
            if self.n_cores_per_node_req != self.script.values['n_cores_per_node']:
                self.n_cores_per_node_req = self.script.values['n_cores_per_node']
                new_request = True
            if new_request:
                self.request_nodes_and_cores_per_node()
                
            self.update_value(self.wEnforceNNodes, 'enforce_n_nodes')
            
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
        
        
if __name__=='__main__':

    Launcher.is_testing = True
    unittest.main()
    