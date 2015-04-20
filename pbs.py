import math,re

class RequestFailed(Exception):
    def __init__(self, message):
        s = "Request cannot be satisfied: "
        self.value = s+message
    def __str__(self):
        return repr(self.value)

class ScriptExtras(object):
    """
    A function object that stores a list of pbs options that must be added to the job script 
    for a particular ComputeNodeSet
    """
    def __init__(self,pbs_options):
        self.pbs_options = pbs_options
    def treat_pbs_options(self,script,remove=False,name=None):
        for o in self.pbs_options:
            v = o[1]
            if name:
                v += ' #required by ComputeNodeSet '+name 
            if not remove:
                script.add_pbs_option(o[0],v)
            else:
                s = '#PBS -l '+v+'\n'
                script.parsed.remove(s)
    def __call__(self,script,remove=False,name=None):
        """ overwrite this to do other things than just add/remove pbs options"""
        self.treat_pbs_options(script, remove=remove, name=name)
        
class ComputeNodeSet(object):
    """
    Objects of this class represent a set of compute nodes with identical properties

    :param int nn: number of compute nodes available in this set.
    :param int cpn: number of cores per node in this set.
    :param float gpn: GB of ram per compute node in this set.
    :param float gbOS: GB of ram reserved for the operating system and thus not available for applicatins in this set.    
    """
    def __init__(self,name,nn,cpn,gpn,gbOS,script_extras=None):
        self.name = name
        self.n_nodes = int(nn)                   #: number of compute nodes available in this set
        self.n_cores_per_node = int(cpn)         #: number of cores per compute nodes in this set
        self.gb_per_node = float(gpn)-gbOS       #: GB of main memory per compute node in this set
        self.gb_per_core = self.gb_per_node/cpn  #: GB of main memory per core in this set
        self._script_extras = script_extras      #: a function object that adds/removes requirements 
        # of the ComputeNodeSet to the script, typically an instance of ScriptExtras (or derived)
    
    def script_extras(self,script,remove=False):
        if self._script_extras:
            self._script_extras(script,remove=remove,name=self.name)
    
    def request_nodes_cores(self,n_nodes,n_cores_per_node):
        if n_nodes>self.n_nodes:
            msg = 'Requesting more nodes than physically available ({}).'.format(self.n_nodes)
            raise RequestFailed(msg)
        if n_cores_per_node>self.n_cores_per_node:
            msg = 'Requesting more cores per node than physically available ({}).'.format(self.n_cores_per_node)
            raise RequestFailed(msg)
        n_cores = n_nodes*n_cores_per_node
        gb_per_core = self.gb_per_node/n_cores_per_node
        return n_cores,gb_per_core
    
    def request_cores_memory(self,n_cores_requested,gb_per_core_requested=0): 
        if gb_per_core_requested > self.gb_per_node:
            msg = 'Requesting more memory per core than physically available ({}).'.format(self.gb_per_node)
            raise RequestFailed(msg)
        if gb_per_core_requested <= self.gb_per_core:
            #use all cores per node since there is sufficient memory
            n_cores_per_node = self.n_cores_per_node
            n_nodes = int(math.ceil(n_cores_requested / float(n_cores_per_node)))
            gb_per_core      = self.gb_per_node/n_cores_per_node
            if n_nodes==1: # respect the number of cores requested instead of returning a full node
                n_cores_per_node = n_cores_requested
                gb_per_core      = self.gb_per_node/n_cores_per_node
        else:
            #use only am many cores per nodes such that each core has at least the requested memory
            n_cores_per_node = int(math.floor(self.gb_per_node / gb_per_core_requested))
            gb_per_core      = self.gb_per_node/n_cores_per_node
            n_nodes = int(math.ceil(n_cores_requested / float(n_cores_per_node)))
        n_cores = n_nodes*n_cores_per_node
        gb = n_nodes*self.gb_per_node
        
        if n_nodes > self.n_nodes:
            msg = 'Requesting more nodes than physically available ({}).'.format(self.n_nodes)
            raise RequestFailed(msg)
        if n_nodes==1: # respect the number of cores requested instead of returning a full node
            n_cores_per_node = n_cores_requested         
        return (n_nodes, n_cores, n_cores_per_node, gb_per_core, gb)
    
#==============================================================================
walltime_units = {'s':    1
                 ,'m':   60
                 ,'h': 3600
                 ,'d':86400 }

walltime_pattern = re.compile(r'((\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?)\s*([dhms])')

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

def format_walltime(walltime):
    if not isinstance(walltime,str):
        walltime = str(walltime)+'s'
    match = walltime_pattern.match(walltime)
    if match:
        number = float( match.groups()[0] )
        unit   =        match.groups()[4]
        seconds = int(number*walltime_units[unit])
        minutes,s = divmod(seconds,60)
        hours,  m = divmod(minutes,60)
        walltime = (str(hours).rjust(2,'0'))+':'+(str(m).rjust(2,'0'))+':'+(str(s).rjust(2,'0'))
        return walltime
    else:
        raise RuntimeError('Bad wall time format: "%s"'%walltime)

#==============================================================================
def set_attributes(obj,**kwargs):
    """ Turn keyword arguments into attributes """
    for k, v in kwargs.iteritems():
        setattr(obj, k, v)
#==============================================================================
shebang = "#!/bin/bash\n"

class Script(object):
    """
    class for manipulating pbs job scripts.
    Keyword arguments (kwargs) become object attributes
    """
    
    def __init__(self, lines=[]):
        """
        Create a pbs job script.
        """
        self.re_nodes    = re.compile(r'nodes=(\d+)')
        self.re_ppn      = re.compile(r'ppn=(\d+)')
        self.re_walltime = re.compile(r'walltime=(\d+):(\d\d):(\d\d)')
        self.re_variable = re.compile(r'\$\{(\w+)\}')
        if lines:
            self.parse(lines)
        else:
            empty_script_lines = [ shebang
                                 , "\n" 
                                 , "\ncd $PBS_O_WORKDIR" 
                                 ]
            self.parse(empty_script_lines)
        
    def parse(self,lines):
        '''
        lines is a list of lines
        a trailing '\n' is appended to lines without one.  
        '''
        self.parsed=[]
        self.values = {}
        for line in lines:
            self.parse1(line)
        #pprint.pprint(self.parsed)

    def parse1(self,line):
        if isinstance(line,(str,unicode)):
            split_line = line.split()
        else:
            split_line = self.parsed[line].split()
            
        if not split_line:
            parsed_line = line
        else:
            # not an empty line
            if split_line[0]=="#PBS":
                #this is a pbs option
                try:
                    key   = split_line[1]
                    value = split_line[2:]
                    if key=='-l':
                        for iv,v in enumerate(value):
                            value[iv] = self.re_nodes   .sub(self.repl_nodes   ,value[iv])
                            value[iv] = self.re_ppn     .sub(self.repl_ppn     ,value[iv])
                            value[iv] = self.re_walltime.sub(self.repl_walltime,value[iv])
                    elif key=='-M' and value:
                        var = 'notify_address'
                        self.values[var] = value[0]
                        value[0] = '${%s}'%var
                    elif key=='-m' and value:
                        var = 'notify_abe'
                        self.values[var] = value[0]
                        value[0] = '${%s}'%var
                    elif key=='-N':
                        var = 'job_name'
                        self.values[var] = value[0]
                        value[0] = '${%s}'%var
                    elif key=='-W':
                        self.values['enforce_n_nodes'] = ('x=nmatchpolicy:exactnode' in value)
                        
                    parsed_line = split_line[0]+' '+split_line[1]
                    for v in value:
                        parsed_line += ' '+v
                    parsed_line += '\n'
                except:
                    parsed_line = '???'+line
            else:
                if line.startswith("#L#"):
                    parsed_line = ''
                else:
                    parsed_line = line
        if isinstance(line,(str,unicode)):
            self.parsed.append(parsed_line)
        else:
            self.parsed[line] = parsed_line
        #pprint.pprint(self.parsed)
    
    def repl_nodes(self,matchobj):
        value = int(matchobj.group(1))
        key = 'n_nodes'
        self.values[key] =  value
        return 'nodes=${%s}'%key 

    def repl_ppn(self,matchobj):
        value = int(matchobj.group(1))
        key = 'n_cores_per_node'
        self.values[key] =  value
        return 'ppn=${%s}'%key 

    def repl_walltime(self,matchobj):
        value = int(matchobj.group(1))*3600 + int(matchobj.group(2))*60 + int(matchobj.group(3))
        key = 'walltime_seconds'
        self.values[key] = value
        return 'walltime=${%s}'%key 

    def add_pbs_option(self,option,value):
        s = '#PBS '+option+' '+value
        if not s.endswith('\n'):
            s+='\n'
        pos = 1 if self.parsed[0].startswith('#!') else 0
#         pprint.pprint(self.parsed)
        if not s in self.parsed:
            self.parsed.insert(pos,s)
        self.parse1(pos)
            
    def compose(self):
        if not self.values:
            return self.parsed
        script = []
        for line in self.parsed:
            s = self.re_variable.sub(self.repl_variable,line)
            script.append(s)
        return script
        
    def repl_variable(self,matchobj):
        key = matchobj.group(1)
        try:
            val = self.values[key]
            if key=='walltime_seconds':
                v = walltime_seconds_to_str(val)
            else:
                v = str(val)
        except KeyError:
            v = '${%s!not_found!}'%key
        return v
