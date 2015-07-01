from __future__ import print_function

import os,sys

from PyQt5.QtCore import QT_VERSION_STR,Qt
__version__ = "PyQt5 "+QT_VERSION_STR

from PyQt5.QtWidgets import QMainWindow,QTabWidget,QWidget,QLineEdit,QLabel,   \
     QComboBox,QSpinBox,QDoubleSpinBox,QHBoxLayout,QVBoxLayout,QGridLayout,    \
     QFrame,QCheckBox,QPushButton,QGroupBox,QSplitter,QApplication,QMessageBox,\
     QFileDialog,QTextEdit

import Launcher
import constants
from log import log_exception,LogItem,LogSession
import cws

################################################################################
class MainWindow(QMainWindow):
################################################################################
    def __init__(self):
        super(MainWindow,self).__init__()
        self.launcher = Launcher.Launcher()
        #enable the launcher to send messages to the gui
        self.launcher.gui_set_status_text = self.set_status_text
        
        self.ignore_signals = True

        self.createUI()
        
        self.cws_lists.c2w()

        with LogItem('Invisible widgets:'):
            print("    wWalltimeSeconds.text() = '{}'".format(self.wWalltimeSeconds.text()))

        n_waiting = len(self.launcher.get_submitted_jobs())
        if n_waiting>0:
            msg = "There are {} finished jobs waiting to be retrieved.\n Do you want to retrieve them now?".format(n_waiting)
            answer = QMessageBox.question(self, '', msg, defaultButton=QMessageBox.Yes)
            if answer==QMessageBox.Yes:
                self.wRetrieveAll_on_clicked()
        else:
            print("\nThere are no jobs waiting to be retrieved.")
                    
        self.ignore_signals = False

    def createUI(self):
        self.setWindowTitle('Launcher {}.{}'.format(*constants._LAUNCHER_VERSION_))
        self.wPages = QTabWidget(parent=self)
        #=======================================================================
        # Resources page
        self.wResourcesPage = QWidget()
        #add resource page widgets
        vbox_page = QVBoxLayout()
        vbox_page.setSpacing(2)
        #-----------------------------------------------------------------------
        cluster_group = QGroupBox("Request resources from:")
        hbox_group = QHBoxLayout()        
        self.wCluster = QComboBox(parent=self.wResourcesPage)
        self.wCluster.setInsertPolicy(QComboBox.NoInsert)
        hbox_group.addWidget(self.wCluster)
        self.wNodeset = QComboBox(parent=self.wResourcesPage)
        self.wNodeset.setInsertPolicy(QComboBox.NoInsert)
        hbox_group.addWidget(self.wNodeset)
        cluster_group.setLayout(hbox_group)
        vbox_page.addWidget(cluster_group)
        #-----------------------------------------------------------------------
        hbox_2groups = QHBoxLayout()
        req_nodes_group = QGroupBox("Request nodes and cores per node:")
        grid = QGridLayout()
        grid.setVerticalSpacing(5)
        r=0
        grid.addWidget(QLabel("Number of nodes:"),r,0)
        self.wNNodes = QSpinBox()
        grid.addWidget(self.wNNodes                        ,r,1)
        r+=1
        grid.addWidget(QLabel("Cores per node:") ,r,0)
        self.wNCoresPerNode = QSpinBox()
        grid.addWidget(self.wNCoresPerNode                 ,r,1)
        r+=1
        label = QLabel("GB per node:")
        label.setEnabled(False)
        grid.addWidget(label                               ,r,0)
        self.wGbPerNode = QLineEdit()
        self.wGbPerNode.setEnabled(False)
        grid.addWidget(self.wGbPerNode                     ,r,1)
        r+=1
        self.wEnforceNNodes = QCheckBox("Enforce nodes")
#         self.wEnforceNNodes.setChecked(self.launcher.get_enforce_n_nodes())
        grid.addWidget(self.wEnforceNNodes                 ,r,0)
        self.wRequestNodes = QPushButton('make request')
        grid.addWidget(self.wRequestNodes                  ,r,1)
        req_nodes_group.setLayout(grid)
        hbox_2groups.addWidget(req_nodes_group)
        #-----------------------------------------------------------------------        
        req_cores_group = QGroupBox("Request cores and memory per core:")
        grid = QGridLayout()
        grid.setVerticalSpacing(5)
        r=0
        grid.addWidget(QLabel("Number of cores:")   ,r,0)
        self.wNCores = QSpinBox()
        grid.addWidget(self.wNCores                 ,r,1)
        r+=1
        grid.addWidget(QLabel("GB per core:")       ,r,0)
        self.wGbPerCore = QDoubleSpinBox()
        self.wGbPerCore.setDecimals(3)
        self.wGbPerCore.setSingleStep(.1)
        grid.addWidget(self.wGbPerCore              ,r,1)
        r+=1
        label = QLabel("GB in total:")
        label.setEnabled(False)
        grid.addWidget(label                        ,r,0)
        self.wGbTotal = QLineEdit()
        self.wGbTotal.setEnabled(False)
        grid.addWidget(self.wGbTotal                ,r,1)
        r+=1
        self.wAutomaticRequests = QCheckBox('Automatic requests')
        self.wRequestCores = QPushButton('make request')
        grid.addWidget(self.wAutomaticRequests      ,r,0)
        grid.addWidget(self.wRequestCores           ,r,1)
        req_cores_group.setLayout(grid)
        hbox_2groups.addWidget(req_cores_group)
        #-----------------------------------------------------------------------
        vbox_page.addLayout(hbox_2groups)
        #-----------------------------------------------------------------------
        hbox_2groups = QHBoxLayout()
        #-----------------------------------------------------------------------
        walltime_group = QGroupBox("Request wall time:")
        vbox_group = QVBoxLayout()
        vbox_group.setSpacing(2)
        self.wWalltime = QDoubleSpinBox()
        self.wWalltime  .setRange(1,1000000)
        self.wWalltime  .setDecimals(3)
        self.wWalltimeSeconds = QSpinBox()
        self.wWalltimeSeconds  .setVisible(False)
        vbox_group.addWidget(self.wWalltime)
        self.wWalltimeUnit = QComboBox()
        self.wWalltimeUnit  .addItems(['seconds','minutes','hours','days'])
        vbox_group.addWidget(self.wWalltimeUnit)
        walltime_group.setLayout(vbox_group)
        hbox_2groups.addWidget(walltime_group)        
        #-----------------------------------------------------------------------
        notify_group = QGroupBox("Nofify:")
        vbox_group = QVBoxLayout()
        vbox_group.setSpacing(2)
        self.wNotifyAddress = QLineEdit()
        vbox_group.addWidget(self.wNotifyAddress)
        hbox_group = QHBoxLayout()
        self.wNotifyBegin = QCheckBox("on begin")
        self.wNotifyEnd   = QCheckBox("on end")
        self.wNotifyAbort = QCheckBox("on abort")
#         self.wNotify_abe  = QLineEdit()
#         self.wNotify_abe   .setVisible(False)
        hbox_group.addWidget(self.wNotifyBegin)
        hbox_group.addWidget(self.wNotifyEnd)
        hbox_group.addWidget(self.wNotifyAbort)
        vbox_group.addLayout(hbox_group)
        notify_group.setLayout(vbox_group)
        hbox_2groups.addWidget(notify_group)        
        #-----------------------------------------------------------------------
        vbox_page.addLayout(hbox_2groups)
        #-----------------------------------------------------------------------
        job_group = QGroupBox("Job management:")
        self.wUsername  = QLineEdit()
        self.wConnected = QLabel("Not Connected")
        self.wConnected   .setEnabled(False)
        self.wLocal     = QLineEdit()
        self.wLocal      .setAlignment(Qt.AlignRight)
        self.wRemote    = QComboBox()
        self.wRemote.setInsertPolicy(QComboBox.NoInsert)
        self.wSubfolder = QLineEdit()
        self.wSubfolder2= QLineEdit()
        self.wSubfolder  .setAlignment(Qt.AlignCenter)
        self.wSubfolder2 .setAlignment(Qt.AlignCenter)
        self.wSubfolder  .setEnabled(False)
        self.wSubfolder2 .setEnabled(False)
        self.wJobname   = QLineEdit()
        self.wJobname2  = QLineEdit()
        self.wJobname .setEnabled(False)
        self.wJobname2.setEnabled(False)
                 
        vbox_group = QVBoxLayout()
        hbox = QHBoxLayout()
        hbox.addWidget(QLabel("username:"))
        hbox.addWidget(self.wUsername)
        hbox.addStretch()
        hbox.addWidget(self.wConnected)
        vbox_group.addLayout(hbox)
        
        hbox  = QHBoxLayout()
        columns = QSplitter(Qt.Horizontal)
        
        col = QFrame()
        col.setFrameShape(QFrame.StyledPanel)
        vbox_column = QVBoxLayout()
        vbox_column.setSpacing(2)
        vbox_column.addWidget(QLabel(" ")     ,Qt.AlignRight)
        vbox_column.addWidget(QLabel("local") ,Qt.AlignRight)
        vbox_column.addWidget(QLabel("remote"),Qt.AlignRight)
        col.setLayout(vbox_column)
        columns.addWidget(col)
        hbox.addWidget(columns)
        
        col = QFrame()
        col.setFrameShape(QFrame.StyledPanel)
        vbox_column = QVBoxLayout()
        vbox_column.setSpacing(2)
        self.wChooseLocal = QPushButton("file location ...")
        vbox_column.addWidget(self.wChooseLocal)
        vbox_column.addWidget(self.wLocal)
        vbox_column.addWidget(self.wRemote)
        col.setLayout(vbox_column)
        columns.addWidget(col)
                
        col = QFrame()
        col.setFrameShape(QFrame.StyledPanel)
        vbox_column = QVBoxLayout()
        vbox_column.setSpacing(2)
        self.wChooseSubfolder = QPushButton("subfolder ...")
        vbox_column.addWidget(self.wChooseSubfolder)
        vbox_column.addWidget(self.wSubfolder )
        vbox_column.addWidget(self.wSubfolder2)
        col.setLayout(vbox_column)
        columns.addWidget(col)
        
        col = QFrame()
        col.setFrameShape(QFrame.StyledPanel)
        vbox_column = QVBoxLayout()
        vbox_column.setSpacing(2)
        self.wChooseJobname = QPushButton("job name ...")
        vbox_column.addWidget(self.wChooseJobname)
        vbox_column.addWidget(self.wJobname )
        vbox_column.addWidget(self.wJobname2)
        col.setLayout(vbox_column)
        columns.addWidget(col)
        
        vbox_group.addWidget(columns)
        
        hbox = QHBoxLayout()
        self.wReload = QPushButton("Reload")
        self.wSave   = QPushButton("Save")
        self.wSubmit = QPushButton("Submit")
        hbox.addWidget(self.wReload)
        hbox.addWidget(self.wSave)
        hbox.addWidget(self.wSubmit)
        
        
#         grid = QGridLayout()
#         grid.setVerticalSpacing(5)
#         grid.addWidget(self.wSave                  ,0,0)
#         grid.addWidget(self.wReload                ,1,0)
#         grid.addWidget(self.wSubmit                ,2,0)
#         grid.addWidget(self.wRetrieveAll           ,0,1)
#         grid.addWidget(self.wCopyToDesktop   ,1,1)
#         grid.addWidget(self.wCopyToVSC_DATA,2,1)
        vbox_group.addLayout(hbox)
        #-----------------------------------------------------------------------        
        job_group.setLayout(vbox_group)
        vbox_page.addWidget(job_group)
        vbox_page.addStretch()
        self.wResourcesPage.setLayout(vbox_page)

        #=======================================================================
        # Script page
        self.wScriptPage = QWidget()
        vbox_page = QVBoxLayout()
        #add resource page widgets
        #-----------------------------------------------------------------------
        self.wScript = QTextEdit(self)
        vbox_page.addWidget(self.wScript)
        hbox = QHBoxLayout()
        hbox.addWidget(QLabel("select module:"))
        self.wModules = QComboBox()
        hbox.addWidget(self.wModules)
        vbox_page.addLayout(hbox)
        #-----------------------------------------------------------------------
        self.wScriptPage.setLayout(vbox_page)
        #=======================================================================
        # Retrieve page
        self.wRetrievePage = QWidget()
        #add restrieve page widgets
        vbox_page = QVBoxLayout()
        hbox = QHBoxLayout()
        self.wJobStatusButton = QPushButton("Job status (qstat -u):")
        hbox.addWidget(self.wJobStatusButton)
        hbox.addStretch()
        self.wJobStatus = QTextEdit()
        self.wSubmitted = QTextEdit()
        self.wRetrieved = QTextEdit()
        vbox_page.addLayout(hbox)
        vbox_page.addWidget(self.wJobStatus)
        vbox_page.addWidget(QLabel("Jobs submitted and not retrieved:"))
        vbox_page.addWidget(self.wSubmitted)
        vbox_page.addWidget(QLabel("Jobs retrieved during this Launcher session:"))
        vbox_page.addWidget(self.wRetrieved)
        grid = QGridLayout()
        self.wRetrieveSelected = QPushButton("Retrieve results for the selected job id")
#         self.wRetrieveSelected  .setEnabled(False)
        self.wDeleteSelected   = QPushButton("Delete selected jobs (no retrieval)")
#         self.wDeleteSelected    .setEnabled(False)
        self.wRetrieveAll      = QPushButton("Retrieve results for all jobs")
        self.wCopyToDesktop    = QCheckBox("Copy results to desktop")
        self.wCopyToVSC_DATA   = QCheckBox("Copy results to $VSC_DATA")
        self.wDeleteLocal      = QCheckBox("Delete local folder")
        self.wDeleteRemote     = QCheckBox("Delete remote folder")
        grid.addWidget(self.wCopyToDesktop   ,0,0)
        grid.addWidget(self.wCopyToVSC_DATA  ,1,0)
        grid.addWidget(self.wRetrieveSelected,2,0)
        grid.addWidget(self.wDeleteLocal     ,0,1)
        grid.addWidget(self.wDeleteRemote    ,1,1)
        grid.addWidget(self.wDeleteSelected  ,2,1)
        grid.addWidget(self.wRetrieveAll     ,3,0)
        vbox_page.addLayout(grid)
        self.wRetrievePage.setLayout(vbox_page)
        #=======================================================================
        self.wPages.addTab(self.wResourcesPage, "Resources")
        self.wPages.addTab(self.wScriptPage   , "Script")
        self.wPages.addTab(self.wRetrievePage , "Retrieve")
        #=======================================================================
        self.setCentralWidget(self.wPages)
        
        #=======================================================================
        # data connections
        #=======================================================================
        cnf = self.launcher.config
        scr = self.launcher.script
        self.cws_lists = cws.CWS_list()
        self.cws_lists.add('cluster'  ,cws.CWS_QComboBox(cnf['cluster'             ],self.wCluster          ,scr,'cluster' ))
        self.cws_lists.add('cluster'  ,cws.CWS_QComboBox(cnf['nodeset'             ],self.wNodeset          ,scr,'nodeset' ))
        self.cws_lists.add('resources',cws.CWS_QSpinBox (cnf['nodes'               ],self.wNNodes           ,scr,'nodes'   ))
        self.cws_lists.add('resources',cws.CWS_QSpinBox (cnf['ppn'                 ],self.wNCoresPerNode    ,scr,'ppn'     ))
        self.cws_lists.add('resources',cws.CWS_QLineEdit(cnf['gbpn'                ],self.wGbPerNode        ,scr, None     ))
        self.cws_lists.add('resources',cws.CWS_QCheckBox(cnf['automatic_requests'  ],self.wAutomaticRequests,scr, None     ))
        self.cws_lists.add('resources',cws.CWS_QCheckBox(cnf['enforce_n_nodes'     ],self.wEnforceNNodes    ,scr,'x'
                                                        ,hide_if_false=True                                                ))
        self.cws_lists.add('walltime' ,cws.CWS_QSpinBox (cnf['walltime_seconds'    ],self.wWalltimeSeconds  ,scr,'walltime'
                                                        ,  to_script=Launcher.walltime_seconds_to_str
                                                        ,from_script=Launcher.str_to_walltime_seconds                      ))
        self.cws_lists.add('walltime' ,cws.CWS_QComboBox(cnf['walltime_unit'       ],self.wWalltimeUnit     ,scr, None     ))
        self.cws_lists.add('notify'   ,cws.CWS_QLineEdit(cnf['notify_M'            ],self.wNotifyAddress    ,scr,'-M'      ))
        self.cws_lists.add('notify'   ,cws.CWS_QCheckBox(cnf['notify_m'            ],self.wNotifyAbort      ,scr,'-m'      
                                                        ,  to_str=Launcher.abort_to_abe
                                                        ,from_str=Launcher.abe_to_abort                                    ))
        self.cws_lists.add('notify'   ,cws.CWS_QCheckBox(cnf['notify_m'            ],self.wNotifyBegin      ,scr,'-m'     
                                                        ,  to_str=Launcher.begin_to_abe
                                                        ,from_str=Launcher.abe_to_begin                                    ))
        self.cws_lists.add('notify'   ,cws.CWS_QCheckBox(cnf['notify_m'            ],self.wNotifyEnd        ,scr,'-m'    
                                                        ,  to_str=Launcher.end_to_abe
                                                        ,from_str=Launcher.abe_to_end                                      ))
        self.cws_lists.add('job'      ,cws.CWS_QLineEdit(cnf['username'            ],self.wUsername         ,scr, None     ))
        self.cws_lists.add('job'      ,cws.CWS_QLineEdit(cnf['local_file_location' ],self.wLocal            ,scr, None     ))
        self.cws_lists.add('job'      ,cws.CWS_QComboBox(cnf['remote_file_location'],self.wRemote           ,scr, None     ))
        self.cws_lists.add('job'      ,cws.CWS_QLineEdit(cnf['subfolder'           ],self.wSubfolder        ,scr, None     ))
        self.cws_lists.add('job'      ,cws.CWS_QLineEdit(cnf['jobname'             ],self.wJobname          ,scr,'-N'      ))
        self.cws_lists.add('job'      ,cws.CWS_QCheckBox(cnf['copy_to_desktop'     ],self.wCopyToDesktop    ,scr, None     ))
        self.cws_lists.add('job'      ,cws.CWS_QCheckBox(cnf['copy_to_VSC_DATA'    ],self.wCopyToVSC_DATA   ,scr, None     ))
        
        self.cws_lists.on_2w['cluster'  ].append(self.on_cluster_2w  )
        self.cws_lists.on_2w['resources'].append(self.on_resources_2w)
        self.cws_lists.on_2w['walltime' ].append(self.on_walltime_2w )
          
        #=======================================================================
        # signal connections
        #=======================================================================
        self.wCluster           .currentIndexChanged .connect(self.wCluster_on_currentIndexChanged)
        self.wNodeset           .currentIndexChanged .connect(self.wNodeset_on_currentIndexChanged)
        self.wRequestNodes      .clicked             .connect(self.wRequestNodes_on_clicked)
        self.wRequestCores      .clicked             .connect(self.wRequestCores_on_clicked)
        self.wAutomaticRequests .toggled             .connect(self.wAutomaticRequests_on_toggled)
        self.wNNodes            .valueChanged        .connect(self.wNNodes_on_valueChanged)
        self.wNCoresPerNode     .valueChanged        .connect(self.wNCoresPerNode_on_valueChanged)
        self.wNCores            .valueChanged        .connect(self.wNCores_on_valueChanged)
        self.wGbPerCore         .valueChanged        .connect(self.wGbPerCore_on_valueChanged)
        self.wWalltime          .valueChanged        .connect(self.wWalltime_on_valueChanged)
        self.wWalltimeUnit      .currentIndexChanged .connect(self.wWalltimeUnit_on_currentIndexChanged)
        self.wNotifyAddress     .returnPressed       .connect(self.wNotifyAddress_on_returnPressed)
        self.wUsername          .returnPressed       .connect(self.wUsername_on_returnPressed)
        self.wChooseLocal       .clicked             .connect(self.wChooseLocal_on_clicked)
        self.wChooseSubfolder   .clicked             .connect(self.wChooseSubfolder_on_clicked)
        self.wChooseJobname     .clicked             .connect(self.wChooseJobname_on_clicked)
        self.wPages             .currentChanged      .connect(self.wPages_on_page_change)
        self.wRemote            .currentIndexChanged .connect(self.wRemote_on_currentIndexChanged)
        self.wSave              .clicked             .connect(self.wSave_on_clicked)
        self.wReload            .clicked             .connect(self.wReload_on_clicked)
        self.wJobStatusButton   .clicked             .connect(self.wJobStatusButton_on_clicked)
        self.wSubmit            .clicked             .connect(self.wSubmit_on_clicked)
        self.wModules           .currentIndexChanged .connect(self.wModules_on_currentIndexChanged)
#         self.wSubmitted         .selectionChanged    .connect(self.wSubmitted_on_selectionChanged)
        self.wRetrieveAll       .clicked             .connect(self.wRetrieveAll_on_clicked)
        self.wRetrieveSelected  .clicked             .connect(self.wRetrieveSelected_on_clicked)
        self.wDeleteSelected    .clicked             .connect(self.wDeleteSelected_on_clicked)
        #=======================================================================
       
        #=======================================================================
        # menu
        #=======================================================================
#         menu = self.menuBar().addMenu('File')
#         action = menu.addAction('Change File Path')
#         action.triggered.connect(self.changeFilePath)   

    def on_cluster_2w(self):
        self.wModules.clear()
        try:
            self.wModules.addItems(self.launcher.get_modules())
        except Exception as e:
            log_exception(e)
        
    def on_resources_2w(self):
        v = self.wNNodes.value() * self.wNCoresPerNode.value()
        self.wNCores.setValue(v)
        
        v = float(self.wGbPerNode.text())/self.wNCoresPerNode.value()
        self.wGbPerCore.setValue(v)
        
        v = float(self.wGbPerNode.text())*self.wNNodes.value()
        self.wGbTotal.setText(str(round(v,3)))
        
        self.wNCores   .setRange(1,self.launcher.nodeset.n_nodes*self.launcher.nodeset.n_cores_per_node)
        
        gb_per_node = float(self.wGbPerNode.text())
        self.wGbPerCore.setRange(gb_per_node/self.wNCoresPerNode.value(),gb_per_node)
        
        v = self.wAutomaticRequests.isChecked()
        self.wRequestCores.setEnabled(not v)
        self.wRequestNodes.setEnabled(not v)
        
    def on_walltime_2w(self):
        walltime = self.wWalltimeSeconds.value()
        walltime/= float(Launcher.walltime_units[self.wWalltimeUnit.currentText()])
        self.wWalltime.setValue(walltime)
        self.wWalltime_adapt_to_unit()
        self.wWalltimeUnit_on_currentIndexChanged()
        
    def wWalltime_adapt_to_unit(self):
        unit = self.wWalltimeUnit.currentText() 
        if unit=='seconds':
            step = 15.
        elif unit=='minutes':
            step = 15.
        elif unit=='hours':
            step = 1
        else:
            step = 0.25
        self.wWalltime.setSingleStep(step)
        walltime_limit = Launcher.clusters.walltime_limit[self.wCluster.currentText()]
        walltime_limit/= Launcher.walltime_units[unit]
        self.wWalltime.setRange(0,walltime_limit)

    def show_ssh_available(self,retry=False):
        if not Launcher.sshtools.Client.last_try_success and retry:
            try:
                ssh = Launcher.sshtools.Client(self.launcher.get_username(),self.launcher.get_login_node())
                ssh.close()
            except Exception as e:
                log_exception(e)
        if Launcher.sshtools.Client.last_try_success:
            text = "Connected"
            self.wConnected.setStyleSheet("")
        else:
            text = "Not connected"
            self.wConnected.setStyleSheet("color: rgb(255,0,0)")
        self.wConnected.setText(text)

    def wAutomaticRequests_on_toggled(self):
        with LogItem("Signal received: {}()".format(sys._getframe().f_code.co_name)):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            manual = not self.wAutomaticRequests.isChecked()
            self.wRequestNodes.setEnabled(manual)
            self.wRequestCores.setEnabled(manual)
            
    def wNNodes_on_valueChanged(self):
        with LogItem("Signal received: {}()".format(sys._getframe().f_code.co_name)):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            if self.wAutomaticRequests.isChecked():
                self.wRequestNodes_on_clicked()
            
    def wNCoresPerNode_on_valueChanged(self):
        with LogItem("Signal received: {}()".format(sys._getframe().f_code.co_name)):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            if self.wAutomaticRequests.isChecked():
                self.wRequestNodes_on_clicked()
            
    def wRequestNodes_on_clicked(self):
        with LogItem("Signal received: {}()".format(sys._getframe().f_code.co_name)):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            n_cores,gb_per_core = self.launcher.nodeset.\
                request_nodes_cores(self.wNNodes.value(),self.wNCoresPerNode.value())
            
            with IgnoreSignals(self):
                self.wNCores   .setValue(n_cores)
                self.wGbPerCore.setValue(gb_per_core)
                self.wGbTotal  .setText (str(round(gb_per_core*n_cores)))
#             self.connections.send_all('nodes_ppn')
        
    def wNCores_on_valueChanged(self):
        with LogItem("Signal received: {}()".format(sys._getframe().f_code.co_name)):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            if self.wAutomaticRequests.isChecked():
                self.wRequestCores_on_clicked()
            
    def wGbPerCore_on_valueChanged(self):
        with LogItem("Signal received: {}()".format(sys._getframe().f_code.co_name)):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            if self.wAutomaticRequests.isChecked():
                self.wRequestCores_on_clicked()
    
    def wWalltime_on_valueChanged(self):
        with LogItem("Signal received: {}()".format(sys._getframe().f_code.co_name)):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            seconds = self.wWalltime.value() * Launcher.walltime_units[self.wWalltimeUnit.currentText()]
            print("    self.wWalltime_seconds.setValue({})".format(seconds))
            self.wWalltimeSeconds.setValue(seconds)
        
    def wWalltimeUnit_on_currentIndexChanged(self):
        with LogItem("Signal received: {}()".format(sys._getframe().f_code.co_name)):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            walltime = self.wWalltimeSeconds.value()
            unit = Launcher.walltime_units[self.wWalltimeUnit.currentText()]
            walltime/= float(unit)
            self.wWalltime_adapt_to_unit()
            self.wWalltime.setValue(walltime)
    
    def wRequestCores_on_clicked(self):
        with LogItem("Signal received: {}()".format(sys._getframe().f_code.co_name)):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            n_nodes, n_cores, n_cores_per_node, gb_per_core, gb = self.launcher.nodeset. \
                request_cores_memory(self.wNCores.value(),self.wGbPerCore.value())
                                  
            with IgnoreSignals(self):
                self.wNNodes       .setValue(n_nodes)
                self.wNCores       .setValue(n_cores)
                self.wNCoresPerNode.setValue(n_cores_per_node)
                self.wGbPerCore    .setValue(gb_per_core)
                self.wGbTotal      .setText (str(round(gb,3)))

#     def update_clusters(self,check_for_new = False):
#         if check_for_new:
#             clusters.import_clusters()
#         self.wCluster.clear()
#         self.wCluster.addItems(Launcher.clusters.cluster_names)
#         self.update_cluster()

    def wCluster_on_currentIndexChanged(self):
        with LogItem("Signal received: {}()".format(sys._getframe().f_code.co_name)):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            self.launcher.change_cluster(value=self.wCluster.currentText(),update=True)
            with IgnoreSignals(self):
                self.cws_lists.c2w('cluster')
                self.cws_lists.c2w('resources')
                self.cws_lists.c2w('walltime')
            
    def wNodeset_on_currentIndexChanged(self,arg=None):
        with LogItem("Signal received: wNodeset_on_currentIndexChanged()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            self.launcher.change_nodeset(value=self.wNodeset.currentText(),update=True)
            with IgnoreSignals(self):
                self.cws_lists.c2w('cluster')
                self.cws_lists.c2w('resources')
                self.cws_lists.c2w('walltime')
            
        
    def change_subfolder(self,subfolder):
        if subfolder:
            subfolder = Launcher.add_trailing_separator(subfolder)
        self.launcher   .set_subfolder(subfolder)
        self.wSubfolder .setText      (subfolder)
        self.wSubfolder2.setText      (Launcher.linux(subfolder))
        
    def change_jobname(self,jobname):
        self.launcher .jobname = jobname
        self.wJobname .setText  (jobname)
        self.wJobname2.setText  (jobname)
    
    def wNotifyAddress_on_returnPressed(self):
        with LogItem("Signal received: wNotifyAddress_on_returnPressed()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            address = Launcher.is_valid_mail_address(self.wNotifyAddress.text())
            if address:
                self.wNotifyAddress.setStyleSheet('color: rgb(0,0,0)')
                self.launcher.set_notify_M(address)
#                 self.connections.send_all('notify')             
            else:
                self.wNotifyAddress.setStyleSheet('color: rgb(255,0,0)')
                QMessageBox.warning( self
                                    ,'Invalid e-mail address'
                                    ,'Please enter a valid e-mail address.'
                                    , buttons=QMessageBox.Ok
                                    )
            
    def wUsername_on_returnPressed(self): 
        with LogItem("Signal received: wUsername_on_returnPressed()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            username = self.wUsername.text()
            if Launcher.sshtools.is_valid_username(username):
                self.wUsername.setStyleSheet('color: rgb(0,0,0)') 
                self.launcher.set_username(username)
                self.show_ssh_available(retry=True)
            else:             
                self.wUsername.setStyleSheet('color: rgb(255,0,0)')
                QMessageBox.warning( self
                                    ,'Invalid username'
                                    ,'Please enter a valid username : vscXXXXX.'
                                    , buttons=QMessageBox.Ok
                                    )
            
    def wChooseLocal_on_clicked(self):
        with LogItem("Signal received: wChooseLocal_on_clicked()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            local_file_location = self.launcher.get_local_file_location()
            local_file_location = QFileDialog.getExistingDirectory( caption='Choose Local file location'
                                                                  , directory=local_file_location, options=QFileDialog.ShowDirsOnly)
            if local_file_location:
                local_file_location = Launcher.add_trailing_separator(local_file_location)
                self.wLocal.setText(local_file_location)
                self.launcher.set_local_file_location(local_file_location)
        
    def wChooseSubfolder_on_clicked(self):
        with LogItem("Signal received: wChooseSubfolder_on_clicked()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            local_file_location = self.launcher.get_local_file_location()
            subfolder = QFileDialog.getExistingDirectory( caption='Choose Local file location'
                                                        , directory=local_file_location, options=QFileDialog.ShowDirsOnly)
            if not subfolder:
                return
            if not subfolder.startswith(local_file_location):
                QMessageBox.warning( self
                                    ,'Invalid subfolder'
                                    ,'You must select/create a subfolder in the local file location.'
                                    , buttons=QMessageBox.Ok
                                    )
                return
            subfolder = subfolder.replace(local_file_location,'')
            self.change_subfolder(subfolder)
            if self.launcher.jobname:
                #clear the jobname if it does not exist
                jobname_folder = self.launcher.local_job_folder()
                if not os.path.exists(jobname_folder):
                    self.change_jobname('')
    
    def wChooseJobname_on_clicked(self):
        with LogItem("Signal received: wChooseJobname_on_clicked()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            parent_folder = os.path.join(self.wLocal.text(),self.wSubfolder.text())
            jobname_folder = QFileDialog.getExistingDirectory( caption='Choose jobname (=folder)'
                                                             , directory=parent_folder, options=QFileDialog.ShowDirsOnly)
            if not jobname_folder:
                return
            if not jobname_folder.startswith(parent_folder):
                QMessageBox.warning( self
                                    ,'Invalid jobname'
                                    ,'The selected jobname folder is not contained in the subfolder.'
                                    , buttons=QMessageBox.Ok
                                    )
            else:
                jobname = jobname_folder.replace(parent_folder,'')
                lst = jobname.rsplit(os.sep,1)
                if len(lst)==2:
                    subfolder = os.path.join(self.wSubfolder,lst[0])
                    self.change_subfolder(subfolder)
                    jobname = lst[1]
                self.change_jobname(jobname)
                pbs_sh = os.path.join(jobname_folder,'pbs.sh')
                if os.path.exists(pbs_sh):
                    answer = QMessageBox.question( self
                                                 ,'Job script found'
                                                 ,"The folder :\n  '{}'\ncontains a job script. Do you want to open it?".format(jobname_folder)
                                                 , defaultButton=QMessageBox.Yes
                                                 )
                    if answer == QMessageBox.Yes:
                        if self.launcher.script.unsaved_changes():
                            answer = QMessageBox.question( self
                                                         ,'Unsaved changes to be overwritten.'
                                                         ,"The current job script has unsaved changes. "\
                                                          "These will be lost when opening a new job script. \n\n"\
                                                          "If you want to save the changes, click 'Cancel', "\
                                                          "specify a NEW jobname and click 'Save'. \n\n"\
                                                          "If you want to continue, click 'Open'." 
                                                         , buttons=QMessageBox.Cancel|QMessageBox.Open
                                                         )
                            if answer==QMessageBox.Cancel:
                                return
                        parent_folder = os.path.join(self.wLocal.text(),self.wSubfolder.text())
                        self.load_job(parent_folder,self.wJobname.text())
    
    def wPages_on_page_change(self):
        with LogItem("Signal received: wPages_on_page_change()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return

            if self.wPages.currentWidget()==self.wResourcesPage:
                self.script_to_resources()
                                
            elif self.wPages.currentWidget()==self.wScriptPage:
                self.resources_to_script()
                
            elif self.wPages.currentWidget()==self.wRetrievePage:
                self.wJobStatusButton_on_clicked()
                self.list_submitted_jobs()
                self.list_retrieved_jobs()
            
            #self.previousPage = self.wPages.currentWidget()
            
    def wJobStatusButton_on_clicked(self):
        s = self.launcher.job_status()
        self.wJobStatus.setText(s)
        
    def list_submitted_jobs(self):
        s = self.launcher.list_jobs(self.launcher.get_submitted_jobs())
        self.wSubmitted.setText(s)        

    def list_retrieved_jobs(self):
        s = self.launcher.list_jobs(self.launcher.retrieved_jobs)
        self.wRetrieved.setText(s)        
        
    def script_to_resources(self):
        text = self.wScript.toPlainText()
        self.launcher.script.parse(text)
        self.cws_lists.s2w('*')
    
    def resources_to_script(self):
        self.cws_lists.w2s('*')
        self.wScript.setPlainText(self.launcher.script.get_text())
    
    def wRemote_on_currentIndexChanged(self):
        with LogItem("Signal received: wRemote_on_currentIndexChanged()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            if self.wRemote.currentText()=="$VSC_SCRATCH":
                self.wCopyToVSC_DATA.setEnabled(True)
            else:
                self.wCopyToVSC_DATA.setChecked(False)
                self.wCopyToVSC_DATA.setEnabled(False)
    
    def wSave_on_clicked(self):
        with LogItem("Signal received: wSave_on_clicked()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            filepath = os.path.join(self.wLocal.text(),self.wSubfolder.text())
            self.launcher.save_job(filepath,self.wJobname.text())
            
    def wReload_on_clicked(self):
        with LogItem("Signal received: wReload_on_clicked()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            if self.launcher.unsaved_changes():
                answer = QMessageBox.question( self
                                             ,'Unsaved changes to be overwritten.'
                                             ,"The current job script has unsaved changes. "\
                                              "These will be lost when reloading the job script.\n\n"\
                                              "Do you want to continue?" 
                                             , defaultButton=QMessageBox.Ok
                                             )
                if answer==QMessageBox.Open:
                    parent_folder = os.path.join(self.wLocal.text(),self.wSubfolder.text())
                    self.load_job(parent_folder,self.wJobname.text())
    
    def load_job(self,parent_folder,jobname):
        self.launcher.load_job(parent_folder,jobname)
        self.cws_lists.s2w()
    
    def wSubmit_on_clicked(self):
        with LogItem("Signal received: wSubmit_on_clicked()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            local_parent_folder  = os.path.join(self.wLocal .text(),self.wSubfolder.text())
            remote_parent_folder = os.path.join(self.wRemote.currentText(),self.wSubfolder.text())
            self.launcher.submit_job(local_parent_folder, remote_parent_folder, self.wJobname.text())
    
    def wRetrieveAll_on_clicked(self):
        with LogItem("Signal received: wRetrieveAll_on_clicked()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            if not self.wCopyToDesktop.isChecked() and not self.wCopyToVSC_DATA.isChecked():
                msg = "Nothing to do.\nCheck at least one of the 'Copy to ...' checkboxes to specify a destination for the results."
                QMessageBox.warning( self, '', msg)
    
            self.launcher.retrieve_finished_jobs()
                
    def wRetrieveSelected_on_clicked(self):
        with LogItem("Signal received: wRetrieveSelected_on_clicked()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            if not self.wCopyToDesktop.isChecked() and not self.wCopyToVSC_DATA.isChecked():
                msg = "Nothing to do.\nCheck at least one of the 'Copy to ...' checkboxes to specify a destination for the results."
                QMessageBox.warning( self, '', msg)
            
            job_id = self.wSubmitted.textCursor().selectedText()
            job_data = self.launcher.get_submitted_jobs().get(job_id,None)
            if job_data:
                print('    retrieving job : '+job_id)
                self.launcher.retrieve_finished_job_wrapper(job_id)
                self.list_retrieved_jobs()
                self.list_submitted_jobs()
            else:
                print("    job '{}' not found.".format(job_id))
                msg = "Double click a job id to make a valid selection."
                QMessageBox.warning( self, '', msg)
                
    def wDeleteSelected_on_clicked(self):
        with LogItem("Signal received: wDeleteSelected_on_clicked()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            job_id = self.wSubmitted.textCursor().selectedText()
            job_data = self.launcher.get_submitted_jobs().get(job_id,None)
            if not job_data:
                job_id = self.wRetrieved.textCursor().selectedText()
                job_data = self.launcher.get_submitted_jobs().get(job_id,None)
            if job_data:
                username = job_data['username']
                cluster  = job_data['cluster' ]
                local_folder = None
                remote_folder= None
                data_folder  = None
                if self.wDeleteLocal.isChecked():
                    local_folder = job_data['local folder'  ]
                if self.wDeleteRemote.isChecked():
                    remote_folder = job_data['remote folder']
                    data_folder   = job_data['data folder'  ]
                print('    retrieving job : '+job_id)
                self.launcher.delete_job(username, cluster, local_folder, remote_folder, data_folder)
                self.list_retrieved_jobs()
                self.list_submitted_jobs()
            else:
                print("    job '{}' not found.".format(job_id))
                msg = "Double click a job id to make a valid selection."
                QMessageBox.warning( self, '', msg)
                    
    def wModules_on_currentIndexChanged(self):
        with LogItem("Signal received: wModules_on_currentIndexChanged()"):
            print("    self.ignore_signals = {}".format(self.ignore_signals))
            if self.ignore_signals:
                return
            if not self.wModules.currentText().startswith('#'):
                self.wScript.append("module load "+self.wModules.currentText())
                    
    def set_status_text(self,text,timeout=10000):
        self.statusBar().showMessage(text,msecs=timeout)
    
    def closeEvent(self, *args, **kwargs):
        with LogItem("Closing LauncherPyQt5.MainWindow"):
            self.cws_lists.w2c()
            self.launcher.config.save()
            self.cws_lists.w2s()
            if self.launcher.script.unsaved_changes():
                pass
            print('unsaved changes =',self.launcher.script.unsaved_changes())
            
        return QMainWindow.closeEvent(self, *args, **kwargs)
    
################################################################################
class IgnoreSignals(object):
################################################################################
    def __init__(self,obj):
        self.obj = obj
        self.previous_ignore_signals = obj.ignore_signals
        obj.ignore_signals = True
    def __enter__(self):    
        pass
    def __exit__(self, exc_type, exc_value, traceback):
        self.obj.ignore_signals = self.previous_ignore_signals
        if not exc_type is None:
            log_exception(exc_value)

################################################################################
def start_gui(argv=[]):
    app = QApplication(argv)
    window = MainWindow()
    window.show()
    #window.setGeometry(500, 300, 300, 300)
    return app.exec_()

################################################################################
if __name__=="__main__":
    with LogSession('LauncherPyQt5'):
        start_gui()