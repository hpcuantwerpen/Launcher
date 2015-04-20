import pbs

node_sets = [ pbs.ComputeNodeSet('Turing-Harpertown'    , 64,  8, 16., 4, pbs.ScriptExtras([('-l','harpertown')]) )
            , pbs.ComputeNodeSet('Turing-Harpertown-GbE', 64,  8, 16., 4, pbs.ScriptExtras([('-l','harpertown')
                                                                                           ,('-l','gbe'       )]) )
            , pbs.ComputeNodeSet('Turing-Harpertown-IB' , 32,  8, 16., 4, pbs.ScriptExtras([('-l','harpertown')
                                                                                           ,('-l','ib'        )]) )
            , pbs.ComputeNodeSet('Turing-Westmere'      , 64, 12, 24., 4, pbs.ScriptExtras([('-l','westmere'  )]) )
            , pbs.ComputeNodeSet('Turing-Westmere-GbE'  , 64, 12, 24., 4, pbs.ScriptExtras([('-l','westmere'  )
                                                                                           ,('-l','gbe'       )]) )
            , pbs.ComputeNodeSet('Turing-Westmere-IB'   ,  8, 12, 24., 4, pbs.ScriptExtras([('-l','westmere'  )
                                                                                           ,('-l','ib'        )]) )
            ]

login_nodes = ['login.turing.calcua.ua.ac.be']
login_nodes.extend(['login{}.turing.calcua.ua.ac.be'.format(i) for i in range(1,3)])
