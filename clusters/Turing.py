import pbs

def script_extras(self,script,remove=False):
    if not remove:
        script.add_pbs_option('-l','advres=fat-reservation.34 #required by '+self.name)
    else:
        s = '#PBS -l advres=fat-reservation.34 #required by hopper_fat_nodes'+self.name+'\n'
        script.parsed.remove(s)

turing_harpertown     = pbs.ComputeNodeSet('Turing-Harpertown'    , 64,  8, 16., 4
                                          , pbs.ScriptExtras([('-l','harpertown')])
                                          ) 
turing_harpertown_GbE = pbs.ComputeNodeSet('Turing-Harpertown-GbE', 64,  8, 16., 4
                                          , pbs.ScriptExtras([('-l','harpertown')
                                                             ,('-l','gbe'       )])
                                          ) 
turing_harpertown_IB  = pbs.ComputeNodeSet('Turing-Harpertown-IB' , 32,  8, 16., 4
                                          , pbs.ScriptExtras([('-l','harpertown')
                                                             ,('-l','ib'        )])
                                          ) 
turing_westmere       = pbs.ComputeNodeSet('Turing-Westmere'      , 64, 12, 24., 4
                                          , pbs.ScriptExtras([('-l','westmere')])
                                          ) 
turing_westmere_GbE   = pbs.ComputeNodeSet('Turing-Westmere-GbE'  , 64, 12, 24., 4
                                          , pbs.ScriptExtras([('-l','westmere')
                                                             ,('-l','gbe'     )])
                                          ) 
turing_westmere_IB    = pbs.ComputeNodeSet('Turing-Westmere-IB'   ,  8, 12, 24., 4
                                          , pbs.ScriptExtras([('-l','westmere')
                                                             ,('-l','ib'      )])
                                          ) 

node_sets = [ turing_westmere
            , turing_westmere_IB
            , turing_westmere_GbE
            , turing_harpertown
            , turing_harpertown_IB
            , turing_harpertown_GbE
            ]

login_nodes = ['login.turing.calcua.ua.ac.be']