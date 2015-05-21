import urllib2
#    http://stackoverflow.com/questions/24346872/python-equivalent-of-a-given-wget-command
import os,shutil,sys,re
import zipfile
import pickle
import datetime
import argparse
import importlib
import traceback
import random

#constants:
MINIMAL_WX_VERSION       = (2,8,10)
MINIMAL_PARAMIKO_VERSION = (1,15,2)
ZIP_FILE = 'Launcher.zip'
UNZIPPED_FOLDER_PREFIX = "hpcuantwerpen-Launcher-"
GIT_COMMIT_ID_PREFIX   = "git_commit_id_"
LAUNCHER_PREVIOUS      = "Launcher.bak"
GLOBAL_PICKLED         = "installer_status.pickled"

class status(object): 
    """ using class object as namespace
        key-value pairs are defined in load_status()
    """
def init_status():
    """
    (re)set the attributes to their default values.
    """
    status.user_home       = None
    status.launcher_home   = None
    status.istep           = 0
    status.unzipped_folder = None
    status.git_commit_id   = None
    status.dependencies_ok = False
    status.zip_folder      = None
    status.install_step_ok = {}

def load_status(reset=False):
    """
    Persistence for status namespace:
      - unpickle the status class object, unless
      - reset == True, in which case the attributes are set to their default values.
    """
    if not reset:
        try:
            d={}
            d=pickle.load(open(GLOBAL_PICKLED))
            for k,v in d.iteritems():
                setattr(status, k, v)
        except: #cannot unpickle, hence reset key-value pair to default
            init_status()
        return
    else:
        init_status()
        
def dump_status():
    """ persistence for status namespace - pickle the status class object."""
    d={}
    for k,v in status.__dict__.iteritems():
        if not (k.startswith('__') and k.endswith('__')):
            d[k]=v 
    pickle.dump(d, open(GLOBAL_PICKLED,"w+b"))

def remove_status():
    """
    Remove the persistent (pickled) status class object.
    On the next load_status() call the status attributes will be set to their default values
    """
    try:
        os.remove(GLOBAL_PICKLED)
    except Exception as e:
        #print type(e),e
        pass

def read_git_commit_id():
    try:
        launcher_src_folder = os.path.join(status.launcher_home,'Launcher')
        lines = open(os.path.join(launcher_src_folder,'git_commit_id.txt')).readlines()
        git_commit_id = lines[0]
    except:
        git_commit_id = None
    return git_commit_id
        
def write_git_commit_id():
    launcher_src_folder = os.path.join(status.launcher_home,'Launcher')
    git_commit_id = status.git_commit_id
    fname = os.path.join(launcher_src_folder,'git_commit_id.txt')
    print '  + writing "{}" << {}'.format(fname,git_commit_id)
    try:
        f = open(fname,'w')
        f.write(str(git_commit_id))
        f.close()
    except:
        print 'oops'
        
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
    launcher_src_previous = os.path.join(status.launcher_home,LAUNCHER_PREVIOUS)
    if os.path.exists(launcher_src_previous):
        launcher_src_folder = os.path.join(status.launcher_home,'Launcher')
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
    status.user_home = os.environ[env_home]
    assert (not status.user_home is None),"Unable to detect home folder."
    if verbose:
        print '  + home folder found :', status.user_home
    # existence of Launcher directory
    status.launcher_home = os.path.join(status.user_home,'Launcher')
    if not os.path.exists(status.launcher_home):
        try:
            os.makedirs(status.launcher_home)
        except:
            print '  - Unable to create folder "'+status.launcher_home+'".'
            raise
        finally:
            if verbose:
                print '  + Folder "'+status.launcher_home+'" created.'
    else:
        if verbose:
            print '  + Folder "'+status.launcher_home+'" exists.'

    # If the current directory is <$home>/Launcher/Launcher move up to parent directory
    cwd = os.getcwd()
    if cwd.endswith('/Launcher') and not cwd==status.launcher_home:
        os.chdir('..')
        if verbose:
            print '  + changing current working directory to parent directory:'
            print '     ', os.getcwd()
    if args.force:
        removed = remove_status() #forget the state of the previous run
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
#             for file in files:
#                 if file.startswith(GIT_COMMIT_ID_PREFIX):
#                     remove_files.append(file)
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

#     if not args.check_updates_only:
#         reinstall_previous_version(verbose)
    
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

    status.dependencies_ok = ok
    return ok

def download(args,branch='master'):
    url="http://github.com/hpcuantwerpen/Launcher/zipball/{}/".format(branch)
    
    if args.check_for_updates_only:
        print 'Checking for updates ...'
        args.no_download = False
    else:
        print 'Downloading from',url,'...'
    
    verbose = not args.quiet
    
    if args.no_download:
        print '  - Warning: --no-download found. Assuming "{}" is present in current directory.'.format(ZIP_FILE)
        assert os.path.exists(ZIP_FILE), 'Zip file "{}" not found.'.format(ZIP_FILE)
        print '  + Found "{}" in current directory.'.format(ZIP_FILE)
    else:
        attempts = 0
        max_attempts = 3
        while attempts < max_attempts:
            attempts += 1        
            try:
                if verbose:
                    print '    attempt {} ...'.format(attempts), 
                response = urllib2.urlopen(url,timeout=5)
                pattern = re.compile(r"attachment; filename=(.*)")
                m = re.match(pattern, response.info()['Content-Disposition'])
                status.zip_folder = m.group(1)

                status.unzipped_folder = os.path.splitext(status.zip_folder)[0]
                status.git_commit_id   = status.unzipped_folder[len(UNZIPPED_FOLDER_PREFIX):]
                previous_git_commit_id = read_git_commit_id()
                status.update_available = True if not previous_git_commit_id \
                                                else ( previous_git_commit_id!=status.git_commit_id )
                if args.check_for_updates_only:
                    pass     
                else:
                    if status.update_available or args.force:
                        content = response.read()
                        f = open(ZIP_FILE, 'w+b' )
                        f.write( content )
                        f.close()
            except urllib2.URLError as e:
                if verbose:
                    print 'failed.'
                print type(e)
                print e
            else:
                if verbose:
                    if args.check_for_updates_only:
                        print 'succeeded'
                        if status.update_available:
                            if not previous_git_commit_id:
                                print "  + No git commit id found in installation, assuming outdated version."
                            print '  + Update available:',status.git_commit_id
                        else:
                            print '  + You have the most recent version already:', status.git_commit_id
                    else:
                        if not (status.update_available or args.force):
                            print 'succeeded'
                            print '  + You have already the most recent version.'
                        elif not status.update_available and args.force:
                            print 'succeeded :"{}" -> "{}"'.format(status.zip_folder,ZIP_FILE)
                            print '  + You have already the most recent version,'\
                                  '    but it is reinstalled because of --force commandline argument.'
                        else:
                            print 'succeeded'
                            print '  + updating to most recent version.'
                if not args.check_for_updates_only and (status.update_available or args.force):
                    assert os.path.exists(ZIP_FILE), 'Zip file "{}" not found.'.format(ZIP_FILE)
                    print '  + Downloaded "{}".'.format(ZIP_FILE)

                return True
            
        if args.check_for_updates_only:
            print '  - Unable to connect to github, after {} attempts.'.format(attempts)
        else:
            print '  - Unable to download "{}", after {} attempts.'.format(url,attempts)
        return False

def unzip(args):
    print 'Unzipping "{}" ...'.format(ZIP_FILE)
    try:
        zf = zipfile.ZipFile(ZIP_FILE,mode='r')
        zf.extractall()
    finally:
        if status.zip_folder is None:
            for folder,sub_folders,files in os.walk('.'):
                for sub_folder in sub_folders:
                    if sub_folder.startswith(UNZIPPED_FOLDER_PREFIX):
                        status.unzipped_folder = sub_folder
                break
        else:
            status.unzipped_folder=os.path.splitext(status.zip_folder)[0]
            assert os.path.exists(status.unzipped_folder)
            
    if status.unzipped_folder is None:
        print '  - Unzipped folder not found. Expecting "{}<status.git_commit_id>"'.format(UNZIPPED_FOLDER_PREFIX)
        return False
    else:
        if not args.quiet:
            print '  + Unzipped folder : "{}"'.format(status.unzipped_folder)
        status.git_commit_id = status.unzipped_folder[len(UNZIPPED_FOLDER_PREFIX):]
        return True
   
def install(args):
    print 'Installing Launcher ...'
    verbose = not args.quiet
    launcher_src_folder = os.path.join(status.launcher_home,'Launcher')
    if os.path.exists(launcher_src_folder):
        backup = LAUNCHER_PREVIOUS+str(random.random())[1:]
        with LogAction('  + making backup of previous version of Launcher -> "{}" ...'.format(backup),verbose=verbose):
            launcher_src_backup = os.path.join(status.launcher_home,backup)
            shutil.move(launcher_src_folder,launcher_src_backup)
#     print os.getcwd()
    try:
        with LogAction('  + moving "{}" to "{}" ...'.format(status.unzipped_folder,launcher_src_folder),verbose=verbose):
            shutil.move(status.unzipped_folder,launcher_src_folder)
        write_git_commit_id() 
        
#         with LogAction('  + Copying "git_commit_id_{}" ...'.format(status.git_commit_id),verbose=verbose):
#             shutil.copy('git_commit_id_'+status.git_commit_id,launcher_src_folder)
    except:
        traceback.print_exc(file=sys.stdout)
        print "Returning to previous version."
        reinstall_previous_version(verbose=verbose)
        return False
    
    return True

def create_startup_script(args):
    print 'Creating install script'
    verbose = not args.quiet
    launcher_src_folder = os.path.join(status.launcher_home,'Launcher')
    ok = True
    with LogAction('  + creating startup script ...',verbose=verbose):
        startup = 'launcher'+ ('.bat' if sys.platform=='win32' else '.sh')
        fpath = os.path.join(status.launcher_home,startup)
        f = open(fpath,'w+')
        if not sys.platform=='win32':
            f.write("#!/bin/bash\n")
        
        if not status.dependencies_ok:
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

    
def install_step(step,args,name=None):
    start = datetime.datetime.now()
    load_status()
    
    status.istep += 1
    print '\nSTEP', status.istep    
    
    step_name = step.__name__ if not name else name
    step_already_ok = status.install_step_ok.get(step_name,False)

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
        
        status.install_step_ok[step_name] = success
        dump_status()
        return success
    else:
        print step_name
        print '  + step not repeated since already successful and --force==False.'
        dump_status()
        return True
    
def install_launcher(argv=[]):
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
    parser.add_argument("--check-for-updates-only"
                       , help="Check only if there is a newer version, but don't download, and don't install. "
                       , action="store_true"
                       )
    args = parser.parse_args(argv)
    if not args.quiet:
        print "Launcher installer called with Commandline arguments:"
        s = str(args)[10:-1]
        s = s.replace(', ', '\n')
        lines = s.split('\n')
        for l in lines:
            n_blanks = 25 - l.find('=')
            print n_blanks*' ',l        
    
    success     = install_step( preconditions, args)
    if success:
        success = install_step( verifyDependencies, args )
    if success:
        if args.check_for_updates_only:
            success = install_step( download, args, name='check for updates')
            if success:
                print '\nChecking for Launcher updates finished successfully.'
                status.istep=0
                dump_status()
            else:
                print '\nChecking for Launcher updates failed.'
        else:
            success     = install_step( download, args )
            if success:
                if (status.update_available or args.force):
                    success = install_step( unzip, args )
                else:
                    print '\nInstallation of Launcher finished early because you already have the most recent version.'
                    return success
            if success:
                success = install_step( install, args )
            if success:
                args.force = True
                success = install_step( create_startup_script, args )    
            if success:
                remove_status() #avoid  interference during installation of next update
                print '\nInstallation of Launcher finished successfully. Enjoy!'

            else:
                status.istep=0
                dump_status()
                print '\nInstallation of Launcher failed.'
                
    return success
    
if __name__=="__main__":
    install_launcher(sys.argv[1:])
    
