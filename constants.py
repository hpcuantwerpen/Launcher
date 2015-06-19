import os,sys
import log

################################################################################
def user_home():
    """return the user's home folder"""
    env_home = "HOME"
    env_fmt = "${}"
    if sys.platform=="win32":
        env_home = "HOMEPATH"
        env_fmt = "%{}%"
    home = os.environ[env_home]
    if not home:
        msg = "Error: environmaent variable {} not found.".format(env_fmt.format(env_home))
        raise EnvironmentError(msg)
    if not os.path.exists(home):
        msg = "Error: environmaent variable '${}' refers to inexisting location.".format(env_fmt.format(env_home))
        raise EnvironmentError(msg)
    return home

################################################################################
def launcher_home():
    """"Return the launcher home folder"""
    home = os.path.join(user_home(),'Launcher')
    if not os.path.exists(home):
        with log.LogItem("Creating Launcher home folder"):
            print("home")
            try:
                os.mkdir(home)
            except Exception as e:
                log.log_exception(e)
    return home

################################################################################
def git_version():
    """
    retrieve the SHA-1 checksum which identifies the git commit of the current version
    """
    try:
        f = open(os.path.join(_LAUNCHER_HOME_,'Launcher/git_commit_id.txt'))
        git_commit_id = f.readlines()[0].strip()
    except:
        git_commit_id = ""
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