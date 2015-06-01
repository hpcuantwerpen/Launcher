from __future__ import print_function

import os,sys,pickle

import log
import constants
from indent import Indent


################################################################################
class Config(object):
################################################################################
    """
    Launcher configuration
    """
    #entries not starting with '_' becom member variables of the LauncherM 
    default_attributes = { 'cluster' : 'some_cluster'
                         , 'mail_to' : 'your mail address goes here'
                         , 'walltime_unit' : 'hours'
                         , 'walltime_value' : 1
                         , 'local_files' : os.path.join(constants._LAUNCHER_HOME_,'jobs')
                         , 'user_id' : 'your_user_id'
                         , 'remote_subfolder' : '.'
                         , '_module_lists':{}
                         , '_submitted_jobs':{}
                         }
    
    def __init__(self,must_load=True,must_save=True):
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
            cfg_file = os.path.join(constants._LAUNCHER_HOME_,'Launcher.config')
            try:
                self.attributes = pickle.load(open(cfg_file))
                self.cfg_file = cfg_file
            except Exception as e:
                log.log_exception(e)

        for k,v in Config.default_attributes.iteritems():
            if not k in self.attributes:
                self.attributes[k] = v

    def save(self):
        if self.must_save:
            pickle.dump(self.attributes,open(self.cfg_file,'w+'))

################################################################################
### test code
################################################################################
import unittest

class TestConfig(unittest.TestCase):
    def testDefaultConfig(self):
        config=Config(must_load=False,must_save=False)
        for k,v in Config.default_attributes.iteritems():
            self.assertEqual( v, config.attributes[k] )
    def testConfig(self):
        config=Config(must_load=True,must_save=False)
        del config
        
if __name__=='__main__':
    unittest.main()