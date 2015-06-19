from collections import OrderedDict
import re,datetime
from indent import Indent
from log import log_exception

################################################################################
def walltime_seconds_to_str(walltime_seconds):
    hh = walltime_seconds/3600
    vv = walltime_seconds%3600 #remaining seconds after subtracting the full hours hh
    mm = vv/60
    ss = vv%60                 #remaining seconds after also subtracting the full minutes mm
    s = str(hh)
    if hh>9:
        s = s.rjust(2,'0')
    s+=':'+str(mm).rjust(2,'0')+':'+str(ss).rjust(2,'0')
    return s

################################################################################
def is_valid_mail_address(address):
    s = address.lower()
    pattern=re.compile(r'\b[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,4}\b')
    match = pattern.match(s)
    return s if match else False

################################################################################
def set_is_modified(object):
    object.text=''

################################################################################
def is_modified(object):
    return object.text==''

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
    
    def __init__(self,text,hidden=False):
        self.hidden = hidden
        self.text = text
        self.compose()

    def compose(self):
        if not self.text.endswith('\n'):
            self.text += '\n'

    def __str__(self):
        self.compose()
        n=20
        h = 'H' if self.hidden else ' '
        s = ('('+self.__class__.__name__+')').ljust(n)+h+self.text
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
        self.hidden = False
        self.text = Shebang.default if not text else text
        ShellCommand.compose(self)
    
    def __eq__(self,right_operand):
        """ Return True if self and right_operand represent the same line in the script."""
        if isinstance(right_operand,Shebang):
            return True
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
    
    lc_line_pattern = re.compile(r'#La#(\s+)(.+)')
    lc_parm_pattern = re.compile(r'(\w+)\s*=\s*(.+)')

    def __init__(self,text):
        text = text.strip()
        m = LauncherComment.lc_line_pattern.match(text)
        self.parms = {}
        if m:
            mgroups=m.groups()
            self.space = mgroups[0]
            self.value = mgroups[1]
            m = LauncherComment.lc_parm_pattern.match(self.value)
            if m:
                mgroups=m.groups()
                self.parms[mgroups[0]] = mgroups[1]
        else:
            raise LauncherCommentNotMatched(text)
        super(LauncherComment,self).__init__(text)
        ShellCommand.compose(self)
    
    def compose(self):
        if is_modified(self):
            if self.parms: #compose self.value
                self.value = ''
                for k,v in self.parms.iteritems():
                    self.value += '{}{} = {}'.format(self.space,k,v) 
            self.text = '#La# '+self.value
            ShellCommand.compose(self)

    def __eq__(self,right_operand):
        """ Return True if self and right_operand represent the same line in the script."""
        if isinstance(right_operand,LauncherComment):
            if not self.parms and not right_operand.parms:
                return False
            #all parameter keys must be the same
            for k in self.parms.iterkeys():
                if not k in right_operand.parms:
                    return False
            return True
        return False
        
    def ordinate(self):
        return 1 #after Shebang and before PBSDirectives

################################################################################
class LauncherCommentNotMatched(Exception):
    pass

################################################################################
class PBSDirective(UserComment):
################################################################################
    Derived=[]

    @classmethod
    def test(cls,text):
        ok = text.startswith('#PBS')
        #print cls,ok
        return ok
    
    pbs_line_pattern = re.compile(r'#PBS\s+(-\w)\s+([^\s]*)(\s*#.+)?')
    pbs_parm_pattern = re.compile(r':((\w+)=((\d{1,2}:\d\d:\d\d)|([\w]+)))(.*)')
    pbs_feat_pattern = re.compile(r':(\w+)(.*)')
    
    def __init__(self,text):
        #remove trailing whitespace
        text = text.strip()
        #Make sure that the line starts with uppercase '#PBS'
        if not text.startswith('#PBS'):
            if text.upper().startswith('#PBS'):
                text = '#PBS '+text[5:]
            else:
                text = '#PBS '+text
        self.parms = OrderedDict()
        self.comment = ''
        #match the basic PBS line pattern
        m = PBSDirective.pbs_line_pattern.match(text)
        if not m:
            raise PbsPatternNotMatched(text)
        mgroups = m.groups()
        #print mgroups
        self.flag  = mgroups[0]
        self.value = mgroups[1] 
        if not mgroups[2] is None:
            self.comment = mgroups[2]
        #match the value pattern
        remainder =':'+self.value
        #look for parameters
        m = PBSDirective.pbs_parm_pattern.match(remainder)
        if m:
            while m:
                mgroups = m.groups()
                #print mgroups
                self.parms[mgroups[1]] = int_float_str(mgroups[2])
                #print self.parms
                remainder = mgroups[-1]
                m = PBSDirective.pbs_parm_pattern.match(remainder)
            #look for features
            self.features = []
            m = PBSDirective.pbs_feat_pattern.match(remainder)
            while m:
                mgroups = m.groups()
                self.features.append(mgroups[0])
                #print self.parms
                remainder = mgroups[-1]
                m = PBSDirective.pbs_feat_pattern.match(remainder)
            assert remainder=='',"PBS line not fully matched, remainder='{}'".format(remainder)
        super(PBSDirective,self).__init__(text)
        ShellCommand.compose(self)
    
    def compose(self):
        if is_modified(self):
            if self.parms:
                value = ''
                for k,v in self.parms.iteritems():
                    value+=':{}={}'.format(k,v)
                for f in self.features:
                    value+=':'+f
                self.value = value[1:]
            self.text = '#PBS {} {}{}'.format(self.flag,self.value,self.comment)
            ShellCommand.compose(self)
        
    def __eq__(self,right_operand):
        """ Return True if self and right_operand represent the same line in the script."""
        if isinstance(right_operand,PBSDirective):
            if not self.flag==right_operand.flag:
                #different flags are never equal
                return False
            #cases below have the same flag
            if not self.parms and not right_operand.parms:
                #lhs and rhs have no parameters, hence equal
                return True
            #otherwise they should have at least one common parameter
            for k in self.parms.iterkeys():
                if k in right_operand.parms:
                    return True
        #in all other cases
        return False

    def ordinate(self):
        return 2 # after LauncherComments and before ShellCommands
    
################################################################################
class PbsPatternNotMatched(Exception):
    pass
################################################################################
def int_float_str(value_str):
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
################################################################################
    auto_unhide = False 
    
    def __init__(self,lines=[],filename=None):
        self.script_lines=[]
        
        #create default lines. which store dummy parameters
        self.add(Shebang())
        
        self.add("#La# generated_on = dummy",hidden=True)
        self.add("#La#      cluster = dummy",hidden=True)
        self.add("#La#      nodeset = dummy",hidden=True)
        
        self.add('#PBS -l nodes=dummy:ppn=dummy',hidden=True)
        self.add('#PBS -l walltime=dummy'       ,hidden=True)
         
        self.add('#PBS -M dummy',hidden=True)
        self.add('#PBS -m dummy',hidden=True)
                    
        self.add('#PBS -W x=nmatchpolicy:exactnode')
            
        self.add('#PBS -N dummy',hidden=True)
        
        self.add('#')
        self.add("#--- shell commands below ".ljust(80,'-'))
        self.add('cd $PBS_O_WORKDIR')
         
        set_is_modified(self)
        
        #parse lines or file
        if filename is None:
            self.parse(lines)
            self.filename = None
        else:
            self.read(filename)
                        
    def parse(self,text,additive=False):
        """
        Text can be 
          . plain text (str or unicode), possibly containing newline characters, each line will be parsed separately
          . a list of single lines
        """
        if not text:
            return
        if not additive:
            # this case is when the text represents the full script - possibly containing new information
            # such as new lines, old lines with new parameter values or features...
            # We first remove all lines which do not contain parameterised information
            # from the current script: 
            #     . UserComment
            #     . ShellCommand
            # This is because we cannot know whether new lines of these type are equal to existing lines
            # in the script as they may occur more than once.
            for i in xrange(len(self.script_lines)-1,-1,-1):
                line_i = self.script_lines[i]
                print
                print line_i,
                print type(line_i)
                if type(line_i) in (ShellCommand,UserComment):
                    self.script_lines.pop(i)
                    print '  deleted'
        else:
            # All UserComment and ShellCommands are appended at the end, in the order presented,
            # thus allowing several occurrences of the same line.
            pass
        text_type = type(text)
        if text_type is unicode:
            text = str(text)
        if text_type is str:
            text = text.splitlines()
        #assert type(text) is list
        for line in text:
            self.add(line)
            
    def add(self,line,hidden=False):
        """
        Note that directly calling add acts like calling parse() with additive=True 
        """
        if issubclass(type(line),ShellCommand):
            new_script_line = line
        else:
            if type(line) is unicode:
                line = str(line)
            #assert isinstance(line,str), "expecting a str/unicode or a subclass of ShellCommand, got '{}'".format(str(line))
            new_script_line = ShellCommand.parse(line)
            new_script_line.hidden = hidden
        
        if type(new_script_line) in (ShellCommand,UserComment):
            self._insert(new_script_line)
        else:
            for old_script_line in self.script_lines:
                if new_script_line==old_script_line:
                    type_new_script_line = type(new_script_line)
                    if type_new_script_line in (LauncherComment,PBSDirective):
                        old_script_line.hidden = new_script_line.hidden
                        old_parms = old_script_line.parms 
                        if not old_parms:
                            old_script_line.value = new_script_line.value
                        else:
                            new_parms = new_script_line.parms 
                            for k,v in new_parms.iteritems():
                                if not k in old_parms or old_parms[k]!=v:
                                    old_parms[k] = v
                                    set_is_modified(old_script_line)
                                    set_is_modified(self)                            
                            if hasattr(old_script_line,'features'):
                                old_features = old_script_line.features
                                new_features = new_script_line.features
                                for f in new_features:
                                    if not f in old_features:
                                        old_features.append(f)
                                        set_is_modified(old_script_line)
                                        set_is_modified(self)                                            
                        return
                    
                    if type_new_script_line is Shebang:
                        if old_script_line.text != new_script_line.text:
                            old_script_line.text=new_script_line.text
                            set_is_modified(old_script_line)
                            set_is_modified(self)
                        return
                    
                    raise Exception('Unexpected Script line type : '+str(type(new_script_line)))
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
        set_is_modified(self)
    
    def remove(self,line):
        i = self.index(line)
        if i>-1:
            self.script_lines.pop(i)
            set_is_modified(self)
            return True
        return False

    def index(self,line):
        new_script_line = ShellCommand.parse(line)
        for i,old_script_line in enumerate(self.script_lines):
            if new_script_line==old_script_line:
                return i
        return -1
        
    def __getitem__(self,key):
        """ hidden lines are ignored """
        if key[0]=='-':
            for l in self.script_lines:
                if not l.hidden:
                    try:
                        if l.flag==key:
                            return l.value
                    except AttributeError:
                        pass
        else:
            for l in self.script_lines:
                if not l.hidden:
                    l_parms = getattr(l,'parms',{})
                    if l_parms:
                        val = l_parms.get(key,None)
                        if not val is None:
                            return val
        raise KeyError("Key '"+key+"' not found in script (may be hidden).")

    def __setitem__(self,key,val):
        if key[0]=='-':
            for l in self.script_lines:
                try:
                    if l.flag==key:
                        l.value = val
                        set_is_modified(l)
                        set_is_modified(self)
                        if Script.auto_unhide:
                            l.hidden = False
                        return
                except AttributeError:
                    pass
        else:
            for l in self.script_lines:
                l_parms = getattr(l,'parms',{})
                if l_parms:
                    if key in l_parms:
                        l_parms[key]=val
                        set_is_modified(l)
                        set_is_modified(self)
                        if l.hidden and Script.auto_unhide:
                            for v in l.parms.itervalues():
                                if v=='dummy':
                                    break
                            else:
                                l.hidden = False
                        return
            raise KeyError("Key '"+key+"' not found in script.")
    
    def add_parameters(self,key,parms,remove=False):
        """ add (or remove) parameters 'parms' from the script line containing the parameter 'key'. """
        assert isinstance(parms,dict)
        if key[0]=='-':
            raise ValueError('Key {} does not accept features.'.format(key))
        else:
            for l in self.script_lines:
                l_parms = getattr(l,'parms',{})
                if l_parms:
                    if key in l_parms:
                        if remove:
                            for k in parms.iterkeys():
                                del l_parms[k]
                        else:
                            l_parms.update(parms)
                        set_is_modified(l)
                        set_is_modified(self)
                        return
            raise KeyError("Key '"+key+"' not found in script.")
        
    def add_features(self,key,features,remove=False):
        """ add (or remove) the features 'features' in the script line containing parameter 'key'. """
        assert isinstance(features,list)
        if key[0]=='-':
            raise ValueError('Key {} does not accept features.'.format(key))
        else:
            for l in self.script_lines:
                l_parms = getattr(l,'parms',{})
                if key in l_parms:
                    l_features = l.features
                    if remove:
                        for f in features:
                            l_features.remove(f)
                    else:
                        for f in features:
                            if f not in l_features:
                                l_features.append(f)
                    set_is_modified(l)
                    set_is_modified(self)
                    return
            raise KeyError("Key '"+key+"' not found in script.")
        
    def set_hidden(self,key,hidden):
        """ hide (or show) the script line(s) containing the parameter or flag 'key'. """
        if key[0]=='-':
            for l in self.script_lines:
                try:
                    flag = l.flag
                except AttributeError:
                    continue
                else:
                    if flag==key:
                        if l.hidden!=hidden:
                            l.hidden = hidden
                            set_is_modified(l)
                            set_is_modified(self)
                        return
        else:
            for l in self.script_lines:
                l_parms = getattr(l,'parms',{})
                if key in l_parms:                    
                    if l.hidden!=hidden:
                        l.hidden = hidden
                        set_is_modified(l)
                        set_is_modified(self)
                    return
        raise KeyError("Key '"+key+"' not found in script.")

    def has_line(self,line):
        i = self.index(line)
        return i>-1
    
    def get_text(self):
        if is_modified(self):
            self['generated_on'] = str(datetime.datetime.now())
            for sl in self.script_lines:
                sl.compose()
                self.text += sl.text
        return self.text
    
    def read(self,filename):
        with open(filename) as f:
            lines = f.readlines()
            self.__init__(lines) 
            self.filename = filename
        
    def write(self,filename):
        try:
            f = open(filename)
            f.write(self.get_text())
        except Exception as e:
            log_exception(e,msg_after='unexpected...')
        self.filename = filename
    
    def count(self,hidden=False):
        if hidden:
            return len(self.script_lines)
        else:
            n=0
            for l in self.script_lines:
                if not l.hidden:
                    n+=1
            return n

################################################################################
### only test code below this line
################################################################################
import unittest
        
################################################################################
class Test0(unittest.TestCase):
################################################################################        
    def testDefaultScript(self):
        script = Script()
        print '0 Test0.testDefaultScript()\n',Indent(script.script_lines,2)
        
        self.assertTrue(script.count(True )==13)
        
        self.assertTrue(script.count(False)==5)
        
        try:
            script['walltime']
        except KeyError as e:
            print 'as expected: ',e
        else:
            assert False, 'Was expecting KeyError'

        script['walltime'] = walltime_seconds_to_str(150)
        try:
            script['walltime']
        except KeyError as e:
            print 'as expected: ',e
        else:
            assert False, 'Was expecting KeyError'
        
        Script.auto_unhide = True
        script['walltime'] = walltime_seconds_to_str(90)
        self.assertTrue(script['walltime']=='0:01:30')
        
        try:
            script['-M']
        except KeyError as e:
            print 'as expected: ',e
        else:
            assert False, 'Was expecting KeyError'
        email='engelbert.tijskens@uantwerpen.be'
        script['-M']=email
        self.assertTrue(script['-M'], email)
        
        lines = [''
                ,'cd $PBS_O_WORKDIR'
                ,'if [ -d "output" ]; then'
                ,'    rm -rf output'
                ,'fi'
                ,'mkdir output'
                ,'cd output'
                ,'echo "this is the output file" > o.txt'
                ,'cd ..'
                ,'#PBS -W x=nmatchpolicy:y=soep:z=ballekes:exactnode'
                ,'#PBS -N a_simple_job'
                ,'#PBS -M engelbert.tijskens@uantwerpen.be #say hello'
                ,'#PBS -m ae'
                ,'#PBS -l walltime=1:00:00'
                ,'#PBS -l nodes=1:ppn=20'
                ,'#La# generated_on = '+str(datetime.datetime.now())
                ,'#La#      cluster = Hopper'
                ,'#La#      nodeset = Hopper-thin-nodes'
                ]
        script.parse(lines)
        print '1 Test0.testDefaultScript()\n',Indent(script.script_lines,2)
        print script.count()
        self.assertTrue(script.count(True )==19)
        self.assertTrue(script.count(False)==19)
        script.set_hidden('-m', True)
        script.set_hidden('walltime', True)
        print script.count(True)
        print script.count(False)
        
        self.assertTrue(script.count(False)==17)

################################################################################        
if __name__=='__main__':
    unittest.main()