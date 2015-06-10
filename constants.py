import os,sys

################################################################################
def launcher_home():
    env_home = "HOMEPATH" if sys.platform=="win32" else "HOME"
    user_home = os.environ[env_home]
    if not user_home or not os.path.exists(user_home):
        msg = "Error: environmaent variable '${}' not found.".format(env_home)
        raise EnvironmentError(msg)
    home = os.path.join(user_home,'Launcher')
    if not os.path.exists(home):
        os.mkdir(home)
    return home

################################################################################
def git_version():
    """
    retrieve the SHA-1 checksum which identifies the git commit of the current version
    """
    f = open(os.path.join(_LAUNCHER_HOME_,'Launcher/git_commit_id.txt'))
    git_commit_id = f.readlines()[0].strip()
    return git_commit_id

################################################################################
_LAUNCHER_HOME_ = launcher_home()
_LAUNCHER_VERSION_ = [2,0]
_LAUNCHER_VERSION_.append(git_version()) 
################################################################################

################################################################################
### only test code below this line
################################################################################
import unittest

class TestConstants(unittest.TestCase):
    def test1(self):
        self.assertTrue( isinstance(_LAUNCHER_HOME_,str) )
        self.assertTrue( isinstance(_LAUNCHER_VERSION_,list) )
        self.assertTrue( len(_LAUNCHER_VERSION_)==3 )
        
if __name__=='__main__':
    unittest.main()