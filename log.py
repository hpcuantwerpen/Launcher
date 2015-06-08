from __future__ import print_function

import datetime,traceback,sys,random,os,shutil,time

from indent import Indent
import constants

################################################################################
class LogItem(object):
################################################################################
    def __init__(self,header='',footer=''):
        self.footer = footer
        self.start = datetime.datetime.now()
        print()
        print(self.start)
        print("<<<",header)
    def __enter__(self):
        pass
    def __exit__(self, type, value, traceback):
        footer =">>> "+self.footer+' '+str((datetime.datetime.now()-self.start).total_seconds())+' s'
        print(footer)

################################################################################
def log_exception(exception,msg_before=None,msg_after=None):
    line = 80*'-'
    if msg_before:
        print(line)
        print(msg_before)
    s = "--- Exception raised: "+str(type(exception))+" "
    s += line[len(s):]
    print
    print(s)
    traceback.print_exc(file=sys.stdout)
    print(line)
    if msg_after:
        print(msg_after)
        print(line)
        
################################################################################
def start_log(wx_version='unknown',paramiko_version='unknown'):
    log_file = default_log_file()
    if os.path.exists(log_file):
        old_log = log_file+str(random.random())[1:]
        shutil.copy(log_file,old_log)

    with LogItem('Launcher session'):
        print('    starting the log.')
    
    with LogItem("Version info of used components"):
        print("    Python version:")
        print(Indent(sys.version,8))
        print("    Python executable:")
        print(Indent(sys.executable,8))
        print("    wxPython version:")
        print(Indent(wx_version,8))
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

################################################################################
def stop_log():
    with LogItem('Launcher session'):
        print('    stopping the log.')

################################################################################
def default_log_file():
    return os.path.join(constants._LAUNCHER_HOME_,'Launcher.log')

################################################################################
### test code
################################################################################

if __name__=='__main__':
    start_log()
    with LogItem('begin','end'):
        print('in between')
    stop_log()
    print('\nThis is a test for visual inspection!')