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
        QString result = username.toLower();
        QRegularExpressionMatch m;
        QRegularExpression re("^vsc\\d{5}$");
        m = re.match( username );
        if( !m.hasMatch() ) {
            result.clear();
        }
        return result;
    }
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
      , previousPage_(MainWindowUnderConstruction)
      , verbosity_(INITIAL_VERBOSITY)
      , pendingRequest_(NoPendingRequest)
      , messages_(TITLE)
    {
        LOG_CALLER_INFO << "\n    Running Launcher version = "<<VERSION;
        ui->setupUi(this);

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

        createActions();
        createMenus();

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
        previousPage_ = MainWindowConstructedButNoPreviousPage;
    }

    void MainWindow::createActions()
    {
        aboutAction_ = new QAction("&About", this);
        connect( aboutAction_, SIGNAL(triggered()), this, SLOT( aboutAction_triggered() ) );

        verboseAction_ = new QAction("Verbose logging", this);
        connect( verboseAction_, SIGNAL(triggered()), this, SLOT( verboseAction_triggered() ) );

        openSessionAction_ = new QAction("&Open session ...", this);
        openSessionAction_ ->setShortcuts(QKeySequence::Open);
        connect( openSessionAction_, SIGNAL(triggered()), this, SLOT( openSessionAction_triggered() ) );

        newSessionAction_ = new QAction("&New session ...", this);
        newSessionAction_ ->setShortcuts(QKeySequence::New);
        connect( newSessionAction_, SIGNAL(triggered()), this, SLOT( newSessionAction_triggered() ) );

        saveSessionAction_ = new QAction("&Save session ...", this);
        saveSessionAction_ ->setShortcuts(QKeySequence::Save);
        connect( saveSessionAction_, SIGNAL(triggered()), this, SLOT( saveSessionAction_triggered() ) );

        authenticateAction_ = new QAction("authenticate ...", this);
        connect( authenticateAction_, SIGNAL(triggered()), this, SLOT( authenticateAction_triggered() ) );

        localFileLocationAction_ = new QAction("Choose new local file location ...", this);
        connect( localFileLocationAction_, SIGNAL(triggered()), this, SLOT( localFileLocationAction_triggered() ) );

        remoteFileLocationAction_ = new QAction("Choose new remote file location ...", this);
        connect( remoteFileLocationAction_, SIGNAL(triggered()), this, SLOT( remoteFileLocationAction_triggered() ) );

        createTemplateAction_ = new QAction("Create a job template ...", this);
        connect( createTemplateAction_, SIGNAL(triggered()), this, SLOT( createTemplateAction_triggered() ) );

        selectTemplateAction_ = new QAction("Select a job template ...", this);
        connect( selectTemplateAction_, SIGNAL(triggered()), this, SLOT( selectTemplateAction_triggered() ) );

        newJobscriptAction_ = new QAction("New job script ...", this);
        connect( newJobscriptAction_, SIGNAL(triggered()), this, SLOT( newJobscriptAction_triggered() ) );

        saveJobscriptAction_ = new QAction("Save the job script", this);
        connect( saveJobscriptAction_, SIGNAL(triggered()), this, SLOT( saveJobscriptAction_triggered() ) );

        openJobscriptAction_ = new QAction("Open a job script ...", this);
        connect( openJobscriptAction_, SIGNAL(triggered()), this, SLOT( openJobscriptAction_triggered() ) );

        submitJobscriptAction_ = new QAction("Submit current job script", this);
        connect( submitJobscriptAction_, SIGNAL(triggered()), this, SLOT( submitJobscriptAction_triggered() ) );

        showFileLocationsAction_ = new QAction("Show file locations ...", this);
        connect( showFileLocationsAction_, SIGNAL(triggered()), this, SLOT( showFileLocationsAction_triggered() ) );

        showLocalJobFolderAction_ = new QAction("Show local jobfolder contents ...", this);
        connect( showLocalJobFolderAction_, SIGNAL(triggered()), this, SLOT( showLocalJobFolderAction_triggered() ) );

        showRemoteJobFolderAction_ = new QAction("Show remote job folder contents ...", this);
        connect( showRemoteJobFolderAction_, SIGNAL(triggered()), this, SLOT( showRemoteJobFolderAction_triggered() ) );
    }

    void MainWindow::createMenus()
    {
        helpMenu_ = menuBar()->addMenu("&Help");
        helpMenu_->addAction(aboutAction_);

        jobMenu_ = menuBar()->addMenu("Job");
        jobMenu_ ->addAction(newJobscriptAction_);
        jobMenu_ ->addAction(saveJobscriptAction_);
        jobMenu_ ->addAction(openJobscriptAction_);
        jobMenu_ ->addAction(submitJobscriptAction_);
        jobMenu_ ->addAction(showFileLocationsAction_);
        jobMenu_ ->addAction(showLocalJobFolderAction_);
        jobMenu_ ->addAction(showRemoteJobFolderAction_);
        jobMenu_ ->addAction(selectTemplateAction_);
        jobMenu_ ->addAction(createTemplateAction_);

        sessionMenu_ = menuBar()->addMenu("&Session");
        sessionMenu_ ->addAction( newSessionAction_);
        sessionMenu_ ->addAction(openSessionAction_);
        sessionMenu_ ->addAction(saveSessionAction_);
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
        //this->ui->wOffline->setText("");

        this->setIgnoreSignals();
        cfg::Item* ci = nullptr;

     // username and keys
        this->getSessionConfigItem("username"  , QString() );
        this->getSessionConfigItem("privateKey", QString() );
#ifndef NO_PUBLIC_KEY_NEEDED
        this->getSessionConfigItem("publicKey" , QString() );
#endif
        this->getSessionConfigItem("passphraseNeeded", true );
     // wWalltimeUnit
        ci = this->getSessionConfigItem("wWalltimeUnit");
        if( !ci->isInitialized() ) {
            QStringList units;
            units <<"seconds"<<"minutes"<<"hours"<<"days"<<"weeks";
            ci->set_choices(units);
            ci->set_value("hours");
        }

        ci = this->getSessionConfigItem("walltimeSeconds","1:00:00");

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
        this->data_.append( dc::newDataConnector( this->ui->wWalltimeUnit , "wWalltimeUnit"  , ""         ) );
        this->data_.append( dc::newDataConnector(                           "walltimeSeconds", "walltime" ) );
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
        this->data_.append( dc::newDataConnector( this->ui->wWalltime      , "wWalltime"      , ""           ) );

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

     // local file location
        this->getSessionConfigItem("localFileLocation", this->launcher_.homePath("jobs") );
     // project folder
        this->getSessionConfigItem("wProjectFolder", "");
     // wJobname
        ci = this->getSessionConfigItem("wJobname","hello_world");

//        if( ci->value().toString().isEmpty() ) {
//            this->ui->wClusterGroup->hide();
//        }

        this->data_.append( dc::newDataConnector( this->ui->wJobname      , "wJobname"      , "-N"           ) );
        this->data_.append( dc::newDataConnector( this->ui->wProjectFolder, "wProjectFolder", ""             ) );
        this->data_.append( dc::newDataConnector(                           "localFolder"   , "local_folder" ) );
        this->data_.append( dc::newDataConnector(                           "remoteFolder"  , "remote_folder") );

     // verify existence of local_file_location[/project_folder[/jobname]]
        QString path;
        try {
            path = this->jobs_project_job_path(LOCAL);
            this->getSessionConfigItem("localFolder" )->set_value_and_type( path );
            this->getSessionConfigItem("remoteFolder")->set_value_and_type( this->jobs_project_job_path( REMOTE, false ) ); // no need to store resolved path
        } catch (std::runtime_error &e ) {
            Log() << "\n    Error: "<< e.what();
        }
        QString qs;
        qs = this->getSessionConfigItem("wProjectFolder")->value().toString();
        this->ui->wProjectFolder->setText(qs);

        qs = this->getSessionConfigItem("wJobname")->value().toString();
        this->ui->wJobname->setText(qs);

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

        if( !this->getSessionConfigItem("wJobname")->value().toString().isEmpty() ) {
            this->lookForJobscript( path );
        } else
        {// no jobname - all values are either default, or come from the config file
         // we consider this as NO unsaved changes (
            this->launcher_.script.set_has_unsaved_changes(false);
        }
        this->check_script_unsaved_changes();
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

    void MainWindow::save_session( QString const& path_to_session_data )
    {
        LOG_CALLER(0) << QString("\n      path: '%1'").arg( path_to_session_data ).C_STR();
        this->launcher_.session_config.save( path_to_session_data );
        this->save_session_path( path_to_session_data );
        QString msg = QString("Launcher session saved as '%1'.").arg(path_to_session_data);
        this->statusBar()->showMessage(msg);
    }

    QString
    MainWindow::
    jobs_project_path( LocalOrRemote local_or_remote, bool resolve )
    {
        QString fileLocation = ( local_or_remote==LOCAL ? "localFileLocation" : "remoteFileLocation" );
        QString path = QString( this->getSessionConfigItem( fileLocation   )->value().toString() ).append("/")
                       .append( this->getSessionConfigItem("wProjectFolder")->value().toString() );
        QDir qdir(path);
        path = qdir.cleanPath(path);
        if( local_or_remote==REMOTE ) {
            if( resolve && path.startsWith('$') && this->sshSession_.isAuthenticated() ) {
                QString envvar = path.split('/')[0];
                QString cmd("echo %1");
                try {
                    this->sshSession_.execute_remote_command(cmd,envvar);
                    QString resolved = this->sshSession_.qout().trimmed();
                    path.replace(envvar,resolved);
                } catch(std::runtime_error &e ) {
                 // pass
                }
            }
            return path;
        } else {
            if( qdir.exists() ) {
                return path;
            } else {
                throw_<std::runtime_error>("Local project folder does not exist: '%1'", path );
            }
        }
        return QString(); // keep compiler happy
    }

    QString
    MainWindow::
    jobs_project_job_path( LocalOrRemote local_or_remote, bool resolve )
    {
        QString fileLocation = ( local_or_remote==LOCAL ? "localFileLocation" : "remoteFileLocation" );
        QString path = QString( this->getSessionConfigItem( fileLocation   )->value().toString() ).append("/")
                       .append( this->getSessionConfigItem("wProjectFolder")->value().toString() ).append("/")
                       .append( this->getSessionConfigItem("wJobname"      )->value().toString() );
        QDir qdir(path);
        path = qdir.cleanPath(path);
        if( local_or_remote==REMOTE ) {
            if( resolve && path.startsWith('$') && this->sshSession_.isAuthenticated() ) {
                QString envvar = path.split('/')[0];
                QString cmd("echo %1");
                try {
                    this->sshSession_.execute_remote_command(cmd,envvar);
                    QString resolved = this->sshSession_.qout().trimmed();
                    path.replace(envvar,resolved);
                } catch(std::runtime_error &e ) {
                 // pass
                }
            }
            return path;
        } else {
            if( qdir.exists() ) {
                return path;
            } else {
                throw_<std::runtime_error>("Local job folder does not exist: '%1'", path );
            }
        }
        return QString(); // keep compiler happy
    }
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
            delete (*iter);
        }

        QSize windowSize = this->size();
        this->getSessionConfigItem("windowWidth" )->set_value( windowSize.width () );
        this->getSessionConfigItem("windowHeight")->set_value( windowSize.height() );

        this->save_session( this->launcher_.session_config.filename() );

        if ( this->launcher_.script.has_unsaved_changes() ) {
            QMessageBox::Button answer =
                    QMessageBox::question( this,TITLE
                                         ,"The current job script has unsaved changes.\n"
                                          "Do you want to save?"
                                         );
            if( answer==QMessageBox::Yes ) {
                Log(1) << "\n    - Calling on_wSave_clicked()";
                this->on_wSave_clicked();
            } else {
                Log(1) << "\n    - Discarding unsaved changes to script.";
            }
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
            IgnoreSignals ignoreSignals( this );
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
            IgnoreSignals ignoreSignals( this );
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
    QString formatWalltimeSeconds( int seconds )
    {
        int h,m,s;
        h       = seconds/3600;
        seconds = seconds%3600;
        m       = seconds/60;
        s       = seconds%60;
        QString format("%1:%2:%3");
        QString formatted = format.arg(h).arg(m,2,10,QLatin1Char('0')).arg(s,2,10,QLatin1Char('0'));
        return formatted;
    }
 //-----------------------------------------------------------------------------
    int unformatWalltimeSeconds( QString formatted_seconds )
    {
        QRegularExpression re("^(\\d+):(\\d\\d):(\\d\\d)$");
        QRegularExpressionMatch match;
        match = re.match(formatted_seconds);
        if( !match.hasMatch() ) {
            throw_<std::runtime_error>("Could not convert walltime '%1' to seconds.", formatted_seconds );
        }
        int s = match.captured(3).toInt();
        int m = match.captured(2).toInt();
        int h = match.captured(1).toInt();
        s += 60*( m + 60*h );
        return s;
    }
 //-----------------------------------------------------------------------------
    void MainWindow::walltimeUnitDependencies( bool updateWidgets )
    {
        QString unit = this->getSessionConfigItem("wWalltimeUnit")->value().toString();
        this->walltimeUnitSeconds_ = nSecondsPerUnit(unit);
        double step = ( unit=="weeks" ? 1.0/7.0 :
                      ( unit=="days"  ? 0.25    :
                      ( unit=="hours" ? 0.25    : 10.0 )));
        double walltime_limit = (double) this->clusterInfo().walltimeLimit();
        walltime_limit /= this->walltimeUnitSeconds_;
        QList<QVariant> range;
        range.append( 0. );
        range.append( walltime_limit );
        range.append( step );

        cfg::Item* ci_wWalltime = this->getSessionConfigItem("wWalltime");
        try {
            ci_wWalltime->set_choices(range,true);
        } catch( cfg::InvalidConfigItemValue& e) {}
        double w = (double) unformatWalltimeSeconds( this->getSessionConfigItem("walltimeSeconds")->value().toString() );
        w /= this->walltimeUnitSeconds_;
        ci_wWalltime->set_value(w);

        if( updateWidgets ) {
            IgnoreSignals ignoreSignals( this );
            this->getDataConnector("wWalltime")->choices_config_to_widget();
            this->getDataConnector("wWalltime")->  value_config_to_widget();
        }
    }
 //-----------------------------------------------------------------------------
    QString MainWindow::check_script_unsaved_changes()
    {
        QString msg;
        QTextStream ts(&msg);
        ts << "\n    Checking unsaved changes : ";
        bool has_unsaved_changes = this->launcher_.script.has_unsaved_changes();
        if( has_unsaved_changes ) {
            ts << "true";
            this->ui->wSave ->setText("*** Save ***");
            this->ui->wSave2->setText("*** Save ***");
            this->ui->wSave ->setToolTip("Save the current script locally (there are unsaved changes).");
            this->ui->wSave2->setToolTip("Save the current script locally (there are unsaved changes).");
        } else {
            ts << "false";
            this->ui->wSave ->setText("Save");
            this->ui->wSave2->setText("Save");
            this->ui->wSave ->setToolTip("Save the current script locally.");
            this->ui->wSave2->setToolTip("Save the current script locally.");
        }
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

#define IGNORE_SIGNALS_UNTIL_END_OF_SCOPE IgnoreSignals ignoreSignals(this)


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
        Log(1) <<"\n    q to on_wRequestNodes_clicked()";
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

void MainWindow::on_wPages_currentChanged(int index_current_page)
{
    LOG_AND_CHECK_IGNORESIGNAL( QString().setNum(index_current_page) );

    this->authenticate( /*silent=*/ this->can_authenticate() && this->sshSession_.isConnected() );

    this->selected_job_.clear();

    Log(1) << QString("\n    changing tab %1->%2: ").arg(this->previousPage_).arg(index_current_page).C_STR();
    switch (index_current_page) {
    case 0:
        if( this->previousPage_==1 )
        {// read the script from wScript
            if( this->launcher_.script.has_unsaved_changes() ) {
                QString text = this->ui->wScript->toPlainText();
                this->launcher_.script.parse( text, false );
            }
        }
        break;
    case 1:        
        {
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
    this->previousPage_ = index_current_page;
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

//void MainWindow::on_wWalltimeUnit_currentTextChanged(const QString &arg1)
//{
//    LOG_AND_CHECK_IGNORESIGNAL( arg1 );

//    this->getDataConnector("wWalltimeUnit")->value_widget_to_config();
//    this->walltimeUnitDependencies(true);
//}

//void MainWindow::on_wWalltime_valueChanged(double arg1)
//{
//    LOG_AND_CHECK_IGNORESIGNAL( QString().setNum(arg1) );

//    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;

//    cfg::Item* walltimeSeconds = this->getSessionConfigItem("walltimeSeconds");
//    double seconds = arg1*this->walltimeUnitSeconds_;
//    walltimeSeconds->set_value( formatWalltimeSeconds( seconds ) );
//    this->getDataConnector("walltimeSeconds")->value_config_to_script();
//    Log(1) <<"    walltime set to: " << seconds << "s.";
//}

void MainWindow::on_wWalltime_editingFinished()
{
    QString input = this->ui->wWalltime->text();
    QStringList input_items = input.split(':');
    int sz = input_items.size();
    switch  (sz) {
        case 1:
        {// expecting "integer d|h|m|s"
            if( input_items.isEmpty() ) break;

            input = input_items.at(0);
            input_items = input.split(' ',QString::SkipEmptyParts);
            try {
                int n = input_items.at(0).toInt();
                int sz1 = input_items.size();
                switch(sz1) {
                case 1:
                {// only one item, no unit, use hour as walltime unit
                    n *= 3600;
                    this->walltime_ = QTime().addSecs(n);
                } break;
                case 2:
                {// second item is unit
                 // interpret unit:
                    QString unit = input_items.at(1).toLower();
                    if     ( unit=="d" ) n*=24*3600;
                    else if( unit=="h" ) n*=3600;
                    else if( unit=="m" ) n*=60;
                    else if( unit=="s" ) n*=1;
                    else break; // bad input
                    this->walltime_ = QTime().addSecs(n);
                } break;
                default:
                {// bad input. don't change the walltime
                } break;
                }// end of switch(sz1)
            } catch (...) {
                break;
            }
        } break;
        default:
        {// expecting "hh:mm[:ss]"
            if( sz>=4 ) break; //bad input
            int hhmmss[3];
            for( int i=0; i<sz; ++i ) {
                if( input_items.at(i).isEmpty() ) {
                    hhmmss[i] = 0;
                } else {
                    try {
                        hhmmss[i] = input_items.at(i).toInt();
                    } catch(...) {
                        break; // bad input
                    }
                }
            }
            hhmmss[2]+=hhmmss[1]*60;
            hhmmss[2]+=hhmmss[0]*3600;
            this->walltime_ = QTime().addSecs( hhmmss[2] );
        }
    }
    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
    input = this->walltime_.toString("hh:mm:ss");
    this->getSessionConfigItem("wWalltime")->set_value(input);
    this->getDataConnector("wWalltime")->value_config_to_widget();
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

    if( local )
    {
        QString target = job. local_job_folder();
        QString source = job.remote_job_folder();
        this->sshSession_.sftp_get_dir(target,source);
        job.status_ = Job::Retrieved;
    }
    if( vsc_data && job.remote_location_!="$VSC_DATA")
    {
        QString cmd = QString("mkdir -p %1");
        QString vsc_data_destination = job.vsc_data_job_parent_folder(this->remote_env_vars_["$VSC_DATA"]);
        int rc = this->sshSession_.execute_remote_command( cmd, vsc_data_destination );

        cmd = QString("cp -rv %1 %2"); // -r : recursive, -v : verbose
        rc = this->sshSession_.execute_remote_command( cmd, job.remote_job_folder(), vsc_data_destination );
        job.status_ = Job::Retrieved;

        if( rc ) {/*keep compiler happy (rc unused variable)*/}
    }
    return job.status()==Job::Retrieved;
}

void MainWindow::getRemoteFileLocations_()
{
    this->remote_env_vars_.clear();
    if( !this->remote_file_locations_.isEmpty() ) {
        this->remote_file_locations_ = this->getSessionConfigItem("wRemote")->choices_as_QStringList();
    }
    this->authenticate();
    if( this->sshSession_.isAuthenticated() ) {
        for ( QStringList::Iterator iter = this->remote_file_locations_.begin()
            ; iter!=this->remote_file_locations_.end(); ++iter )
        {
            if( iter->startsWith('$') ) {
                QString cmd("echo %1");
                try {
                    this->sshSession_.execute_remote_command( cmd, *iter );
                    *iter = this->sshSession_.qout().trimmed();
                } catch(std::runtime_error &e ) {
                 // pass
                }
            }
        }
    }
}

//void MainWindow::on_wLocalFileLocationButton_clicked()
//{
//    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

//    QString directory = QFileDialog::getExistingDirectory( this, "Select local file location:", this->launcher_.homePath() );
//    if( directory.isEmpty() ) return;

//    this->getSessionConfigItem("localFileLocation")->set_value(directory);

//    this->getDataConnector("localFileLocation")->value_config_to_widget();

//    this->getSessionConfigItem   ("wProjectFolder")->set_value("");
//    this->getDataConnector("wProjectFolder")->value_config_to_widget();
//}

void MainWindow::on_wProjectFolderButton_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    QString parent = this->local_file_location();
    QString folder = QFileDialog::getExistingDirectory( this, "Select or create a project folder:", parent );
    if( folder.isEmpty() ) {
        this->statusBar()->showMessage("No project folder selected.");
        return;
    }
    folder = QDir::cleanPath(folder);
    if( !folder.startsWith(parent) ) {
        QMessageBox::critical( this, TITLE, QString("The selected folder is not a subfolder of '%1'. "
                                                    "You must change the local file location to a folder "
                                                    "that is at least one level above this folder.").arg(parent) );
        return;
    }
    this->statusBar()->clearMessage();
    parent.append("/");
    QString relative = folder;
            relative.remove(parent);

    this->getSessionConfigItem   ("wProjectFolder")->set_value( relative );
    this->getDataConnector("wProjectFolder")->value_config_to_widget();
}

void MainWindow::on_wJobnameButton_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

//    if( this->ui->wLocal->text().isEmpty() ) {
//        QMessageBox::warning( this, TITLE, "You must select a local file location first.");
//        return;
//    }
//    if( this->ui->wProjectFolder->text().isEmpty() ) {
//        QMessageBox::warning( this, TITLE, "You must select a subfolder first.");
//        return;
//    }

    QString parent = this->jobs_project_path(LOCAL);
    QString selected = QFileDialog::getExistingDirectory( this, "Select subfolder:", parent );
    if( selected.isEmpty() ) {
        QMessageBox::warning( this, TITLE, "No directory selected.");
        return;
    }
    selected = QDir::cleanPath(selected);
    if( !selected.startsWith(parent) ) {
        QMessageBox::critical( this, TITLE, QString("The selected directory is not a subdirectory of '%1'. "
                                                    "You must change the Subfolder to a folder that is at "
                                                    "least one level above this folder.").arg(parent) );
        return;
    }
    this->statusBar()->clearMessage();
    parent.append("/");
    QString selected_leaf = selected;
    selected_leaf.remove(parent);
    this->getSessionConfigItem   ("wJobname")->set_value(selected_leaf);
    this->getDataConnector("wJobname")->value_config_to_widget(); // also triggers wJobname2

    QString remote_folder = this->jobs_project_job_path(REMOTE);

    this->getSessionConfigItem( "localFolder")->set_value(selected);
    this->getSessionConfigItem("remoteFolder")->set_value(remote_folder);
    this->getDataConnector    ( "localFolder")->value_config_to_script();
    this->getDataConnector    ("remoteFolder")->value_config_to_script();

    this->lookForJobscript( selected );
}

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
        this->getSessionConfigItem   ("wNotifyAbort")->set_value( abe.contains('a') );
        this->getSessionConfigItem   ("wNotifyBegin")->set_value( abe.contains('b') );
        this->getSessionConfigItem   ("wNotifyEnd"  )->set_value( abe.contains('e') );
        this->getDataConnector("wNotifyAbort")->value_config_to_widget();
        this->getDataConnector("wNotifyBegin")->value_config_to_widget();
        this->getDataConnector("wNotifyEnd"  )->value_config_to_widget();

//        if( !this->username().isEmpty() ) {
//            //this->on_username_editingFinished();
//            //this->on_username_returnPressed();
//            this->on_username_textChanged( this->username() );
//        }
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
 // read from file, hence no unsaved changes sofar
    return true;
}

bool MainWindow::saveJobscript( QString const& filepath )
{
    Log() << QString ("\n    writing script '%1' ... ").arg(filepath).C_STR();
    try {
        this->launcher_.script.write( filepath, false );
        Log() << "done.";
    } catch( pbs::WarnBeforeOverwrite &e ) {
        QMessageBox::StandardButton answer =
                QMessageBox::question( this, TITLE
                                     , QString("Ok to overwrite existing job script '%1'?\n(a backup will be made).")
                                          .arg(filepath)
                                     );
        if( answer == QMessageBox::Yes )
        {// make a back up
            QFile qFile(filepath);
            QString backup = QString( this->jobs_project_job_path(LOCAL) ).append("/~pbs.sh.").append( QString().setNum( qrand() ) );
            qFile.copy(backup);
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

//void MainWindow::on_wProjectFolder_textChanged(const QString &arg1)
//{
//    LOG_AND_CHECK_IGNORESIGNAL( arg1 );

//    this->getSessionConfigItem("wJobname" )->set_value("");
//    this->getDataConnector    ("wJobname" )->value_config_to_widget();
//}

//void MainWindow::on_wJobname_textChanged(const QString &arg1)
//{
//    LOG_AND_CHECK_IGNORESIGNAL( arg1 );
//    this->getSessionConfigItem("wJobname" )->set_value( arg1 );
//}

void MainWindow::on_wSave_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    QString filepath = QString( this->jobs_project_job_path(LOCAL) );
    if( filepath.isEmpty() ) {
        QMessageBox::critical(this,TITLE,"Local file location/subfolder/jobname not fully specified.\nSaving as ");
    } else {
        filepath.append("/pbs.sh");
        this->saveJobscript( filepath );
    }
}

void MainWindow::on_wReload_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    QString script_file = QString( this->jobs_project_job_path(LOCAL) ).append("/pbs.sh");
    QFileInfo qFileInfo(script_file);
    if( !qFileInfo.exists() ) {
        this->statusBar()->showMessage("No job script found, hence not loaded.");
    } else {
        QMessageBox::StandardButton answer = QMessageBox::Yes;
        if( this->launcher_.script.has_unsaved_changes() ) {
            answer = QMessageBox::question( this, TITLE, "Unsaved changes to the current job script will be lost on reloading. Continue?");
        }
        if( answer==QMessageBox::Yes ) {
            this->loadJobscript(script_file);
            this->statusBar()->showMessage("job script reloaded.");
        }
    }
}

void MainWindow::on_wSubmit_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    if( !this->requiresAuthentication("Submit job") ) {
        return;
    }

    if(  this->launcher_.script.has_unsaved_changes()
     || !this->launcher_.script.touch_finished_found()
      )
    {
        QMessageBox::Button answer = QMessageBox::question(this,TITLE,"The job script has unsaved changes.\nSave Job script?"
                                                          , QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel
                                                          , QMessageBox::Cancel
                                                          );
        if( answer==QMessageBox::Cancel ) {
            this->statusBar()->showMessage( QString("Submit job '%1' CANCELED!").arg( this->ui->wJobname->text() ) );
            return;
        } else
        if( answer==QMessageBox::No ) {
            this->statusBar()->showMessage("Job script not saved.");
        } else {// Yes
            QString filepath = QString( this->jobs_project_job_path(LOCAL) ).append("/pbs.sh");
            if( !this->saveJobscript(filepath) ) {
                return;
            }
        }
    }
 // test if the remote job folder is inexisting or empty
    QString cmd("ls %1");
    int rc = this->sshSession_.execute_remote_command( cmd, this->jobs_project_job_path(REMOTE,false) );
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
            int rc = this->sshSession_.execute_remote_command( cmd, this->jobs_project_job_path(REMOTE,false) );
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
                        this->statusBar()->showMessage( QString("Submit job '%1' CANCELED!").arg( this->ui->wJobname->text() ) );
                        return;
                    }
                }
            }
            break;
          }
          default:
            this->statusBar()->showMessage( QString("Submit job '%1' CANCELED!").arg( this->ui->wJobname->text() ) );
            return;
        }
    }
    QString remote_job_folder = this->jobs_project_job_path(REMOTE);
    this->sshSession_.sftp_put_dir( this->jobs_project_job_path(LOCAL), remote_job_folder, true );

    cmd = QString("cd %1 && qsub pbs.sh");
    this->sshSession_.execute_remote_command( cmd, remote_job_folder );
    QString job_id = this->sshSession_.qout().trimmed();
    this->statusBar()->showMessage(QString("Job submitted: %1").arg(job_id));
    cfg::Item* ci_job_list = this->getSessionConfigItem("job_list");
    if( ci_job_list->value().type()==QVariant::Invalid )
        ci_job_list->set_value_and_type( QStringList() );
    QStringList job_list = ci_job_list->value().toStringList();

    Job job( job_id, this->local_file_location(), this->remote_file_location(), this->ui->wProjectFolder->text(), this->ui->wJobname->text(), Job::Submitted );
    job_list.append( job.toString() );

    ci_job_list->set_value( job_list );
}

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
    while( true )
    {
        QTextCursor cursor = qTextEdit->textCursor();
        cursor.select(QTextCursor::LineUnderCursor);
        selection = cursor.selectedText();
        if( selection.startsWith(">>> ") ) {
            qTextEdit->setTextCursor(cursor);
            break;
        } else {
            int bn = cursor.blockNumber(); // = line number, starts at line 0, which is empty
            if( bn==0 ) {
                this->statusBar()->showMessage("No job Selected");
                return selection;
            }
            cursor.movePosition( QTextCursor::Up);
            qTextEdit->setTextCursor(cursor);
        }
    }
    selection = selection.mid(4);
    //this->statusBar()->showMessage(selection);
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
        ci_joblist->set_value( joblist.toStringList( Job::Submitted | Job::Finished ) );
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

    this->ui->wFinished->setText( joblist.toString(Job::Finished) );

    QString text = joblist.toString(Job::Submitted);
    QString username = this->username();
    if( !username.isEmpty() ) {
        QString cmd = QString("qstat -u %1");
        this->sshSession_.execute_remote_command( cmd, username );
        if( !this->sshSession_.qout().isEmpty() ) {
            text.append("\n--- ").append( cmd ).append( this->sshSession_.qout() );
             // using '---' rather than '>>>' avoids that the user can
             // select the output of qstat as if it was a job entry.
        }
    }
    this->ui->wNotFinished->setText( text);

    this->ui->wRetrieved->setText( joblist.toString(Job::Retrieved) );

    this->getSessionConfigItem("job_list")->set_value( joblist.toStringList(Job::Submitted|Job::Finished) );

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

void MainWindow::on_wReload2_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);
    Log(1) <<"\n    Forwarding to on_wReload_clicked()";
    this->on_wReload_clicked();
    if( this->statusBar()->currentMessage().startsWith("No script found") ) {
        this->statusBar()->showMessage("Script reloaded from resources page");
    }

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

}

void MainWindow::on_wSave2_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);
    if( this->launcher_.script.has_unsaved_changes() ) {
        Log(1) <<"\n    parsing script text";
        QString text = this->ui->wScript->toPlainText();
        this->launcher_.script.parse( text, false );
        Log(1) <<"\n    Forwarding to on_wSave_clicked()";
        this->on_wSave_clicked();
    }
}

void MainWindow::on_wSubmit2_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);
    Log(1) <<"\n    Forwarding to on_wSave2_clicked()";
    this->on_wSave2_clicked();
    Log(1) <<"\n    Forwarding to on_wSubmit_clicked()";
    this->on_wSubmit_clicked();
}

void MainWindow::on_wNotFinished_selectionChanged()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
    this->selected_job_ = this->selectedJob( this->ui->wNotFinished );
    if( !this->selected_job_.isEmpty() ) {
        this->statusBar()->showMessage( QString("Selected job:%1.").arg( this->selected_job_ ) );
    }
}

void MainWindow::on_wFinished_selectionChanged()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
    this->selected_job_ = this->selectedJob( this->ui->wFinished );
//    this->statusBar()->showMessage( QString("Selected job ").append( this->selected_job_ ) );
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
        this->clearSelection( this->ui->wNotFinished );
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
                                      , "Continue ..."          // button1
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

    if( this->previousPage_==MainWindowUnderConstruction )
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
            if( !silent ) {
                QString msg= QString("Could not getaddrinfo for %1.").arg( this->sshSession_.loginNode() );
                this->statusBar()->showMessage(msg);
                QMessageBox::warning( this,TITLE,msg.append("\n  . Check your internet connection."
                                                            "\n  . Check the availability of the cluster."
                                                            ) );
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
                this->warn("You must authenticate first (extra/authenticate...)");
            result = failed;
        }
        catch( ssh2::MissingKey& e ) {
            if( !silent )
                this->warn("You must authenticate first (extra/authenticate...)");
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
                this->getRemoteFileLocations_();
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
                 .arg( this->ui->wJobname->text() )
                , QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel
                , QMessageBox::Cancel
                );
        if( answer==QMessageBox::Cancel ) {
            this->statusBar()->showMessage("Local file location is unchanged.");
            return;
        } else if( answer==QMessageBox::No ) {
            this->statusBar()->showMessage("Current job script %1 NOT saved.");
        } else {// answer==QMessageBox::Yes
            QString filepath = QString( this->jobs_project_job_path(LOCAL) ).append("/pbs.sh");
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
    this->wAuthentIndicator_->setText( QString("[%1]")
                                          .arg( this->sshSession_.isAuthenticated()
                                              ? this->username()
                                              : ( !this->sshSession_.isConnected()
                                                ? QString("offline")
                                                : QString("not authenticated")
                                                )
                                              )
                                     );
    QString s;
    s = this->getSessionConfigItem("wJobname")->value().toString();
    if( this->launcher_.script.has_unsaved_changes() ) {
        s.prepend('*').append('*');
     // pallette not honored on mac?
        this->wJobnameIndicator_->setPalette(*this->paletteRedForeground_);
    } else {
        this->wJobnameIndicator_->setPalette( QApplication::palette( this->wJobnameIndicator_ ) );

    }
    this->wJobnameIndicator_->setText(s);

    s = this->getSessionConfigItem("wProjectFolder")->value().toString();
    if( !s.endsWith('/') ) s.append('/');
    this->wProjectIndicator_->setText(s);
}

 void MainWindow::createTemplateAction_triggered()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    if( this->ui->wJobname->text().isEmpty() ) {
        this->statusBar()->showMessage("No job folder, cannot create template.");
        return;
    }
    QString template_folder = QString("templates/").append( this->ui->wJobname->text() );
    template_folder = this->launcher_.homePath( template_folder );

    QString msg = QString("Creating job template from job folder '%1' ...")
                     .arg( this->ui->wJobname->text() );

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
                             , QString("Create templates/").append( this->ui->wJobname->text() )
                             ,"Select another template location"
                             );
        switch( answer ) {
          case 1:
            qdir_template_folder.mkpath( QString() );
            msg = QString("Creating 'templates/%1' ... ").arg( this->ui->wJobname->text() );
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
    copy_folder_recursively( this->jobs_project_job_path(LOCAL), template_folder );
    this->statusBar()->showMessage( msg.append( QString("'%1' created.").arg(template_folder) ) );
}


void MainWindow::selectTemplateAction_triggered()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    QString msg;
    if( this->ui->wJobname->text().isEmpty() ) {
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
    copy_folder_recursively( template_folder, this->jobs_project_job_path(LOCAL) );
    if( template_folder_contains_pbs_sh ) {
        this->loadJobscript( QString( this->jobs_project_job_path(LOCAL) ).append("/pbs.sh") );
    }
    this->statusBar()->showMessage(msg);
    return;
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

QString MainWindow::remote_file_location() {
    return this->getSessionConfigItem("remoteFileLocation")->value().toString();
}

void MainWindow::on_wShowFilelocations_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    QString job = this->ui->wJobname->text();
    if( job.isEmpty() ) {
        job = "<None>";
    }
    QString msg = QString("File locations for job name '%1':").arg(job)
                  .append("\n\nlocal: \n  ").append( this->jobs_project_job_path(LOCAL) )
                  .append("\n\nremote;\n  ").append( this->jobs_project_job_path(REMOTE,false) );
    QMessageBox::information(this, TITLE, msg, QMessageBox::Close, QMessageBox::Close );
}

void MainWindow::on_wShowLocalJobFolder_clicked()
{
    if( this->ui->wJobname->text().isEmpty() ) {
        this->statusBar()->showMessage("No job folder is selected, there is nothing to show.");
    } else {
        QFileDialog::getOpenFileName( this, TITLE, this->jobs_project_job_path(LOCAL) );
    }
}

void MainWindow::on_wShowRemoteJobFolder_clicked()
{
    this->statusBar()->showMessage("This feature is not yet available.");
}

void MainWindow::newJobscriptAction_triggered()
{
    QString example("[path/to/project_folder/]job_folder");
    bool ok;
    QString result =
            QInputDialog::getText( this,TITLE
                                 ,"Enter a file path for to the new job folder"
                                 , QLineEdit::Normal, example, &ok );
    QString absolute;
    if( ok && !result.isEmpty() && result!=example )
    {
#ifdef STAY_IN_LOCAL_FILE_LOCATION
        if( !this->is_in_local_file_location( result, &absolute ) ) {
            QString msg( this->statusBar()->currentMessage() );
            msg.append(" No new jobscript created.");
            return;
        }
#endif
        if( this->lookForJobscript( absolute,/*ask_to_load=*/true,/*create_if_inexisting=*/true ) )
        {// not canceled
         // decompose relative path in project folder and job folder:
            QStringList folders = result.split('/');
            QString job_folder = folders.last();
            folders.pop_back();
            QString project_folder = folders.join('/');
         // store in session config
            this->getSessionConfigItem("wJobname"      )->set_value(job_folder);
            this->getDataConnector    ("wJobname"      )->value_config_to_widget();
            this->getSessionConfigItem("wProjectFolder")->set_value(project_folder);
            this->getDataConnector    ("wProjectFolder")->value_config_to_widget();
        }
    }
}

void MainWindow::saveJobscriptAction_triggered()
{

}

void MainWindow::openJobscriptAction_triggered()
{

}

void MainWindow::submitJobscriptAction_triggered()
{

}

void MainWindow::showFileLocationsAction_triggered()
{

}

void MainWindow::showLocalJobFolderAction_triggered()
{

}

void MainWindow::showRemoteJobFolderAction_triggered()
{

}

bool is_absolute_path( QString const& folder )
{
    bool is_absolute = (folder.at(0)=='/');
#ifdef Q_OS_WIN
         is_absolute|= (folder.at(1)==':');
#endif
    return is_absolute;
}

bool MainWindow::is_in_local_file_location( QString const& folder, QString *absolute_folder )
{/* verify that job_folder is in the local_file_location.
  * If not, a message is sent to the the statusBar.
  * if absolute_folder is not null it contains the absolute file path without
  * redundant '/', './' or '../'
  */
    QString absolute_folder_;
    if( is_absolute_path(folder) ) {
        absolute_folder_ = QDir::cleanPath(folder);
    } else
    {// relative path, make absolute
        QDir parent( this->local_file_location() );
        absolute_folder_ = QDir::cleanPath( parent.absoluteFilePath(folder) );
    }
    if( absolute_folder ) {
        *absolute_folder = absolute_folder_;
    }
    if( absolute_folder->startsWith( this->local_file_location() ) ) {
        return true;
    } else {
        QString msg = QString("Folder '%1' is located outside the local file location '%2'.")
                         .arg(absolute_folder_)
                         .arg(this->local_file_location());
        this->statusBar()->showMessage(msg);
        return false;
    }
}

