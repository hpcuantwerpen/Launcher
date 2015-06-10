from collections import OrderedDict
import re,datetime
from indent import Indent

################################################################################
class ShellCommand(object):
    Derived=[]
    @classmethod
    def parse(cls,text):
        #print cls,text
        if cls.test(text):
            for D in cls.Derived:
                result = D.parse(text)
                if result:
                    return result
            return cls(text)
        else:
            return None
    
    @classmethod
    def test(cls,text):
        return True
    
    def __init__(self,text):
        self.text = text

    def __str__(self):
        n=20
        s = ('('+self.__class__.__name__+') ').ljust(n)+self.text
        return s 
    
    def __eq__(self,right_operand):
        """ Return True if self and right_operand represent the same line in the script."""
        if isinstance(right_operand,ShellCommand):
            return self.text==right_operand.text
        return False
            
    def ordinate(self):
        return 10 # always at end of script
    
################################################################################
class UserComment(ShellCommand):
    Derived=[]
    @classmethod
    def test(cls,text):
        ok = text.startswith('#')
        #print cls,ok
        return ok
    pass

    def __eq__(self,right_operand):
        """ Return True if self and right_operand represent the same line in the script."""
        if isinstance(right_operand,UserComment):
            return self.text==right_operand.text
        return False
        
    def ordinate(self):
        return -1 # can appear anywhere in script

################################################################################
class Shebang(UserComment):
    default = '#!/bin/bash'
    Derived=[]
    @classmethod
    def test(cls,text):
        ok = text.startswith('#!')
        #print cls,ok
        return ok
    def __init__(self,text=None):
        self.text = Shebang.default if not text else text
    
    def __eq__(self,right_operand):
        """ Return True if self and right_operand represent the same line in the script."""
        if isinstance(right_operand,Shebang):
            return self.text==right_operand.text
        return False
        
    def ordinate(self):
        return 0 # always on first line of script 

################################################################################
class LauncherComment(UserComment):
    Derived=[]
    @classmethod
    def test(cls,text):
        ok = text.startswith('#La#')
        #print cls,ok
        return ok
    
    re_param = re.compile(r'#La#\s+(\w*)\s*=\s*(.*)')

    def __init__(self,text):
        self.params = {}
        m = LauncherComment.re_param.match(text)
        if m:
            groups=m.groups()
            self.params[groups[0]] = groups[1]
            self.frmt = '    '+groups[0]+' = {}'
        else:
            self.frmt = text[5:]
        text = self.compose()
        super(LauncherComment,self).__init__(text)
    
    def compose(self):
        text = '#La# '+self.frmt.format(*self.params.itervalues())
        return text

    def __eq__(self,right_operand):
        """ Return True if self and right_operand represent the same line in the script."""
        if isinstance(right_operand,LauncherComment):
            n=16
            eq = self.text[0:n]==right_operand.text[0:n]
            eq = eq and have_common_params(self.params,right_operand.params)
            # the idea is that a parameter key must be unique, hence
            # if one parameter key occurs in both operands, they represent 
            # the same line in the script 
            return eq
        return False
        
    def ordinate(self):
        return 1 #after Shebang and before PBSDirectives

################################################################################
class PBSDirective(UserComment):
    Derived=[]
    @classmethod
    def test(cls,text):
        ok = text.startswith('#PBS')
        #print cls,ok
        return ok
    
    res = [ re.compile(r'#PBS\s+-(\w)\s+(.*)')
          , re.compile(r'#PBS\s+-(\w)\s+(.*){(.*)=(.*)}(.*)')
          , re.compile(r'#PBS\s+-(\w)\s+(.*){(.*)=(.*)}(.*){(.*)=(.*)}(.*)')
          ]
    
    def __init__(self,text):
        #remove trailing whitespace
        text = text.strip()
        #remove trailing comment (but store the comment)
        if not text:
            self.comment=''
        else:
            comment = text[1:].find('#')
            if comment>0:
                self.comment = text[comment:]
                text = text[0:comment]
            else:
                self.comment=''
        #Make sure that the line starts with uppercase '#PBS'
        if not text.startswith('#PBS'):
            if text.upper().startswith('#PBS'):
                text = '#PBS '+text[5:]
            else:
                text = '#PBS '+text
        #count the number of brace pairs
        n_braces = (text.count('{'),text.count('}'))
        if not n_braces[0]==n_braces[1]:
            raise UnmatchedBraces(text)
        n_braces = n_braces[0]
        self.params = {}
        if n_braces>=len(PBSDirective.res):
            raise TooManyBraces(text)
        
        m = PBSDirective.res[n_braces].match(text)
        if not m:
            raise UnableToParsePBSDirective(text)
        groups = m.groups()
        #print groups
        self.flag = groups[0]
        self.frmt = groups[1]        
        for i in range(n_braces):
            i_k = 2+3*i
            i_v = i_k+1
            i_s = i_k+2
            key = groups[i_k]
            val = int_float_str(groups[i_v])
            self.params[key]=val
            self.frmt+='{}'+groups[i_s]
        text = self.compose()
        #print 'text=',text
        super(PBSDirective,self).__init__(text)
        
    def compose(self):
        text = '#PBS -'+self.flag+' '+self.frmt.format(*self.params.itervalues())+self.comment
        return text
    
    def __eq__(self,right_operand):
        """ Return True if self and right_operand represent the same line in the script."""
        if isinstance(right_operand,PBSDirective):
            eq = self.flag==right_operand.flag
            eq = eq and ( self.text==right_operand.text or have_common_params(self.params,right_operand.params) )
                         
            # the idea is that a parameter key must be unique, hence
            # if one parameter key occurs in both operands, they represent 
            # the same line in the script 
            return eq
        return False

    def ordinate(self):
        return 2 # after LauncherComments and before ShellCommands

################################################################################
def int_float_str(value_str):
    assert isinstance(value_str,str),"expecting str, got {}=({})".format(str(type(value_str)),value_str)
    try:
        v=int(value_str)
        return v
    except:
        try:
            v=float(value_str)
            return v
        except:
            return value_str
    
################################################################################
def have_common_params(params1,params2):
    if len(params1)>len(params2):
        p1 = params1.iterkeys()
        p2 = params2.iterkeys()
    else:  # swap
        p1 = params2.iterkeys()
        p2 = params1.iterkeys()
    for p in p1:
        if p in p2:
            return True
    return False

################################################################################
class UnableToParsePBSDirective(Exception):
    pass
################################################################################
class TooManyBraces(Exception):
    pass
################################################################################
class UnmatchedBraces(Exception):
    pass

################################################################################
ShellCommand.Derived = [UserComment]
UserComment .Derived = [Shebang,LauncherComment,PBSDirective]

################################################################################
class Script(object):
    def __init__(self,lines=[]):
        self.script_lines=[]
        self.parse(lines)
            
    def parse(self,lines):
        for line in lines:
            self.add(line)
            
    def add(self,line):
        if issubclass(type(line),ShellCommand):
            new_script_line = line
        else:
            if isinstance(line,unicode):
                line = str(unicode)
            assert isinstance(line,str), "expecting a str or a subclass of ShellCommand, got '{}'".format(str(line))
            new_script_line = ShellCommand.parse(line)
            
        for i,old_script_line in enumerate(self.script_lines):
            if new_script_line==old_script_line:
                # replace the old script line with the new as it may have other parameter values
                self.script_lines[i] = new_script_line
                break
        else:
            #this line is completly new and must be inserted
            self._insert(new_script_line)
                
    def _insert(self,line):
        ordinate = line.ordinate()
        if ordinate==-1:
            self.script_lines.append(line)
        else:
            for i,l in enumerate(self.script_lines):
                l_ordinate = l.ordinate()
                if ordinate < l_ordinate:
                    self.script_lines.insert(i,line)
                    break
            else:
                self.script_lines.append(line)
    
    def remove(self,line):
        i = self.index(line)
        if i>-1:
            self.script_lines.pop(i)
            return True
        return False

    def index(self,line):
        new_script_line = ShellCommand.parse(line)
        for i,old_script_line in enumerate(self.script_lines):
            if new_script_line==old_script_line:
                return i
        return -1
        
    def __getitem__(self,key):
        for l in self.script_lines:
            l_params = getattr(l,'params',{})
            if l_params:
                val = l_params.get(key,None)
                if val:
                    return val
        raise KeyError("Key '"+key+"' not found in script.")

    def __setitem__(self,key,val):
        for l in self.script_lines:
            l_params = getattr(l,'params',{})
            if l_params:
                if key in l_params:
                    l_params[key]=val
                    l.text = l.compose()
                    return
        raise KeyError("Key '"+key+"' not found in script.")
    
    def has_line(self,line):
        i = self.index(line)
        return i>-1
    
    def get_lines(self):
        """return list of lines (str)"""
        lst = [ sl.text for sl in self.script_lines ]
        return lst
    
################################################################################
### only test code below this line
################################################################################
import unittest

class Test0(unittest.TestCase):
    def setUp(self):
        unittest.TestCase.setUp(self)
        self.lines0 = [ Shebang.default
                      ,'#blablabla'
                      ,'echo hello'
                      ,'#La# blablabla'
                      ]
    def test(self):
        script = Script(self.lines0)
        self.assertTrue(isinstance(script.script_lines[0], Shebang))
        self.assertTrue(isinstance(script.script_lines[1], UserComment))
        self.assertTrue(isinstance(script.script_lines[2], LauncherComment))
        self.assertTrue(isinstance(script.script_lines[3], ShellCommand))
        print Indent(script.script_lines,2)
        
class TestPBSDirective(unittest.TestCase):
    def setUp(self):
        unittest.TestCase.setUp(self)
        self.lines = ['#!/bin/bash'
                     ,'#La# Launcher generated this job script on '+str(datetime.datetime.now())
                     ,'#La#    cluster = Hopper'
                     ,'#La#    nodeset = Hopper-thin-nodes'
                     ,'#PBS -W x=nmatchpolicy:exactnode'
                     ,'#PBS -N {jobname=a_simple_job}'
                     ,'#PBS -M {notify_address=engelbert.tijskens@uantwerpen.be} #say hello'
                     ,'#PBS -m {notify_when=ae}'
                     ,'#PBS -l walltime={walltime=1:00:00}'
                     ,'#PBS -l nodes={nodes=1}:ppn={ppn=20}'
                     ,''
                     ,'cd $PBS_O_WORKDIR'
                     ,'if [ -d "output" ]; then'
                     ,'    rm -rf output'
                     ,'fi'
                     ,'mkdir output'
                     ,'cd output'
                     ,'echo "this is the output file" > o.txt'
                     ,'cd ..'
                     ]
    def test(self):
        script = Script(self.lines)
        self.assertTrue(isinstance(script.script_lines[0], Shebang))
        print Indent(script.script_lines,2)
        print '__cluster=',script['cluster']
        print '__adress =',script['notify_address']
        print '__ppn    =',script['ppn']
        script['notify_when'] = 'abe'
        print Indent(script.script_lines,2)
        
class TestPBSDirective2(unittest.TestCase):
    def setUp(self):
        unittest.TestCase.setUp(self)
        self.lines = [''
                     ,'cd $PBS_O_WORKDIR'
                     ,'if [ -d "output" ]; then'
                     ,'    rm -rf output'
                     ,'fi'
                     ,'mkdir output'
                     ,'cd output'
                     ,'echo "this is the output file" > o.txt'
                     ,'cd ..'
                     ,'#PBS -W x=nmatchpolicy:exactnode'
                     ,'#PBS -N {jobname=a_simple_job}'
                     ,'#PBS -M {notify_address=engelbert.tijskens@uantwerpen.be} #say hello'
                     ,'#PBS -m {notify_when=ae}'
                     ,'#PBS -l walltime={walltime=1:00:00}'
                     ,'#PBS -l nodes={nodes=1}:ppn={ppn=20}'
                     ,'#La# Launcher generated this job script on '+str(datetime.datetime.now())
                     ,'#La#    cluster = Hopper'
                     ,'#La#    nodeset = Hopper-thin-nodes'
                     ,'#!/bin/bash'
                     ]
    def test(self):
        script = Script()
        for line in self.lines:
            script.add(line)
        print Indent(script.script_lines,2)
        script.add('#PBS -l walltime={walltime=5:55:55}')
        print Indent(script.script_lines,2)
        script.remove('#PBS -W x=nmatchpolicy:exactnode')
        print Indent(script.script_lines,2)
        
if __name__=='__main__':
    unittest.main()