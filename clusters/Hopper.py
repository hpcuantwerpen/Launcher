import pbs

class HopperFatNodes(object):
    def __call__(self,script,remove=False):
        script.add('#PBS -l advres=fat-reservation.34')
        if remove:
            script.set_hidden('advres',True)
        else:        
            script.set_hidden('advres',True)

nodesets = [ pbs.ComputeNodeSet('Hopper-thin-nodes', 96, 20,  64., 6)
           , pbs.ComputeNodeSet('Hopper-fat-nodes' , 24, 20, 256., 6, HopperFatNodes()) 
           ]

login_nodes = ['login.hpc.uantwerpen.be']
login_nodes.extend(['login{}-hopper.uantwerpen.be'.format(i) for i in range(1,5)])

day=60*60*24
walltime_limit = 7*day
