import urllib2
#    http://stackoverflow.com/questions/24346872/python-equivalent-of-a-given-wget-command
import os,shutil,sys,re
import zipfile
import pickle
import datetime
import argparse
import importlib
import traceback
from argparse import Namespace

#constants:
MINIMAL_WX_VERSION       = (2,8,10)
MINIMAL_PARAMIKO_VERSION = (1,15,2)
ZIP_FILE = 'Launcher.zip'
UNZIPPED_FOLDER_PREFIX = "hpcuantwerpen-Launcher-"
GIT_COMMIT_ID_PREFIX   = "Global.git_commit_id_"
LAUNCHER_PREVIOUS      = "Launcher.bak"
GLOBAL_PICKLED         = "installer_status.pickled"

class Global(object): 
    """ using class as namespace
        key-value pairs are defined in loadGlobal()
    """

def loadGlobal(reset=False):
    """ persistence for Global namespace"""
    if reset:
        #set default key value pairs
        Global.user_home       = None
        Global.launcher_home   = None
        Global.istep           = 0
        Global.unzipped_folder = None
        Global.git_commit_id   = None
        Global.dependencies_ok = False
        Global.zip_folder      = None
        Global.install_step_ok = {}
        return
    try:
        d={}
        d=pickle.load(open(GLOBAL_PICKLED))
        for k,v in d.iteritems():
            setattr(Global, k, v)
    except:
        loadGlobal(reset=True)
        
def dumpGlobal():
    """ persistence for Global namespace"""
    d={}
    for k,v in Global.__dict__.iteritems():
        if not (k.startswith('__') and k.endswith('__')):
            d[k]=v 
    pickle.dump(d, open(GLOBAL_PICKLED,'w+'))

def removeGlobal():
    try:
        os.remove(GLOBAL_PICKLED)
    except Exception as e:
        #print type(e),e
        pass

class LogAction(object):
    def __init__(self,before='',after='done',verbose=True):
        self.verbose=verbose
        if verbose:
            print before,
        self.after = after
    def __enter__(self):
        pass
    def __exit__(self, type, value, traceback):
        if self.verbose:
            print self.after
  
def tuple2version_string(t):
    s=str(t[0])
    for i in t[1:]:
        s+='.'+str(i)
    return s

def module_not_available(module_name,minimal_version):
    s = '  - WARNING:\n'\
        '    The {module_name} module is not available to the current Python executable.\n'\
        '    It is recommended to install {module_name} version >= {minimal_version}\n'\
        '    in the current Python executable:\n'\
        '      "{python_exe}",\n'\
        '    or to use another Python distribution with the correct {module_name} version.'
    s = s.format( module_name    = module_name
                , minimal_version= tuple2version_string(minimal_version)
                , python_exe     = sys.executable
                )
    return s

def module_version_insufficient(module_name,version,minimal_version):
    s = '  - WARNING:\n'\
        '    This version of module {module_name} is not supported: {version} \n'\
        '    It is recommended to upgrade to {module_name} version >= {minimal_version}\n'\
        '    in the current Python executable:\n'\
        '      "{python_exe}",\n'\
        '    or to use another Python distribution with the correct {module_name} version.'
    s = s.format( module_name    = module_name
                , version        = tuple2version_string(version)
                , minimal_version= tuple2version_string(minimal_version)
                , python_exe     = sys.executable
                )
    return s

def check_module(module_name, minimal_version, verbose=False, module_name_verbose=None):
    name = module_name if (module_name_verbose is None) else module_name_verbose
    ok = False
    try:
        module = importlib.import_module(module_name)
    except ImportError:
        print module_not_available(name,minimal_version)
    finally:
        version = None
        try:
            version = module.version() #works for wx
        except:
            try:
                version = module.__version__
            except:
                msg="Don't know how to get the version number of module "+name
                if module_name_verbose:
                    msg+=' ('+module_name+')'
                msg+='.'
                raise Exception()
        version = version.split(' ')[0]
        version = version.split('.')
        version = [int(i) for i in version]
        version = tuple(version)
        if version < minimal_version:
            print module_version_insufficient(module_name, version, minimal_version)
        else:
            ok = True
            if verbose:
                print '  + module '+name+' found, version '+tuple2version_string(version)+' complies with requirement.'
    return ok
        
def reinstall_previous_version(verbose):
    launcher_src_previous = os.path.join(Global.launcher_home,LAUNCHER_PREVIOUS)
    if os.path.exists(launcher_src_previous):
        launcher_src_folder = os.path.join(Global.launcher_home,'Launcher')
        if os.path.exists(launcher_src_folder):
            with LogAction('  + removing recently installed Launcher version ...',verbose=verbose):
                shutil.rmtree(launcher_src_folder)
        with     LogAction('  + reinstalling previous Launcher version ...',verbose=verbose):
            shutil.move(launcher_src_previous,launcher_src_folder)
    
def preconditions(args):
    print 'Verifying preconditions ...'
    verbose=not args.quiet
    # existence of home directory
    env_home = "HOMEPATH" if sys.platform=="win32" else "HOME"
    Global.user_home = os.environ[env_home]
    assert (not Global.user_home is None),"Unable to detect home folder."
    if verbose:
        print '  + home folder found :', Global.user_home
    # existence of Launcher directory
    Global.launcher_home = os.path.join(Global.user_home,'Launcher')
    if not os.path.exists(Global.launcher_home):
        try:
            os.makedirs(Global.launcher_home)
        except:
            print '  - Unable to create folder "'+Global.launcher_home+'".'
            raise
        finally:
            if verbose:
                print '  + Folder "'+Global.launcher_home+'" created.'
    else:
        if verbose:
            print '  + Folder "'+Global.launcher_home+'" exists.'

    # If the current directory is <$home>/Launcher/Launcher move up to parent directory
    cwd = os.getcwd()
    if cwd.endswith('/Launcher') and not cwd==Global.launcher_home:
        os.chdir('..')
        if verbose:
            print '  + changing current working directory to parent directory:'
            print '     ', os.getcwd()
    if args.force:
        removed = removeGlobal() #forget the state of the previous run
        if verbose and removed:
            print '  + removed old "{}" (--force):'.format(GLOBAL_PICKLED)
    
    if args.force:
        # remove files left by previous run
        remove_files   = []
        remove_folders = []
        z = os.path.join('./',ZIP_FILE)
        if os.path.exists(z):
            remove_files.append(z)
        for folder,sub_folders,files in os.walk('.'):
            for sub_folder in sub_folders:
                if sub_folder.startswith(UNZIPPED_FOLDER_PREFIX):
                    remove_folders.append(sub_folder)
            for file in files:
                if file.startswith(GIT_COMMIT_ID_PREFIX):
                    remove_files.append(file)
            break
        if remove_folders:
            with LogAction('  + removing old folders :',verbose=verbose):
                for f in remove_folders:
                    if verbose:
                        print f,'..',
                    shutil.rmtree(f)
        if remove_files:
            with LogAction('  + removing old files :',verbose=verbose):
                for f in remove_files:
                    if verbose:
                        print f,'..',
                    os.remove(f)

    reinstall_previous_version(verbose)
    
    return True
        
def verifyDependencies(args):
    print "Verifying dependencies ..."
    # compliance of non standard python libraries 
    verbose = not args.quiet
    ok = True
    ok = ok and check_module('wx'      ,MINIMAL_WX_VERSION      ,verbose,module_name_verbose='wxPython')
    ok = ok and check_module('paramiko',MINIMAL_PARAMIKO_VERSION,verbose)
    if ok:
        if verbose:
            print '  + This python distribution is compliant with the Launcher requirements.'
    else:
        if args.ignore_deps:
            ok = True # force return value to continu if False
            print '  - WARNING:'\
                  '    This python distribution is NOT compliant with the Launcher requirements.'\
                  '    Continuing since --ignore-deps found.'
        else:
            ext = '.bat' if sys.platform=='win32' else '.sh'
            print '  - WARNING:'\
                  '    This python distribution is NOT compliant with the Launcher requirements.'\
                  '    You can continue the installation by providing the --ignore-deps command '\
                  '    line argument. The startup script "launcher'+ext+'" will use the current '\
                  '    non comliant Python distribution.'

    Global.dependencies_ok = ok
    return ok
    
def download(args,branch='master'):
    url="http://github.com/hpcuantwerpen/Launcher/zipball/{}/".format(branch)
    print 'Downloading from',url,'...'
    verbose = not args.quiet
    if args.no_download:
        print '  - Warning: --no-download found. Assuming "{}" is present in current directory.'.format(ZIP_FILE)
    else:
        attempts = 0
        max_attempts = 3
        while attempts < max_attempts:
            attempts += 1        
            try:
                if verbose:
                    print '    attempt {} ...'.format(attempts), 
                response = urllib2.urlopen(url,timeout=5)
#                 print response.info()['Content-Disposition']
#                 print type(response.info()['Content-Disposition'])
                pattern = re.compile(r"attachment; filename=(.*)")
                m = re.match(pattern, response.info()['Content-Disposition'])
                Global.zip_folder = m.group(1)
                content = response.read()
                f = open(ZIP_FILE, 'w+' )
                f.write( content )
                f.close()
                break
            except urllib2.URLError as e:
                if verbose:
                    print 'failed.'
                print type(e)
                print e
            finally:
                if verbose:
                    print 'succeeded :"{}" -> "{}"'.format(Global.zip_folder,ZIP_FILE)
        if not attempts<max_attempts:
            print '  - Unable to download "{}", after {} attempts.'.format(url,attempts)
            return False
    assert os.path.exists(ZIP_FILE), 'Zip file "{}" not found.'.format(ZIP_FILE)
    if verbose:
        if args.no_download:
            msg='  + Found "{}" in current directory.'
        else:
            msg='  + Downloaded "{}".'
        print msg.format(ZIP_FILE)
    return True

def unzip(args):
    print 'Unzipping "{}" ...'.format(ZIP_FILE)
    try:
        zf = zipfile.ZipFile(ZIP_FILE,mode='r')
        zf.extractall()
    finally:
        if Global.zip_folder is None:
            for folder,sub_folders,files in os.walk('.'):
                for sub_folder in sub_folders:
                    if sub_folder.startswith(UNZIPPED_FOLDER_PREFIX):
                        Global.unzipped_folder = sub_folder
                break
        else:
            Global.unzipped_folder=os.path.splitext(Global.zip_folder)[0]
            assert os.path.exists(Global.unzipped_folder)
            
    if Global.unzipped_folder is None:
        print '  - Unzipped folder not found. Expecting "{}<Global.git_commit_id>"'.format(UNZIPPED_FOLDER_PREFIX)
        return False
    else:
        if not args.quiet:
            print '  + Unzipped folder : "{}"'.format(Global.unzipped_folder)
        Global.git_commit_id = Global.unzipped_folder[len(UNZIPPED_FOLDER_PREFIX):]
        open('git_commit_id_'+Global.git_commit_id,'w+') #empty file to store the git commit id 
        return True
   
def install(args):
    print 'Installing Launcher ...'
    verbose = not args.quiet
    launcher_src_folder = os.path.join(Global.launcher_home,'Launcher')
    if os.path.exists(launcher_src_folder):
        with LogAction('  + making backup of previous version of Launcher -> "{}" ...'.format(LAUNCHER_PREVIOUS),verbose=verbose):
            launcher_src_backup = os.path.join(Global.launcher_home,LAUNCHER_PREVIOUS)
            shutil.move(launcher_src_folder,launcher_src_backup)
#     print os.getcwd()
    try:
        with LogAction('  + moving "{}" to "{}" ...'.format(Global.unzipped_folder,launcher_src_folder),verbose=verbose):
            shutil.move(Global.unzipped_folder,launcher_src_folder)
        
        with LogAction('  + Copying "git_commit_id_{}" ...'.format(Global.git_commit_id),verbose=verbose):
            shutil.copy('git_commit_id_'+Global.git_commit_id,launcher_src_folder)
    except:
        traceback.print_exc(file=sys.stdout)
        print "Returning to previous version."
        reinstall_previous_version(verbose=verbose)
        return False
    
    return True

def create_startup_script(args):
    print 'Creating install script'
    verbose = not args.quiet
    launcher_src_folder = os.path.join(Global.launcher_home,'Launcher')
    ok = True
    with LogAction('  + creating startup script ...',verbose=verbose):
        startup = 'launcher'+ ('.bat' if sys.platform=='win32' else '.sh')
        fpath = os.path.join(Global.launcher_home,startup)
        f = open(fpath,'w+')
        if not sys.platform=='win32':
            f.write("#!/bin/bash\n")
        
        if not Global.dependencies_ok:
            print '  - WARNING:'\
                  '    The startup script "'+startup+'" uses a non compliant Python distribution.'\
                  '    Launcher may not work properly.'
            f.write('echo WARNING: this script uses a NON compliant Python distribution\n')
            ok = False
        f.write(sys.executable+' '+os.path.join(launcher_src_folder,'launch.py')+'\n')
        f.close()
        if not sys.platform=='win32':
            import stat
            os.chmod(fpath,stat.S_IRWXU|stat.S_IRWXG)
    return ok

    
def install_step(step,args):
    start = datetime.datetime.now()
    loadGlobal()
    
    Global.istep += 1
    print '\nSTEP', Global.istep    
    
    step_name = step.__name__
    step_already_ok = Global.install_step_ok.get(step_name,False)

    if not step_already_ok:
        try:
            success = step(args)
        except Exception as e:
            success = False
            traceback.print_exc(file=sys.stdout)
            print e
        msg = step_name.capitalize() \
            + (' succeeded in ' if success else ' failed after ')\
            + str((datetime.datetime.now()-start).total_seconds())+'s.'
        print msg
        
        Global.install_step_ok[step_name] = success
        dumpGlobal()
        return success
    else:
        print '  + step not repeated since already successful and --force==False.'
        return True

    
if __name__=="__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-f","--force"
                       , help="Force all steps to execute, even when reported to be successful"\
                              " by a previous run of the installer."
                       , action="store_true"
                       )
    parser.add_argument("-q","--quiet"
                       , help="Do not produce detailed overview of the installation progress."
                       , action="store_true"
                       )
    parser.add_argument("--ignore-deps"
                       , help="Proceed with installation if the Python distribution uses is not compliant."
                       , action="store_true"
                       )
    parser.add_argument("--no-download"
                       , help="Do not download - for testing purposes only."
                       , action="store_true"
                       )
    args = parser.parse_args()
    if not args.quiet:
        print "Commandline arguments:",args
    
    success     = install_step( preconditions, args)
    if success:
        success = install_step( verifyDependencies, args )
    if success:
        success = install_step( download, args )
    if success:
        success = install_step( unzip, args )
    if success:
        success = install_step( install, args )
    if success:
        args.force = True
        success = install_step( create_startup_script, args )    
    
    if success:
        removeGlobal() #will interfere with next update
        print '\nInstallation of Launcher finished successfully. Enjoy!'
    else:
        print '\nInstallation of Launcher failed.'
    