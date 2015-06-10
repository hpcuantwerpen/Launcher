from __future__ import print_function

import argparse,shutil,os,random,sys,time,logging,importlib

import constants
from log import log_exception,LogItem,LogSession
import Launcher
from indent import Indent
"""
Command line interface 
"""

################################################################################
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-p","--printlog"
                       , help="Print the log instead of redirecting it to the log file."
                       , action="store_true"
                       )
    parser.add_argument("-q","--pyqt5"
                       , help="Use PyQt5 graphical users interface."
                       , action="store_true"
                       )
    parser.add_argument("-w","--wxpython"
                       , help="Use wxPython graphical users interface."
                       , action="store_true"
                       )
    args = parser.parse_args()
    
    
    log_file = os.path.join(constants._LAUNCHER_HOME_,'Launcher.log')
    if os.path.exists(log_file):
        old_log = log_file+str(random.random())[1:]
        shutil.copy(log_file,old_log)
    if args.printlog:
        log_file = None        
    session = LogSession(name='Launcher',filename=log_file)
    
    if args.pyqt5 and args.wxpython:
        with LogItem('command line issue'):
            print('    Both --pyqt5 and --wxpython have been specified on the command line.')
            print('    PyQt5 has priority.')
            args.wxpython = False
    if not args.pyqt5 and not args.wxpython:
        with LogItem('command lind issue'):
            print('    No gui has been specified on the command line.')
            print('    PyQt5 has priority.')
            args.pyqt5 = True
    
    if args.pyqt5:
        gui_module = importlib.import_module('LauncherPyQt5')
        gui_version = gui_module.__version__
    elif args.wxpython:
        gui_module  = None
        gui_version = None
        
    
    with LogItem("Version info of used components",log_with=logging.info):
        paramiko_version = Launcher.sshtools.paramiko.__version__
        print("    Python version:")
        print(Indent(sys.version,8))
        print("    Python executable:")
        print(Indent(sys.executable,8))
        print("    gui version:")
        print(Indent(gui_version,8))
        print("    paramiko version:")
        print(Indent(paramiko_version,8))
        print("    Launcher version:")
        s="v{}.{}.{}".format(*constants._LAUNCHER_VERSION_)
        print(Indent(s,8))
        print("    Platform:")
        print(Indent(sys.platform,8))
    
    with LogItem("cleaning log files"):
        if old_log:
            print("    Renaming previous log file 'Launcher.log' to '{}'".format(old_log))
        remove_logs_older_than = 14 
        removed=0
        if remove_logs_older_than:
            now = time.time()
            threshold = now - 60*60*24*remove_logs_older_than # Number of seconds in two days
            for folder,sub_folders,files in os.walk(constants._LAUNCHER_HOME_):
                if folder==constants._LAUNCHER_HOME_:
                    for fname in files:
                        if fname.startswith("Launcher.log."):
                            fpath = os.path.join(folder,fname)
                            ftime = os.path.getctime(fpath)
                            if ftime<threshold:
                                os.remove(fpath)
                                removed += 1
                    break
            print("    Removed {} log files older than {} days.".format(removed,remove_logs_older_than))
    
    del session