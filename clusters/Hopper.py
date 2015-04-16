import pbs

def hopper_fat_nodes_script_extras(script,remove=False):
    if not remove:
        script.add_pbs_option('-l','advres=fat-reservation.34 #required by hopper_fat_nodes')
    else:
        s = '#PBS -l advres=fat-reservation.34 #required by hopper_fat_nodes\n'
        #print script.parsed
        script.parsed.remove(s)
        #print script.parsed
        
#: ComputeNodeSet for Hopper's thin nodes
hopper_thin_nodes = pbs.ComputeNodeSet('hopper_thin_nodes', 96, 20,  64., 6) 

#: ComputeNodeSet for Hopper's fat nodes
hopper_fat_nodes  = pbs.ComputeNodeSet('hopper_fat_nodes' , 24, 20, 256., 6, script_extras=hopper_fat_nodes_script_extras) 

node_sets = [ hopper_thin_nodes
            , hopper_fat_nodes
            ]

login_nodes = ['login.hpc.uantwerpen.be']