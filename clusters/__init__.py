import os, inspect, importlib

def node_set_names(cluster):
    """ make a list with ComputeNodeSet names for cluster <cluster>
    """
    return [ node_set.name+' ({}Gb/node, {} cores/node)'.format(node_set.gb_per_node,node_set.n_cores_per_node)
             for node_set in node_sets[cluster]
           ] 
    
# get the path to the folder in which this file lives ( see also:
#   http://stackoverflow.com/questions/50499/in-python-how-do-i-get-the-path-and-name-of-the-file-that-is-currently-executin/50905#50905
#   http://stackoverflow.com/questions/247770/retrieving-python-module-path
# )
clusters_folder = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))

def import_clusters():
    """
    create 
      - a list of cluster names
      - a dictionary of available ComputeNodeSets per each cluster
      - a dictionary of available login nodes     per each cluster
      - a dictionary of commands that need to be executed before qsub can succeed
        This is typically the case for the Ghent clusters where the cluster where 
        you will submit to is selected by loading a module, e.g. 
            module load cluster/haunter
    This is achieved by importing the cluster modules in the clusters package
    There should be a module for each cluster and each module must export
      - node_sets   : a list of ComputeNodesets for that cluster
      - login_nodes : a list of login nodes for that cluster
      - before_qsub : [optional] a string with linux commands that must be executed before qsub.
                      The commands are separated with '&&' 
    """
    cluster_list=[] 
    node_sets={}
    login_nodes={}
    before_qsub={}
    for folder, subfolders, files in os.walk(clusters_folder):
        for fname in files:
            if fname.endswith('.py') and not fname=='__init__.py':
                cluster = os.path.splitext(fname)[0]
                module = importlib.import_module('clusters.'+cluster)
                cluster_list.append(cluster)
                node_sets  [cluster] = module.node_sets
                login_nodes[cluster] = module.login_nodes
                
                tmp = getattr(module,'before_qsub','').strip()
                if tmp and not tmp.endswith('&&'):
                    tmp += ' && '
                before_qsub[cluster] = tmp 
    return cluster_list,node_sets,login_nodes,before_qsub
                
cluster_list,node_sets,login_nodes,before_qsub = import_clusters()
