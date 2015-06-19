import math,re,datetime

class RequestFailed(Exception):
    def __init__(self, message):
        s = "Request cannot be satisfied: "
        self.value = s+message
    def __str__(self):
        return repr(self.value)
        
class ComputeNodeSet(object):
    """
    Objects of this class represent a set of compute nodes with identical properties

    :param int nn: number of compute nodes available in this set.
    :param int cpn: number of cores per node in this set.
    :param float gpn: GB of ram per compute node in this set.
    :param float gbOS: GB of ram reserved for the operating system and thus not available for applicatins in this set.    
    """
    def __init__(self,name,nn,cpn,gpn,gbOS,nodeset_features=None):
        self.name = name
        self.n_nodes = int(nn)                   #: number of compute nodes available in this set
        self.n_cores_per_node = int(cpn)         #: number of cores per compute nodes in this set
        self.gb_per_node = float(gpn)-gbOS       #: GB of main memory per compute node in this set
        self.gb_per_core = self.gb_per_node/cpn  #: GB of main memory per core in this set
        self._nodeset_features = nodeset_features      #: a function object that adds/removes requirements 
        # of the ComputeNodeSet to the script, typically an instance of ScriptExtras (or derived)
    
    def nodeset_features(self,script,remove=False):
        if self._nodeset_features:
            self._nodeset_features(script,remove=remove)
    
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

walltime_pattern = re.compile(r'((\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?)\s*([dhms])')
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
