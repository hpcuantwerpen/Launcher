from __future__ import print_function
import datetime

class Indent(object):
    def __init__(self,n=None,tab=4):
        self._indentation=''
        self.tab=tab
        self._stack=[]
        if n:
            self.indent(n)
        
    def indent(self,n=None):
        tab = n if (n) else self.tab
        self._indentation += tab*' '
        self._stack.append(tab)
        
    def __iadd__(self,n):
        self.indent(n)
        
    def dedent(self,nlevels=1):
        len_stack = len(self._stack)
        if nlevels<0 or nlevels>len_stack:
            n = len_stack
        else:
            n = nlevels
        for i in range(1,n+1):
            self._indentation = self._indentation[:-self._stack[-i]]
        del self._stack[-n:]
        
    def __isub__(self,n):
        self.dedent(n)
    
    def reset(self):
        self.dedent(-1)
        
    def __call__(self,text,indent=None,dedent=True):
        if isinstance(text,(list,dict)):
            if indent:
                self.indent(indent)              
            s = ''
            if isinstance(text,list):
                for t in text:
                    s += self(t)
            elif isinstance(text,dict):
                for k,v in text.iteritems():
                    s += self._indentation+"{} : {}\n".format(k,v)
                if s:
                    s = s[:-1]
            if indent :
                if dedent is True:
                    self.dedent()
            return s
                
        if indent:
            self.indent(indent)

        assert isinstance(text,(str,unicode))
        lines = text.splitlines(True)
        indented_text = ''
        for line in lines:
            indented_text += self._indentation+line
        
        if indent :
            if dedent is True:
                self.dedent()
        
        return indented_text
    
    def format(self,text,indent=None,dedent=True):
        self(text,indent=None,dedent=True)
                
_timestamps = []
def print_item_header(title=""):
    global _start
    now = datetime.datetime.now()
    _timestamps.append(now)
    print()
    print(now)
    print("<<<",title)

def print_item_footer(footer=""):
    if footer:
        print(">>>",footer,(datetime.datetime.now()-_timestamps.pop()).total_seconds(),'s')
    else:
        print(">>>",(datetime.datetime.now()-_timestamps.pop()).total_seconds(),'s')