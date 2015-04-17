import pbs

#: ComputeNodeSet for Hopper's thin nodes
hopper_thin_nodes = pbs.ComputeNodeSet('Hopper-thin-nodes', 96, 20,  64., 6) 

#: ComputeNodeSet for Hopper's fat nodes
hopper_fat_nodes  = pbs.ComputeNodeSet('Hopper-fat-nodes' , 24, 20, 256., 6
                                      , script_extras=pbs.ScriptExtras([('-l','advres=fat-reservation.34')])) 

node_sets = [ hopper_thin_nodes
            , hopper_fat_nodes
            ]

login_nodes = ['login.hpc.uantwerpen.be']
login_nodes.extend(['login{}-hopper.uantwerpen.be'.format(i) for i in range(1,5)])
