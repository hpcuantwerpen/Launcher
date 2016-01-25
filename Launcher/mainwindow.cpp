#define QT_DEBUG
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QProcess>

#include <warn_.h>
//#include <ssh2tools.h>
#include <cmath>
#include <time.h>

#define NEWJOB "__new_job__"
#define STAY_IN_LOCAL_FILE_LOCATION
#define OFFLINE "Offline"
#define INITIAL_VERBOSITY 2
#define NO_ARGUMENT QString()

//#define     OPEN_LOG_ITEM  "\n[[[ "
//#define    CLOSE_LOG_ITEM "\n]]]\n"
//#define NO_CLOSE_LOG_ITEM ""
//#define ARG(A) QString("\n      path; '%1'").arg(A).toStdString()

//#define LOG_AND_CHECK_IGNORESIGNAL( ARGUMENT, CLOSE ) \
//  {\
//    QString msg("\n[[[ [ MainWindow::%1, %2, %3 ]");\
//    msg = msg.arg(__func__).arg(__FILE__).arg(__LINE__);\
//    if( this->verbosity_ && !QString((ARGUMENT)).isEmpty() ) \
//        msg.append("\n    Function was called with argument : ").append(ARGUMENT);\
//    if( !this->ignoreSignals_ ) {\
//        msg.append( this->check_script_unsaved_changes() ) ;\
//    }\
//    if( this->verbosity_ > 1 ) {\
//        if( this->ignoreSignals_ ) msg.append("\n    Ignoring signal");\
//        else                       msg.append("\n    Executing signal");\
//    }\
//    msg.append( CLOSE );\
//    this->log( msg.toStdString() );\
//    if( this->ignoreSignals_ ) return;\
//  }

#define CHECK_IGNORESIGNAL                      \
    if( this->ignoreSignals_ ) {                \
        this->log_ << "\n    Ignoring signal";  \
        return;                                 \
    } else {                                    \
        this->log_ << "\n    Executing signal"; \
    }

 //-----------------------------------------------------------------------------
#define CALLEE( ARGUMENT ) \
    QString("MainWindow::%1, argument=%2, %3, %4").arg(__func__).arg(ARGUMENT).arg(__FILE__).arg(__LINE__)
#define CALLEE0 \
    QString("MainWindow::%1, %2, %3").arg(__func__).arg(__FILE__).arg(__LINE__)
#define BOOL2STRING(b) (b?"true":"false")

//#define LOG_CALLER(VERBOSITY) \
//    QString caller = QString("\n    [ MainWindow::%1, %2, %3 ]") \
//                     .arg(__func__).arg(__FILE__).arg(__LINE__);\
//    this->this->log( std::ostringstream() << INFO << caller.C_STR(), VERBOSITY);

// LOG_CALLER_INFO has no terminating ';' so we can do
//     LOG_CALLER_INFO << whatever << and << even << more;
//-----------------------------------------------------------------------------
    QString validateEmailAddress( QString const & address )
    {
        QString result = address.toLower();
        QRegularExpressionMatch m;
        QRegularExpression re("^[a-z0-9._%+-]+@[a-z0-9.-]+\\.[a-z]{2,4}$");
        m = re.match( address );
        if( !m.hasMatch() ) {
            result.clear();
        }
        return result;
    }
 //-----------------------------------------------------------------------------
    QString validateUsername( QString const & username )
    {
        QString result = username .toLower();
        QRegularExpressionMatch m;
        QRegularExpression re("^vsc\\d{5}$");
        m = re.match( username );
        if( !m.hasMatch() ) {
            result.clear();
        }
        return result;
    }
 //-----------------------------------------------------------------------------
    class IgnoreSignals
    {
    public:
        IgnoreSignals( MainWindow* mainWindow )
          : previousValue_( mainWindow->getIgnoreSignals() )
          , mainWindow_(mainWindow)
        {
            mainWindow->setIgnoreSignals();
        }
        ~IgnoreSignals() {
            mainWindow_->setIgnoreSignals(this->previousValue_);
        }
    private:
        bool previousValue_;
        MainWindow* mainWindow_;
    };
    #define IGNORE_SIGNALS_UNTIL_END_OF_SCOPE IgnoreSignals ignoreSignals(this)
 //-----------------------------------------------------------------------------
#define TITLE "Launcher"

    enum {
        MainWindowUnderConstruction = -2
      , MainWindowConstructedButNoPreviousPage = -1
    };

    QString short_version(int level=2)
    {
        QString version(VERSION);
        QRegularExpressionMatch m;
        QRegularExpression re("^(\\d+)\\.(\\d+)\\.(\\d+)-(\\d+)-([a-z0-9]+)$");

        m = re.match(version);
        if( m.hasMatch() ) {
            if( level<2 ) level=2;
            if( level>5 ) level=5;
            QString short_version;
            switch(level) {
            case 5:
                short_version = m.captured(0);
                break;
            case 4:
                short_version.prepend(m.captured(4)).prepend('-');
            case 3:
                short_version.prepend(m.captured(3)).prepend('.');
            case 2:
                short_version.prepend(m.captured(2)).prepend('.')
                             .prepend(m.captured(1));
            }
            return short_version;
        } else {
            return QString(VERSION);
        }
    }

 //-----------------------------------------------------------------------------
 // MainWindow::
 //-----------------------------------------------------------------------------
    MainWindow::MainWindow
      (toolbox::Log & log
      , QWidget     * parent
      )
      : QMainWindow(parent)
      , log_(log)
      , ui(new Ui::MainWindow)
      , ignoreSignals_(false)
      , current_page_(MainWindowUnderConstruction)
      , ssh( &log )
      , verbosity_(INITIAL_VERBOSITY)
      , walltime_(nullptr)
      , file_system_model( nullptr )
      , pendingRequest_(NoPendingRequest)
      , proceed_offline_(false)
    {
        this->log_call( 0, CALLEE0, QString("\n    Running Launcher version = ").append(VERSION) ) ;


        ui->setupUi(this);

        createActions();
        createMenus();
//        this->useGitSynchronizationAction_triggered();
     // so far there is only one implementation, so we don't need the line above.
        this->ssh.set_impl(true);

//        ssh::Libssh2Impl::autoOpen = true;

        this->paletteRedForeground_ = new QPalette();
        this->paletteRedForeground_->setColor(QPalette::Text,Qt::red);

        if( !QFile(this->launcher_.homePath("Launcher.cfg")).exists() )
        {// This must be the first time after installation...
            this->setupHome();
        }
        this->update_WindowTitle();

     // permanent widgets on statusbar
        this->wAuthenticationStatus_ = new QLabel();
        this->wJobnameIndicator_ = new QLabel();
        this->wProjectIndicator_ = new QLabel();
        this->statusBar()->addPermanentWidget(this->wProjectIndicator_);
        this->statusBar()->addPermanentWidget(this->wJobnameIndicator_);
        this->statusBar()->addPermanentWidget(this->wAuthenticationStatus_);
        this->wAuthenticationStatus_->setToolTip("Authentication status");
        this->wJobnameIndicator_->setToolTip("Job folder and job name, surrounded by asterisks in case of unsaved changes");
        this->wProjectIndicator_->setToolTip("Project folder");

        qsrand( time(nullptr) );

     // read the cfg::Config object from file
     // (or create an empty one if there is no config file yet)
        QString path_to_session_data = this->load_session_path();
        if( path_to_session_data.isEmpty() || !QFileInfo(path_to_session_data).exists() ) {
            this->new_session( this->launcher_.homePath("session.data"));
        } else {
            this->open_session( path_to_session_data );
        }

#if defined(QT_DEBUG) || defined(BETA_RELEASE)
        this->verbosity_= 2;
#endif

#ifdef Q_OS_WIN
        QFont font("Courier New",7);

        this->ui->wFinished   ->setFont(font);
//        this->ui->wNotFinished->setFont(font);
//        this->ui->wRetrieved  ->setFont(font);
#endif
     // keep this the last line
        current_page_ = MainWindowConstructedButNoPreviousPage;
    }

    void MainWindow::log_call
      ( int            verbosity_level
      , QString const& callee
      , QString const& msg
      )
    {
        QString s = QString("\n\n*** [ %1 ]").arg(callee).append(msg);
        if( this->current_page_!=MainWindowUnderConstruction
         && !this->ignoreSignals_
         && this->log_.verbosity()>1
          )
        {
            s.append( this->check_script_unsaved_changes() ) ;
        }
        std::ostream* po = this->log_.ostream(verbosity_level);
        *po << s.toStdString() << std::flush;
        this->log_.cleanup();
    }

    void MainWindow::createActions()
    {
        this->log_call( 1, CALLEE0 );

        aboutAction_ = new QAction("&About", this);
        connect( aboutAction_, SIGNAL(triggered()), this, SLOT( aboutAction_triggered() ) );

        verboseAction_ = new QAction("Verbose logging", this);
        verboseAction_->setCheckable(true);
        connect( verboseAction_, SIGNAL(triggered()), this, SLOT( verboseAction_triggered() ) );

        openSessionAction_ = new QAction("&Open...", this);
        openSessionAction_ ->setShortcut(QKeySequence("SHIFT+CTRL+O"));
        connect( openSessionAction_, SIGNAL(triggered()), this, SLOT( openSessionAction_triggered() ) );

        newSessionAction_ = new QAction("New", this);
        newSessionAction_ ->setShortcut(QKeySequence("SHIFT+CTRL+N"));
        connect( newSessionAction_, SIGNAL(triggered()), this, SLOT( newSessionAction_triggered() ) );

        saveSessionAction_ = new QAction("Save", this);
        saveSessionAction_ ->setShortcut(QKeySequence("SHIFT+CTRL+S"));
        connect( saveSessionAction_, SIGNAL(triggered()), this, SLOT( saveSessionAction_triggered() ) );

        saveAsSessionAction_ = new QAction("Save As...", this);
        connect( saveAsSessionAction_, SIGNAL(triggered()), this, SLOT( saveAsSessionAction_triggered() ) );

        authenticateAction_ = new QAction("Authenticate...", this);
        authenticateAction_ ->setShortcut(QKeySequence("SHIFT+CTRL+L"));
        connect( authenticateAction_, SIGNAL(triggered()), this, SLOT( authenticateAction_triggered() ) );

        localFileLocationAction_ = new QAction("Change local file location...", this);
        connect( localFileLocationAction_, SIGNAL(triggered()), this, SLOT( localFileLocationAction_triggered() ) );

        remoteFileLocationAction_ = new QAction("Change remote file location...", this);
        connect( remoteFileLocationAction_, SIGNAL(triggered()), this, SLOT( remoteFileLocationAction_triggered() ) );

        createTemplateAction_ = new QAction("Create a job template...", this);
        connect( createTemplateAction_, SIGNAL(triggered()), this, SLOT( createTemplateAction_triggered() ) );

        selectTemplateAction_ = new QAction("Select a job template...", this);
        connect( selectTemplateAction_, SIGNAL(triggered()), this, SLOT( selectTemplateAction_triggered() ) );

        newJobscriptAction_ = new QAction("&New", this);
        newJobscriptAction_ ->setShortcut(QKeySequence::New);
        connect( newJobscriptAction_, SIGNAL(triggered()), this, SLOT( newJobscriptAction_triggered() ) );

        saveJobscriptAction_ = new QAction("&Save", this);
        saveJobscriptAction_ ->setShortcut(QKeySequence::Save);
        connect( saveJobscriptAction_, SIGNAL(triggered()), this, SLOT( saveJobscriptAction_triggered() ) );

        saveAsJobscriptAction_ = new QAction("Save as...", this);
        connect( saveAsJobscriptAction_, SIGNAL(triggered()), this, SLOT( saveAsJobscriptAction_triggered() ) );

        openJobscriptAction_ = new QAction("Open...", this);
        openJobscriptAction_ ->setShortcut(QKeySequence::Open);
        connect( openJobscriptAction_, SIGNAL(triggered()), this, SLOT( openJobscriptAction_triggered() ) );

        reloadJobscriptAction_ = new QAction("reload", this);
        connect( reloadJobscriptAction_, SIGNAL(triggered()), this, SLOT( reloadJobscriptAction_triggered() ) );

        submitJobscriptAction_ = new QAction("Submit", this);
        submitJobscriptAction_ ->setShortcut(QKeySequence("CTRL+R"));
        connect( submitJobscriptAction_, SIGNAL(triggered()), this, SLOT( submitJobscriptAction_triggered() ) );

        showFileLocationsAction_ = new QAction("Show file locations...", this);
        connect( showFileLocationsAction_, SIGNAL(triggered()), this, SLOT( showFileLocationsAction_triggered() ) );

        showLocalJobFolderAction_ = new QAction("Show local jobfolder contents...", this);
        connect( showLocalJobFolderAction_, SIGNAL(triggered()), this, SLOT( showLocalJobFolderAction_triggered() ) );

        showRemoteJobFolderAction_ = new QAction("Show remote job folder contents...", this);
        connect( showRemoteJobFolderAction_, SIGNAL(triggered()), this, SLOT( showRemoteJobFolderAction_triggered() ) );

        useGitSynchronizationAction_ = new QAction("Use git synchronization",this);
        useGitSynchronizationAction_->setCheckable(true);
        useGitSynchronizationAction_->setChecked  (true);
        connect( useGitSynchronizationAction_, SIGNAL(triggered()), this, SLOT( useGitSynchronizationAction_triggered() ) );

        removeRepoAction_ = new QAction("remove jobfolder repository ...", this );
        connect( removeRepoAction_, SIGNAL(triggered()), this, SLOT( removeRepoAction_triggered() ) );

        createRepoAction_ = new QAction("create jobfolder repository ...", this );
        connect( createRepoAction_, SIGNAL(triggered()), this, SLOT( createRepoAction_triggered() ) );
    }

    void MainWindow::createMenus()
    {
        this->log_call( 1, CALLEE0 );

        helpMenu_ = menuBar()->addMenu("&Help");
        helpMenu_->addAction(aboutAction_);

        jobMenu_ = menuBar()->addMenu("Job script");
        jobMenu_ ->addAction(newJobscriptAction_);
        jobMenu_ ->addAction(openJobscriptAction_);
        jobMenu_ ->addAction(saveJobscriptAction_);
        jobMenu_ ->addAction(saveAsJobscriptAction_);
        jobMenu_ ->addAction(reloadJobscriptAction_);
        jobMenu_ ->addAction(submitJobscriptAction_);
        jobMenu_ ->addAction(showFileLocationsAction_);
//        jobMenu_ ->addAction(showLocalJobFolderAction_);
//        jobMenu_ ->addAction(showRemoteJobFolderAction_);
        jobMenu_ ->addAction(removeRepoAction_);
        jobMenu_ ->addAction(createRepoAction_);

        templatesMenu_ = menuBar()->addMenu("templates");
         // todo : test these
//        templatesMenu_ ->addAction(selectTemplateAction_);
//        templatesMenu_ ->addAction(createTemplateAction_);

        sessionMenu_ = menuBar()->addMenu("&Session");
         // todo : test these
//        sessionMenu_ ->addAction( newSessionAction_);
//        sessionMenu_ ->addAction(openSessionAction_);
//        sessionMenu_ ->addAction(saveSessionAction_);
        sessionMenu_ ->addAction(authenticateAction_);
        sessionMenu_ ->addAction(localFileLocationAction_);
        sessionMenu_ ->addAction(remoteFileLocationAction_);

        extraMenu_ = menuBar()->addMenu("&Extra");
        extraMenu_ ->addAction(verboseAction_);
        extraMenu_ ->addAction(useGitSynchronizationAction_);
    }

    void MainWindow::aboutAction_triggered()
    {
        this->log_call( 1,CALLEE0);

        QString msg;
        msg.append( QString("program : %1\n").arg(TITLE  ) )
           .append( QString("version : %1\n").arg(VERSION) )
           .append(         "contact : engelbert.tijskens@uantwerpen.be")
           ;
        QMessageBox::about(this,TITLE,msg);
    }

    void MainWindow::verboseAction_triggered()
    {
        this->log_call( 1,CALLEE0);
        this->verbosity_ = ( this->verboseAction_->isChecked() ? 2 : 0 );
    }

    void MainWindow::useGitSynchronizationAction_triggered()
    {
//        LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);
//        if( this->useGitSynchronizationAction_->isChecked() ) {
//            this->ssh.set_impl(true);
//        }
    }

    void MainWindow::newSessionAction_triggered()
    {
        this->log_call( 1,CALLEE0);

        QString session_data = this->launcher_.homePath("");
        session_data = QFileDialog::getSaveFileName( this, "Enter a new filename or select a .data file for the new session"
                                                  , session_data
                                                  ,"Launcher sessions (*.data)"
                                                  , nullptr
                                                  , QFileDialog::DontConfirmOverwrite
                                                  );

        if( !session_data.isEmpty() ) {
            if( !session_data.endsWith(".data") ) {
                session_data.append(".data");
            }
            this->log_ << ( QString("\n    Ready to open new session: ").append(session_data).toStdString() );
            this->new_session(session_data);
        } else {
            this->log_ << ("\n    Session/New... canceled.");
        }
    }

    void MainWindow::new_session( QString const& path )
    {
        this->log_call( 1, CALLEE(path) );

        this->launcher_.session_config.clear();
        this->setup_session();
        this->launcher_.session_config.save(path);
        this->save_session_path(path);
    }

    void MainWindow::save_session_path( QString const& path )
    {
        this->log_call( 1, CALLEE(path) );

        cfg::Config launcher_config;
        QString launcher_cfg( this->launcher_.homePath("launcher.cfg") );
        if( QFileInfo(launcher_cfg).exists() ) {
            launcher_config.load( launcher_cfg );
            launcher_config.getItem("session")->set_value_and_type( path );
            launcher_config.save();
        } else {
            launcher_config.getItem("session")->set_value_and_type( path );
            launcher_config.save(launcher_cfg);
        }
    }

    QString MainWindow::load_session_path()
    {
        this->log_call( 1, CALLEE0 );

        cfg::Config launcher_config;
        QString path_to_launcher_config( Launcher::homePath("Launcher.cfg") );
        if( QFileInfo(path_to_launcher_config).exists() ) {
            launcher_config.load( path_to_launcher_config );
            return launcher_config.getItem("session")->value().toString();
        } else  {
            return QString();
        }
    }

    void MainWindow::openSessionAction_triggered()
    {
        this->log_call( 1, CALLEE0 );
        CHECK_IGNORESIGNAL;

        QString path_to_session_data =
                QFileDialog::getOpenFileName( this, "Select a .data file"
                                            , this->launcher_.homePath()
                                            ,"Launcher sessions (*.data)"
                                            );
        if( !path_to_session_data.isEmpty() ) {
            this->log_ << ( QString("\n    Ready to open session: ").append(path_to_session_data).toStdString() );
            this->open_session(path_to_session_data);
        } else {
            this->log_ << ("\n    Session/open... Canceled.");
        }
    }

    void MainWindow::open_session( QString const& path_to_session_data )
    {
        this->log_call( 1, CALLEE0, QString("\n      path to session.data; '%1'").arg(path_to_session_data) );

        this->launcher_.session_config.load(path_to_session_data);
        this->setup_session();

        QString msg = QString("Current Launcher session '%1'.").arg(path_to_session_data);
        this->statusBar()->showMessage(msg);
    }

    void MainWindow::setup_session()
    {
        this->log_call( 1, CALLEE0 );
        this->data_.clear();

        if( this->launcher_.session_config.contains("windowHeight") ) {
            this->resize( this->launcher_.session_config["windowWidth" ].value().toInt()
                        , this->launcher_.session_config["windowHeight"].value().toInt() );
        }
     // initialize gui items
     /* Every QWidget in the gui (almost) has a cfg::Item that has to be
      * initialized first (either by constructing it or by reading it
      * from the config file). Both are conneced by a DataConnector<W>
      * object. For all widgets with such DataConnector, the policy is
      * to fist modify the corresponding cfg::Item, and then percolate
      * the change to the widget by calling the virtual DataConnectorBase::
      * config_to_widget() member function.
      * Hence, we first create and initialize the cfg::Items, next we
      * create the corresponding DataConnector<W> objects.
      */
        this->setIgnoreSignals();
        cfg::Item* ci = nullptr;

     // username and keys
        this->getSessionConfigItem("username"  , QString() );
        this->getSessionConfigItem("privateKey", QString() );
        this->getSessionConfigItem("passphraseNeeded", true );

        ci = this->getSessionConfigItem("wWalltime","1:00:00");
        this->walltime_ = new Walltime(this->ui->wWalltime,ci);

        ci = this->getSessionConfigItem("wNotifyAddress", QString() );
        ci = this->getSessionConfigItem("notifyWhen");
        if( !ci->isInitialized() ) {
            ci->set_value("");
            this->getSessionConfigItem("wNotifyAbort", false );
            this->getSessionConfigItem("wNotifyBegin", false );
            this->getSessionConfigItem("wNotifyEnd"  , false );
        }

        dc::DataConnectorBase::config = &(this->launcher_.session_config);
        dc::DataConnectorBase::script = &(this->launcher_.script);
        this->data_.append( dc::newDataConnector( this->ui->wNotifyAddress, "wNotifyAddress" , "-M"       ) );
        this->data_.append( dc::newDataConnector(                           "notifyWhen"     , "-m"       ) );
        this->data_.append( dc::newDataConnector( this->ui->wNotifyAbort  , "wNotifyAbort"   , ""         ) );
        this->data_.append( dc::newDataConnector( this->ui->wNotifyEnd    , "wNotifyEnd"     , ""         ) );
        this->data_.append( dc::newDataConnector( this->ui->wNotifyBegin  , "wNotifyBegin"   , ""         ) );

     // this->ui->wCluster
        QString path_to_clusters = this->get_path_to_clusters();
        if( path_to_clusters.isEmpty() ) {
            QString msg("No "
                        ""
                        ""
                        ""
                        ""
                        ""
                        "s found (.info files).\n   ABORTING!");
            QMessageBox::critical( this, TITLE, msg );
            throw_<NoClustersFound>( msg.toStdString().c_str() );
        } else {
            this->launcher_.readClusterInfo( path_to_clusters );
        }
        ci = this->getSessionConfigItem("wCluster");
        try {
            ci->set_choices( this->launcher_.clusters.keys() );
        } catch( cfg::InvalidConfigItemValue & e) {
            ci->set_value( ci->default_value() );
        }
        if( !ci->isInitialized() )
            ci->set_value( ci->default_value() );

        this->clusterDependencies(false);

        this->data_.append( dc::newDataConnector( this->ui->wCluster       , "wCluster"       , "cluster"    ) );
        this->data_.append( dc::newDataConnector( this->ui->wNodeset       , "wNodeset"       , "nodeset"    ) );
        this->data_.append( dc::newDataConnector( this->ui->wNNodes        , "wNNodes"        , "nodes"      ) );
        this->data_.append( dc::newDataConnector( this->ui->wNCoresPerNode , "wNCoresPerNode" , "ppn"        ) );
        this->data_.append( dc::newDataConnector( this->ui->wNCores        , "wNCores"        , "n_cores"    ) );
        this->data_.append( dc::newDataConnector( this->ui->wGbPerCore     , "wGbPerCore"     , "GB_per_core") );
        this->data_.append( dc::newDataConnector( this->ui->wGbTotal       , "wGbTotal"       , "GB_total"   ) );
//        this->data_.append( dc::newDataConnector( this->ui->wWalltime      , "wWalltime"      , ""           ) );
        //this->walltime_->

        this->storeResetValues();

     //=====================================
     // remote file location/local file location/wProjectFolder/wJobname
     // Remote file location
        ci = this->getSessionConfigItem("remoteFileLocation");
        if( !ci->isInitialized() ) {
            QStringList options({"$VSC_SCRATCH/Launcher-jobs","$VSC_DATA/Launcher-jobs"});
            ci->set_choices(options);
            ci->set_value(options[0]);
        }

     // project folder and job name
        this->getSessionConfigItem("wProjectFolder", "");
        this->getSessionConfigItem("wJobname"      , "");
     // local file location
        ci = this->getSessionConfigItem("localFileLocation", this->launcher_.homePath("jobs") );
        {
            QString s = ci->value().toString();
            QDir dir;
            if( dir.cd(s) )
            {
                QString jobname = this->job_folder();
                if( !jobname.isEmpty() ) {
                    if( dir.cd(jobname) ) {
                        if( dir.exists("pbs.sh") ) {
                             this->loadJobscript( dir.absoluteFilePath("pbs.sh") );
                        }
                    } else {
                        this->getSessionConfigItem("wJobname")->set_value("");
                    }
                }
            } else
            {// the local file location is not existing => also clear the job name
                this->getSessionConfigItem("wJobname")->set_value("");
             // try the standard local file location
                QString msg = QString("Inexisting local file location '%1'.").arg( s );
                QString s2 = this->launcher_.homePath("jobs");
                if( QDir().mkpath(s2) ) {
                    msg.append(QString("\nReverting to '%2'").arg( s2 ) );
                } else {
                    msg.append("\nNo alternative available :(");
                    s2.clear();
                 }
                ci->set_value(s2);// empty if inexisting and no alternative
                this->statusBar()->showMessage(msg);
            }
        }

        this->data_.append( dc::newDataConnector("wJobname"      , "-N"           ) );
        this->data_.append( dc::newDataConnector("localFolder"   , "local_folder" ) );
        this->data_.append( dc::newDataConnector("remoteFolder"  , "remote_folder") );

        this->getSessionConfigItem("wCheckCopyToDesktop", true );
        this->data_.append( dc::newDataConnector( this->ui->wCheckCopyToDesktop, "wCheckCopyToDesktop", "") );

        this->getSessionConfigItem("CheckCopyToVscData", false );
        this->data_.append( dc::newDataConnector( this->ui->wCheckCopyToVscData, "CheckCopyToVscData", "") );

        this->getSessionConfigItem("wCheckDeleteLocalJobFolder", false );
        this->data_.append( dc::newDataConnector( this->ui->wCheckDeleteLocalJobFolder, "wCheckDeleteLocalJobFolder", "") );

        this->getSessionConfigItem("wCheckDeleteRemoteJobFolder", false );
        this->data_.append( dc::newDataConnector( this->ui->wCheckDeleteRemoteJobFolder, "wCheckDeleteRemoteJobFolder", "") );

        this->setIgnoreSignals(false);

        this->ui->wAutomaticRequests->setChecked( true );

        bool empty_jobname = this->job_folder().isEmpty();
        this->ui->wResourcesGroup    ->setVisible(!empty_jobname );
        this->ui->wScriptPage        ->setEnabled(!empty_jobname );
        this->ui->wNewJobscriptButton->setVisible( empty_jobname );
        this->reloadJobscriptAction_ ->setEnabled(!empty_jobname );

        if( empty_jobname )
        {// no jobname - hide resources and script
            this->launcher_.script.set_has_unsaved_changes(false);
        }
        this->check_script_unsaved_changes(); // ?? nessecary ?? todo
        this->update_StatusbarWidgets();


//        this->file_system_model = new QFileSystemModel(this);
//        QString s = this->local_file_location();
//        this->file_system_model->setRootPath(s);
//        QTreeView& tree = *(this->ui->wLocalFileView);
//        tree.setModel(this->file_system_model);
//        if( !s.isEmpty()) {
//            const QModelIndex rootIndex = this->file_system_model->index(QDir::cleanPath(s));
//            if( rootIndex.isValid() )
//                tree.setRootIndex(rootIndex);
//        }
////        tree.setIndentation(20);
//        tree.setSortingEnabled(true);

//        this->statusBar()->showMessage("tree");

//        QString txt;
//        if( this->ssh.authenticated() ) {
//            QString cmd = QString("tree ").append( this->remote_file_location() );
//            this->ssh.execute(cmd,300,"get tree of remote root folder");
//            txt = this->ssh.standardOutput();
//        } else {
//            txt = "You must authenticate to see the remote files location.";
//        }
//        this->ui->wRemoteFileView->setPlainText(txt);
    }

    bool MainWindow::isScriptOpen() const {
        return !this->ui->wResourcesGroup->isHidden();
    }

    void MainWindow::saveSessionAction_triggered()
    {
        this->log_call( 1, CALLEE0 );
        CHECK_IGNORESIGNAL;

        QString path_to_session_data = this->launcher_.session_config.filename();
        if( path_to_session_data.isEmpty() ) {
            path_to_session_data = this->launcher_.homePath("session.data");
        }
        path_to_session_data = QFileDialog::getSaveFileName( this, "Save this Launcher session"
                                                  , path_to_session_data
                                                  ,"Launcher sessions (*.data)"
                                                  , nullptr
                                                  , QFileDialog::DontConfirmOverwrite
                                                  );
        if( !path_to_session_data.isEmpty() ) {
            this->log_ << ( QString("\n    Ready to save session: ").append(path_to_session_data).toStdString() );
            this->save_session( path_to_session_data );
        } else {
            this->log_ << ("\n    Session/save... canceled.");
        }
    }

    void MainWindow::saveAsSessionAction_triggered()
    {
        this->log_call( 1, CALLEE0 );
        CHECK_IGNORESIGNAL;

//        QString path_to_session_data = this->launcher_.session_config.filename();
//        if( path_to_session_data.isEmpty() ) {
//            path_to_session_data = this->launcher_.homePath("session.data");
//        }
//        path_to_session_data = QFileDialog::getSaveFileName( this, "Save this Launcher session"
//                                                  , path_to_session_data
//                                                  ,"Launcher sessions (*.data)"
//                                                  , nullptr
//                                                  , QFileDialog::DontConfirmOverwrite
//                                                  );
//        if( !path_to_session_data.isEmpty() ) {
//            this->save_session( path_to_session_data );
//        } else {
//            this->log() << "\n    canceled.";
//        }
    }

    void MainWindow::save_session( QString const& path_to_session_data )
    {
        this->log_call( 1, CALLEE0, QString("\n      path: '%1'").arg( path_to_session_data ) );

        this->launcher_.session_config.save( path_to_session_data );
        this->save_session_path( path_to_session_data );
        QString msg = QString("Launcher session saved as '%1'.").arg(path_to_session_data);
        this->statusBar()->showMessage(msg);
    }

//    QString
//    MainWindow::
//    jobs_project_path( LocalOrRemote local_or_remote, bool resolve )
//    {
//        QString fileLocation = ( local_or_remote==LOCAL ? "localFileLocation" : "remoteFileLocation" );
//        QString path = QString( this->getSessionConfigItem( fileLocation   )->value().toString() ).append("/")
//                       .append( this->project_folder() );
//        QDir qdir(path);
//        path = qdir.cleanPath(path);
//        if( local_or_remote==REMOTE ) {
//            if( resolve && path.startsWith('$') && this->sshSession_.can_authenticate() ) {
//                QString envvar = path.split('/')[0];
//                QString cmd("echo %1");
//                try {
//                    this->sshSession_.execute_remote_command(cmd,envvar);
//                    QString resolved = this->sshSession_.qout().trimmed();
//                    path.replace(envvar,resolved);
//                } catch(std::runtime_error &e ) {
//                 // pass
//                }
//            }
//            return path;
//        } else {
//            if( !qdir.exists() ) {
//                throw_<std::runtime_error>("Local project folder does not exist: '%1'", path );
//            }
//            return path;
//        }
//    }

QString MainWindow::local_path_to( PathTo path_to, QString const& add_message )
{
    QDir dir( this->local_file_location() );
    if( path_to >= RootFolder ) {
        if( !verify_directory(&dir) ) {
            this->error_message_missing_local_file_location(add_message);
            return QString();
        }
        if( path_to == RootFolder ){
            return dir.absolutePath();
        }
    }
    if( path_to >= ProjectFolder ) {
        if( !verify_directory(&dir,this->project_folder(),false,true) ) {
            this->error_message_missing_project_folder(add_message);
            return QString();
        }
        if( path_to == ProjectFolder ) {
            return dir.absolutePath();
        }
    }
    if( path_to >= JobFolder ) {
        if( !verify_directory(&dir,this->job_folder(),false,true) ) {
            this->error_message_missing_job_folder(add_message);
            return QString();
        }
        if( path_to == JobFolder ) {
            return dir.absolutePath();
        }
    }
    if( path_to >= Script ) {
        if( verify_file(&dir,"pbs.sh") ) {
            return dir.absoluteFilePath("pbs.sh");
        } else {
            this->error_message_missing_job_script(add_message);
            return QString();
        }
    } else {
        return QString();// keep compiler happy
    }

}

bool
MainWindow::
resolve( QString & s )
{
    QString envvar = s.split('/')[0];
    QString cmd = QString("echo ").append(envvar);
    try {
        this->ssh.execute(cmd,300);
        QString resolved = this->ssh.standardOutput().trimmed();
        s.replace(envvar,resolved);
    } catch(std::runtime_error &e ) {
        return false;
    }
    return true;
}

QString
MainWindow::
remote_path_to( PathTo path_to, bool resolve )
{
    QString path( this->remote_file_location() );
    if( path_to >= RootFolder ) {
        if( resolve && path.startsWith('$') && this->ssh.authenticated() ) {
            this->resolve(path);
        }
        if( path_to == RootFolder ){
            return QDir::cleanPath(path);
        }
    }
    if( path_to >= ProjectFolder ) {
        path.append('/').append( this->project_folder() );
        if( path_to == ProjectFolder ) {
            return QDir::cleanPath(path);
        }
    }
    if( path_to >= JobFolder ) {
        path.append('/').append( this->job_folder() );
        if( path_to == JobFolder ) {
            return QDir::cleanPath(path);
        }
    }
    if( path_to >= Script ) {
        path.append('/').append("pbs.sh");
    }
    return QDir::cleanPath(path);
}


//    QString
//    MainWindow::
//    jobs_project_job_path( LocalOrRemote local_or_remote, bool resolve )
//    {
//        QString fileLocation = ( local_or_remote==LOCAL ? "localFileLocation" : "remoteFileLocation" );
//        QString path = QString( this->getSessionConfigItem( fileLocation   )->value().toString() ).append("/")
//                       .append( this->project_folder() ).append("/")
//                       .append( this->getSessionConfigItem("wJobname"      )->value().toString() );
//        QDir qdir(path);
//        path = qdir.cleanPath(path);
//        if( local_or_remote==REMOTE ) {
//            if( resolve && path.startsWith('$') && this->sshSession_.can_authenticate() ) {
//                QString envvar = path.split('/')[0];
//                QString cmd("echo %1");
//                try {
//                    this->sshSession_.execute_remote_command(cmd,envvar);
//                    QString resolved = this->sshSession_.qout().trimmed();
//                    path.replace(envvar,resolved);
//                } catch(std::runtime_error &e ) {
//                 // pass
//                }
//            }
//            return path;
//        } else {
//            if( !qdir.exists() ) {
//                throw_<std::runtime_error>("Local job folder does not exist: '%1'", path );
//            }
//            return path;
//        }
//    }

//    QString MainWindow::jobs_project_job_script_path( LocalOrRemote local_or_remote, bool resolve )
//    {
//        QString tmp = QString( this->jobs_project_job_path(local_or_remote,resolve) )
//                      .append("/pbs.sh");
//        return tmp;
//    }

 //-----------------------------------------------------------------------------
    void MainWindow::storeResetValues()
    {
        this->nodesetInfo().storeResetValues
                ( this->ui->wNNodes       ->value()
                , this->ui->wNCoresPerNode->value()
                , this->ui->wNCores       ->value()
                , this->ui->wGbPerCore    ->value()
                , this->ui->wGbTotal      ->text().toDouble()
                );
    }
 //-----------------------------------------------------------------------------
    MainWindow::~MainWindow()
    {
        this->log_call( 1, CALLEE0, "\n    Launcher closing down");

        for( QList<dc::DataConnectorBase*>::iterator iter = this->data_.begin(); iter!=this->data_.end(); ++iter ) {
            (*iter)->value_widget_to_config();
        }

        QSize windowSize = this->size();
        this->getSessionConfigItem("windowWidth" )->set_value( windowSize.width () );
        this->getSessionConfigItem("windowHeight")->set_value( windowSize.height() );

        if ( this->launcher_.script.has_unsaved_changes() ) {
            QMessageBox::Button answer =
                    QMessageBox::question( this,TITLE
                                         ,"The current job script has unsaved changes.\n"
                                          "Do you want to save?"
                                         );
            if( answer==QMessageBox::Yes ) {
                this->log_ << ( QString("\n    - Saving changes to ").append( this->local_path_to(Script) ).toStdString() );
                this->saveJobscriptAction_triggered();
            } else {
                this->log_ << ( QString("\n    - Discarding unsaved changes for ").append( this->local_path_to(Script) ).toStdString() );
            }
        }

        this->save_session( this->launcher_.session_config.filename() );
         // don't modify the session config after this line!

     // Clean up
        for( QList<dc::DataConnectorBase*>::iterator iter = this->data_.begin(); iter!=this->data_.end(); ++iter ) {
            (*iter)->value_widget_to_config();
        }
        if( this->walltime_ ) {
            delete this->walltime_;
            this->walltime_ = nullptr;
        }
        delete ui;
//        ssh::Libssh2Impl::cleanup();

        delete this->paletteRedForeground_;
        this->log_ << "\n    Launcher closed.";
    }
 //-----------------------------------------------------------------------------
    void MainWindow::setIgnoreSignals( bool ignore )
    {
        this->log_call( 1, CALLEE( BOOL2STRING(ignore) ) );

        if( this->verbosity_ > 2 ) {
            if( ignore ) {
                if(!this->ignoreSignals_ )
                    this->log_ << "\n    Ignoring signals from here";
            } else {
                if( this->ignoreSignals_ )
                    this->log_ << "\n    Signals are no longer ignored from here.";
            }
        }
        this->ignoreSignals_ = ignore;
    }
 //-----------------------------------------------------------------------------
 //-----------------------------------------------------------------------------
    dc::DataConnectorBase*
    MainWindow::
    getDataConnector( QString const& widgetName )
    {
        for( QList<dc::DataConnectorBase*>::iterator iter=this->data_.begin(); iter!=this->data_.end(); ++iter ) {
            if( (*iter)->name()== widgetName ) {
                return (*iter);
            }
        }
        throw_<std::logic_error>("Widget '%1' not found in QList<DataConnector*>.", widgetName );
        return nullptr;
    }
 //-----------------------------------------------------------------------------
    template <class T>
    cfg::Item*
    MainWindow::
    getSessionConfigItem( QString const& name, T default_value )
    {
        cfg::Item* ci = this->launcher_.session_config.getItem(name);
        if( ci->value() == QVariant::Invalid )
            ci->set_value_and_type( default_value );
        return ci;
    }
 //-----------------------------------------------------------------------------
    cfg::Item*
    MainWindow::
    getSessionConfigItem( QString const& name ) {
        return this->launcher_.session_config.getItem(name);
    }
 //-----------------------------------------------------------------------------
    ClusterInfo const&
    MainWindow::clusterInfo() const
    {
        QString            clusterName = const_cast<MainWindow*>(this)->launcher_.session_config.getItem("wCluster")->value().toString();
        ClusterInfo const& clusterInfo = *( this->launcher_.clusters.find( clusterName ) );
        return clusterInfo;
    }
 //-----------------------------------------------------------------------------
    NodesetInfo const&
    MainWindow::nodesetInfo() const
    {
        MainWindow* non_const_this = const_cast<MainWindow*>(this);
        QString nodesetName = non_const_this->getSessionConfigItem("wNodeset")->value().toString();
        QString clusterName = non_const_this->getSessionConfigItem("wCluster")->value().toString();
        NodesetInfo const& nodesetInfo = *( this->launcher_.clusters[clusterName].nodesets().find( nodesetName ) );
        return nodesetInfo;
    }
 //------------------------------------------------------------------------------
    void MainWindow::clusterDependencies( bool updateWidgets )
    {
        this->log_call( 1, CALLEE( BOOL2STRING(updateWidgets) ) );

        {// act on config items only...
            IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
            QString cluster = this->getSessionConfigItem("wCluster")->value().toString();
            ClusterInfo& clusterInfo = *(this->launcher_.clusters.find( cluster ) );
         // Initialize Config items that depend on the cluster
            cfg::Item* ci = this->getSessionConfigItem("wNodeset");
            ci->set_choices      ( clusterInfo.nodesets().keys() );
            ci->set_default_value( clusterInfo.defaultNodeset() );
            ci->set_value        ( clusterInfo.defaultNodeset() );

            if( updateWidgets ) {
                this->getDataConnector("wNodeset")->choices_config_to_widget();
                this->getDataConnector("wNodeset")->  value_config_to_widget();
            }

            this->nodesetDependencies     (updateWidgets);
            this->walltimeUnitDependencies(updateWidgets);

            this->ssh.set_login_nodes( clusterInfo.loginNodes() );
         // ok = this->ssh.set_login_node ( clusterInfo.loginNodes()[0] );
            this->ssh.set_RemoteCommandMap( *clusterInfo.remote_commands() );
        }
        bool ok = this->ssh.test_login_nodes();
        this->log_call( 1, CALLEE0, QString("\n    ... continued")
                                    .append("\n    internet connection : ").append( this->ssh.connected_to_internet() ? "yes" : "no")
                                    .append("\n    access to login node: ").append( this->ssh.login_node_alive()      ? "yes" : "no")
                      );
        if( ok ) // login node is ok
        {// try to authenticate to refresh the list of available modules and the
         // remote file locations $VSC_DATA and $VSC_SCRATCH
            if( this->ssh.set_username( this->username() ) ) {
                if( this->ssh.set_private_key( this->getSessionConfigItem("privateKey")->value().toString() ) )
                {// attempt silent authentication
                    this->authenticate();
                }
            }
        }
   }
 //-----------------------------------------------------------------------------
    double round( double x, int decimals )
    {
        for( int i=0; i<decimals; ++i ) {
            x*=10.;
        }
        x = round(x);
        for( int i=0; i<decimals; ++i ) {
            x/=10.;
        }
        return x;
    }
 //-----------------------------------------------------------------------------
    void MainWindow::nodesetDependencies( bool updateWidgets )
    {
        NodesetInfo const& nodesetInfo = this->nodesetInfo();
        cfg::Item* ci = this->getSessionConfigItem("wNNodes");
        QList<QVariant> range;
        range.append( 1 );
        range.append( nodesetInfo.nNodes() );
        ci->set_choices( range, true );
        ci->set_value( 1 );

        ci = this->getSessionConfigItem("wNCoresPerNode");
        range.last() = nodesetInfo.nCoresPerNode();
        ci->set_choices( range, true );
        ci->set_value( range.last() );

        this->ui->wGbPerNode->setText( QString().setNum( nodesetInfo.gbPerNodeAvailable(),'g',3) );
         // This one has no slots connected - no need to ignore signals
         // It is also not connected to an cfg::Item by a DataConnector, hence
         // we do update it even if updateWidgets is false.

        ci = this->getSessionConfigItem("wNCores");
        range.last() = nodesetInfo.nCoresAvailable();
        ci->set_choices( range, true );
        ci->set_value( nodesetInfo.nCoresPerNode() );

        ci = this->getSessionConfigItem("wGbPerCore");
        range.clear();
        range.append( round(nodesetInfo.gbPerCoreAvailable(),3) );
        range.append( round(nodesetInfo.gbPerNodeAvailable(),3) );
        ci->set_choices( range, true );
        ci->set_value( nodesetInfo.gbPerCoreAvailable() );

        double gbTotal  = this->ui->wGbPerNode->text().toDouble();
               gbTotal *= this->getSessionConfigItem("wNNodes")->value().toInt();
        this->getSessionConfigItem("wGbTotal")->set_value( QString().setNum( gbTotal, 'g', 3 ) );

        if( updateWidgets ) {
            IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
            this->getDataConnector("wNNodes"       )->choices_config_to_widget();
            this->getDataConnector("wNNodes"       )->  value_config_to_widget();
            this->getDataConnector("wNCoresPerNode")->choices_config_to_widget();
            this->getDataConnector("wNCoresPerNode")->  value_config_to_widget();
            this->getDataConnector("wNCores"       )->choices_config_to_widget();
            this->getDataConnector("wNCores"       )->  value_config_to_widget();
            this->getDataConnector("wGbPerCore"    )->choices_config_to_widget();
            this->getDataConnector("wGbPerCore"    )->  value_config_to_widget();
            this->getDataConnector("wGbTotal"      )->  value_config_to_widget();
        }
    }
 //-----------------------------------------------------------------------------
    void MainWindow::walltimeUnitDependencies( bool updateWidgets )
    {
        this->log_call( 1, CALLEE(BOOL2STRING(updateWidgets)) );

        IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;

        this->walltime_->set_limit( this->clusterInfo().walltimeLimit() );
        if( !this->walltime_->validate() ) {
            this->walltime_->set_seconds( this->walltime_->limit() );
        }
        this->walltime_->config_to_widget();
        if( updateWidgets ) {}
    }
 //-----------------------------------------------------------------------------
    QString MainWindow::check_script_unsaved_changes()
    {
        QString msg = QString("\n    Checking unsaved changes : ").append( BOOL2STRING( this->launcher_.script.has_unsaved_changes() ) );

        this->update_StatusbarWidgets();
        if( this->verbosity_ ) {
            return msg;
        } else {
            return QString();
        }
    }
 //-----------------------------------------------------------------------------

 //-----------------------------------------------------------------------------
 // SLOTS
 //-----------------------------------------------------------------------------

void MainWindow::on_wCluster_currentIndexChanged( QString const& arg1 )
{
    this->log_call( 1, CALLEE(arg1) );
    CHECK_IGNORESIGNAL

    this->launcher_.session_config["wCluster"].set_value(arg1);
    this->clusterDependencies(true);
}

void MainWindow::on_wNodeset_currentTextChanged(const QString &arg1)
{
    this->log_call( 1, CALLEE(arg1) );
    CHECK_IGNORESIGNAL

    this->getSessionConfigItem("wNodeset")->set_value(arg1);
    this->nodesetDependencies(true);
}

void MainWindow::updateResourceItems()
{
    this->log_call( 1,CALLEE0);

    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;

    QStringList itemList({"wNNodes", "wNCoresPerNode", "wNCores", "wGbPerCore", "wGbTotal"});
    for( QStringList::const_iterator iter=itemList.cbegin(); iter!=itemList.cend(); ++iter )
    {
        dc::DataConnectorBase* dataConnector = this->getDataConnector(*iter);
        dataConnector->value_config_to_widget();
        dataConnector->value_config_to_script();
    }
    this->check_script_unsaved_changes();
}

void MainWindow::on_wRequestNodes_clicked()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    NodesetInfo const& nodeset = this->nodesetInfo();
    try {
        nodeset.requestNodesAndCores( this->ui->wNNodes->value(), this->ui->wNCoresPerNode->value() );
    } catch( std::runtime_error & e ) {
        warn_( e.what() );
        this->statusBar()->showMessage(e.what());
        return;
    }
    this->getSessionConfigItem("wNNodes"       )->set_value( nodeset.granted().nNodes );
    this->getSessionConfigItem("wNCoresPerNode")->set_value( nodeset.granted().nCoresPerNode );
    this->getSessionConfigItem("wNCores"       )->set_value( nodeset.granted().nCores );
    this->getSessionConfigItem("wGbPerCore"    )->set_value( nodeset.granted().gbPerCore );
    this->getSessionConfigItem("wGbTotal"      )->set_value( nodeset.granted().gbTotal );
    this->updateResourceItems();
    this->pendingRequest_ = NoPendingRequest;
}

void MainWindow::on_wAutomaticRequests_toggled(bool checked)
{
    this->log_call( 1, CALLEE( BOOL2STRING(checked) ) );
    CHECK_IGNORESIGNAL

    bool manual = !checked;
    this->ui->wRequestNodes->setEnabled(manual);
    this->ui->wRequestCores->setEnabled(manual);
}

void MainWindow::on_wNNodes_valueChanged(int arg1)
{
    this->log_call( 1, CALLEE(arg1) );
    CHECK_IGNORESIGNAL

    if( this->ui->wAutomaticRequests->isChecked() ) {
        on_wRequestNodes_clicked();
    } else {
        this->pendingRequest_ = NodesAndCoresPerNode;
    }
}

void MainWindow::on_wNCoresPerNode_valueChanged(int arg1)
{
    this->log_call( 1, CALLEE(arg1) );
    CHECK_IGNORESIGNAL

    if( this->ui->wAutomaticRequests->isChecked() ) {
        on_wRequestNodes_clicked();
    } else {
        this->pendingRequest_ = NodesAndCoresPerNode;
    }
}

void MainWindow::on_wNCores_valueChanged(int arg1)
{
    this->log_call( 1, CALLEE(arg1) );
    CHECK_IGNORESIGNAL

    if( this->ui->wAutomaticRequests->isChecked() ) {
        on_wRequestCores_clicked();
    } else {
        this->pendingRequest_ = CoresAndGbPerCore;
    }
}

void MainWindow::on_wGbPerCore_valueChanged(double arg1)
{
    this->log_call( 1, CALLEE(arg1) );
    CHECK_IGNORESIGNAL

    if( this->ui->wAutomaticRequests->isChecked() ) {
        on_wRequestCores_clicked();
    } else {
        this->pendingRequest_ = CoresAndGbPerCore;
    }
}

void MainWindow::on_wRequestCores_clicked()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    NodesetInfo const& nodeset = this->nodesetInfo();
    try {
        NodesetInfo::IncreasePolicy increasePolicy = (this->ui->wGbPerCoreLabel->isChecked() ? NodesetInfo::IncreaseCores : NodesetInfo::IncreaseMemoryPerCore );
        nodeset.requestCoresAndMemory( this->ui->wNCores->value(), this->ui->wGbPerCore->value(), increasePolicy );
    } catch( std::runtime_error & e ) {
        warn_( e.what() );
        this->statusBar()->showMessage(e.what());
        return;
    }
    this->getSessionConfigItem("wNNodes"       )->set_value( nodeset.granted().nNodes );
    this->getSessionConfigItem("wNCoresPerNode")->set_value( nodeset.granted().nCoresPerNode );
    this->getSessionConfigItem("wNCores"       )->set_value( nodeset.granted().nCores );
    this->getSessionConfigItem("wGbPerCore"    )->set_value( nodeset.granted().gbPerCore );
    this->getSessionConfigItem("wGbTotal"      )->set_value( nodeset.granted().gbTotal );
    this->updateResourceItems();
    this->pendingRequest_ = NoPendingRequest;
}

void MainWindow::on_wPages_currentChanged(int index_new_page)
{
    this->log_call( 1, CALLEE(index_new_page), QString("\n    Moving from page[%1] to page[%2].").arg(this->current_page_).arg(index_new_page) );
    CHECK_IGNORESIGNAL

//    this->authenticate( /*silent=*/ this->can_authenticate() && this->sshSession_.isConnected() );

    this->selected_job_.clear();

    switch (index_new_page) {
    case 0:
        if( this->current_page_==1 )
        {// read the script from wScript
            if( this->isScriptOpen()
             && this->launcher_.script.has_unsaved_changes() )
            {
                this->script_text_to_config();
            }
        }
        break;
    case 1:        
        {
            if( this->isScriptOpen() ) {
                switch( this->pendingRequest_ ) {
                  case NodesAndCoresPerNode:
                    this->on_wRequestNodes_clicked();
                    break;
                  case( CoresAndGbPerCore ):
                    this->on_wRequestCores_clicked();
                    break;
                  default:
                    break;
                }
                for( QList<dc::DataConnectorBase*>::iterator iter=this->data_.begin(); iter!=this->data_.end(); ++iter )
                {
                    if( (*iter)->value_widget_to_config() ) {
                        (*iter)->value_config_to_script();
                    }
                }
                QString text = this->launcher_.script.text();
                IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
                this->ui->wScript->setText(text);
            } else {
                IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
                this->ui->wScript->setText("Create a new job script : menu Job/New job script\n"
                                           "or open an existing one : menu Job/Open job script...");
            }
        }
        break;
    case 2:
        {
            bool ok = this->ssh.authenticated();
            if( !ok ) {
                QMessageBox::warning(this,TITLE,"Since you are not authenticated, you have no access to the remote host. Remote functionality on this tab is disabled.");
            }
            this->ui->wRetrieveAllJobs    ->setEnabled(ok);
            this->ui->wRetrieveSelectedJob->setEnabled(ok);
            this->ui->wCheckDeleteRemoteJobFolder->setEnabled(ok);
            JobList joblist( this->getSessionConfigItem("job_list")->value().toStringList() );
            this->refreshJobs( joblist );
        }
        break;
    case 3:
        {
            this->on_wRefreshLocalFileView_clicked();
            this->on_wRefreshRemoteFileView_clicked();
        }
        break;
    default:
        break;
    }
    this->current_page_ = index_new_page;
}

void MainWindow::on_wEnforceNNodes_toggled(bool checked)
{
    this->log_call( 1, CALLEE( BOOL2STRING(checked) ) );
    CHECK_IGNORESIGNAL

    bool hide = !checked;
    this->launcher_.script.find_key("-W")->setHidden(hide);
}

void MainWindow::on_wSingleJob_toggled(bool checked)
{
    this->log_call( 1, CALLEE( BOOL2STRING(checked) ) );
    CHECK_IGNORESIGNAL

    bool hide = !checked;
    this->launcher_.script.find_key("naccesspolicy")->setHidden(hide);
}

void MainWindow::on_wWalltime_editingFinished()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    QString input = this->ui->wWalltime->text();
    int s = this->walltime_->toInt(input);
    if( s < 0 ) {
        this->statusBar()->showMessage("Bad input for walltime. Expecting 'hh:mm[:ss]' or 'N d|h|m|s'.");
    } else {
        IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
        this->walltime_->set_seconds(s);
    }
}

void MainWindow::on_wNotifyAddress_editingFinished()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    QString validated = validateEmailAddress( this->ui->wNotifyAddress->text() );
    if( validated.isEmpty() )
    {// not a valid address
        IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
        this->ui->wNotifyAddress->setPalette(*this->paletteRedForeground_);
        this->statusBar()->showMessage("Invalid email address.");
    } else {
        this->ui->wNotifyAddress->setPalette( QApplication::palette(this->ui->wNotifyAddress) );
        this->statusBar()->clearMessage();
        this->getSessionConfigItem("wNotifyAddress")->set_value(validated);

        IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
        dc::DataConnectorBase* link = this->getDataConnector("wNotifyAddress");
        if( link->value_config_to_widget() ) {
            link->value_config_to_script();
        }
    }
}

void MainWindow::update_abe( QChar c, bool checked )
{
    cfg::Item* ci_notifyWhen = this->getSessionConfigItem("notifyWhen");
    QString abe = ci_notifyWhen->value().toString();
    if( checked && !abe.contains(c) ) {
        abe.append(c);
    } else if( !checked && abe.contains(c) ) {
        int i = abe.indexOf(c);
        abe.remove(i,1);
    }
    ci_notifyWhen->set_value( abe );
    this->getDataConnector("notifyWhen")->value_config_to_script();
}

void MainWindow::on_wNotifyAbort_toggled(bool checked)
{
    this->log_call( 1, CALLEE( BOOL2STRING(checked) ) );
    CHECK_IGNORESIGNAL

    this->update_abe('a', checked );
}

void MainWindow::on_wNotifyBegin_toggled(bool checked)
{
    this->log_call( 1, CALLEE( BOOL2STRING(checked) ) );
    CHECK_IGNORESIGNAL

    this->update_abe('b', checked );
}

void MainWindow::on_wNotifyEnd_toggled(bool checked)
{
    this->log_call( 1, CALLEE( BOOL2STRING(checked) ) );
    CHECK_IGNORESIGNAL

    this->update_abe('e', checked );
}

void MainWindow::warn( QString const& msg1, QString const& msg2 )
{
    this->log_call( 1, CALLEE0, QString("\n    message: ").append(msg1).append(msg2) );

    this->statusBar()->showMessage(msg1);
    QMessageBox::warning(this,TITLE,QString(msg1).append(msg2));
}

bool MainWindow::isFinished( Job const& job ) const
{
    if( job.status_ == Job::Submitted )
    {// test if the file pbs.sh.o<short_id> exists, if so, the job is finished.

     /* TODO make sure this also works if the #PBS -o redirect_stdout is used
      * and pbs.sh.oXXXXXX is replaced by redirect_stdout.
      * On second thought NOT a good idea. If this option is exposed to the
      * user he will probably use it to put a fixed file name and it will
      * become impossible to judge whether the file is there because the job
      * is finished, or whether it is a left over from an older run that was
      * not cleaned up

mn01.hopper.antwerpen.vsc:
                                                                                 Req'd    Req'd       Elap
Job ID                  Username    Queue    Jobname          SessID  NDS   TSK   Memory   Time    S   Time
----------------------- ----------- -------- ---------------- ------ ----- ------ ------ --------- - ---------
128279.mn.hopper.antwe  vsc20170    q1h      a_simple_job        --      1     20    --   01:00:00 Q       --
      */
        try
        {// Test for the existence of finished.<jobid>
         // The job cannot be finished if finished.<jobid> does not exist.
            QString cmd = QString("ls ")
                .append( job.remote_location_ )
                .append('/' ).append(job.subfolder_)
                .append('/' ).append(job.jobname_)
                .append("/finished.").append( job.long_id() );
            int rv = this->ssh.execute( cmd, 300, "Test the existence of finished.<jobid>");
            bool is_finished = ( rv==0 );
            if( !is_finished ) {
                return false;
            }
         // It is possible but rare that finished.<jobid> exists, but the
         // epilogue is still running. To make sure:
         // Check the output of qstat -u username
            cmd = QString("qstat -u %1").arg( this->ssh.username() ) ;
            rv = this->ssh.execute( cmd, 300, "Make sure that the job's epilogue is finished too.");
            QStringList qstat_lines = this->ssh.standardOutput().split('\n',QString::SkipEmptyParts);
            for ( QStringList::const_iterator iter=qstat_lines.cbegin()
                ; iter!=qstat_lines.cend(); ++iter ) {
                if( iter->startsWith(job.job_id_)) {
                    if ( iter->at(101)=='C' ) {
                        job.status_ = Job::Finished;
                        return true;
                    } else {
                        return false;
                    }
                }
            }
         // it is no longer in qstat so it must be finished.
            job.status_ = Job::Finished;
            return true;
        } catch( std::runtime_error& e ) {
            std::cout << e.what() << std::endl;
        }
        return false; // never reached, but keep the compiler happy
    } else {
        return true;
    }
}

bool MainWindow::retrieve( Job const& job, bool local, bool /*vsc_data*/ )
{
    this->log_call( 1, CALLEE0, QString("\n    Job: ").append( job.toString() ) );

    if( job.status()==Job::Retrieved ) return true;

    QString msg = QString("Retrieve job ").append( job.short_id() );

    if( !this->actionRequiringAuthentication(msg) ) return false;

    if( local )
    {
        QString  local_jobfolder = job. local_job_folder();
        QString remote_jobfolder = job.remote_job_folder();
        this->resolve(remote_jobfolder); // authentication already verified
        int rc = this->ssh.remote_sync_to_local(local_jobfolder, remote_jobfolder ) ;
        if( rc==0 ) {
            job.status_ = Job::Retrieved;
        }
    }
/* not trivial with git
    if( vsc_data && !job.remote_location_.startsWith("$VSC_DATA") )
    {
        QString cmd = QString("mkdir -p %1");
        QString vsc_data_destination
                = job.vsc_data_job_parent_folder( this->vscdata_file_location() );
        this->resolve( vsc_data_destination );
        int rc = this->ssh.execute_remote_command( cmd, vsc_data_destination );
        cmd = QString("cp -rv %1 %2"); // -r : recursive, -v : verbose
        rc = this->ssh.execute_remote_command( cmd, job.remote_job_folder(), vsc_data_destination );
        job.status_ = Job::Retrieved;
    }
*/
    return job.status()==Job::Retrieved;
}

//void MainWindow::getRemoteFileLocations_()
//{
//    this->remote_env_vars_.clear();
//    if( !this->remote_file_locations_.isEmpty() ) {
//        this->remote_file_locations_ = this->getSessionConfigItem("wRemote")->choices_as_QStringList();
//    }
//    this->authenticate();
//    if( this->sshSession_.can_authenticate() ) {
//        for ( QStringList::Iterator iter = this->remote_file_locations_.begin()
//            ; iter!=this->remote_file_locations_.end(); ++iter )
//        {
//            if( iter->startsWith('$') ) {
//                QString cmd("echo %1");
//                try {
//                    this->sshSession_.execute_remote_command( cmd, *iter );
//                    *iter = this->sshSession_.qout().trimmed();
//                } catch(std::runtime_error &e ) {
//                 // pass
//                }
//            }
//        }
//    }
//}

bool MainWindow::lookForJobscript(QString const& job_folder, bool ask_to_load , bool create_if_inexistent)
{// return false if canceled
    this->log_call( 1, CALLEE0, QString("\n    in: ").append(job_folder) );

    QDir qdir;
    bool exists=false;
    if( !is_absolute_path(job_folder) ) {
        qdir = QDir( this->local_file_location() );
        exists = qdir.cd(job_folder);
    } else {
        qdir = QDir(job_folder);
        exists = qdir.exists();
    }
    QString absolute_job_folder = qdir.absoluteFilePath("");
    QString msg;
    if( exists )
    {// the job folder exists
        if( qdir.exists("pbs.sh") )
        {// and it contains a job script
            if( ask_to_load )
            {
                QString script_file( qdir.absoluteFilePath("pbs.sh") );
                msg = QString("A job script was found:\n"
                              "  %1"
                              "Do you want to load it?")
                         .arg( script_file );
                QMessageBox::StandardButton answer =
                        QMessageBox::question( this, TITLE, msg
                                             , QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel
                                             , QMessageBox::Cancel);
                switch( answer) {
                  case QMessageBox::Yes:
                    this->loadJobscript(script_file);
                    msg = QString("Job script '%1' loaded.").arg(script_file);
                    break;
                  case QMessageBox::No:
                    QFile(script_file).rename( QString("~pbs.sh.").append( QString().setNum( qrand() ) ) );
                    break;
                  case QMessageBox::Cancel:
                  default:
                    this->statusBar()->showMessage("Canceled");
                    return false;
                }
            }
        } else
        {// existing folder but no jobscript
            msg = QString("Found existing job folder '%1', but no jobscript sofar.")
                     .arg( absolute_job_folder );
        }
    } else
    {// inexisting folder
        if( create_if_inexistent ) {
            QDir().mkpath(absolute_job_folder);
            msg = QString("Created new job folder '%1'.").arg(absolute_job_folder);
        } else {
            return false;
        }
    }
    this->log_ <<  msg.toStdString();
    this->statusBar()->showMessage(msg);
    return true;
}

bool MainWindow::loadJobscript( QString const& filepath )
{    
    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
    this->log_call( 1, CALLEE0, QString("\n    loading job script '%1'.").arg(filepath) );

    try
    {
        this->log_ << "\n    reading the job script ... ";
        this->launcher_.script.read(filepath);
     // make sure the jobscript's jobname corresponds to the name of the job folder.
        QDir qdir(filepath);
        qdir.cdUp();
        QString jobfolder( qdir.dirName() );
        if( const_cast<pbs::Script const&>(this->launcher_.script)["-N"] != jobfolder ) {
            this->launcher_.script["-N"]  = jobfolder;
        }
        this->log_ << " done";
    } catch( std::runtime_error& e ) {
        QString msg = QString("An error occurred while reading the jobscript '%1'.\n Error:\n")
                         .arg( filepath )
                      .append( e.what() )
                      .append("\n\nEdit the script to correct it, then press 'Save' and 'Reload'.");
        this->log_ <<  msg.toStdString();
        QMessageBox::critical(this,TITLE,msg);
    }

    try
    {
     // the order is important!
        this->log_ << "\n    Setting gui elements from job script ... ";
        QStringList first({"wCluster","wNodeset","wNNodes", "wNCoresPerNode"}) ;
        for(QStringList::const_iterator iter = first.cbegin(); iter!=first.cend(); ++iter ) {
            dc::DataConnectorBase* dataConnector = this->getDataConnector(*iter);
            if( dataConnector->value_script_to_config() ) {
                dataConnector->value_config_to_widget();
                this->log_ << QString( "\n      %1 : ok").arg(*iter).toStdString();
            } else {
                this->log_ << QString( "\n      %1 : unchanged (=ok)").arg(*iter).toStdString();
            }
        }
     // do the same for all other DataConnectors, for which the order does not matter.
     // the items in first are visited twice, but the second time has no effect.
        for( QList<dc::DataConnectorBase*>::Iterator iter = this->data_.begin(); iter!=this->data_.end(); ++iter )
        {
            if( (*iter)->value_script_to_config() ) {
                (*iter)->value_config_to_widget();
                this->log_ << QString( "\n      %1 : ok").arg((*iter)->name()).toStdString();
            } else {
                this->log_ << QString( "\n      %1 : unchanged (=ok)").arg((*iter)->name()).toStdString();
            }
        }
     // a few special cases:
        QString abe = this->getSessionConfigItem("notifyWhen")->value().toString();
        this->getSessionConfigItem("wNotifyAbort")->set_value( abe.contains('a') );
        this->getSessionConfigItem("wNotifyBegin")->set_value( abe.contains('b') );
        this->getSessionConfigItem("wNotifyEnd"  )->set_value( abe.contains('e') );
        this->getDataConnector("wNotifyAbort")->value_config_to_widget();
        this->getDataConnector("wNotifyBegin")->value_config_to_widget();
        this->getDataConnector("wNotifyEnd"  )->value_config_to_widget();
    }
    catch( std::runtime_error& e )
    {
        QString msg = QString("An error occurred while updating the Launcher GUI from jobscript '%1'.\n Error:\n")
                         .arg( filepath )
                      .append( e.what() )
                      .append("\n\nEdit the script to correct it, then press 'Save' and 'Reload'.");
        this->log_ << msg.toStdString();
        QMessageBox::critical(this,TITLE,msg);
        return false;
    }
    this->reloadJobscriptAction_->setEnabled(true);
    this->update_StatusbarWidgets();
 // read from file, hence no unsaved changes sofar
    return true;
}

bool MainWindow::saveJobscript( QString const& filepath )
{
    this->log_call( 1,CALLEE0);
    QString msg;
    try {
        this->log_ << QString ("\n    Writing script '%1' ...").arg(filepath).toStdString();
        this->launcher_.script.write( filepath, /*warn_before_overwrite=*/false );
        this->log_ << " done.";
    } catch( pbs::WarnBeforeOverwrite &e ) {
        QMessageBox::StandardButton answer =
                QMessageBox::question( this, TITLE
                                     , QString("Ok to overwrite existing job script '%1'?\n(a backup will be made).")
                                          .arg(filepath)
                                     );
        if( answer == QMessageBox::Yes )
        {// make a back up
            QFile f(filepath);
            QDir  d(filepath);
            d.cdUp();
            QString backup = d.absoluteFilePath( QString("~pbs.sh.").append( toolbox::now2() ) );
            f.copy(backup);
         // and save
            this->launcher_.script.write( filepath, false );
            this->log_ << QString(" done (overwritten, but made backup '%1')").arg(backup).toStdString();
        } else {
            this->log_ << " canceled";
            this->statusBar()->showMessage( QString("Job script NOT SAVED: '%1'").arg(filepath) );
            return false;
        }
    }

    QDir  d(filepath);
    d.cdUp();
    int rc = this->ssh.local_save( d.absolutePath() );
    msg = QString( rc ? "Job script saved: '%1', but local repository could not be updated!!"
                      : "Job script saved: '%1', local repository updated"
                 ).arg(filepath);
    this->statusBar()->showMessage(msg);

 // just saved, hence no unsaved changes yet.
    this->launcher_.script.set_has_unsaved_changes(false);
    this->check_script_unsaved_changes();
    return true;
}

//void MainWindow::reload()
//{
//    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

//    QString script_file = QString( this->jobs_project_job_path(LOCAL) ).append("/pbs.sh");
//    QFileInfo qFileInfo(script_file);
//    if( !qFileInfo.exists() ) {
//        this->statusBar()->showMessage("No job script found, hence not loaded.");
//    } else {
//        QMessageBox::StandardButton answer = QMessageBox::Yes;
//        if( this->launcher_.script.has_unsaved_changes() ) {
//            answer = QMessageBox::question( this, TITLE, "Unsaved changes to the current job script will be lost on reloading. Continue?");
//        }
//        if( answer==QMessageBox::Yes ) {
//            this->loadJobscript(script_file);
//            this->statusBar()->showMessage("job script reloaded.");
//        }
//    }
//}

void MainWindow::on_wScript_textChanged()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    this->launcher_.script.set_has_unsaved_changes(true);
    this->check_script_unsaved_changes();
}

void MainWindow::on_wRefresh_clicked()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    cfg::Item* ci_joblist = this->getSessionConfigItem("job_list");
    JobList joblist( ci_joblist->value().toStringList() );
    this->refreshJobs(joblist);
}

QString MainWindow::selectedJob( QTextEdit* qTextEdit )
{
    this->log_call( 1, CALLEE0 );
    QString selection;
    QTextCursor cursor = qTextEdit->textCursor();
    cursor.select(QTextCursor::LineUnderCursor);
    selection = cursor.selectedText();

    if( selection.isEmpty()
     || selection.startsWith("*** ")
      ) {
        this->statusBar()->showMessage("No job Selected");
        selection.clear();
        this->log_ << "\n    Nothing selected";
    } else {
        qTextEdit->setTextCursor(cursor);
        selection = selection.split(':')[0];
        this->log_ << QString("\n    selected: ").append(selection).toStdString();
    }
    return selection;
}

void MainWindow::clearSelection( QTextEdit* qTextEdit )
{
    this->log_call( 1, CALLEE0 );

    QTextCursor cursor = qTextEdit->textCursor();
    cursor.movePosition( QTextCursor::Start);
    qTextEdit->setTextCursor(cursor);
}

void MainWindow::on_wRetrieveSelectedJob_clicked()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    if( this->selected_job_.isEmpty() ) {
        this->statusBar()->showMessage("Nothing selected.");
        return;
    }

    cfg::Item* ci_joblist = this->getSessionConfigItem("job_list");
    JobList joblist( ci_joblist->value().toStringList() );
    Job const* job = joblist[this->selected_job_];
    if( !job ) {
        this->statusBar()->showMessage( QString("job not found: %1").arg(this->selected_job_) );
        return;
    }
    if( job->status()==Job::Retrieved ) {
        this->statusBar()->showMessage( QString("Job allready retrieved: %1").arg(this->selected_job_) );
        return;
    }
    if( job->status()==Job::Submitted ) {
        this->statusBar()->showMessage( QString("Job not finished: %1").arg(this->selected_job_) );
        return;
    }

    this->statusBar()->showMessage( QString("Retrieving job : %1").arg(this->selected_job_) );

    bool toDesktop = this->ui->wCheckCopyToDesktop->isChecked();
    bool toVscData = this->ui->wCheckCopyToVscData->isChecked();
    if( this->retrieve( *job, toDesktop, toVscData ) ) {
        ci_joblist->set_value( joblist.toStringList( Job::Submitted|Job::Finished|Job::Retrieved ) );
        this->refreshJobs(joblist);
    }
    this->selected_job_.clear();
}

void MainWindow::on_wRetrieveAllJobs_clicked()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    cfg::Item* ci_joblist = this->getSessionConfigItem("job_list");
    JobList joblist( ci_joblist->value().toStringList() );

    bool toDesktop = this->ui->wCheckCopyToDesktop->isChecked();
    bool toVscData = this->ui->wCheckCopyToVscData->isChecked();

    for ( JobList::const_iterator iter=joblist.cbegin()
        ; iter!=joblist.cend(); ++iter )
    {
        this->retrieve( *iter, toDesktop, toVscData );
    }

    QStringList list = joblist.toStringList( Job::Submitted | Job::Finished );
    ci_joblist->set_value( list );
    this->refreshJobs(joblist);
}

void MainWindow::refreshJobs( JobList const& joblist )
{ 
    for ( JobList::const_iterator iter=joblist.cbegin()
        ; iter!=joblist.cend(); ++iter )
    {
        this->isFinished(*iter);
    }

    QString text("*** JOBS READY FOR RETRIEVAL ***\n");
    text.append( joblist.toString(Job::Finished) );
    text.append("\n*** UNFINISHED JOBS ***\n");
    text.append( joblist.toString(Job::Submitted) );
    text.append("\n*** RETRIEVED JOBS ***\n");
    text.append( joblist.toString(Job::Retrieved) );
    this->ui->wFinished->setText( text );

//    QString username = this->username();
//    if( !username.isEmpty() ) {
//        QString cmd = QString("qstat -u %1");
//        this->sshSession_.execute_remote_command( cmd, username );
//        if( !this->sshSession_.qout().isEmpty() ) {
//            text.append("\n--- ").append( cmd ).append( this->sshSession_.qout() );
//             // using '---' rather than '>>>' avoids that the user can
//             // select the output of qstat as if it was a job entry.
//        }
//    }
//    this->ui->wNotFinished->setText( text);

//    this->ui->wRetrieved->setText( joblist.toString(Job::Retrieved) );

    this->getSessionConfigItem("job_list")->set_value( joblist.toStringList(Job::Submitted|Job::Finished|Job::Retrieved) );

    this->selected_job_.clear();
}

void MainWindow::deleteJob( QString const& jobid )
{
    QString msg = QString("Removing job : ").append(jobid).append(" ... ");
    this->statusBar()->showMessage(msg);

    cfg::Item* ci_job_list = this->getSessionConfigItem("job_list");
    QStringList job_list = ci_job_list->value().toStringList();
    for( int i=0; i<job_list.size(); ++i ) {
        if( job_list[i].startsWith(jobid) )
        {
            Job job(job_list[i]);
         // kill job if not finished
            if( job.status()==Job::Submitted ) {
                QString msg = QString("killing job %1 ... ").arg( job.long_id() );
                this->statusBar()->showMessage(msg);
                QString cmd = QString("qdel %1").arg( job.short_id() );
                this->ssh.execute(cmd,300,"deleteJob : deleting running jobs necessitates killing it.");
                this->statusBar()->showMessage( msg.append("done") );
            }
         // remove local job folder
            if( this->ui->wCheckDeleteLocalJobFolder->isChecked() ) {
                QString msg = QString("Removing: ").append( job.local_job_folder() ).append(" ... ");
                this->statusBar()->showMessage(msg);
                QDir qDir( job.local_job_folder() );
                qDir.removeRecursively();
                this->statusBar()->showMessage(msg.append("done"));
            }
         // remove remote jobfolder
            if( this->ui->wCheckDeleteRemoteJobFolder->isChecked() ) {
                QString msg = QString("Removing: ").append( job.remote_job_folder() ).append(" ... ");
                this->statusBar()->showMessage(msg);
                QString cmd = QString("rm -rf %1");
                this->ssh.execute( cmd, 300,job.remote_job_folder() );
                this->statusBar()->showMessage( msg.append("done") );
            }
         // remove from job list
            job_list.removeAt(i);
            this->refreshJobs(job_list);
            this->statusBar()->showMessage( QString("Job %1 removed.").arg(jobid) );
            return;
        }
    }
    this->statusBar()->showMessage( msg.append(" ... not found.") );
}

void MainWindow::on_wDeleteSelectedJob_clicked()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    if( this->selected_job_.isEmpty() ) {
        this->statusBar()->showMessage("Nothing Selected.");
        return;
    }
    this->deleteJob( this->selected_job_ );
    this->selected_job_.clear();
}

void MainWindow::on_wCheckCopyToDesktop_toggled(bool checked)
{
    this->log_call( 1, CALLEE( BOOL2STRING(checked) ) );
    CHECK_IGNORESIGNAL
    this->getSessionConfigItem("wCheckCopyToDesktop")->set_value(checked);
}

void MainWindow::on_wCheckCopyToVscData_toggled(bool checked)
{
    this->log_call( 1, CALLEE( BOOL2STRING(checked) ) );
    CHECK_IGNORESIGNAL
    this->getSessionConfigItem("wCheckCopyToVscData")->set_value(checked);
}

void MainWindow::on_wCheckDeleteLocalJobFolder_toggled(bool checked)
{
    this->log_call( 1, CALLEE( BOOL2STRING(checked) ) );
    CHECK_IGNORESIGNAL
    this->getSessionConfigItem("wCheckDeleteLocalJobFolder")->set_value(checked);
}

void MainWindow::on_wCheckDeleteRemoteJobFolder_toggled(bool checked)
{
    this->log_call( 1, CALLEE( BOOL2STRING(checked) ) );
    CHECK_IGNORESIGNAL
    this->getSessionConfigItem("wCheckDeleteRemoteJobFolder")->set_value(checked);
}

SUBCLASS_EXCEPTION(BadLauncherHome,std::runtime_error)
SUBCLASS_EXCEPTION(BadDistribution,std::runtime_error)

QString
copy_folder_recursively
  ( QString const& source
  , QString const& destination
  , int level )
{
    QDir().mkpath(destination);
    QString log_msg = QString("\n      ").append(destination);
    QDir qdir(source);
    QFileInfoList qfileinfolist = qdir.entryInfoList();
 // loop over files
    for ( QFileInfoList::ConstIterator iter=qfileinfolist.cbegin()
        ; iter!=qfileinfolist.cend(); ++iter )
    {
        if( iter->isFile() ) {
            log_msg.append("\n      ");
            for( int i=1; i<=level; ++i ) { log_msg.append("  "); }
            log_msg.append(iter->fileName());

            QString tmp = iter->absoluteFilePath();
            QFile qFile( tmp );
            tmp = QString(destination).append('/').append( iter->fileName() );
            qFile.copy( tmp );
        }
    }
 // loop over folders
    for ( QFileInfoList::ConstIterator iter=qfileinfolist.cbegin()
        ; iter!=qfileinfolist.cend(); ++iter )
    {
        if( iter->isDir() ) {
            QString folder = iter->fileName();
            if( folder!="." && folder!=".." ) {
                log_msg.append( copy_folder_recursively( QString(source     ).append('/').append(folder)
                                                       , QString(destination).append('/').append(folder)
                                                       , level+1 )
                              );
            }
        }
    }
    return log_msg;
}

void MainWindow::setupHome()
{
    this->log_call( 1, CALLEE0 );

 // get the location of the application and derive the location of the clusters info files.
 // see http://stackoverflow.com/questions/4515602/how-to-get-executable-name-in-qt
    QString applicationFilePath = QCoreApplication::applicationFilePath();
    QDir dir = QFileInfo( applicationFilePath ).absoluteDir();
    QStringList folders = {"clusters","jobs"};
    QStringList path_to_folders;
    for ( QStringList::const_iterator iter = folders.cbegin()
        ; iter!=folders.cend(); ++iter )
    {
        path_to_folders.append(
#ifdef Q_OS_WIN
                                dir.absoluteFilePath(*iter)
#else
                                QDir::cleanPath( dir.absoluteFilePath(QString("../Resources/").append(*iter) ) )
#endif
                              );
    }
    this->log_ << "\n    Copying standard folders.";
    for ( int i=0; i<folders.size(); ++i ) {
        this->log_ << copy_folder_recursively( path_to_folders[i], this->launcher_.homePath( folders[i] ) ).toStdString();
    }
}

QString MainWindow::get_path_to_clusters()
{
 // look for standard clusters path
    QString path_to_clusters = Launcher::homePath("clusters");
    QDir dir( path_to_clusters, "*.info");
    if( dir.entryInfoList().size() > 0 ) return path_to_clusters;

 // look for non standard clusters path in config file:
    path_to_clusters = this->getSessionConfigItem("path_to_clusters", QString() )->value().toString();
    dir = QDir( path_to_clusters, "*.info");
    if( dir.entryInfoList().size() > 0 ) return path_to_clusters;

 // ask the user for the path to the clusters directory
    path_to_clusters = "keep_trying";
    while( !path_to_clusters.isEmpty() ) {
        path_to_clusters = QFileDialog::getExistingDirectory
                ( this, "Select a clusters directory (containing at least 1 .info file):"
                , this->launcher_.homePath() );
        if( path_to_clusters.isEmpty() ) return path_to_clusters;
        dir = QDir( path_to_clusters, "*.info");
        if( dir.entryInfoList().size() > 0 ) return path_to_clusters;
    }
    return QString();
}

void MainWindow::on_wSelectModule_currentIndexChanged(const QString &arg1)
{
    this->log_call( 1, CALLEE(arg1) );
    CHECK_IGNORESIGNAL

    if( arg1.startsWith("---") )
    {// ignore this dummy entry...
    } else {
        QTextCursor cursor;
        if( this->ui->wScript->find("cd $PBS_O_WORKDIR") ) {
            while( this->ui->wScript->find("module load") ) {}
            cursor = this->ui->wScript->textCursor();
        } else {// insert at current position (or rather just before the next line)
            cursor = this->ui->wScript->textCursor();
        }
        cursor.movePosition( QTextCursor::EndOfLine );
        cursor.movePosition( QTextCursor::NextCharacter );
        cursor.insertText( QString("module load ").append(arg1).append('\n') );
        this->ui->wScript->setTextCursor( cursor );
        this->launcher_.script.set_has_unsaved_changes();
    }
}

//void MainWindow::on_wNotFinished_selectionChanged()
//{
//    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

//    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
//    this->selected_job_ = this->selectedJob( this->ui->wNotFinished );
//    if( !this->selected_job_.isEmpty() ) {
//        this->selected_job_ = this->selected_job_.split(':')[0];
//        this->statusBar()->showMessage( QString("Selected job: %1.").arg( this->selected_job_ ) );
//    }
//}

void MainWindow::on_wFinished_selectionChanged()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
    this->selected_job_ = this->selectedJob( this->ui->wFinished );
    if( !this->selected_job_.isEmpty() ) {
        this->statusBar()->showMessage( QString("Selected job:%1.").arg( this->selected_job_ ) );
    }
}

void MainWindow::on_wClearSelection_clicked()
{
    if( this->selected_job_.isEmpty() ) {
        this->statusBar()->showMessage("Nothing selected.");
    } else {
        this->clearSelection( this->ui->wFinished    );
//        this->clearSelection( this->ui->wNotFinished );
        this->selected_job_.clear();
        this->statusBar()->showMessage("Selection cleared.");
    }
}

bool isNonEmptyDir( QDir const& qdir ) {
    QStringList entries( qdir.entryList() );
    return entries.size()>2;  // even empty directory contains "." and ".."
}
bool isNonEmptyDir( QString const& dirpath ) {
    return isNonEmptyDir( QDir(dirpath) );
}


void MainWindow::authenticateAction_triggered()
{/* ask for a username
  * if it is a valid username, store it in the sshSession_
  * otherwise cancel the authentication
  * ask for a private key
  * if we get one store it in the sshSession_
  * otherwise cancel the authentication
  * authenticate()
  */
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    this->proceed_offline_ = false;

    if( this->ssh.authenticated() ) {
        QString msg = QString("This session is already authenticated:"
                              "\n  user: %1"
                              "\n  key : %2"
                              "\n\nDo you want to re-authenticate?")
                         .arg( this->ssh.username() )
                         .arg( this->ssh.private_key() );
        QMessageBox::StandardButton answer = QMessageBox::question(this,TITLE,msg);
        if( answer!=QMessageBox::Yes ) return;
    }

 // Ask for username username
    QString msg = "Enter authentication details ...\nusername [vscXXXXX]:";
    cfg::Item* ci = this->getSessionConfigItem("username");
    bool ok;
    QString username;
    while(1) {
        username = QInputDialog::getText(this, TITLE, msg, QLineEdit::Normal, ci->value().toString(), &ok );
        if( ok && !username.isEmpty() ) {
            username = validateUsername(username);
            if( !username.isEmpty() )
            {// username is ok
                this->ssh.set_username(username);
                break;
            }
        } else {
            this->statusBar()->showMessage("Authentication canceled by user.");
            return;
        }
    }

 // aks for openssh key(s)
    QString start_in;
#if defined(Q_OS_MAC)||defined(Q_OS_LINUX)
    QDir home = QDir::home();
    if( home.exists(".ssh") ) {
        start_in = home.filePath(".ssh");
    } else {
        start_in = this->launcher_.homePath();
    }
#else
    start_in = this->launcher_.homePath();
#endif
    msg = QString("Enter authentication details ...\nProvide private openssh key for user %1:").arg(username);
//#ifdef Q_OS_WIN
//    msg+="\n((Windows users should be aware that PuTTY keys are NOT in openssh format. "
//         "Consult the VSC website for how to convert your PuTTY keys.]\n";
//#endif
    int answer = QMessageBox::question( this, TITLE, msg
                                      , "Cancel"    // button0
                                      , "Continue..."          // button1
                                      , QString()               // button2
                                      , 1                       // default button
                                      );
    QString private_key;
    switch( answer ) {
      case 1:
        private_key = QFileDialog::getOpenFileName( this, QString("Select your (%1) PRIVATE key file :").arg(username), start_in );
        if( this->ssh.set_private_key(private_key) ) {
            if( this->authenticate( /*silent=*/false ) ) {
                this->getSessionConfigItem("username"  )->set_value( this->ssh.username   () );
                this->getSessionConfigItem("privateKey")->set_value( this->ssh.private_key() );
                this->update_StatusbarWidgets();
            }
        } else {
            this->statusBar()->showMessage( QString( "Authentication canceled : private key '%1' not found.").arg(private_key) );
        }
        break;
      default:
        this->statusBar()->showMessage("Authentication canceled by user.");
    }
}

bool MainWindow::authenticate(bool silent)
{/*
  * Wrapper around this->ssh.authenticate() providing feedback to the user.
  */
    this->log_call( 1, CALLEE( BOOL2STRING(silent) ) );
    if( this->ssh.authenticated() ) {
        this->log_ << "\n    Already authenticated";
        return true;
    }

    int rc = this->ssh.authenticate();
    QString msg;
    switch (rc) {
    case 0:
        msg = "Authentication successfull.";
        break;
    case toolbox::Ssh::MISSING_USERNAME:
        msg = "Authentication failed: missing username.";
        break;
    case toolbox::Ssh::MISSING_LOGINNODE:
        msg = "Authentication failed: missing login node.";
        break;
    case toolbox::Ssh::MISSING_PRIVATEKEY:
        msg = "Authentication failed: missing private key.";
        break;
    case toolbox::Ssh::NO_INTERNET:
        msg = "Authentication failed: no internet connection.";
        this->is_uptodate_for_.clear();
        break;
    case toolbox::Ssh::LOGINNODE_NOT_ALIVE:
        msg = "Authentication failed: current login node '%1' not alive.";
        msg = msg.arg( this->ssh.login_node() );
        this->is_uptodate_for_.clear();
        break;
    default:
        msg = "Authentication failed: cause unknown.";
        break;
    }
    this->log_ << QString("\n    ").append(msg).toStdString();
    this->statusBar()->showMessage(msg);
    this->update_StatusbarWidgets();
    if( rc==0 )
    {
        QString cluster = this->getSessionConfigItem("wCluster")->value().toString();
        if( this->is_uptodate_for_ != cluster )
        {// get list of available cluster modules
            if( this->ssh.execute("__module_avail",600,"Collect available modules.") == toolbox::Ssh::REMOTE_COMMAND_NOT_DEFINED_BY_CLUSTER ) {
                QString msg = QString("Failed to find __module_avail command for cluster %1.\n"
                                      "This is an error in the cluster's .info file: '%2'.\n"
                                      "Contact support team.")
                                 .arg( this->clusterInfo().name() )
                                 .arg( this->clusterInfo().filename() )
                            ;
                QMessageBox::critical(this,TITLE,msg);
            }
            QStringList modules = this->ssh.standardOutput().split("\n");
            modules.prepend("--- select a module below ---");
            this->ui->wSelectModule->clear();
            this->ui->wSelectModule->addItems(modules);

            QString modules_cluster = QString("modules_").append(this->getSessionConfigItem("wCluster")->value().toString() );
            this->getSessionConfigItem(modules_cluster)->set_choices(modules);

            this->is_uptodate_for_ = cluster;
        }

        return true;
    }
    if( silent || this->proceed_offline_ )
    {// be silent
    } else
    {// make some noise
        switch (rc) {
        case toolbox::Ssh::NO_INTERNET:
        {   int answer = QMessageBox::question
                    ( this, TITLE
                    , msg.append("\nClick 'Proceed offline' if you want to work "
                                 "offline and don't want to see this message again.")
                    ,"Proceed offline" // button0
                    ,"Ok"              // button1
                    , QString()        // button2
                    , 1                // default button
                    );
            this->proceed_offline_ = (answer==0);
        }   break;

        case toolbox::Ssh::LOGINNODE_NOT_ALIVE:
        {   int answer = QMessageBox::question
                    ( this, TITLE
                    , msg.append("\n(If your are working off-campus, you might need "
                                 "VPN to reach the cluster.)"
                                 "\n\nClick 'Proceed offline' if you want to work "
                                 "offline and don't want to see this message again.")
                    ,"Proceed offline" // button0
                    ,"Ok"              // button1
                    , QString()        // button2
                    , 1                // default button
                    );
            this->proceed_offline_ = (answer==0);
        }   break;
        case toolbox::Ssh::MISSING_USERNAME:
        case toolbox::Ssh::MISSING_PRIVATEKEY:
            QMessageBox::warning( this, TITLE, "You must authenticate first (menu Session/Authenticate...)");
        default:
            msg = "Authentication failed: wrong login node/username/private key?";
            QMessageBox::warning( this, TITLE, msg.append("You re-authenticate  (menu Session/Authenticate...)") );
            break;
        }
    }
    return false;
}

bool MainWindow::actionRequiringAuthentication(QString const& action )
{/*
  * verify that authentication is possible, with feedback to the user if automatic authentication fails.
  */
    if( !this->ssh.authenticated() )
    {// attempt automatic authentication (silent)
        if( !this->authenticate() )
        {// automatic authentication fails
            int answer = QMessageBox::question( this, TITLE, "This action requires that you authenticate."
                                              , QString("Cancel '%1'").arg(action)  // button0
                                              ,"Authenticate now..."                // button1
                                              , 0                                   // default button
                                              );
            switch( answer ) {
              case 1: {
                this->authenticateAction_triggered();
                return this->ssh.authenticated();
              }
              default:
                this->statusBar()->showMessage( QString(action).append(" aborted: authentication failed.") );
                return false;
            }
        }
    }
    return true;
}

void MainWindow::localFileLocationAction_triggered()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

 // warn the user of unsaved changes to current script
    if(  this->launcher_.script.has_unsaved_changes()
     || !this->launcher_.script.touch_finished_found()
      )
    {
        QMessageBox::Button answer = QMessageBox::question
                (this,TITLE
                , QString("The current job script %1 has unsaved changes.\n"
                          "(Changes will be lost if you do not save).\n"
                          "Save Job script?")
                 .arg( this->job_folder() )
                , QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel
                , QMessageBox::Cancel
                );
        if( answer==QMessageBox::Cancel ) {
            this->statusBar()->showMessage("Local file location is unchanged.");
            return;
        } else if( answer==QMessageBox::No ) {
            this->statusBar()->showMessage("Current job script %1 NOT saved.");
        } else {// answer==QMessageBox::Yes
            QString filepath = this->local_path_to(JobFolder).append("/pbs.sh");
            if( !this->saveJobscript(filepath) ) {
                return;
            }
        }
    }
    
    this->statusBar()->showMessage("Select an existing directory as a new local file location ...");

    QString new_local_file_location = QFileDialog::getExistingDirectory
            ( this, "Select new local file location...", this->launcher_.homePath("..") );

    if( new_local_file_location.isEmpty()
     || new_local_file_location==this->local_file_location() )
    {// cancel pressed, or same folder selected
        this->statusBar()->showMessage("Local file location is unchanged.");
    } else
    {// new folder selected
        IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
        this->getSessionConfigItem("localFileLocation")->set_value( new_local_file_location );
        this->getSessionConfigItem("wJobname")->set_value("");
        this->getDataConnector    ("wJobname")->value_config_to_widget();

        this->statusBar()->showMessage( QString("Local file location is now '%1'.").arg(new_local_file_location) );
    }
}

void MainWindow::remoteFileLocationAction_triggered()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    cfg::Item* ci = this->getSessionConfigItem("remoteFileLocation");
    QStringList options = ci->choices_as_QStringList();
    QString     current = ci->value().toString();
    int icurrent = options.indexOf(current);
    bool ok;
    QString choice = QInputDialog::getItem( this, TITLE, "Select new remote location:", options, icurrent, /*editable=*/ false, &ok );
    if( ok && !choice.isEmpty() && choice!=current ) {
        ci->set_value(choice);
        this->statusBar()->showMessage( QString("New Remote location is '%1'.").arg(choice) );
    } else {
        this->statusBar()->showMessage( QString("Remote file location is unchanged.") );
    }
}

QString MainWindow::username()  {
    return this->getSessionConfigItem("username")->value().toString();
}

void MainWindow::update_WindowTitle()
{
#ifdef QT_DEBUG
    this->setWindowTitle( QString("%1 %2").arg(TITLE).arg(VERSION) );
#else
    this->setWindowTitle( QString("%1").arg(TITLE);
#endif
}

void MainWindow::update_StatusbarWidgets()
{
    QString s = QString("[%1]")
                   .arg( this->ssh.authenticated()
                       ? ( this->username().append('@').append( this->ssh.login_node() ) )
                       : ( !this->ssh.connected_to_internet() || !this->ssh.login_node_alive()
                         ? QString("offline")
                         : QString("not authenticated")
                         )
                       );
    this->wAuthenticationStatus_->setText(s);

    s = this->job_folder();
    if( this->launcher_.script.has_unsaved_changes() ) {
        s.prepend('*').append('*');
     // pallette not honored on mac?
        this->wJobnameIndicator_->setPalette(*this->paletteRedForeground_);
    } else {
        this->wJobnameIndicator_->setPalette( QApplication::palette( this->wJobnameIndicator_ ) );

    }
    this->wJobnameIndicator_->setText(s);

    s = this->project_folder();
    if( !s.isEmpty() && !s.endsWith('/') ) s.append('/');
    this->wProjectIndicator_->setText(s);
}

 void MainWindow::createTemplateAction_triggered()
{
     this->log_call( 1, CALLEE0 );
     CHECK_IGNORESIGNAL

    if( this->job_folder().isEmpty() ) {
        this->statusBar()->showMessage("No job folder, cannot create template.");
        return;
    }
    QString template_folder = QString("templates/").append( this->job_folder() );
    template_folder = this->launcher_.homePath( template_folder );

    QString msg = QString("Creating job template from job folder '%1' ...")
                     .arg( this->job_folder() );

    QDir qdir_template_folder(template_folder);
    if( qdir_template_folder.exists() )
    {
        int answer = QMessageBox::question( this, TITLE, QString(msg).append("\n(the template folder already exists).")
                                          ,"Cancel"
                                          ,"Overwrite existing template"
                                          ,"Select new template location..."
                                          );
        switch (answer) {
          case 1:
            msg = "Overwriting existing template...";
            this->statusBar()->showMessage(msg);
            break;
          case 2:
            template_folder = this->select_template_location();
            if( !template_folder.isEmpty() )
                break;
            // no break here: we are canceling!
          default:
            msg = "Template creation canceled.";
            this->statusBar()->showMessage(msg);
            return; // without creating the template, since we are canceling.
        }
    } else {
        int answer =
        QMessageBox::question( this, TITLE, msg
                             ,"Cancel"
                             , QString("Create templates/").append( this->job_folder() )
                             ,"Select another template location"
                             );
        switch( answer ) {
          case 1:
            qdir_template_folder.mkpath( QString() );
            msg = QString("Creating 'templates/%1' ... ").arg( this->job_folder() );
            this->statusBar()->showMessage(msg);
            break;
          case 2:
            template_folder = this->select_template_location();
            if( !template_folder.isEmpty() )
                break;
            // no break here: we are canceling!
          default:
            msg = "Template creation canceled.";
            this->statusBar()->showMessage(msg);
            return; // without creating the template, since we are canceling.
        }
    }
 // create the template by copying the entire folder
    QString jobfolder = this->local_path_to(JobFolder);
    if( !jobfolder.isEmpty() ) {
        copy_folder_recursively( jobfolder, template_folder );
        this->statusBar()->showMessage( msg.append( QString("'%1' created.").arg(template_folder) ) );
    }
}

void MainWindow::selectTemplateAction_triggered()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    QString msg;
    if( this->job_folder().isEmpty() ) {
        msg = "You must select a job folder first!";
        QMessageBox::warning(this,TITLE,msg);
        this->statusBar()->showMessage(msg);
        return;
    }
    msg = "Select template folder:";
    this->statusBar()->showMessage(msg);
    QString template_folder = QFileDialog::getExistingDirectory
            ( this, "Select template location (existing or new directory):"
            , this->launcher_.homePath("templates")
            );
    QDir qdir_template_folder(template_folder);
    if( !isNonEmptyDir(qdir_template_folder) ) {
        msg = "Empty folder selected, no template found!";
        QMessageBox::warning(this,TITLE,msg);
        this->statusBar()->showMessage(msg);
        return;
    }
    bool template_folder_contains_pbs_sh = qdir_template_folder.exists("pbs.sh");
    if( !template_folder_contains_pbs_sh ) {
        msg = "The selected folder does not contain a jobscript ('pbs.sh').\n"
              "Continue to use as a job template?";
        QMessageBox::Button answer =
        QMessageBox::question(this,TITLE,msg,QMessageBox::Yes|QMessageBox::No,QMessageBox::No);
        if( answer!=QMessageBox::Yes ) {
            return;
        }
    }
    QString jobfolder = this->local_path_to(JobFolder);
    if( !jobfolder.isEmpty() ) {
        copy_folder_recursively( template_folder, jobfolder );
        if( template_folder_contains_pbs_sh ) {
            this->loadJobscript( QString( jobfolder ).append("/pbs.sh") );
        }
        this->statusBar()->showMessage(msg);
        return;
    }
}

QString MainWindow::select_template_location()
{
    QString msg("Select new template location:");
    this->statusBar()->showMessage(msg);
    QString template_folder = QFileDialog::getExistingDirectory
            ( this, "Select template location (existing or new directory):"
            , this->launcher_.homePath("templates")
            );

    if( !template_folder.isEmpty() )
    {// the user has selected a directory
     // verify that the selected directory is not the templates directory itself.
        if( template_folder==this->launcher_.homePath("templates") )
        {
            msg = QString("Directory '%1' must not be used as a template location. \nYou must select a subdirectory.");
            QMessageBox::warning(this,TITLE,msg);
            template_folder.clear();
        }
        else
        {// the user has selected a valid directory,
            QDir qdir_template_folder(template_folder);
            if( !qdir_template_folder.exists() )
            {// non-existing directory selected, this shouldn't happen, though
                qdir_template_folder.mkpath( QString() );
            } else {// make sure that the directory is empty
                if( isNonEmptyDir(qdir_template_folder) )
                {// ask permission to overwrite
                    msg = QString("Directory '%1' is not empty.\n"
                                  "Overwrite?").arg(template_folder);
                    QMessageBox::Button answer =
                    QMessageBox::question( this,TITLE,msg
                                         , QMessageBox::Cancel|QMessageBox::Yes
                                         , QMessageBox::Yes );
                    if( answer==QMessageBox::Yes )
                    {// clear the selected folder
                        msg = QString("Overwriting directory '%1' ... ").arg(template_folder);
                        this->statusBar()->showMessage(msg);
                     // remove the directory and its contents
                        qdir_template_folder.removeRecursively();
                     // create the directory again, now empty
                        qdir_template_folder.mkpath( QString() );
                    } else {
                        template_folder.clear();
                    }
                }
            }
        }
    }
    return template_folder; // an empty string implies Cancel template creation.
}

QString MainWindow::local_file_location() {
    return this->getSessionConfigItem("localFileLocation")->value().toString();
}
QString MainWindow::project_folder() {
    return this->getSessionConfigItem("wProjectFolder")->value().toString();
}

QString MainWindow::job_folder() {
    return this->getSessionConfigItem("wJobname")->value().toString();
}

QString MainWindow::remote_file_location() {
    return this->getSessionConfigItem("remoteFileLocation")->value().toString();
}

QString MainWindow::vscdata_file_location() {
     QString vscdata( this->getSessionConfigItem("remoteFileLocation")->value().toString() );
     vscdata.replace("$VSC_SCRATCH","$VSC_DATA");
     return vscdata;
}

//void MainWindow::on_wShowLocalJobFolder_clicked()
//{
//    if( this->job_folder().isEmpty() ) {
//        this->statusBar()->showMessage("No job folder is selected, there is nothing to show.");
//    } else {
//        QFileDialog::getOpenFileName( this, TITLE, this->jobs_project_job_path(LOCAL) );
//    }
//}

//void MainWindow::on_wShowRemoteJobFolder_clicked()
//{
//    this->statusBar()->showMessage("This feature is not yet available.");
//}

void MainWindow::newJobscriptAction_triggered()
{
    this->getSessionConfigItem("wJobname")->set_value(NEWJOB);
    this->getDataConnector("wJobname")->value_config_to_script();
    if( !this->isScriptOpen() ) {
        this->ui->wNewJobscriptButton->setVisible(false);
        this->ui->wResourcesGroup    ->setVisible(true );
        this->ui->wScriptPage        ->setEnabled(true );
    }
    this->reloadJobscriptAction_->setEnabled(false);
    this->update_StatusbarWidgets();
}

void MainWindow::script_text_to_config()
{
    QString text = this->ui->wScript->toPlainText();
    this->launcher_.script.parse( text, false );

    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
    for( QList<dc::DataConnectorBase*>::iterator iter=this->data_.begin(); iter!=this->data_.end(); ++iter ) {
        if( (*iter)->value_script_to_config() )
            (*iter)->value_config_to_widget();
    }
 // we must honour signals during on_wRequestNodes_clicked(), otherwise it does nothing.
    this->ignoreSignals_ = false;
    this->on_wRequestNodes_clicked();
    this->ignoreSignals_ = true;

    for( QList<dc::DataConnectorBase*>::iterator iter=this->data_.begin(); iter!=this->data_.end(); ++iter ) {
        if( (*iter)->value_widget_to_config() )
            (*iter)->value_config_to_script();
    }
 // revert to pre-IGNORE_SIGNALS_UNTIL_END_OF_SCOPE state.
}

void MainWindow::saveJobscriptAction_triggered()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    switch( this->current_page_) {
    case 0:
    case MainWindowConstructedButNoPreviousPage:
    {// make sure that the widgets' values are transferred to the script
        for( QList<dc::DataConnectorBase*>::iterator iter=this->data_.begin(); iter!=this->data_.end(); ++iter )
        {
            if( (*iter)->value_widget_to_config() ) {
                (*iter)->value_config_to_script();
            }
        }
    }   break;

    case 1:
    {// make sure that changes to the script text are parsed and saved
        this->log_ << "\n    parsing script text";
        QString text = this->ui->wScript->toPlainText();
        this->script_text_to_config();
    }   break;

    default:
        break;
    }

    if( this->job_folder() == NEWJOB )
    {// forward to saveAs
        this->saveAsJobscriptAction_triggered();
    } else {
        QStringList folders;
        int verify_error;
        QDir dir( this->local_file_location() );
        if( !verify_directory(&dir) ) {
            this->error_message_missing_local_file_location("'save job script' was canceled.");
            return;
        }
        if( !verify_directory(&dir,this->project_folder(),false,true,&folders,&verify_error) ) {
            this->error_message_missing_project_folder("'save job script' was canceled.");
        }
        if( !verify_directory(&dir,this->job_folder(),false,true,&folders,&verify_error) ) {
            this->error_message_missing_job_folder("'save job script' was canceled.");
        }
//        if( !verify_file(&dir,"pbs.sh") ) {
//            this->error_message_missing_job_script("'save job script' was canceled.");
//        }
        QString filepath = this->local_path_to(Script);
        if( !filepath.isEmpty() ) {
            this->saveJobscript( dir.absoluteFilePath("pbs.sh") );
      } else {
            return;
        }
    }
    this->reloadJobscriptAction_->setEnabled(true);
}

void MainWindow::saveAsJobscriptAction_triggered()
{
    this->log_call( 1, CALLEE0 );

    QStringList folders;
    int verify_error;
    QDir dir( this->local_file_location() );
    if( !verify_directory(&dir) ) {
        this->error_message_missing_local_file_location();
        return;
    }
    QString example = this->job_folder();
    if( example == NEWJOB ) {
        example = "[path/to/project_folder/]job_folder";
    }
    bool ok;
    QString projectpath_jobfolder =
            QInputDialog::getText( this,TITLE
                                 ,"Enter a (relative) file path for the new job folder"
                                 , QLineEdit::Normal, example, &ok );
    if( ok && !projectpath_jobfolder.isEmpty() && projectpath_jobfolder!=example )
    {
        if( verify_directory( &dir, projectpath_jobfolder, /*create_path=*/false, /*stay_inside=*/true, &folders, &verify_error ) )
        {
            if( verify_file ( &dir,"pbs.sh") )
            {// ask permission to overwrite
                QString msg = QString("The folder '%1' already contains a jobscript.\n"
                                      "Do you want to overwrite it?")
                                 .arg(dir.path());
                QMessageBox::StandardButton answer
                        = QMessageBox::question( this, TITLE, msg
                                               , QMessageBox::Yes|QMessageBox::Cancel
                                               , QMessageBox::Cancel
                                               );
                if( answer != QMessageBox::Yes ) {
                    this->statusBar()->showMessage("Canceled: job script is NOT saved.");
                    return;
                }
            }
        } else {
            if( !verify_directory( &dir, projectpath_jobfolder, /*create_path=*/true, /*stay_inside=*/true, &folders, &verify_error ) ) {
                QString msg = QString("Could not create directory '%1/%2'.\n"
                                      "Do you have sufficient permissions?")
                                 .arg( this->local_file_location() )
                                 .arg( projectpath_jobfolder )
                                 ;
                this->message(msg,QMessageBox::critical);
                return;
            }
        }
    }
 // do the save
    this->getSessionConfigItem("wProjectFolder")->set_value(folders.at(1));
    this->getSessionConfigItem("wJobname"      )->set_value(folders.at(2));
    this->saveJobscript( dir.absoluteFilePath("pbs.sh") );
    if( this->use_git_sync_ ) {
//        this->sshSession_.git_local_save( this->local_path_to(JobFolder) );
    }
}

void MainWindow::openJobscriptAction_triggered()
{
 // first check for unsaved changes to the current jobscript
    if( this->isScriptOpen() && this->launcher_.script.has_unsaved_changes() )
    {
        int answer = QMessageBox::question(this,TITLE
                                          ,"The current job script has unsaved changes.\nDo you want to:"
                                          ,"Save changes and continue"
                                          ,"Continue without saving"
                                          ,"Cancel"
                                          , 2
                                          );
        switch( answer ) {
        case 0:
            this->statusBar()->showMessage("'Job script Open...' canceled, current job script still has unsaved changes!");
            return;
        case 1:
            this->statusBar()->showMessage("Continuing, previous job script was not saved!");
            break;
        default:
            this->saveJobscriptAction_triggered();
            break;
        }
    }
    QStringList folders;
    int verify_error;
    QDir dir( this->local_file_location() );
    if( !verify_directory(&dir) ) {
        this->error_message_missing_local_file_location();
        return;
    }

    QString jobfolder =
        QFileDialog::getExistingDirectory( this, "Select a job folder file"
                                         , this->getSessionConfigItem("localFileLocation")->value().toString()
                                         );
    if( jobfolder.isEmpty() ) {
        this->statusBar()->showMessage("'Job script Open...' canceled.");
        return;
    }
    if( !verify_directory(&dir,jobfolder,/*create_path=*/false,/*stay_inside=*/true,&folders,&verify_error) ) {
        QString msg;
        switch(verify_error) {
        case Outside:
            msg = "You must select a directory inside the local file location.";
            break;
        case Inexisting:
            msg = "You must select an existing directory or create a new one, inside the local file location.";
            break;
        default:
            break;
        }
        this->error_message_missing_job_folder(msg);
        return;
    }
    if( verify_file(&dir,"pbs.sh") ) {
        this->ui->wNewJobscriptButton->setVisible(false);
        this->ui->wResourcesGroup    ->setVisible(true );
        this->ui->wScriptPage        ->setEnabled(true );

     this->getSessionConfigItem("wProjectFolder")->set_value(folders.at(1));
        this->getSessionConfigItem("wJobname"      )->set_value(folders.at(2));
        this->loadJobscript( dir.filePath("pbs.sh") );

    } else {
        QString msg = QString("No job script found in Folder '%1'. \n"
                              "Create a new one?")
                         .arg( dir.absolutePath() );
        QMessageBox::StandardButton answer = QMessageBox::question
                ( this,TITLE,msg
                , QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Cancel
                );
        if( answer==QMessageBox::Yes ) {
            this->newJobscriptAction_triggered();
            this->getSessionConfigItem("wProjectFolder")->set_value(folders.at(1));
            this->getSessionConfigItem("wJobname"      )->set_value(folders.at(2)); // overwrite __NEW_JOB__ with wJobname
            this->update_StatusbarWidgets();
            this->statusBar()->showMessage("Click menu Job/Save to create the job script.");
        } else {
            this->statusBar()->showMessage("'Open job script...' Canceled.");
        }
    }
}

void MainWindow::reloadJobscriptAction_triggered()
{// check that the path to the jobfolder and the job script exist
    QStringList folders;
    int verify_error;
    QDir dir( this->local_file_location() );
    if( !verify_directory(&dir) ) {
        this->error_message_missing_local_file_location("'reload job script' was canceled.");
        return;
    }
    if( !verify_directory(&dir,this->project_folder(),false,true,&folders,&verify_error) ) {
        this->error_message_missing_project_folder("'reload job script' was canceled.");
    }
    if( !verify_directory(&dir,this->job_folder(),false,true,&folders,&verify_error) ) {
        this->error_message_missing_job_folder("'reload job script' was canceled.");
    }
    if( !verify_file(&dir,"pbs.sh") ) {
        this->error_message_missing_job_script("'reload job script' was canceled.");
    }
 // check for unsaved changes
    QMessageBox::StandardButton answer = QMessageBox::Yes;
    if( this->launcher_.script.has_unsaved_changes() ) {
        answer = QMessageBox::question( this, TITLE, "Unsaved changes to the current job script will be lost on reloading. Continue?");
    }
    if( answer==QMessageBox::Yes ) {
        this->loadJobscript(dir.absoluteFilePath("pbs.sh"));
        for( QList<dc::DataConnectorBase*>::iterator iter=this->data_.begin(); iter!=this->data_.end(); ++iter )
        {
            if( (*iter)->value_widget_to_config() ) {
                (*iter)->value_config_to_script();
            }
        }
        QString text = this->launcher_.script.text();
        this->ui->wScript->setText(text);
     // no unsaved changes since read from file, but the above iteration over DataConnectors
     // requires that it is explicitly reset.
        this->launcher_.script.set_has_unsaved_changes(false);
        this->check_script_unsaved_changes();
        this->statusBar()->showMessage("job script reloaded.");
    } else {
        this->statusBar()->showMessage("reload job script canceled by user.");
    }
}

void MainWindow::submitJobscriptAction_triggered()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    if( !this->actionRequiringAuthentication("Submit job") ) {
        return;
    }

    QString canceled = QString("Submit job '%1' CANCELED!").arg( this->job_folder() );

    if( this->job_folder()==NEWJOB ) {
        QMessageBox::critical(this,TITLE,"You must save your job script First.\n"
                                         "(menu item Job script/Save)");
        this->statusBar()->showMessage(canceled);
        return;
    }

    QString remote_job_folder = this->remote_path_to(JobFolder);
    QString  local_job_folder = this-> local_path_to(JobFolder,canceled);
    if(  this->launcher_.script.has_unsaved_changes()
     || !this->launcher_.script.touch_finished_found()
      )
    {
        QMessageBox::Button answer = QMessageBox::question(this,TITLE,"The job script has unsaved changes.\nSave Job script?"
                                                          , QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel
                                                          , QMessageBox::Cancel
                                                          );
        if( answer==QMessageBox::Cancel ) {
            this->statusBar()->showMessage(canceled);
            return;
        } else
        if( answer==QMessageBox::No ) {
            this->statusBar()->showMessage("Job script not saved.");
        } else {// Yes
            QString filepath = this->local_path_to(Script);
            if( !this->saveJobscript(filepath) ) {
                return;
            }
        }
    }

    int rc = this->ssh.local_sync_to_remote( local_job_folder, remote_job_folder );
    if( rc ) {
        this->statusBar()->showMessage("Local to remote synchronization failed. Job NOT submitted.");
        return;
    }
    if( this->current_page_==3 ) {
        this->on_wRefreshLocalFileView_clicked();
        this->on_wRefreshRemoteFileView_clicked();
    } else {
        this->ui-> wLocalJobFolderHasDotGit->setChecked(true);
        this->ui->wRemoteJobFolderHasDotGit->setChecked(true);
    }
/*
 // test if the remote job folder is inexisting or empty
    QString cmd("ls %1");
    int rc = this->ssh.execute( cmd, remote_job_folder );
    if( rc==0 && !this->ssh.qout().isEmpty() )
    {// ask the user if he wants to empty the remote job folder first
        int answer =
                QMessageBox::question( this,TITLE
                                     ,"The remote job folder is not empty."
                                      "Do you want to erase the remote job folder first?\n"
                                      "(If you answer No, new files will be added and "
                                      "existing files will be overwritten)."
                                     ,"Cancel"                          // button0
                                     ,"Keep remote job folder contents" // button1
                                     ,"Erase remote job folder contents"// button2
                                     , 0                                // default button
                                     );
        switch(answer) {
          case 1:
            this->statusBar()->showMessage("Remote job folder not erased. Files are added or overwritten.");
            break;
          case 2:
          { cmd = QString("rm -rf %1/*");
            int rc = this->ssh.execute_remote_command( cmd, remote_job_folder );
            if( rc==0 ) {
                this->statusBar()->showMessage("Erased remote job folder.");
            } else {
                while( rc != 0 )
                {
                    int answer = QMessageBox::question( this,TITLE,"An error occurred while erasing remote job folder contents"
                                                      ,"Retry"          // button0
                                                      ,"Ignore"         // button1
                                                      ,"Cancel submit"  // button2
                                                      , 0 );             // default button
                    switch(answer) {
                      case 0: // retry
                        rc = this->ssh.execute_remote_command( cmd );
                        break; // out of switch, but continue with while loop
                      case 1: // ignore
                        rc = 0; // break out of while loop
                        break;  // break out of switch
                      case 2:
                      default:
                        this->statusBar()->showMessage(canceled);
                        return;
                    }
                }
            }
            break;
          }
          default:
            this->statusBar()->showMessage(canceled);
            return;
        }
    }
    this->ssh.sftp_put_dir( local_job_folder, remote_job_folder, true );
*/

    QString cmd = QString("cd %1 && qsub pbs.sh").arg( remote_job_folder );
    rc = this->ssh.execute( cmd, 300, QString("Submitting job: ").append( local_job_folder ) );
    if( rc ) {
        this->statusBar()->showMessage("Submit failed.");
        return;
    }
    QString job_id = this->ssh.standardOutput().trimmed();
    this->statusBar()->showMessage(QString("Job submitted: %1").arg(job_id));
    cfg::Item* ci_job_list = this->getSessionConfigItem("job_list");
    if( ci_job_list->value().type()==QVariant::Invalid )
        ci_job_list->set_value_and_type( QStringList() );
    QStringList job_list = ci_job_list->value().toStringList();

    Job job( job_id, this->local_file_location(), this->remote_file_location()
           , this->project_folder()
           , this->getSessionConfigItem("wJobname"      )->value().toString()
           , Job::Submitted );
    job_list.append( job.toString() );

    ci_job_list->set_value( job_list );
}

void MainWindow::showFileLocationsAction_triggered()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    QString job = this->job_folder();
    if( job.isEmpty() ) {
        job = "<None>";
    }
    QString msg = QString("File locations for job name '%1':").arg(job)
                  .append("\n\nlocal: \n  ").append( this-> local_path_to(JobFolder) )
                  .append("\n\nremote;\n  ").append( this->remote_path_to(JobFolder,false) );
    QMessageBox::information(this, TITLE, msg, QMessageBox::Close, QMessageBox::Close );
}

void MainWindow::showLocalJobFolderAction_triggered()
{

}

void MainWindow::showRemoteJobFolderAction_triggered()
{

}


//bool MainWindow::is_in_local_file_location( QString const& folder, QString *absolute_folder )
//{/* verify that job_folder is in the local_file_location.
//  * If not, a message is sent to the the statusBar.
//  * if absolute_folder is not null it contains the absolute file path without
//  * redundant '/', './' or '../'
//  */
//    QString absolute_folder_;
//    if( is_absolute_path(folder) ) {
//        absolute_folder_ = QDir::cleanPath(folder);
//    } else
//    {// relative path, make absolute
//        QDir parent( this->local_file_location() );
//        absolute_folder_ = QDir::cleanPath( parent.absoluteFilePath(folder) );
//    }
//    if( absolute_folder ) {
//        *absolute_folder = absolute_folder_;
//    }
//    if( absolute_folder->startsWith( this->local_file_location() ) ) {
//        return true;
//    } else {
//        QString msg = QString("Folder '%1' is located outside the local file location '%2'.")
//                         .arg(absolute_folder_)
//                         .arg(this->local_file_location());
//        this->statusBar()->showMessage(msg);
//        return false;
//    }
//}


void MainWindow::on_wNewJobscriptButton_clicked()
{
    this->log_call( 1, CALLEE0 );
    CHECK_IGNORESIGNAL

    this->newJobscriptAction_triggered();
}

QMessageBox::StandardButton
MainWindow::
message( QString msg, message_box_t message_box ) {
   QMessageBox::StandardButton answer = (*message_box)(this,TITLE,msg,QMessageBox::Ok,QMessageBox::Ok);
   return answer;
}

void
MainWindow::
error_message_missing_local_file_location
  ( QString const& msg1
  , message_box_t message_box )
{
    QString tmp = this->local_file_location();
    QString msg;
    if( tmp.isEmpty() ) {
        msg = "Local file location is empty string.";
    } else {
        msg = QString("Inexisting local file location: '%1'")
                 .arg( tmp );
    }
    if( !msg1.isEmpty() ) {
        msg.append("\n\n").append(msg1);
    }
    message(msg,message_box);
}

void
MainWindow::
error_message_missing_project_folder
  ( QString const& msg1
  , message_box_t message_box )
{
    QString tmp = this->project_folder();
    QString msg = QString("Inexisting project folder: '%1'"
                          "(Local file location:\n    '%2'")
                     .arg( tmp )
                     .arg( this->local_file_location() )
                     ;
    if( !msg1.isEmpty() ) {
        msg.append("\n\n").append(msg1);
    }
    message(msg,message_box);
}

void
MainWindow::
error_message_missing_job_folder
  ( QString const& msg1
  , message_box_t message_box )
{
    QString tmp = this->job_folder();
    QString msg;
    if( tmp.isEmpty() ) {
        msg = "Jobname is empty string.";
    } else {
        msg = QString("Inexisting job folder:\n    '%1'"
                      "(Project folder:\n    '%2'"
                      "Local file location:\n    '%3')")
                 .arg( tmp )
                 .arg( this->project_folder() )
                 .arg( this->local_file_location() )
                 ;
    }
    if( !msg1.isEmpty() ) {
        msg.append("\n\n").append(msg1);
    }
    message(msg,message_box);
}

void
MainWindow::
error_message_missing_job_script
  ( QString const& msg1
  , message_box_t message_box )
{
    QString msg;
    msg = QString("Inexisting job script:\n    'pbs.sh'"
                  "(Job folder:\n    '%1'"
                  "Project folder:\n    '%2'"
                  "Local file location:\n    '%3')")
             .arg( this->job_folder() )
             .arg( this->project_folder() )
             .arg( this->local_file_location() )
             ;
    if( !msg1.isEmpty() ) {
        msg.append("\n\n").append(msg1);
    }
    message(msg,message_box);
}

float kB( int B ) {
    return B/1000.;
}

float MB( int B ) {
    return B/1000000.;
}

void
MainWindow::
removeRepoAction_triggered()
{
    this->log_call( 1, CALLEE0);
    this->statusBar()->clearMessage();

    if( ! this->isScriptOpen() ) {
        this->statusBar()->showMessage("Action 'Remove job folder repository' canceled: no job script open.");
        return;
    }
    if( !this->actionRequiringAuthentication("Remove job folder repository") ) {
        return;
    }

    QString  local_job_folder = this-> local_path_to(JobFolder);
    QString remote_job_folder = this->remote_path_to(JobFolder);
    QString cmd;

    int sz0 = this->ssh.remote_size( remote_job_folder );
    QString size;
    if( sz0>=0 ) {
        size = QString(" ( currently %1 kB)").arg( kB(sz0) );
    }
    QString msg = QString("If you remove the job folder repository '%1', you discard\n"
                          "  . the history of your job folder\n"
                          "  . all contents of the remote job folder\n"
                          "This action is typically used to reduce the size of the (remote) "
                          "repository%2.\n\n"
                          "Do you want to remove the job folder repository?")
                     .arg( this->local_path_to(JobFolder) )
                     .arg( size )
                     ;
    QMessageBox::StandardButton answer = QMessageBox::question
        ( this, TITLE, msg, QMessageBox::Yes|QMessageBox::No, QMessageBox::No );
    if( answer!=QMessageBox::Yes ) {
        this->statusBar()->showMessage("Action 'Remove job folder repository' canceled.");
        return;
    }
    QString msg_local;
    QString msg_remote;
    bool ok  = this->remove_repo_local (&msg_local );
         ok &= this->remove_repo_remote(&msg_remote);
    this->statusBar()->showMessage
            ( QString("remove job folder repository [local:%1][remote:%2]")
                 .arg(msg_local).arg(msg_remote)
            );

    if( !this->ssh. local_exists( local_job_folder)
     && !this->ssh.remote_exists(remote_job_folder) )
    {
        QString msg = QString("Remove job folder repository finished successfully.\n"
                              "Do you want to create a new job folder repository (based "
                              "on the contents of the local job folder)?\nIf not it will "
                              "automatically be created when you submit.")
//                         .arg( this->local_path_to(JobFolder) )
//                         .arg( size )
                         ;
        QMessageBox::StandardButton answer = QMessageBox::question
            ( this, TITLE, msg, QMessageBox::Yes|QMessageBox::No, QMessageBox::No );
        if( answer!=QMessageBox::Yes ) {
            this->statusBar()->showMessage("Action 'Remove job folder repository' canceled.");
            return;
        }
    }
}


void
MainWindow::
createRepoAction_triggered()
{
    this->log_call( 1, CALLEE0);
    this->statusBar()->clearMessage();

    if( ! this->isScriptOpen() ) {
        this->statusBar()->showMessage("Action 'Remove job folder repository' canceled: no job script open.");
        return;
    }
    if( !this->actionRequiringAuthentication("Remove job folder repository") ) {
        return;
    }

    if( this->ssh.local_exists( this->local_path_to(JobFolder) ) ) {
        this->statusBar()->showMessage("Canceled: local job folder repository already existing");
    }
    if( this->ssh.remote_exists( this->remote_path_to(JobFolder) ) ) {
        this->statusBar()->showMessage("Canceled: remote job folder repository already existing");
    }
    QString msg("create job folder repository ");
    QString msg_local;
    QString msg_remote("local-repo-missing");
    if( this->create_repo_local (&msg_local ) ) {
        this->create_repo_remote(&msg_remote);
    }
    this->statusBar()->showMessage
        ( QString("create job folder repository [local:%1][remote:%2]")
             .arg(msg_local).arg(msg_remote)
        );
}

bool MainWindow::remove_repo_local( QString* pmsg )
{
    QString local_job_folder = this->local_path_to(JobFolder);
    this->log_call( 1, CALLEE0, QString("\n    ").append(local_job_folder) );

    bool exists = this->ssh.local_exists(local_job_folder);
    bool ok = exists;
    if( exists )
    {
#if defined(Q_OS_WIN) || defined(LOCAL_REMOVE_REPO_WITH_RM)
     // to remove the .git directory we use the rm provided with git.
     // . QDir::removeRecursively() does not delete ./git/objects on windows
     // . del and rmdir do not work as an os command in toolbox::Execute
        toolbox::Execute x(nullptr,&this->log_);
        x.set_working_directory(local_job_folder);
        QString cmd = QString("rm -rf .git");
        int rc = x(cmd,300,"remove local job folder repository");
        ok = (rc==0);
#else
        ok = dir.removeRecursively();
#endif
    }
    QString msg = ( exists
                  ? ( ok ? "removed" : "remove-failed" )
                  : "inexisting"
                  );
    this->log_ << QString("\n    ").append(msg).toStdString();
    if( pmsg ) {
        *pmsg = msg;
    }
    if( ok ) {
        if( this->current_page_==3 ) {
            this->on_wRefreshLocalFileView_clicked();
        } else {
            this->ui->wLocalJobFolderHasDotGit->setChecked(false);
        }
    }
    return ok;
}

bool MainWindow::remove_repo_remote( QString* pmsg )
{
    QString remote_job_folder = this->remote_path_to(JobFolder);
    this->log_call( 1, CALLEE0, QString("\n    ").append(remote_job_folder) );

    QString cmd = QString("rm -rf %1").arg( remote_job_folder );
    int rc = this->ssh.execute(cmd,300,"remove remote job folder (because of remove job folder repository)");
    QString msg( rc==0 ? "removed":"failed-to-remove");

    this->log_ << QString("\n    ").append(msg).toStdString();

    bool ok = (rc==0);
    if( pmsg ) {
        *pmsg = msg;
    }
    if( ok ) {
        if( this->current_page_==3 ) {
            this->on_wRefreshRemoteFileView_clicked();
        } else {
            this->ui->wRemoteJobFolderHasDotGit->setChecked(false);
        }
    }
    return ok;
}

bool MainWindow::create_repo_local( QString* pmsg )
{
    QString local_job_folder = this->local_path_to(JobFolder);
    this->log_call( 1, CALLEE0, QString("\n    ").append(local_job_folder) );

    int rc = this->ssh.local_save(local_job_folder);
    QString msg( rc==0 ? "created" : "failed-to-create");

    this->log_ << QString("\n    ").append(msg).toStdString();

    bool ok = (rc==0);
    if( pmsg ) {
        *pmsg = msg;
    }
    if( ok ) {
        if( this->current_page_==3 ) {
            this->on_wRefreshLocalFileView_clicked();
        } else {
            this->ui-> wLocalJobFolderHasDotGit->setChecked(true);
        }
    }
    return ok;
}

bool MainWindow::create_repo_remote( QString* pmsg )
{
    QString local_job_folder = this->local_path_to(JobFolder);
    QString remote_job_folder = this->remote_path_to(JobFolder);
    this->log_call( 1, CALLEE0, QString("\n    local : ").append( local_job_folder)
                                .append("\n    remote: ").append(remote_job_folder)
                    );

    QString msg;
    QString cmd = QString("mkdir -p %1").arg( remote_job_folder );
    int rc = this->ssh.execute(cmd,300,"recreate remote job folder)");
    if( rc ) {
        msg = "mkdir-failed";
    } else
    {// remote recreate
        rc = this->ssh.local_sync_to_remote(local_job_folder,remote_job_folder,/*save_first=*/false);
        msg = ( rc==0 ? "synchronized" : "failed-to-synchronize");
    }

    this->log_ << QString("\n    ").append(msg).toStdString();

    bool ok = (rc==0);
    if( pmsg ) {
        *pmsg = msg;
    }
    if( ok ) {
        if( this->current_page_==3 ) {
            this->on_wRefreshRemoteFileView_clicked();
        } else {
            this->ui->wRemoteJobFolderHasDotGit->setChecked(true);
        }
    }
    return ok;
}


void MainWindow::on_wRefreshLocalFileView_clicked()
{
    if( !this->isScriptOpen() ) {
        return;
    }
    this->log_call(1,CALLEE0);

    QString s = this->local_path_to(JobFolder);
    this->ui->wRefreshLocalFileView->setText( QString("Refresh ").append(s) );

    if(this->file_system_model ) {
        delete this->file_system_model;
    }
    this->file_system_model = new QFileSystemModel(this);
    this->file_system_model->setFilter(QDir::AllEntries|QDir::NoDotAndDotDot);
    this->file_system_model->setRootPath(s);
    this->ui->wLocalFileView->setModel(this->file_system_model);

    if( !s.isEmpty()) {
        const QModelIndex rootIndex = this->file_system_model->index(QDir::cleanPath(s));
        if( rootIndex.isValid() )
            this->ui->wLocalFileView->setRootIndex(rootIndex);
    }
//    tree.setIndentation(20);
    this->ui->wLocalFileView->setSortingEnabled(true);

    this->statusBar()->showMessage("tree");
    bool has_dot_git = QDir(s).cd(".git");
    this->ui->wLocalJobFolderHasDotGit->setChecked(has_dot_git);
//
    this->ui->wLocalJobFolderHasDotGit->setEnabled(false);
}

void MainWindow::on_wRefreshRemoteFileView_clicked()
{
    if( !this->isScriptOpen() ) {
        return;
    }
    this->log_call(1,CALLEE0);

    QString txt;
    bool has_dot_git = false;

    QString remote_job_folder = this->remote_path_to(JobFolder);
    this->ui->wRefreshRemoteFileView->setText( QString("Refresh ").append(remote_job_folder) );
    if( this->ssh.authenticated() ) {
        QString cmd = QString("tree ").append( remote_job_folder );
        this->ssh.execute(cmd,300,"get tree of remote root folder");
        txt = this->ssh.standardOutput();
        cmd = QString("ls %1/.git").arg( remote_job_folder );
        int rc = this->ssh.execute(cmd,300,"has .git?");
        has_dot_git = (rc==0);
    } else {
        txt = "You must authenticate to see the remote files location.";
    }
    this->ui->wRemoteFileView->setPlainText(txt);
    this->ui->wRemoteJobFolderHasDotGit->setChecked(has_dot_git);
//    this->ui->wLocalJobFolderHasDotGit->setEnabled(false);
}
