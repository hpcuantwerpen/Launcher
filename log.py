from __future__ import print_function

import logging,datetime,traceback,sys,StringIO

from indent import Indent


################################################################################
class LogSession(object):
    def __init__(self
                , name  = None
                , filename = None
                , format   = '%(levelname)s:%(message)s'
                , level    = logging.INFO
                ):
        self.name = name
        logging.basicConfig(filename=filename,format=format,level=level)
        
        with LogItem(header='Opening session:',log_with=logging.info):
            print("    "+self.name)
            
    def __del__(self):
        with LogItem(header='Closing session:',footer='Closed.',log_with=logging.info):
            print("    "+self.name)
        
################################################################################
class RedirectStdStreams(object): #context manager
################################################################################
    def __init__(self, stdout=None, stderr=None):
        self._stdout = stdout or sys.stdout
        self._stderr = stderr or sys.stderr

    def __enter__(self):
        self.old_stdout, self.old_stderr = sys.stdout, sys.stderr
        self.old_stdout.flush(); self.old_stderr.flush()
        sys.stdout, sys.stderr = self._stdout, self._stderr

    def __exit__(self, exc_type, exc_value, traceback):
        self._stdout.flush(); self._stderr.flush()
        sys.stdout = self.old_stdout
        sys.stderr = self.old_stderr

################################################################################
class LogItem(RedirectStdStreams): #context manager
################################################################################
    def __init__(self,header='',footer='',log_with=logging.info):
        self.msg = StringIO.StringIO()
        super(LogItem,self).__init__(stdout=self.msg,stderr=self.msg)
        self.footer = footer
        self.start = datetime.datetime.now()
        self.log = log_with
        super(LogItem,self).__enter__()
        print()
        print(self.start)
        print("<<<",header)
    def __enter__(self):
        pass
    def __exit__(self, type_, value, traceback):
        if self.footer:
            self.footer+=' '
        footer =">>> "+self.footer+str((datetime.datetime.now()-self.start).total_seconds())+' s'
        print(footer)
        super(LogItem,self).__exit__(type_, value, traceback)
        self.log(self.msg.getvalue())

################################################################################
def log_exception(exception,msg_before=None,msg_after=None,log_with=logging.error):
    with LogItem(header='Exception raised',log_with=log_with):
        line = 80*'-'
        if msg_before:
            print('    '+line)
            print('    '+msg_before)
        print(("    --- Exception raised: "+str(type(exception))+" ").ljust(84,'-'))
        trace = StringIO.StringIO()
        traceback.print_exc(file=trace)
        print(Indent(trace.getvalue(),4))
        print('    '+line)
        if msg_after:
            print('    '+msg_after)
            print('    '+line)
        
################################################################################
### only test code below
################################################################################
import unittest
logfile = None
class TestRedirectStdStreams(unittest.TestCase):
    def test0(self):
        with RedirectStdStreams():
            print('hello')
            print('hello',file=sys.stderr)
    def test1(self):
        out = StringIO.StringIO()
        err = StringIO.StringIO()
        with RedirectStdStreams(stdout=out,stderr=err):
            print('hello out')
            print('hello err',file=sys.stderr)
        out = out.getvalue()
        self.assertEqual(out,'hello out\n')
        err = err.getvalue()
        self.assertEqual(err,'hello err\n')
         
class TestLogItem(unittest.TestCase):
    def setUp(self):
        unittest.TestCase.setUp(self)
#         start_log(filename=logfile)
    def test0(self):
        with LogItem(header='header',footer='footer'):
            print('    body')
    def test1(self):
        with LogItem(header='header',footer='footer',log_with=logging.info):
            print('    body')
         
class Test_log_exception(unittest.TestCase):
    def setUp(self):
        unittest.TestCase.setUp(self)
#         start_log(filename=logfile)
    def test0(self):
        try:
            assert False, "whodunit?"
        except Exception as e:
            log_exception(e, 'Test_log_exception.test0', 'Test_log_exception.test0') 

class Test0LogSession(unittest.TestCase):
    def test(self):
        session = LogSession(name='session one',level=logging.DEBUG)
        del session
        
if __name__=='__main__':
    unittest.main()
    