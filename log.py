from __future__ import print_function
import datetime

class LogItem(object):
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
        
### test code ###
if __name__=='__main__':
    with LogItem('begin','end'):
        print('in between')