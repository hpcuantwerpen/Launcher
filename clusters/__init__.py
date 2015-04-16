import os, inspect, importlib

def import_clusters():
    cluster_list=[]
    node_sets={}
    login_nodes={}
    # get the path to the folder in which this file lives ( see also:
    #   http://stackoverflow.com/questions/50499/in-python-how-do-i-get-the-path-and-name-of-the-file-that-is-currently-executin/50905#50905
    #   http://stackoverflow.com/questions/247770/retrieving-python-module-path
    # )
    clusters_folder = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
    for folder, subfolders, files in os.walk(clusters_folder):
        for fname in files:
            if fname.endswith('.py') and not fname=='__init__.py':
                cluster = os.path.splitext(fname)[0]
                module = importlib.import_module('clusters.'+cluster)
                cluster_list.append(cluster)
                node_sets  [cluster] = module.node_sets
                login_nodes[cluster] = module.login_nodes
    return cluster_list,node_sets,login_nodes
                
cluster_list,node_sets,login_nodes = import_clusters()

def node_set_names(cluster):
    return [ node_set.name+' ({}Gb/node, {} cores/node)'.format(node_set.gb_per_node,node_set.n_cores_per_node)
             for node_set in node_sets[cluster]
           ] 