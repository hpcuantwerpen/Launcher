import pbs

class HopperFeatures(object):
    def __init__(self,features):
        self.features = features
    def __call__(self,script,remove=False):
        script.add_parameters('nodes',self.features,remove=remove)        

nodesets = [ pbs.ComputeNodeSet('Hopper-thin-nodes', 96, 20,  64., 6)
           , pbs.ComputeNodeSet('Hopper-fat-nodes' , 24, 20, 256., 6, HopperFeatures({'advres':'fat-reservation.34'})) 
           ]

login_nodes = ['login.hpc.uantwerpen.be']
login_nodes.extend(['login{}-hopper.uantwerpen.be'.format(i) for i in range(1,5)])