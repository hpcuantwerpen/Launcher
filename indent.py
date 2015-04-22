from __future__ import print_function

class Indent(object):
    def __init__(self,source,indent=0,tabs=4,default_tab=4):
        """
        source : object for which to produce an indented string representation
        tabs   : replace every occurence of '\t' by the appropriate number of spaces.
                 If tabs is an integer the text is tabbed at indent, indent+tabs,
                 indent+2*tabs, ...
                 If tabs is a list of integers, then the text is tabbes at indent,
                 indent+tabs[0], indent+tabs[1], ... If the list is exhausted, the
                 previous scheme takes over as if tabs==default_tab
        indent : integer, indent spaces are inserted at the beginning of every line        
        """
        self.source=source
        self.indent=indent
        self.default_tab = default_tab
        self.tabs=tabs
        self.extend_tabs(10)
                    
    def extend_tabs(self,n):
        if isinstance(self.tabs, list):
            self.tabs.sort()
            for i in range(10):
                nxt=self.tabs[-1] - self.tabs[-1]%self.default_tab + self.default_tab
                self.tabs.append(nxt)
        else:
            self.default_tab=self.tabs
            self.tabs=[self.default_tab]
            self.extend_tabs(n-1)

    def __str__(self):
        s=''
        s0 = self.indent*' '
        if isinstance(self.source,dict):
            s_source = self.dict2str(self.source)
        if isinstance(self.source,list):
            s_source = self.list2str(self.source)
        else:
            s_source = str(self.source)
        lines = s_source.splitlines(True)
        for l in lines:
            s += s0
            i=0
            while True:
                try:
                    jtab=l.index('\t')
                    ll=l[:jtab]
                    while jtab>self.tabs[i]:
                        i+=1
                    ll+=(self.tabs[i]-jtab)*' '
                    ll+=l[jtab+1:]
                    l=ll
                except ValueError:
                    break
            s+=l
        return s
    
    def dict2str(self,d):
        s=str(d)
        s=s.replace('{','{ ')
        s=s.replace(':','\t:\n\t')
        s=s.replace(',', '\n,')
        s=s.replace('}', '\n}')
        return s
        
    def list2str(self,d):
        s=str(d)
        s=s.replace('[','[ ')
        s=s.replace(',', '\n,')
        s=s.replace(']', '\n]')
        return s
        
#----------------------#
# below only test code #
#----------------------#

if __name__=="__main__":
    def print_ruler(s):
        print('----|----1----|----2----|----3----|----4----|----5----|----6----|----7----|----8')
        print(s)
    text="hello\nworld"
    s=str(Indent(text))
    lines=s.splitlines(True)
    print_ruler(s)
    assert lines[0]=='hello\n'
    assert lines[1]=='world'
 
    s=str(Indent(text,indent=4))
    print_ruler(s)
    lines=s.splitlines(True)
    assert lines[0]=='    hello\n'
    assert lines[1]=='    world'

    text="hel\tlo\nmy\tdear\tworld"
    s=str(Indent(text,indent=6,tabs=4))
    print_ruler(s)
    lines=s.splitlines(True)
    assert lines[0]=='      hel lo\n'
    assert lines[1]=='      my  dearworld'

    s=str(Indent(text,indent=6,tabs=5))
    print_ruler(s)
    lines=s.splitlines(True)
    assert lines[0]=='      hel  lo\n'
    assert lines[1]=='      my   dear world'

    source = {"one":1
             ,"two":2
             }
    s=str(Indent(source,indent=6,tabs=5))
    print_ruler(s)
    lines=s.splitlines(True)
    
    source = ["one"
             ,"two"
             ]
    s=str(Indent(source,indent=6,tabs=5))
    print_ruler(s)
    lines=s.splitlines(True)