#define QT_DEBUG
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QTextStream>
#include <QMessageBox>

#include <warn_.h>
#include <ssh2tools.h>
#include <cmath>
#include <time.h>

#define NEWJOB "__new_job__"
#define STAY_IN_LOCAL_FILE_LOCATION
#define OFFLINE "Offline"
#define INITIAL_VERBOSITY 2
#define NO_ARGUMENT QString()
#define LOG_AND_CHECK_IGNORESIGNAL( ARGUMENT ) \
  {\
    QString msg("\n[ MainWindow::%1, %2, %3 ]");\
    msg = msg.arg(__func__).arg(__FILE__).arg(__LINE__);\
    if( this->verbosity_ && !QString((ARGUMENT)).isEmpty() ) \
        msg.append("\n    Function was called with argument : ").append(ARGUMENT);\
    if( !this->ignoreSignals_ ) {\
        msg.append( this->check_script_unsaved_changes() ) ;\
    }\
    if( this->verbosity_ > 1 ) {\
        if( this->ignoreSignals_ ) msg.append("\n    Ignoring signal");\
        else                       msg.append("\n    Executing signal");\
    }\
    Log() << INFO << msg.toStdString();\
    if( this->ignoreSignals_ ) return;\
  }

 //-----------------------------------------------------------------------------
#define LOG_CALLER_INFO \
    QString caller = QString("\n[ MainWindow::%1, %2, %3 ]") \
                     .arg(__func__).arg(__FILE__).arg(__LINE__);\
    Log() << INFO << caller.C_STR()

#define LOG_CALLER(VERBOSITY) \
    QString caller = QString("\n    [ MainWindow::%1, %2, %3 ]") \
                     .arg(__func__).arg(__FILE__).arg(__LINE__);\
    Log(VERBOSITY) << caller.C_STR()

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
    MainWindow::MainWindow(QWidget *parent)
      : QMainWindow(parent)
      , ui(new Ui::MainWindow)
      , ignoreSignals_(false)
      , current_page_(MainWindowUnderConstruction)
      , verbosity_(INITIAL_VERBOSITY)
      , walltime_(nullptr)
      , pendingRequest_(NoPendingRequest)
      , messages_(TITLE)
      , proceed_offline_(false)
    {
        LOG_CALLER_INFO << "\n    Running Launcher version = "<<VERSION;
        ui->setupUi(this);

        createActions();
        createMenus();

        ssh2::Session::autoOpen = true;

        this->paletteRedForeground_ = new QPalette();
        this->paletteRedForeground_->setColor(QPalette::Text,Qt::red);

        if( !QFile(this->launcher_.homePath("Launcher.cfg")).exists() )
        {// This must be the first time after installation...
            this->setupHome();
        }
        this->update_WindowTitle();

     // permanent widgets on statusbar
        this->wAuthentIndicator_ = new QLabel();
        this->wJobnameIndicator_ = new QLabel();
        this->wProjectIndicator_ = new QLabel();
        this->statusBar()->addPermanentWidget(this->wProjectIndicator_);
        this->statusBar()->addPermanentWidget(this->wJobnameIndicator_);
        this->statusBar()->addPermanentWidget(this->wAuthentIndicator_);
        this->wAuthentIndicator_->setToolTip("Authentication indicator");
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
        this->ui->wNotFinished->setFont(font);
        this->ui->wRetrieved  ->setFont(font);
#endif
     // keep this the last line
        current_page_ = MainWindowConstructedButNoPreviousPage;
    }

    void MainWindow::createActions()
    {
        aboutAction_ = new QAction("&About", this);
        connect( aboutAction_, SIGNAL(triggered()), this, SLOT( aboutAction_triggered() ) );

        verboseAction_ = new QAction("Verbose logging", this);
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
    }

    void MainWindow::createMenus()
    {
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
        jobMenu_ ->addAction(showLocalJobFolderAction_);
        jobMenu_ ->addAction(showRemoteJobFolderAction_);

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
    }

    void MainWindow::aboutAction_triggered()
    {
        LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

        QString msg;
        msg.append( QString("program : %1\n").arg(TITLE  ) )
           .append( QString("version : %1\n").arg(VERSION) )
           .append(         "contact : engelbert.tijskens@uantwerpen.be")
           ;
        QMessageBox::about(this,TITLE,msg);
    }

    void MainWindow::verboseAction_triggered()
    {
        LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);
        this->verbosity_ = ( this->verboseAction_->isChecked() ? 2 : 0 );
    }

    void MainWindow::newSessionAction_triggered()
    {
        LOG_AND_CHECK_IGNORESIGNAL( NO_ARGUMENT );

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
            this->new_session(session_data);
        } else {
            Log()<<"\n    Canceled.";
        }
    }

    void MainWindow::new_session( QString const& path )
    {
        LOG_CALLER(0) << QString("\n      path; '%1'").arg(path).C_STR();
        this->launcher_.session_config.clear();
        this->setup_session();
        this->launcher_.session_config.save(path);
        this->save_session_path(path);
    }

    void MainWindow::save_session_path( QString const& path )
    {
        LOG_CALLER(0) << QString("\n      path; '%1'").arg(path).C_STR();
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
        LOG_AND_CHECK_IGNORESIGNAL( NO_ARGUMENT );

        QString path_to_session_data =
                QFileDialog::getOpenFileName( this, "Select a .data file"
                                            , this->launcher_.homePath()
                                            ,"Launcher sessions (*.data)"
                                            );
        if( !path_to_session_data.isEmpty() ) {
            this->open_session(path_to_session_data);
        } else {
            Log()<<"\n    Canceled.";
        }
    }

    void MainWindow::open_session( QString const& path_to_session_data )
    {
        LOG_CALLER(0) << QString("\n      path; '%1'").arg(path_to_session_data).C_STR();

        this->launcher_.session_config.load(path_to_session_data);
        this->setup_session();

        QString msg = QString("Current Launcher session '%1'.").arg(path_to_session_data);
        this->statusBar()->showMessage(msg);
    }

    void MainWindow::setup_session( )
    {
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
#ifndef NO_PUBLIC_KEY_NEEDED
        this->getSessionConfigItem("publicKey" , QString() );
#endif
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
            QString msg("No clusters found (.info files).\n   ABORTING!");
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
//        QString path;
//        try {
//            path = this->jobs_project_job_path(LOCAL);
//            this->getSessionConfigItem("localFolder" )->set_value_and_type( path );
//            this->getSessionConfigItem("remoteFolder")->set_value_and_type( this->jobs_project_job_path( REMOTE, false ) ); // no need to store resolved path
//        } catch (std::runtime_error &e ) {
//            Log() << "\n    Error: "<< e.what();
//            this->getSessionConfigItem("wJobname")->set_value("");
//        }


        this->data_.append( dc::newDataConnector("wJobname"      , "-N"           ) );
        this->data_.append( dc::newDataConnector("localFolder"   , "local_folder" ) );
        this->data_.append( dc::newDataConnector("remoteFolder"  , "remote_folder") );

        this->getSessionConfigItem("wCheckCopyToDesktop", true );
        this->data_.append( dc::newDataConnector( this->ui->wCheckCopyToDesktop, "wCheckCopyToDesktop", "") );

        this->getSessionConfigItem("CheckCopyToVscData", true );
        this->data_.append( dc::newDataConnector( this->ui->wCheckCopyToVscData, "CheckCopyToVscData", "") );

        this->getSessionConfigItem("wCheckDeleteLocalJobFolder", true );
        this->data_.append( dc::newDataConnector( this->ui->wCheckDeleteLocalJobFolder, "wCheckDeleteLocalJobFolder", "") );

        this->getSessionConfigItem("wCheckDeleteRemoteJobFolder", true );
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
    }

    bool MainWindow::isScriptOpen() const {
        return !this->ui->wResourcesGroup->isHidden();
    }

    void MainWindow::saveSessionAction_triggered()
    {
        LOG_AND_CHECK_IGNORESIGNAL( NO_ARGUMENT );

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
            this->save_session( path_to_session_data );
        } else {
            Log() << "\n    canceled.";
        }
    }

    void MainWindow::saveAsSessionAction_triggered()
    {
        LOG_AND_CHECK_IGNORESIGNAL( NO_ARGUMENT );

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
//            Log() << "\n    canceled.";
//        }
    }

    void MainWindow::save_session( QString const& path_to_session_data )
    {
        LOG_CALLER(0) << QString("\n      path: '%1'").arg( path_to_session_data ).C_STR();
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
//            if( resolve && path.startsWith('$') && this->sshSession_.isAuthenticated() ) {
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
    QString cmd("echo %1");
    try {
        this->sshSession_.execute_remote_command(cmd,envvar);
        QString resolved = this->sshSession_.qout().trimmed();
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
        if( resolve && path.startsWith('$') && this->sshSession_.isAuthenticated() ) {
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
//            if( resolve && path.startsWith('$') && this->sshSession_.isAuthenticated() ) {
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
        LOG_CALLER_INFO
         << "\n    Launcher closing down";

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
                Log(1) << "\n    - Calling save()";
                this->saveJobscriptAction_triggered();
            } else {
                Log(1) << "\n    - Discarding unsaved changes to script.";
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
        ssh2::Session::cleanup();

        delete this->paletteRedForeground_;
        Log() << "\n    Launcher closed.";
    }
 //-----------------------------------------------------------------------------
    void MainWindow::setIgnoreSignals( bool ignore )
    {
        if( this->verbosity_ > 2 ) {
            if( ignore ) {
                if(!this->ignoreSignals_ ) Log(1) << "\n>>> ignoring signals";
            } else {
                if( this->ignoreSignals_ ) Log(1) << "\n>>> stopped ignoring signals";
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
        LOG_CALLER_INFO;
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

            this->sshSession_.setLoginNode( clusterInfo.loginNodes()[0] );
            this->sshSession_.execute_remote_command.set_remote_commands( clusterInfo.remote_commands() );
        }       
     // try to authenticate to refresh the list of available modules and the
     // remote file locations $VSC_DATA and $VSC_SCRATCH
        this->sshSession_.setUsername( this->username() );
        this->sshSession_.setPrivatePublicKeyPair( this->getSessionConfigItem("privateKey")->value().toString()
#ifndef NO_PUBLIC_KEY_NEEDED
                                                 , this->getSessionConfigItem("publicKey")->value().toString()
#endif
                                                 , /*must_throw=*/ false );
        this->authenticate();
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
        QString msg;
        QTextStream ts(&msg);
        ts << "\n    Checking unsaved changes : "
           << ( this->launcher_.script.has_unsaved_changes() ? "true" : "false");

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
    LOG_AND_CHECK_IGNORESIGNAL( arg1 );

    this->launcher_.session_config["wCluster"].set_value(arg1);
    this->clusterDependencies(true);
}

void MainWindow::on_wNodeset_currentTextChanged(const QString &arg1)
{
    LOG_AND_CHECK_IGNORESIGNAL( arg1 );

    this->getSessionConfigItem("wNodeset")->set_value(arg1);
    this->nodesetDependencies(true);
}

void MainWindow::updateResourceItems()
{
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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

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
    LOG_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") );

    bool manual = !checked;
    this->ui->wRequestNodes->setEnabled(manual);
    this->ui->wRequestCores->setEnabled(manual);
}

void MainWindow::on_wNNodes_valueChanged(int arg1)
{
    LOG_AND_CHECK_IGNORESIGNAL( QString().setNum(arg1) );

    if( this->ui->wAutomaticRequests->isChecked() ) {
        Log(1) <<"\n    Forwarding to on_wRequestNodes_clicked()";
        on_wRequestNodes_clicked();
    } else {
        this->pendingRequest_ = NodesAndCoresPerNode;
    }
}

void MainWindow::on_wNCoresPerNode_valueChanged(int arg1)
{
    LOG_AND_CHECK_IGNORESIGNAL( QString().setNum(arg1) );

    if( this->ui->wAutomaticRequests->isChecked() ) {
        Log(1) <<"\n    Forwarding to on_wRequestNodes_clicked()";
        on_wRequestNodes_clicked();
    } else {
        this->pendingRequest_ = NodesAndCoresPerNode;
    }
}

void MainWindow::on_wNCores_valueChanged(int arg1)
{
    LOG_AND_CHECK_IGNORESIGNAL( QString().setNum(arg1) );

    if( this->ui->wAutomaticRequests->isChecked() ) {
        Log(1) <<"\n    Forwarding to on_wRequestCores_clicked()";
        on_wRequestCores_clicked();
    } else {
        this->pendingRequest_ = CoresAndGbPerCore;
    }
}

void MainWindow::on_wGbPerCore_valueChanged(double arg1)
{
    LOG_AND_CHECK_IGNORESIGNAL( QString().setNum(arg1) )

    if( this->ui->wAutomaticRequests->isChecked() ) {
        Log(1) <<"\n    Forwarding to on_wRequestCores_clicked()";
        on_wRequestCores_clicked();
    } else {
        this->pendingRequest_ = CoresAndGbPerCore;
    }
}

void MainWindow::on_wRequestCores_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

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

bool MainWindow::can_authenticate()
{
    bool ok = !this->getSessionConfigItem("username"  )->value().toString().isEmpty()
            &&!this->getSessionConfigItem("privateKey")->value().toString().isEmpty()
#ifndef NO_PUBLIC_KEY_NEEDED
            &&!this->getSessionConfigItem("publicKey" )->value().toString().isEmpty()
#endif
            ;
    return ok;
}

void MainWindow::on_wPages_currentChanged(int index_new_page)
{
    LOG_AND_CHECK_IGNORESIGNAL( QString().setNum(index_new_page) );

    this->authenticate( /*silent=*/ this->can_authenticate() && this->sshSession_.isConnected() );

    this->selected_job_.clear();

    Log(1) << QString("\n    changing tab %1->%2: ").arg(this->current_page_).arg(index_new_page).C_STR();
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
            bool ok = this->sshSession_.isAuthenticated();
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
    default:
        break;
    }
    this->current_page_ = index_new_page;
}

void MainWindow::on_wEnforceNNodes_toggled(bool checked)
{
    LOG_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") );

    bool hide = !checked;
    this->launcher_.script.find_key("-W")->setHidden(hide);
}

void MainWindow::on_wSingleJob_toggled(bool checked)
{
    LOG_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") );

    bool hide = !checked;
    this->launcher_.script.find_key("naccesspolicy")->setHidden(hide);
}

void MainWindow::on_wWalltime_editingFinished()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

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
    Log(1) << QString("    abe set to: '%1'.").arg(abe).C_STR();

    this->getDataConnector("notifyWhen")->value_config_to_script();
}

void MainWindow::on_wNotifyAbort_toggled(bool checked)
{
    LOG_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->update_abe('a', checked );
}

void MainWindow::on_wNotifyBegin_toggled(bool checked)
{
    LOG_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->update_abe('b', checked );
}

void MainWindow::on_wNotifyEnd_toggled(bool checked)
{
    LOG_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->update_abe('e', checked );
}

void MainWindow::warn( QString const& msg1, QString const& msg2 )
{
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
        {// Test for the existence fo finished.<jobid>
         // The job cannot be finished if finished.<jobid> does not exist.
            QString cmd = QString("ls ")
                .append( job.remote_location_ )
                .append('/' ).append(job.subfolder_)
                .append('/' ).append(job.jobname_)
                .append("/finished.").append( job.long_id() );
            int rv = this->sshSession_.execute_remote_command(cmd);
            bool is_finished = ( rv==0 );
            if( !is_finished ) {
                return false;
            }
         // It is possible but rare that finished.<jobid> exists, but the
         // epilogue is still running. To make sure:
         // Check the output of qstat -u username
            cmd = QString("qstat -u %1");
            rv = this->sshSession_.execute_remote_command( cmd, this->sshSession_.username() );
            QStringList qstat_lines = this->sshSession_.qout().split('\n',QString::SkipEmptyParts);
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

bool MainWindow::retrieve( Job const& job, bool local, bool vsc_data )
{
    if( job.status()==Job::Retrieved ) return true;
    QString msg = QString("Retrieve job ").append( job.short_id() );
    if( !this->requiresAuthentication(msg) ) return false;

    if( local )
    {
        QString target = job. local_job_folder();
        QString source = job.remote_job_folder();
        this->resolve(source); // authentication already verified
        this->sshSession_.sftp_get_dir(target,source);
        job.status_ = Job::Retrieved;
    }
    if( vsc_data && !job.remote_location_.startsWith("$VSC_DATA") )
    {
        QString cmd = QString("mkdir -p %1");
        QString vsc_data_destination
                = job.vsc_data_job_parent_folder( this->vscdata_file_location() );
        this->resolve( vsc_data_destination );
        int rc = this->sshSession_.execute_remote_command( cmd, vsc_data_destination );
        cmd = QString("cp -rv %1 %2"); // -r : recursive, -v : verbose
        rc = this->sshSession_.execute_remote_command( cmd, job.remote_job_folder(), vsc_data_destination );
        job.status_ = Job::Retrieved;

        if( rc ) {/*keep compiler happy (rc unused variable)*/}
    }
    return job.status()==Job::Retrieved;
}

//void MainWindow::getRemoteFileLocations_()
//{
//    this->remote_env_vars_.clear();
//    if( !this->remote_file_locations_.isEmpty() ) {
//        this->remote_file_locations_ = this->getSessionConfigItem("wRemote")->choices_as_QStringList();
//    }
//    this->authenticate();
//    if( this->sshSession_.isAuthenticated() ) {
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
    this->statusBar()->showMessage(msg);
    return true;
}

bool MainWindow::loadJobscript( QString const& filepath )
{
    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;

    LOG_CALLER_INFO << QString("\n    loading job script '%1'.").arg(filepath).C_STR();
    try
    {
        Log(1) << "\n    reading the job script ... ";
        this->launcher_.script.read(filepath);
     // make sure the jobscript's jobname corresponds to the name of the job folder.
        QDir qdir(filepath);
        qdir.cdUp();
        QString jobfolder( qdir.dirName() );
        if( const_cast<pbs::Script const&>(this->launcher_.script)["-N"] != jobfolder ) {
            this->launcher_.script["-N"]  = jobfolder;
        }
        Log(1) << "done";
    } catch( std::runtime_error& e ) {
        QString msg = QString("An error occurred while reading the jobscript '%1'.\n Error:\n")
                         .arg( filepath )
                      .append( e.what() )
                      .append("\n\nEdit the script to correct it, then press 'Save' and 'Reload'.");
        Log() << msg.C_STR();
        QMessageBox::critical(this,TITLE,msg);
    }

    try
    {
     // the order is important!
        Log(1) << "\n    Setting gui elements from job script ... ";
        QStringList first({"wCluster","wNodeset","wNNodes", "wNCoresPerNode"}) ;
        for(QStringList::const_iterator iter = first.cbegin(); iter!=first.cend(); ++iter ) {
            Log(1) << QString( "\n      %1 :").arg(*iter).C_STR();
            dc::DataConnectorBase* dataConnector = this->getDataConnector(*iter);
            if( dataConnector->value_script_to_config() ) {
                dataConnector->value_config_to_widget();
                Log(1) << "ok";
            } else {
                Log(1) << "unchanged (=ok)";
            }
        }
     // do the same for all other DataConnectors, for which the order does not matter.
     // the items in first are visited twice, but the second time has no effect.
        for( QList<dc::DataConnectorBase*>::Iterator iter = this->data_.begin(); iter!=this->data_.end(); ++iter )
        {
            Log(1) << QString( "\n      %1 :").arg( (*iter)->name() ).C_STR();

            if( (*iter)->value_script_to_config() ) {
                (*iter)->value_config_to_widget();
                Log(1) << "ok";
            } else {
                Log(1) << "unchanged (=ok)";
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
        Log() << msg.C_STR();
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
    Log() << QString ("\n    writing script '%1' ... ").arg(filepath).C_STR();
    try {
        this->launcher_.script.write( filepath, /*warn_before_overwrite=*/false );
        Log() << "done.";
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
            Log() << QString("done (overwritten, but made backup '%1').").arg(backup).C_STR();
        } else {
            Log() << "aborted.";
            this->statusBar()->showMessage( QString("Job script NOT SAVED: '%1'").arg(filepath) );
            return false;
        }
    }
    this->statusBar()->showMessage( QString("Job script saved: '%1'").arg(filepath) );
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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    this->launcher_.script.set_has_unsaved_changes(true);
    this->check_script_unsaved_changes();
}

void MainWindow::on_wRefresh_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    cfg::Item* ci_joblist = this->getSessionConfigItem("job_list");
    JobList joblist( ci_joblist->value().toStringList() );
    this->refreshJobs(joblist);
}

QString MainWindow::selectedJob( QTextEdit* qTextEdit )
{
    QString selection;
    QTextCursor cursor = qTextEdit->textCursor();
    cursor.select(QTextCursor::LineUnderCursor);
    selection = cursor.selectedText();

    if( selection.isEmpty()
     || selection.startsWith("*** ")
      ) {
        this->statusBar()->showMessage("No job Selected");
        selection.clear();
    } else {
        qTextEdit->setTextCursor(cursor);
        selection = selection.split(':')[0];
    }
    return selection;
}

void MainWindow::clearSelection( QTextEdit* qTextEdit )
{
    QTextCursor cursor = qTextEdit->textCursor();
    cursor.movePosition( QTextCursor::Start);
    qTextEdit->setTextCursor(cursor);
}

void MainWindow::on_wRetrieveSelectedJob_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

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
                this->sshSession_.execute_remote_command(cmd);
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
                this->sshSession_.execute_remote_command( cmd, job.remote_job_folder() );
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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    if( this->selected_job_.isEmpty() ) {
        this->statusBar()->showMessage("Nothing Selected.");
        return;
    }
    this->deleteJob( this->selected_job_ );
    this->selected_job_.clear();
}

void MainWindow::on_wCheckCopyToDesktop_toggled(bool checked)
{
    LOG_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->getSessionConfigItem("wCheckCopyToDesktop")->set_value(checked);
}

void MainWindow::on_wCheckCopyToVscData_toggled(bool checked)
{
    LOG_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->getSessionConfigItem("wCheckCopyToVscData")->set_value(checked);
}

void MainWindow::on_wCheckDeleteLocalJobFolder_toggled(bool checked)
{
    LOG_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->getSessionConfigItem("wCheckDeleteLocalJobFolder")->set_value(checked);
}

void MainWindow::on_wCheckDeleteRemoteJobFolder_toggled(bool checked)
{
    LOG_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
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
    Log() << "\n    Copying standard folders.";
    for ( int i=0; i<folders.size(); ++i ) {
        Log() << copy_folder_recursively( path_to_folders[i], this->launcher_.homePath( folders[i] ) ).C_STR();
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
    LOG_AND_CHECK_IGNORESIGNAL( arg1 )

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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT)

    if( this->sshSession_.isAuthenticated() ) {
        QString msg = QString("This session is already authenticated:"
                              "\n  user: %1"
                              "\n  key : %2"
                              "\n\nDo you want to re-authenticate?")
                         .arg( this->sshSession_.username() )
                         .arg( this->sshSession_.privateKey() );
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
                this->sshSession_.setUsername(username);
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
        if( !QFileInfo( private_key ).exists() ) {
            this->statusBar()->showMessage( QString( "Authentication canceled : private key '%1' not found.").arg(private_key) );
            return;
        }
        break;
      default:
        this->statusBar()->showMessage("Authentication canceled by user.");
        return;
    }

#ifdef NO_PUBLIC_KEY_NEEDED
    this->sshSession_.setPrivatePublicKeyPair(private_key);
#else
 // private key is provided, now public key
    QString public_key = private_key;
    public_key.append(".pub"); // make an educated guess...
    if( !QFileInfo( public_key ).exists() )
    {
        answer = QMessageBox::question(this,TITLE,"You must also provide the corresponding PUBLIC key file.\n Continue?", QMessageBox::Ok|QMessageBox::No,QMessageBox::Ok);
        if( answer==QMessageBox::Ok ) {
            public_key = QFileDialog::getOpenFileName( this, "Select PUBLIC key file:", start_in );
            if( public_key.isEmpty() || !QFileInfo(public_key).exists() )
            {// not provided or inexisting
                this->statusBar()->showMessage(QString("Public key '%1' not found or not provided.").arg(public_key) );
                return false;
            }
        }
    }
    this->statusBar()->showMessage("Private/public key pair found.");
    this->sshSession_.setPrivatePublicKeyPair( private_key, public_key );
#endif
    this->authenticate( /*silent=*/false );
}

bool MainWindow::authenticate(bool silent)
{
//    LOG_AND_CHECK_IGNORESIGNAL(silent)

    if( this->sshSession_.isAuthenticated() ) {
        LOG_CALLER_INFO << "\n    already authenticated.";
        return true;
    }

    LOG_CALLER_INFO;

    if( this->current_page_==MainWindowUnderConstruction )
    {// try to authenticate ONLY if no user interaction is required.
        QString uname = this->username();
        if( uname.isEmpty() ) {
            Log() << "\n    Automatic authentication canceled: missing user name.";
            return false;
        }
        if( this->getSessionConfigItem("privateKey")->value().toString().isEmpty() ) {
            Log() << QString("\n    Automatic authentication canceled for '%1': missing private key.").arg(uname).C_STR();
            return false;
        } // do we have a private key
#ifndef NO_PUBLIC_KEY_NEEDED
        if( this->getSessionConfigItem("publicKey" )->value().toString().isEmpty() ) {
            Log() << QStrng("\n    Automatic authentication canceled for '%1': missing public key.").arg(uname).C_STR();
            return false;
        }
#endif
        if( this->getSessionConfigItem("passphraseNeeded")->value().toBool() ) {
            Log() << QString("\n    Automatic authentication canceled  for '%1': passphrase needed.").arg(uname).C_STR();
            return false;
        }
    }

    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;

    int max_attempts = 3;
    bool no_connection = false;
    int attempts = max_attempts;
    enum {
        success, retry, failed
    } result;
    while( attempts )
    {
        try {
            this->sshSession_.open();
            result = success;
        }
        catch( ssh2::NoAddrInfo &e ) {
            QString msg= QString("Launcher could not reach %1. ").arg( this->sshSession_.loginNode() );
            this->statusBar()->showMessage(msg);
            Log(0) << msg.prepend("\n    ").C_STR();

            if( this->proceed_offline_ || silent )
            {// be silent
            } else {
                int answer = QMessageBox::question( this, TITLE
                                                  , msg.append("Possible causes are"
                                                               "\n  . you are offline (no internet connection)"
                                                               "\n  . the cluster's login nodes are offline"
                                                               "\nClick 'Proceed offline' if you are working "
                                                               "offline and don't want to see this message again.")
                                                  ,"Proceed offline" // button0
                                                  ,"Ok"              // button1
                                                  , QString()        // button2
                                                  , 1                // default button
                                                  );
                this->proceed_offline_ = (answer==0);
            }
            no_connection=true;
            result = failed;
        }
        catch( ssh2::ConnectTimedOut &e ) {
            if( !silent ) {
                QString msg= QString("Could not reach  %1.").arg( this->ui->wCluster->currentText() );
                this->statusBar()->showMessage(msg);
                QMessageBox::warning( this,TITLE,msg.append("\n  . Check your internet connection."
                                                            "\n  . Check your VPN connection if you are outside the university."
                                                            "\n  . Check the availability of the cluster."
                                                            ) );
            }
            result = failed;
            no_connection=true;
        }
        catch( ssh2::MissingUsername& e ) {
            if( !silent )
                this->warn("You must authenticate first (menu Session/Authenticate...)");
            result = failed;
        }
        catch( ssh2::MissingKey& e ) {
            if( !silent )
                this->warn("You must authenticate first (menu Session/Authenticate...)");
            result = failed;
        }
        catch( ssh2::PassphraseNeeded& e ) {
            QString msg = QString("Enter passphrase for unlocking %1 key '%2':")
                             .arg( this->sshSession_.username() )
                             .arg( this->sshSession_.privateKey() );
            QString passphrase = QInputDialog::getText( 0, TITLE, msg, QLineEdit::Password );
            this->sshSession_.setPassphrase( passphrase );
            result = retry;
        }
        catch( ssh2::WrongPassphrase& e ) {
            if( attempts ) {
                QString msg = QString("Could not unlock %1 key.\n"
                                      "%2 attempts left.\n"
                                      "Enter passphrase for unlocking key '%3':")
                                 .arg( this->sshSession_.username() )
                                 .arg( attempts )
                                 .arg( this->sshSession_.privateKey() );
                QString passphrase = QInputDialog::getText( 0, "Passphrase needed", msg );
                this->sshSession_.setPassphrase( passphrase );
                --attempts;
                result = retry;
            } else {
                if( !silent )
                    this->warn( QString("Cannot authenticate: failed to provide passphrase after %n attempts.")
                                   .arg(max_attempts)
                              );
                result = failed;
            }
        }
        catch( std::runtime_error& e ) {
            QString msg = QString("Error attempting to authenticate on %1: ").append( e.what() )
                             .arg( this->ui->wCluster->currentText() );
            if( !silent ) {
                this->warn(msg);
            }
            Log(0) << msg.prepend("\n    ").C_STR();
         // remove keys from ssh session (but keep the username)
            this->sshSession_.setUsername( this->getSessionConfigItem("username")->value().toString() );
            result = failed;
        }
        if( result==success )
        {
            this->statusBar()->showMessage( QString("User %1 is authenticated.").arg( this->sshSession_.username() ) );
            this->getSessionConfigItem("username"  )->set_value( this->sshSession_.username  () );
            this->getSessionConfigItem("privateKey")->set_value( this->sshSession_.privateKey() );
#ifndef NO_PUBLIC_KEY_NEEDED
            this->getSessionConfigItem("publicKey" )->set_value( this->sshSession_. publicKey() );
#endif
            this->getSessionConfigItem("passphraseNeeded")->set_value( this->sshSession_.hasPassphrase() );

            this->update_StatusbarWidgets();

            if( this->is_uptodate_for_!=this->ui->wCluster->currentText() )
            {
                Log()<< "automatic authentication succeeded.";
                //this->getRemoteFileLocations_();
             // get list of available cluster modules
                QString cmd("__module_avail");
                this->sshSession_.execute_remote_command(cmd);
                QStringList modules = this->sshSession_.qout().split("\n");
                modules.prepend("--- select a module below ---");
                this->ui->wSelectModule->clear();
                this->ui->wSelectModule->addItems(modules);

                QString modules_cluster = QString("modules_").append(this->getSessionConfigItem("wCluster")->value().toString() );
                this->getSessionConfigItem(modules_cluster)->set_choices(modules);

                this->is_uptodate_for_ = this->ui->wCluster->currentText();
            }
            break; //out of while loop
        }
        if( result==retry ) {
            continue; // = retry
        }
        if( result==failed )
        {
            this->update_StatusbarWidgets();

            QString msg;
            if( attempts==0 )
            {// after 3 unsuccesfull attempts to authenticate, we remove the keys from the session as they are probably wrong
             // (but we keep the username)
                this->sshSession_.setUsername( this->getSessionConfigItem("username")->value().toString() );

                msg = QString("Authentication failed: %1 failed attempts to provide passphrase.").arg(max_attempts);
            } else {
                QString msg = "Authentication failed.";
            }
            this->statusBar()->showMessage(msg);
            Log(0) << msg.prepend("\n    ").C_STR();

            break; //out of while loop
        }
    }
    if( no_connection )
    {
        this->update_StatusbarWidgets();

        this->is_uptodate_for_.clear();

        this->statusBar()->showMessage("Launcher was unable to make a connection. Proceeding offline.");
        {// attempt to obtain cluster modules from config file, to enable the user to work offline.
            QString modules_cluster = QString("modules_").append(this->getSessionConfigItem("wCluster")->value().toString() );
            QStringList modules = this->getSessionConfigItem(modules_cluster)->choices_as_QStringList();
            if( modules.isEmpty() ) {
                this->statusBar()->showMessage("List of modules not available for current cluster ");
            } else {
                this->ui->wSelectModule->clear();
                this->ui->wSelectModule->addItems({"--- select a module below ---"});
                this->ui->wSelectModule->addItems(modules);
                this->statusBar()->showMessage("Using List of modules from a previous session. Authenticate to update it.");
            }
        }
    }

    return result==success;
}

bool MainWindow::requiresAuthentication(QString const& action )
{
    if( !this->sshSession_.isAuthenticated() ) {
        if( !this->authenticate() )
        {// automatic authentication fails
            if( this->sshSession_.isConnected() ) {
                int answer = QMessageBox::question( this, TITLE, "This action requires that you authenticate."
                                                  , QString("Cancel '%1'").arg(action)  // button0
                                                  ,"Authenticate now..."                // button1
                                                  , 0                                   // default button
                                                  );
                switch( answer ) {
                  case 1: {
                    this->authenticateAction_triggered();
                    return this->sshSession_.isAuthenticated();
                  }
                  default:
                    this->statusBar()->showMessage("Not authenticated, but authentication required to complete.");
                    break;
                }

            } else {
                QString msg = QString("Authentication is required to complete, but\n"
                                      "Cluster %1 cannot be reached. Possible causes are:\n"
                                      "  - you have no internet connection\n"
                                      "  - you are outside the university and VPN is not connected\n"
                                      "  - the cluster is down\n"
                                      "Actions requiring identification are not available.")
                                 ,arg( this->ui->wCluster->currentText() );
                QMessageBox::critical( this, TITLE, msg );
                return false;
            }
        }
    }
    return true;
}

void MainWindow::localFileLocationAction_triggered()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

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
                   .arg( this->sshSession_.isAuthenticated()
                       ? this->username()
                       : ( !this->sshSession_.isConnected()
                         ? QString("offline")
                         : QString("not authenticated")
                         )
                       );
    this->wAuthentIndicator_->setText(s);

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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);
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
        Log(1) <<"\n    parsing script text";
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
}

void MainWindow::openJobscriptAction_triggered()
{
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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    if( !this->requiresAuthentication("Submit job") ) {
        return;
    }

    QString canceled = QString("Submit job '%1' CANCELED!").arg( this->job_folder() );
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
 // test if the remote job folder is inexisting or empty
    QString cmd("ls %1");
    int rc = this->sshSession_.execute_remote_command( cmd, remote_job_folder );
    if( rc==0 && !this->sshSession_.qout().isEmpty() )
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
            int rc = this->sshSession_.execute_remote_command( cmd, remote_job_folder );
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
                        rc = this->sshSession_.execute_remote_command( cmd );
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
    this->sshSession_.sftp_put_dir( local_job_folder, remote_job_folder, true );

    cmd = QString("cd %1 && qsub pbs.sh");
    this->sshSession_.execute_remote_command( cmd, remote_job_folder );
    QString job_id = this->sshSession_.qout().trimmed();
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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

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
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

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
