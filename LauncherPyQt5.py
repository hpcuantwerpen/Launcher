from __future__ import print_function

import os

from PyQt5.QtCore import QT_VERSION_STR,Qt,QStringListModel
__version__ = "PyQt5 "+QT_VERSION_STR

from PyQt5.QtWidgets import QMainWindow,QTabWidget,QWidget,QLineEdit,QLabel,   \
     QComboBox,QSpinBox,QDoubleSpinBox,QHBoxLayout,QVBoxLayout,QGridLayout,    \
     QFrame,QCheckBox,QPushButton,QGroupBox,QSplitter,QApplication,QMessageBox,\
     QFileDialog,QTextEdit

import Launcher
import constants,log
from DataConnections import DataConnection, add_data_connection
from sshtools import is_valid_username

################################################################################
class MainWindow(QMainWindow):
################################################################################
    def __init__(self):
        super(MainWindow,self).__init__()
        self.launcher = Launcher.Launcher()
        self.ignore_signals = False
        self.createUI()

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
        self.wEnforceNNodes.setChecked(self.launcher.get_enforce_n_nodes())
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
        grid.addWidget(QLabel("Number of cores:"),r,0)
        self.wNCores = QSpinBox()
        grid.addWidget(self.wNCores                        ,r,1)
        r+=1
        grid.addWidget(QLabel("GB per core:")    ,r,0)
        self.wGbPerCore = QDoubleSpinBox()
        self.wGbPerCore.setDecimals(3)
        self.wGbPerCore.setSingleStep(.1)
        grid.addWidget(self.wGbPerCore                     ,r,1)
        r+=1
        label = QLabel("GB in total:")
        label.setEnabled(False)
        grid.addWidget(label                               ,r,0)
        self.wGbTotal = QLineEdit()
        self.wGbTotal.setEnabled(False)
        grid.addWidget(self.wGbTotal                       ,r,1)
        r+=1
        self.wAutomaticRequests = QCheckBox('Automatic requests')
        self.wRequestCores = QPushButton('make request')
        grid.addWidget(self.wAutomaticRequests             ,r,0)
        grid.addWidget(self.wRequestCores                  ,r,1)
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
        self.wWalltime = QSpinBox()
        self.wWalltime  .setRange(1,100)
        vbox_group.addWidget(self.wWalltime)
        self.wWalltimeUnit = QComboBox()
        self.wWalltimeUnit  .addItems(['seconds','minutes','hours','days'])
        self.wWalltimeUnit  .setCurrentText('hours')
        vbox_group.addWidget(self.wWalltimeUnit)
        walltime_group.setLayout(vbox_group)
        hbox_2groups.addWidget(walltime_group)        

        #-----------------------------------------------------------------------
        notify_group = QGroupBox("Nofify:")
        vbox_group = QVBoxLayout()
        vbox_group.setSpacing(2)
        self.wNotifyAddress = QLineEdit()
        notify = self.launcher.get_notify()            
        vbox_group.addWidget(self.wNotifyAddress)
        hbox_group = QHBoxLayout()
        self.wNotifyBegin = QCheckBox("on begin")
        self.wNotifyEnd   = QCheckBox("on end")
        self.wNotifyAbort = QCheckBox("on abort")
        hbox_group.addWidget(self.wNotifyBegin)
        hbox_group.addWidget(self.wNotifyEnd)
        hbox_group.addWidget(self.wNotifyAbort)
        vbox_group.addLayout(hbox_group)
        notify_group.setLayout(vbox_group)
        hbox_2groups.addWidget(notify_group)
        
        if notify['address']:
            self.wNotifyAddress.setText(notify['address'])
        self.wNotifyAbort.setChecked(notify['a'])    
        self.wNotifyBegin.setChecked(notify['b'])    
        self.wNotifyEnd  .setChecked(notify['e'])    
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
        
        self.wUsername.setText(self.launcher.config['username'].value)
        self.is_connected()
        cfg_item = self.launcher.config['local_file_location']
        self.wLocal.setText(Launcher.add_trailing_separator(cfg_item.value))
        cfg_item = self.launcher.config['remote_file_location']
        self.wRemote.addItems(cfg_item.choices)
        self.wRemote.setCurrentText(cfg_item.value)
        self.change_subfolder(self.launcher.config['subfolder'].value)
         
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
        
        self.wSave        = QPushButton("Save")
        self.wReload      = QPushButton("Reload")
        self.wSubmit      = QPushButton("Submit")
        self.wRetrieveAll = QPushButton("Retrieve results")
        self.wRetrieveCopyToLocal = QCheckBox("Copy results to local file location.")
        self.wRetrieveCopyToData  = QCheckBox("Copy results to $VSC_DATA.")
        
        self.wRetrieveCopyToLocal.setChecked(self.launcher.config['retrieve_copy_to_local_file_location'].value)
        self.wRetrieveCopyToData .setChecked(self.launcher.config['retrieve_copy_to_$VSC_DATA'          ].value)
        grid = QGridLayout()
        grid.setVerticalSpacing(5)
        grid.addWidget(self.wSave               ,0,0)
        grid.addWidget(self.wReload             ,1,0)
        grid.addWidget(self.wSubmit             ,2,0)
        grid.addWidget(self.wRetrieveAll        ,0,1)
        grid.addWidget(self.wRetrieveCopyToLocal,1,1)
        grid.addWidget(self.wRetrieveCopyToData ,2,1)
        vbox_group.addLayout(grid)
        #-----------------------------------------------------------------------        
        job_group.setLayout(vbox_group)
        vbox_page.addWidget(job_group)
        vbox_page.addStretch()
        self.wResourcesPage.setLayout(vbox_page)

        self.update_clusters()
        #=======================================================================
        # Script page
        self.wScriptPage = QWidget()
        vbox_page = QVBoxLayout()
        #add resource page widgets
        #-----------------------------------------------------------------------
        self.wScript = QTextEdit(self)
        vbox_page.addWidget(self.wScript)
        #-----------------------------------------------------------------------
        self.wScriptPage.setLayout(vbox_page)
        #=======================================================================
        # Retrieve page
        self.wRetrievePage = QWidget()
        #add restrieve page widgets
        
        #=======================================================================
        self.wPages.addTab(self.wResourcesPage, "Resources")
        self.wPages.addTab(self.wScriptPage   , "Script")
        self.wPages.addTab(self.wRetrievePage , "Retrieve")
        #=======================================================================
        self.setCentralWidget(self.wPages)
        
        #=======================================================================
        # data connections
        #=======================================================================
        add_data_connection(self.wNNodes       , self.launcher,'n_nodes_req'         ,'request_resources')
        add_data_connection(self.wNCoresPerNode, self.launcher,'n_cores_per_node_req','request_resources')
        add_data_connection(self.wNCores       , self.launcher,'n_cores_req'         ,'request_resources')
        add_data_connection(self.wGbPerCore    , self.launcher,'gb_per_core_req'     ,'request_resources')
        
        add_data_connection(self.wCluster      , self.launcher,'cluster'            ,'*')
        
        #=======================================================================
        # signal connections
        #=======================================================================
        self.wCluster           .currentIndexChanged .connect(self.update_cluster)
        self.wNodeset           .currentIndexChanged .connect(self.update_nodeset)
        self.wRequestNodes      .clicked             .connect(self.wRequestNodes_on_clicked)
        self.wRequestCores      .clicked             .connect(self.wRequestCores_on_clicked)
        self.wAutomaticRequests .toggled             .connect(self.wAutomaticRequests_on_toggled)
        self.wNNodes            .valueChanged        .connect(self.wNNodes_on_valueChanged)
        self.wNCoresPerNode     .valueChanged        .connect(self.wNCoresPerNode_on_valueChanged)
        self.wNCores            .valueChanged        .connect(self.wNCores_on_valueChanged)
        self.wGbPerCore         .valueChanged        .connect(self.wGbPerCore_on_valueChanged)
        self.wNotifyAddress     .returnPressed       .connect(self.notify_changed)
        self.wNotifyAbort       .toggled             .connect(self.notify_changed)
        self.wNotifyBegin       .toggled             .connect(self.notify_changed)
        self.wNotifyEnd         .toggled             .connect(self.notify_changed)
        self.wUsername          .returnPressed       .connect(self.wUsername_on_returnPressed)
        self.wChooseLocal       .clicked             .connect(self.wChooseLocal_on_clicked)
        self.wChooseSubfolder   .clicked             .connect(self.wChooseSubfolder_on_clicked)
        self.wChooseJobname     .clicked             .connect(self.wChooseJobname_on_clicked)
        self.wPages             .currentChanged      .connect(self.wPages_on_page_change)
        self.wRemote            .currentIndexChanged .connect(self.wRemote_currentIndexChanged)
        self.wSave              .clicked             .connect(self.wSave_on_clicked)
        self.wReload            .clicked             .connect(self.wReload_on_clicked)
        self.wSubmit            .clicked             .connect(self.wSubmit_on_clicked)
        
        #=======================================================================
        # initialiazation of values that trigger side effect
        #=======================================================================
        self.wAutomaticRequests.setChecked(self.launcher.get_automatic_requests())
       
        #=======================================================================
        # menu
        #=======================================================================
#         menu = self.menuBar().addMenu('File')
#         action = menu.addAction('Change File Path')
#         action.triggered.connect(self.changeFilePath)   

    def is_connected(self):
        if Launcher.sshtools.Client.last_try_success:
            text = "Connected"
            self.wConnected.setStyleSheet("")
        else:
            text = "Not connected"
            self.wConnected.setStyleSheet("color: rgb(255,0,0)")
        self.wConnected.setText(text)

    def wAutomaticRequests_on_toggled(self):
        if self.ignore_signals:
            return
        manual = not self.wAutomaticRequests.isChecked()
        self.wRequestNodes.setEnabled(manual)
        self.wRequestCores.setEnabled(manual)
            
    def wNNodes_on_valueChanged(self):
        if self.ignore_signals:
            return
        if self.wAutomaticRequests.isChecked():
            self.wRequestNodes_on_clicked()
            
    def wNCoresPerNode_on_valueChanged(self):
        if self.ignore_signals:
            return
        if self.wAutomaticRequests.isChecked():
            self.wRequestNodes_on_clicked()
            
    def wRequestNodes_on_clicked(self):
        if self.ignore_signals:
            return
        clist = 'request_resources'
        DataConnection.send_all(clist)
        try:
            self.launcher.request_nodes_and_cores_per_node()
        except Exception as e:
            log.log_exception(e)
            DataConnection.undo_all(clist)
        else:
            self.ignore_signals = True
            try:
                DataConnection.receive_all(clist)
                self.wGbTotal.setText(str(self.launcher.get_gb_total_granted()))
            except Exception as e:
                log.log_exception(e)
            self.ignore_signals = False
    
    def wNCores_on_valueChanged(self):
        if self.ignore_signals:
            return
        if self.wAutomaticRequests.isChecked():
            self.wRequestCores_on_clicked()
            
    def wGbPerCore_on_valueChanged(self):
        if self.ignore_signals:
            return
        if self.wAutomaticRequests.isChecked():
            self.wRequestCores_on_clicked()
            
    def wRequestCores_on_clicked(self):
        if self.ignore_signals:
            return
        clist = 'request_resources'
        DataConnection.send_all(clist)
        try:
            self.launcher.request_cores_and_memory_per_core()
        except Exception as e:
            log.log_exception(e)
            DataConnection.undo_all(clist)
        else:
            self.ignore_signals = True
            try:
                DataConnection.receive_all(clist)
                self.wGbTotal.setText(str(self.launcher.get_gb_total_granted()))
            except Exception as e:
                log.log_exception(e,msg_after="This is suspicious...")
            self.ignore_signals = False

    def update_clusters(self,check_for_new = False):
        """"""
        if check_for_new:
            Launcher.clusters.import_clusters()
        self.wCluster.clear()
        self.wCluster.addItems(Launcher.clusters.cluster_names)
        self.update_cluster()

    def update_cluster(self,arg=None):
        if arg is None:
            cluster = self.launcher.get_cluster()            
            self.wCluster.setCurrentText(cluster)
        else:
            self.launcher.change_cluster(arg)
            cluster = self.wCluster.currentText()
        self.wNodeset.clear()  
        self.wNodeset.addItems(Launcher.clusters.decorated_nodeset_names(cluster))
        self.update_nodeset()
        
    def update_nodeset(self,arg=None):
        if arg is None:
            arg = self.wNodeset.currentText()
        else:
            if isinstance(arg,int):
                arg = self.launcher.nodeset_names[arg]
        self.launcher.change_selected_nodeset_name(arg)
        
        self.wNNodes       .setRange(1,self.launcher.n_nodes_max())
        self.wNCoresPerNode.setRange(1,self.launcher.n_cores_per_node_max)
        self.wNCores       .setRange(1,self.launcher.n_nodes_max()*self.launcher.n_cores_per_node_max)
        self.wGbPerCore    .setRange(0,self.launcher.gb_per_core_max())
        with IgnoreSignals(self):
            self.wNCoresPerNode.setValue(self.launcher.n_cores_per_node_req)
            self.wNCores       .setValue(self.launcher.n_cores_per_node_req)
            self.wGbPerCore    .setValue(self.launcher.selected_nodeset.gb_per_node/self.launcher.n_cores_per_node_req)
            self.wGbPerNode .setText(str(self.launcher.selected_nodeset.gb_per_node))
            self.wGbTotal   .setText(str(self.launcher.selected_nodeset.gb_per_node*self.launcher.n_nodes_req))
        
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
            
    def notify_changed(self):
        notify = self.launcher.get_notify()
        notify['a'] = self.wNotifyAbort.isChecked()
        notify['b'] = self.wNotifyBegin.isChecked()
        notify['e'] = self.wNotifyEnd  .isChecked()
        address = Launcher.is_valid_mail_address(self.wNotifyAddress.text())
        if address:
            self.wNotifyAddress.setStyleSheet('color: rgb(0,0,0)')
            notify['address'] = address             
        else:
            self.wNotifyAddress.setStyleSheet('color: rgb(255,0,0)')
            QMessageBox.warning( self
                                ,'Invalid e-mail address'
                                ,'Please enter a valid e-mail address.'
                                , buttons=QMessageBox.Ok
                                )

    def wUsername_on_returnPressed(self): 
        username = self.wUsername.text()
        if is_valid_username(username):
            self.wUsername.setStyleSheet('color: rgb(0,0,0)') 
            self.launcher.set_username(username)
        else:             
            self.wUsername.setStyleSheet('color: rgb(255,0,0)')
            QMessageBox.warning( self
                                ,'Invalid username'
                                ,'Please enter a valid username : vscXXXXX.'
                                , buttons=QMessageBox.Ok
                                )
            
    def wChooseLocal_on_clicked(self):
        local_file_location = self.launcher.get_local_file_location()
        local_file_location = QFileDialog.getExistingDirectory(caption='Choose Local file location', directory=local_file_location, options=QFileDialog.ShowDirsOnly)
        if local_file_location:
            local_file_location = Launcher.add_trailing_separator(local_file_location)
            self.wLocal.setText(local_file_location)
            self.launcher.set_local_file_location(local_file_location)
        
    def wChooseSubfolder_on_clicked(self):
        local_file_location = self.launcher.get_local_file_location()
        subfolder = QFileDialog.getExistingDirectory(caption='Choose Local file location', directory=local_file_location, options=QFileDialog.ShowDirsOnly)
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
        parent_folder = self.launcher.get_subfolder()
        if parent_folder:
            parent_folder = os.path.join(self.launcher.get_local_file_location(),parent_folder)
        else: #no subfolder
            parent_folder = self.launcher.get_local_file_location()
        jobname_folder = QFileDialog.getExistingDirectory(caption='Choose jobname (=folder)', directory=parent_folder, options=QFileDialog.ShowDirsOnly)
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
                self.change_subfolder(os.path.join(self.launcher.get_subfolder(),lst[0]))
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
                    if self.launcher.unsaved_changes():
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
                    self.launcher.load_job()
    
    def wPages_on_page_change(self):
        if self.wPages.currentWidget()==self.wResourcesPage:
            text = self.wScript.toPlainText()
            self.launcher.script.parse(text)
            self.launcher.update_resources_from_script()
            DataConnection.receive_all('request_resources')
            DataConnection.receive_all('*')
            
        elif self.wPages.currentWidget()==self.wScriptPage:
            self.launcher.update_script_from_resources()
            self.wScript.setPlainText(self.launcher.script.get_text())
        elif self.wPages.currentWidget()==self.wRetrievePage:
            print("retrieve")
    
    def wRemote_currentIndexChanged(self):
        if self.wRemote.currentText()=="$VSC_SCRATCH":
            self.wRetrieveCopyToData.setEnabled(True)
        else:
            self.wRetrieveCopyToData.setChecked(False)
            self.wRetrieveCopyToData.setEnabled(False)
    
    def wSave_on_clicked(self):
        self.launcher.save_job()
        
    def wReload_on_clicked(self):
        if self.launcher.unsaved_changes():
            answer = QMessageBox.question( self
                                         ,'Unsaved changes to be overwritten.'
                                         ,"The current job script has unsaved changes. "\
                                          "These will be lost when reloading the job script.\n\n"\
                                          "Do you want to continue?" 
                                         , defaultButton=QMessageBox.Ok
                                         )
            if answer==QMessageBox.Open:
                self.launcher.load_job()
    
    def wSubmit_on_clicked(self):
        pass
    
    def closeEvent(self, *args, **kwargs):
        self.launcher.config.save()
        return QMainWindow.closeEvent(self, *args, **kwargs)
    
################################################################################
class IgnoreSignals(object):
################################################################################
    def __init__(self,obj):
        self.obj = obj
        obj.ignore_signals = True
    def __enter__(self):    
        pass
    def __exit__(self, exc_type, exc_value, traceback):
        self.obj.ignore_signals = False
        if not exc_type is None:
            log.log_exception(exc_value)

################################################################################
def start_gui(argv=[]):
    app = QApplication(argv)
    window = MainWindow()
    window.show()
    #window.setGeometry(500, 300, 300, 300)
    return app.exec_()

################################################################################
if __name__=="__main__":
    with log.LogSession('LauncherPyQt5'):
        start_gui()