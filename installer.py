import urllib2
#    http://stackoverflow.com/questions/24346872/python-equivalent-of-a-given-wget-command
import os,shutil,sys
import zipfile
import pickle
import datetime
import argparse
import importlib
import traceback

#constants:
MINIMAL_WX_VERSION       = (2,8,10)
MINIMAL_PARAMIKO_VERSION = (1,15,2)
ZIP_FILE = 'Launcher.zip'
UNZIPPED_FOLDER_PREFIX = "hpcuantwerpen-Launcher-"
GIT_COMMIT_ID_PREFIX   = "git_commit_id_"
LAUNCHER_PREVIOUS      = "Launcher.bak"

#global variables:
user_home     = None
launcher_home = None
issues = 0
istep  = 0
unzipped_folder = None
git_commit_id = None
dependencies_ok = False

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
    global issues
    issues += 1
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
    global issues
    issues += 1
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
    launcher_src_previous = os.path.join(launcher_home,LAUNCHER_PREVIOUS)
    if os.path.exists(launcher_src_previous):
        launcher_src_folder = os.path.join(launcher_home,'Launcher')
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
    user_home = os.environ[env_home]
    assert (not user_home is None),"Unable to detect home folder."
    if verbose:
        print '  + home folder found :', user_home
    # existence of Launcher directory
    global launcher_home
    launcher_home = os.path.join(user_home,'Launcher')
    if not os.path.exists(launcher_home):
        try:
            os.makedirs(launcher_home)
        except:
            print '  - Unable to create folder "'+launcher_home+'".'
            raise
        finally:
            if verbose:
                print '  + Folder "'+launcher_home+'" created.'
    else:
        if verbose:
            print '  + Folder "'+launcher_home+'" exists.'

    # If the current directory is <$home>/Launcher/Launcher move up to parent directory
    if os.getcwd() == os.path.join(launcher_home,'Launcher'):
        os.chdir(launcher_home)
        if verbose:
            print '  + changing current working directory to parent directory:'
            print '     ', os.getcwd()
    
    if args.force:
        # remove files left by previous run
        remove_files   = []
        remove_folders = []
        move_backup    = []
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
        with LogAction('  + removing folders :',verbose=verbose):
            for f in remove_folders:
                if verbose:
                    print f,'..',
                shutil.rmtree(f)
        with LogAction('  + removing files :',verbose=verbose):
            for f in remove_files:
                if verbose:
                    print f,'..',
                os.remove(f)
         
    return True
        
def verifyDependencies(args):
    print "Verifying dependencies ..."
    # compliance of non standard python libraries 
    verbose = not args.quiet
    ok=True
    ok = ok and check_module('wx'      ,MINIMAL_WX_VERSION      ,verbose,module_name_verbose='wxPython')
    ok = ok and check_module('paramiko',MINIMAL_PARAMIKO_VERSION,verbose)
    if ok:
        if verbose:
            print '  + This python distribution is compliant with the Launcher requirements.'
    else:
        ext = '.bat' if sys.platform=='win32' else '.sh'
        print '  - WARNING:'\
              '    This python distribution is NOT compliant with the Launcher requirements.'\
              '    You can continue the installation by providing the --ignore-deps command '\
              '    line argument. The startup script "launcher'+ext+'" will use the current '\
              '    non comliant Python distribution.'
    return ok
    
def download(args,branch='master'):
    url="http://github.com/hpcuantwerpen/Launcher/zipball/{}/".format(branch)
    print 'Downloading from',url,'...'
    verbose = not args.quiet
    attempts = 0
    max_attempts = 3
    while attempts < max_attempts:
        attempts += 1        
        try:
            if verbose:
                print '    attempt {} ...'.format(attempts), 
            response = urllib2.urlopen(url,timeout=5)
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
                print 'succeeded :',ZIP_FILE
    if not attempts<max_attempts:
        print '  - Unable to download "{}", after {} attempts.'.format(url,attempts)
        return False
    else:
        assert os.path.exists(ZIP_FILE), 'Zip file "{}" not found.'.format(ZIP_FILE)
        if verbose:
            print '  + Downloaded "Launcher.zip"'
        return True

def unzip(args):
    print 'Unzipping "{}" ...'.format(ZIP_FILE)
    try:
        zf = zipfile.ZipFile(ZIP_FILE,mode='r')
        zf.extractall()
    finally:
        global unzipped_folder
        for folder,sub_folders,files in os.walk('.'):
            for sub_folder in sub_folders:
                if sub_folder.startswith(UNZIPPED_FOLDER_PREFIX):
                    unzipped_folder = sub_folder
                    git_commit_id   = sub_folder[len(UNZIPPED_FOLDER_PREFIX):]
            break
        if unzipped_folder is None:
            print '  - Unzipped folder not found. Expecting "{}<git_commit_id>"'.format(UNZIPPED_FOLDER_PREFIX)
            return False
        else:
            if not args.quiet:
                print '  + Unzipped folder : "{}"'.format(unzipped_folder)
            open('git_commit_id_'+git_commit_id,'w+') #empty file
            return True
   
def install(args):
    print 'Installing Launcher ...'
    verbose = not args.quiet
    launcher_src_folder = os.path.join(launcher_home,'Launcher')
    if os.path.exists(launcher_src_folder):
        with LogAction('  + making backup of previous version of Launcher -> "{}" ...'.format(LAUNCHER_PREVIOUS),verbose=verbose):
            launcher_src_backup = os.path.join(launcher_home,LAUNCHER_PREVIOUS)
            shutil.move(launcher_src_folder,launcher_src_backup)
       
    with LogAction('  + moving "{}" to "{}" ...'.format(unzipped_folder,launcher_src_folder),verbose=verbose):
        shutil.move(unzipped_folder,launcher_src_folder)

    ok = True
    with LogAction('  + creating startup script'):
        startup = 'launcher'+ ('.bat' if sys.platform=='win32' else '.sh')
        fpath = os.path.join(launcher_home,startup)
        f = open(fpath,'w+')
        if not sys.platform=='win32':
            f.write("#!/bin/bash")
        installer_log = pickle.load(open('installer.log'))
        if not installer_log['verifyDependencies']:
            print '  - WARNING:'\
                  '    The startup script "'+startup+'" uses a non compliant Python distribution.'\
                  '    Launcher may not work properly.'
            f.write('echo WARNING: this script uses a NON compliant Python distribution')
            ok = False
        f.write(sys.executable+' '+os.path.join(launcher_src_folder,'launch.py'))
        f.close()
        if not sys.platform=='win32':
            from stat import S_IEXEC
            os.chmod(fpath,S_IEXEC)
    return ok

    
def install_step(step,args,force_always=False):
    start = datetime.datetime.now()
    global istep
    istep += 1
    print '\nSTEP', istep    
    step_name = step.__name__
    try:
        installer_log = pickle.load(open('installer.log'))
    except:
        installer_log={}
    if force_always:
        step_already_ok = False
    else:
        step_already_ok = installer_log.get(step_name,False)

    if args.force or not step_already_ok:
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
        
        installer_log[step_name] = success
        pickle.dump(installer_log,open('installer.log','w+'))
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
    args = parser.parse_args()
    
    success = install_step( preconditions, args, force_always=True )
    if success:
        success = install_step( verifyDependencies, args )
    if success:
        success = install_step( download          , args )
    if success:
        success = install_step( unzip             , args )
    if success:
        success = install_step( install           , args )