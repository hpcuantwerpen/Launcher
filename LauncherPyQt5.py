from __future__ import print_function

from PyQt5.QtCore import QT_VERSION_STR
__version__ = "PyQt5 "+QT_VERSION_STR
from PyQt5 import QtWidgets

import Launcher
import constants

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super(MainWindow,self).__init__()
        self.data = Launcher.Launcher()
        self.createUI()

    def createUI(self):
        self.setWindowTitle('Launcher {}.{}'.format(*constants._LAUNCHER_VERSION_))
        
        self.wPages = QtWidgets.QTabWidget(parent=self)
        #=======================================================================
        # Resources page
        self.wResourcesPage = QtWidgets.QWidget()
        #add resource page widgets
        vbox_page = QtWidgets.QVBoxLayout()
        #-----------------------------------------------------------------------
        cluster_group = QtWidgets.QGroupBox("Request resources from:")
        hbox0 = QtWidgets.QHBoxLayout()        
        cluster = self.data.get_cluster()

        self.wCluster = QtWidgets.QComboBox(parent=self.wResourcesPage)
        self.wCluster.setInsertPolicy(QtWidgets.QComboBox.NoInsert)
        self.wCluster.addItems(Launcher.clusters.cluster_names)
        self.wCluster.setCurrentText(cluster)
        hbox0.addWidget(self.wCluster)
        
        self.wNodeset = QtWidgets.QComboBox(parent=self.wResourcesPage)
        self.wNodeset.setInsertPolicy(QtWidgets.QComboBox.NoInsert)
        self.wNodeset.addItems(Launcher.clusters.decorated_nodeset_names(cluster))
        self.wCluster.setCurrentText(self.data.get_selected_nodeset_name())
        hbox0.addWidget(self.wNodeset)

        cluster_group.setLayout(hbox0)
        vbox_page.addWidget(cluster_group)
        #-----------------------------------------------------------------------
        hbox0 = QtWidgets.QHBoxLayout()
        req_nodes_group = QtWidgets.QGroupBox("Request nodes and cores per node:")
        grid = QtWidgets.QGridLayout()
        r=0
        grid.addWidget(QtWidgets.QLabel("Number of nodes:"),r,0)
        self.wNNodes = QtWidgets.QSpinBox()
        grid.addWidget(self.wNNodes                        ,r,1)
        r+=1
        grid.addWidget(QtWidgets.QLabel("Cores per node:") ,r,0)
        self.wNCoresPerNode = QtWidgets.QSpinBox()
        grid.addWidget(self.wNCoresPerNode                 ,r,1)
        r+=1
        label = QtWidgets.QLabel("GB per node:")
        label.setEnabled(False)
        grid.addWidget(label                               ,r,0)
        self.wGbPerCore = QtWidgets.QLineEdit()
        self.wGbPerCore.setEnabled(False)
        grid.addWidget(self.wGbPerCore                     ,r,1)
        req_nodes_group.setLayout(grid)
        hbox0.addWidget(req_nodes_group)
        #-----------------------------------------------------------------------        
        req_cores_group = QtWidgets.QGroupBox("Request cores and memory per core:")
        grid = QtWidgets.QGridLayout()
        r=0
        grid.addWidget(QtWidgets.QLabel("Number of cores:"),r,0)
        self.wNCores = QtWidgets.QSpinBox()
        grid.addWidget(self.wNCores                        ,r,1)
        r+=1
        grid.addWidget(QtWidgets.QLabel("GB per core:")    ,r,0)
        self.wGbPerCore = QtWidgets.QDoubleSpinBox()
        grid.addWidget(self.wGbPerCore                     ,r,1)
        r+=1
        label = QtWidgets.QLabel("GB in total:")
        label.setEnabled(False)
        grid.addWidget(label                               ,r,0)
        self.wGbTotal = QtWidgets.QLineEdit()
        self.wGbTotal.setEnabled(False)
        grid.addWidget(self.wGbTotal                       ,r,1)
        req_cores_group.setLayout(grid)
        hbox0.addWidget(req_cores_group)
        #-----------------------------------------------------------------------
        vbox_page.addLayout(hbox0)
        #-----------------------------------------------------------------------
        self.wResourcesPage.setLayout(vbox_page)
        #=======================================================================
        # Script page
        self.wScriptPage = QtWidgets.QWidget()
        #add resource page widgets
        
        #=======================================================================
        # Retrieve page
        self.wRetrievePage = QtWidgets.QWidget()
        #add restrieve page widgets
        
        #=======================================================================
        self.wPages.addTab(self.wResourcesPage, "Resources")
        self.wPages.addTab(self.wScriptPage   , "Script")
        self.wPages.addTab(self.wRetrievePage , "Retrieve")
        #=======================================================================
        self.setCentralWidget(self.wPages)

#         menu = self.menuBar().addMenu('File')
#         action = menu.addAction('Change File Path')
#         action.triggered.connect(self.changeFilePath)   

def start_gui(argv=[]):
    app = QtWidgets.QApplication(argv)
    window = MainWindow()
    window.show()
    #window.setGeometry(500, 300, 300, 300)
    return app.exec_()

if __name__=="__main__":
    start_gui()