import pbs
from script import Script

class TuringFeatures(object):
    def __init__(self,features):
        self.features = features
    def __call__(self,script,remove=False):
        script.add_features('nodes',self.features,remove=True)        

nodesets = [ pbs.ComputeNodeSet('Turing-Harpertown'    , 64,  8, 16., 2, TuringFeatures(['harpertown'      ]) )
           , pbs.ComputeNodeSet('Turing-Harpertown-GbE', 64,  8, 16., 2, TuringFeatures(['harpertown','gbe']) )
           , pbs.ComputeNodeSet('Turing-Harpertown-IB' , 32,  8, 16., 2, TuringFeatures(['harpertown','ib' ]) )
           , pbs.ComputeNodeSet('Turing-Westmere'      , 64, 12, 24., 2, TuringFeatures(['westmere'        ]) )
           , pbs.ComputeNodeSet('Turing-Westmere-GbE'  , 64, 12, 24., 2, TuringFeatures(['westmere'  ,'gbe']) )
           , pbs.ComputeNodeSet('Turing-Westmere-IB'   ,  8, 12, 24., 2, TuringFeatures(['westmere'  ,'ib' ]) )
           ]

login_nodes = ['login.turing.calcua.ua.ac.be']
login_nodes.extend(['login{}.turing.calcua.ua.ac.be'.format(i) for i in range(1,3)])

day=60*60*24
walltime_limit = 21*day
        