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

#define INITIAL_VERBOSITY 2

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
    #define LOG_CALLER \
        QString caller = QString("\n[ MainWindow::%1, %2, %3 ]") \
                         .arg(__func__).arg(__FILE__).arg(__LINE__);\
        Log() << INFO << caller.C_STR()

 // LOG_CALLER has no terminating ';' so we can do
 //     LOG_CALLER << whatever << and << even << more;
 //-----------------------------------------------------------------------------


#define TITLE "Launcher"

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
      , previousPage_(0)
      , execute_remote_command_(&this->sshSession_)
      , verbosity_(INITIAL_VERBOSITY)
      , pendingRequest_(NoPendingRequest)
    {
        LOG_CALLER << "\n    Running Launcher version = "<<VERSION;
        ui->setupUi(this);

        ssh2::Session::autoOpen = true;

        if( !QFile(this->launcher_.homePath("Launcher.cfg")).exists() )
        {// This must be the first time after installation...
            this->setupHome();
        }
#ifdef QT_DEBUG
        this->setWindowTitle(QString("%1 %2").arg(TITLE).arg(VERSION));
#else
        this->setWindowTitle(QString("%1 %2").arg(TITLE).arg(short_version(3)));
#endif
        qsrand( time(nullptr) );

     // read the cfg::Config object from file
     // (or create an empty one if there is no config file yet)
        QString launcher_cfg( Launcher::homePath("Launcher.cfg") );
        if( QFileInfo(launcher_cfg).exists() ) {
            this->launcher_.config.load(launcher_cfg);
            if( this->launcher_.config.contains("windowHeight") ) {
                this->resize( this->launcher_.config["windowWidth" ].value().toInt()
                            , this->launcher_.config["windowHeight"].value().toInt() );
            }
        } else {
            this->launcher_.config.setFilename( launcher_cfg );
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
     // wWalltimeUnit
        ci = this->getConfigItem("wWalltimeUnit");
        if( !ci->isInitialized() ) {
            QStringList units;
            units <<"seconds"<<"minutes"<<"hours"<<"days"<<"weeks";
            ci->set_choices(units);
            ci->set_value("hours");
        }

        ci = this->getConfigItem("walltimeSeconds","1:00:00");

        ci = this->getConfigItem("wNotifyAddress", QString() );
        ci = this->getConfigItem("notifyWhen");
        if( !ci->isInitialized() ) {
            ci->set_value("");
            this->getConfigItem("wNotifyAbort", false );
            this->getConfigItem("wNotifyBegin", false );
            this->getConfigItem("wNotifyEnd"  , false );
        }
//        ci = this->getConfigItem("wJobname", QString() );

        dc::DataConnectorBase::config = &(this->launcher_.config);
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
        ci = this->getConfigItem("wCluster");
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

        this->data_.append( dc::newDataConnector( this->ui->wUsername, "wUsername", "" ) );
        bool can_authenticate = !this->getConfigItem("wUsername")->value().toString().isEmpty();
        this->activateAuthenticateButton( can_authenticate );

     //=====================================
     // wRemote/wLocal/wSubfolder/wJobname
     // wRemote
        ci = this->getConfigItem("wRemote");
        if( !ci->isInitialized() ) {
            ci->set_choices( QStringList({"$VSC_SCRATCH","$VSC_DATA"}) );
            ci->set_value("$VSC_SCRATCH");
        }
        this->data_.append( dc::newDataConnector( this->ui->wRemote, "wRemote", "") );
     // wLocal
        ci = this->getConfigItem("wLocal");
        if( !ci->isInitialized() ) {
            ci                               ->set_value_and_type( this->launcher_.homePath() );
            this->getConfigItem("wSubfolder")->set_value_and_type("jobs");
            this->getConfigItem("wJobname"  )->set_value_and_type("hello_world");
        }
        this->data_.append( dc::newDataConnector( this->ui->wLocal, "wLocal", "") );
     // Subfolder
        QString value = this->getConfigItem("wSubfolder")->value().toString();
        if( !value.isEmpty() ) {
            this->ui->wSubfolder ->setText(value);
            this->ui->wSubfolder2->setText(value);
        }
     // wJobname
        value = this->getConfigItem("wJobname")->value().toString();
        if( !value.isEmpty() ) {
            this->ui->wJobname ->setText(value);
            this->ui->wJobname2->setText(value);
        }

     // verify existence of local file location (if present) and set tooltips
        QString local_subfolder_jobname = this->local_subfolder_jobname();
        this->ui->wSubfolder ->setToolTip( this-> local_subfolder() );
        this->ui->wSubfolder2->setToolTip( this->remote_subfolder() );
        this->ui->wJobname   ->setToolTip(        local_subfolder_jobname   );
        this->ui->wJobname2  ->setToolTip( this->remote_subfolder_jobname() );

        this->data_.append( dc::newDataConnector( this->ui->wJobname  , "wJobname"    , "-N" ) );
        this->data_.append( dc::newDataConnector( this->ui->wSubfolder, "wSubfolder"  , ""   ) );
        this->data_.append( dc::newDataConnector( this->ui->wLocal    , "wLocal"      , ""   ) );
        this->data_.append( dc::newDataConnector(                       "localFolder" , "local_folder" ) );
        this->data_.append( dc::newDataConnector(                       "remoteFolder", "remote_folder") );

        QString qs;
        qs = this->getConfigItem("wSubfolder")->value().toString();
        this->ui->wSubfolder ->setText(qs);
        this->ui->wSubfolder2->setText(qs);

        qs = this->getConfigItem("wJobname")->value().toString();
        this->ui->wJobname ->setText(qs);
        this->ui->wJobname2->setText(qs);

        this->getConfigItem("wEmptyRemoteFolder", true );
        this->data_.append( dc::newDataConnector( this->ui->wEmptyRemoteFolder, "wEmptyRemoteFolder", "") );

        this->getConfigItem("wCheckCopyToDesktop", true );
        this->data_.append( dc::newDataConnector( this->ui->wCheckCopyToDesktop, "wCheckCopyToDesktop", "") );

        this->getConfigItem("CheckCopyToVscData", true );
        this->data_.append( dc::newDataConnector( this->ui->wCheckCopyToVscData, "CheckCopyToVscData", "") );

        this->getConfigItem("wCheckDeleteLocalJobFolder", true );
        this->data_.append( dc::newDataConnector( this->ui->wCheckDeleteLocalJobFolder, "wCheckDeleteLocalJobFolder", "") );

        this->getConfigItem("wCheckDeleteRemoteJobFolder", true );
        this->data_.append( dc::newDataConnector( this->ui->wCheckDeleteRemoteJobFolder, "wCheckDeleteRemoteJobFolder", "") );

        this->setIgnoreSignals(false);

        this->ui->wAutomaticRequests->setChecked( true );

        this->paletteRedForeground_ = new QPalette();
        this->paletteRedForeground_->setColor(QPalette::Text,Qt::red);

        if( !this->getConfigItem("wJobname")->value().toString().isEmpty() ) {
            this->lookForJobscript( local_subfolder_jobname );
        } else
        {// no jobname - all values are either default, or come from the config file
         // we consider this as NO unsaved changes (
            this->launcher_.script.set_has_unsaved_changes(false);
        }
        this->on_wAuthenticate_clicked();

        createActions();
        createMenus();
#ifdef QT_DEBUG
        this->verbosity_= 2;
#endif
    }

    void MainWindow::createActions()
    {
        aboutAction_ = new QAction(tr("&About"), this);
        connect( aboutAction_, SIGNAL(triggered()), this, SLOT( about() ) );

        verboseAction_ = new QAction(tr("Verbose logging"), this);
        connect( verboseAction_, SIGNAL(triggered()), this, SLOT( verbose_logging() ) );
    }

    void MainWindow::createMenus()
    {
        helpMenu_ = menuBar()->addMenu("&Help");
        helpMenu_->addAction(aboutAction_);
        extraMenu_ = menuBar()->addMenu("&Extra");
        extraMenu_ ->addAction(verboseAction_);
    }

    void MainWindow::about()
    {
        QString msg;
        msg.append( QString("program : %1\n").arg(TITLE  ) )
           .append( QString("version : %1\n").arg(VERSION) )
           .append(         "contact : engelbert.tijskens@uantwerpen.be")
           ;
        QMessageBox::about(this,TITLE,msg);
    }


    void MainWindow::verbose_logging()
    {
        this->verbosity_ = ( this->verboseAction_->isChecked() ? 2 : 0 );
    }

    QString
    MainWindow::
    local_subfolder()
    {
        cfg::Item* ci = this->getConfigItem("wLocal");
        QString path = ci->value().toString();
        if( path.isEmpty() || !QFile::exists(path) ) {
            if( !path.isEmpty() )
                this->statusBar()->showMessage( QString("Local file location '%1' was not found").arg(path) );
            QString empty;
            ci->set_value(empty);
            this->getConfigItem("wSubfolder")->set_value(empty);
            this->getConfigItem("wJobname"  )->set_value(empty);
            return empty;
        }
        ci = this->getConfigItem("wSubfolder");
        QString s = ci->value().toString();
        path.append("/").append(s);
        if( s.isEmpty() || !QFile::exists(path) ) {
            if( !s.isEmpty() )
                this->statusBar()->showMessage( QString("Local file location '%1' was not found").arg(path) );
            QString empty;
            ci->set_value(empty);
            this->getConfigItem("wJobname"  )->set_value(empty);
            return empty;
        }
        return path;
    }

    QString
    MainWindow::
    local_subfolder_jobname()
    {
        QString path = this->local_subfolder();
        if( path.isEmpty() ) {
            return path;
        }
        cfg::Item* ci = this->getConfigItem("wJobname");
        QString s = ci->value().toString();
        path.append("/").append(s);
        if( s.isEmpty() || !QFile::exists(path) ) {
            if( !s.isEmpty() )
                this->statusBar()->showMessage( QString("Local file location '%1' was not found").arg(path) );
            QString empty;
            ci->set_value(empty);
            return empty;
        }
        return path;
    }

    QString
    MainWindow::
    remote_subfolder()
    {
        QString path = this->getConfigItem("wRemote")->value().toString();
        if( this->remote_env_vars_.contains(path) )
            path = this->remote_env_vars_[path];
        QString s = this->getConfigItem("wSubfolder")->value().toString();
        if( s.isEmpty() )
            return s;
        path.append("/").append(s);
        return path;
    }

    QString
    MainWindow::
    remote_subfolder_jobname()
    {
        QString path = this->remote_subfolder();
        if( path.isEmpty() )
            return path;
        QString s = this->getConfigItem("wJobname")->value().toString();
        if( s.isEmpty() )
            return s;
        path.append("/").append(s);
        return path;
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
        LOG_CALLER
         << "\n    Launcher closing down";

        for( QList<dc::DataConnectorBase*>::iterator iter = this->data_.begin(); iter!=this->data_.end(); ++iter ) {
            (*iter)->value_widget_to_config();
            delete (*iter);
        }

        QSize windowSize = this->size();
        this->getConfigItem("windowWidth" )->set_value( windowSize.width () );
        this->getConfigItem("windowHeight")->set_value( windowSize.height() );

        Log() << "\n    - saving config file.";
        this->launcher_.config.save();

        if ( this->launcher_.script.has_unsaved_changes() ) {
            QMessageBox::Button answer = QMessageBox::question(this,TITLE,"The current script has unsaved changes.\nDo you want to save?");
            if(answer==QMessageBox::Yes) {
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
    getConfigItem( QString const& name, T default_value )
    {
        cfg::Item* ci = this->launcher_.config.getItem(name);
        if( ci->type() == QVariant::Invalid )
            ci->set_value_and_type( default_value );
        return ci;
    }
 //-----------------------------------------------------------------------------
    cfg::Item*
    MainWindow::
    getConfigItem( QString const& name ) {
        return this->launcher_.config.getItem(name);
    }
 //-----------------------------------------------------------------------------
    ClusterInfo const&
    MainWindow::clusterInfo() const
    {
        QString            clusterName = const_cast<MainWindow*>(this)->launcher_.config.getItem("wCluster")->value().toString();
        ClusterInfo const& clusterInfo = *( this->launcher_.clusters.find( clusterName ) );
        return clusterInfo;
    }
 //-----------------------------------------------------------------------------
    NodesetInfo const&
    MainWindow::nodesetInfo() const
    {
        MainWindow* non_const_this = const_cast<MainWindow*>(this);
        QString nodesetName = non_const_this->getConfigItem("wNodeset")->value().toString();
        QString clusterName = non_const_this->getConfigItem("wCluster")->value().toString();
        NodesetInfo const& nodesetInfo = *( this->launcher_.clusters[clusterName].nodesets().find( nodesetName ) );
        return nodesetInfo;
    }
 //------------------------------------------------------------------------------
    void MainWindow::clusterDependencies( bool updateWidgets )
    {// act on config items only...
        {
            IgnoreSignals ignoreSignals( this );
            QString cluster = this->getConfigItem("wCluster")->value().toString();
            ClusterInfo& clusterInfo = *(this->launcher_.clusters.find( cluster ) );
         // Initialize Config items that depend on the cluster
            cfg::Item* ci = this->getConfigItem("wNodeset");
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
            this->execute_remote_command_.set_remote_commands( clusterInfo.remote_commands() );
        }
     // try to authenticate to refresh the list of available modules and the
     // remote file locations $VSC_DATA and $VSC_SCRATCH
        this->on_wAuthenticate_clicked();
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
        cfg::Item* ci = this->getConfigItem("wNNodes");
        QList<QVariant> range;
        range.append( 1 );
        range.append( nodesetInfo.nNodes() );
        ci->set_choices( range, true );
        ci->set_value( 1 );

        ci = this->getConfigItem("wNCoresPerNode");
        range.last() = nodesetInfo.nCoresPerNode();
        ci->set_choices( range, true );
        ci->set_value( range.last() );

        this->ui->wGbPerNode->setText( QString().setNum( nodesetInfo.gbPerNodeAvailable(),'g',3) );
         // This one has no slots connected - no need to ignore signals
         // It is also not connected to an cfg::Item by a DataConnector, hence
         // we do update it even if updateWidgets is false.

        ci = this->getConfigItem("wNCores");
        range.last() = nodesetInfo.nCoresAvailable();
        ci->set_choices( range, true );
        ci->set_value( nodesetInfo.nCoresPerNode() );

        ci = this->getConfigItem("wGbPerCore");
        range.clear();
        range.append( round(nodesetInfo.gbPerCoreAvailable(),3) );
        range.append( round(nodesetInfo.gbPerNodeAvailable(),3) );
        ci->set_choices( range, true );
        ci->set_value( nodesetInfo.gbPerCoreAvailable() );

        double gbTotal  = this->ui->wGbPerNode->text().toDouble();
               gbTotal *= this->getConfigItem("wNNodes")->value().toInt();
        this->getConfigItem("wGbTotal")->set_value( QString().setNum( gbTotal, 'g', 3 ) );

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
        QString unit = this->getConfigItem("wWalltimeUnit")->value().toString();
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

        cfg::Item* ci_wWalltime = this->getConfigItem("wWalltime");
        try {
            ci_wWalltime->set_choices(range,true);
        } catch( cfg::InvalidConfigItemValue& e) {}
        double w = (double) unformatWalltimeSeconds( this->getConfigItem("walltimeSeconds")->value().toString() );
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

#define NO_ARGUMENT QString()
#define LOG_AND_CHECK_IGNORESIGNAL( ARGUMENT ) \
  {\
    QString msg("\n[ MainWindow::%1, %2, %3 ]");\
    msg = msg.arg(__func__).arg(__FILE__).arg(__LINE__);\
    if( this->verbosity_ && !QString((ARGUMENT)).isEmpty() ) \
        msg.append("\n    Function was called with argument : ").append(ARGUMENT);\
    msg.append( this->check_script_unsaved_changes() ) ;\
    if( this->verbosity_ > 1 ) {\
        if( this->ignoreSignals_ ) msg.append("\n    Ignoring signal");\
        else                       msg.append("\n    Executing signal");\
    }\
    Log() << INFO << msg.toStdString();\
    if( this->ignoreSignals_ ) return;\
  }

void MainWindow::on_wCluster_currentIndexChanged( QString const& arg1 )
{
    LOG_AND_CHECK_IGNORESIGNAL( arg1 );

    this->launcher_.config["wCluster"].set_value(arg1);
    this->clusterDependencies(true);
}

void MainWindow::on_wNodeset_currentTextChanged(const QString &arg1)
{
    LOG_AND_CHECK_IGNORESIGNAL( arg1 );

    this->getConfigItem("wNodeset")->set_value(arg1);
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
    this->getConfigItem("wNNodes"       )->set_value( nodeset.granted().nNodes );
    this->getConfigItem("wNCoresPerNode")->set_value( nodeset.granted().nCoresPerNode );
    this->getConfigItem("wNCores"       )->set_value( nodeset.granted().nCores );
    this->getConfigItem("wGbPerCore"    )->set_value( nodeset.granted().gbPerCore );
    this->getConfigItem("wGbTotal"      )->set_value( nodeset.granted().gbTotal );
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
    this->getConfigItem("wNNodes"       )->set_value( nodeset.granted().nNodes );
    this->getConfigItem("wNCoresPerNode")->set_value( nodeset.granted().nCoresPerNode );
    this->getConfigItem("wNCores"       )->set_value( nodeset.granted().nCores );
    this->getConfigItem("wGbPerCore"    )->set_value( nodeset.granted().gbPerCore );
    this->getConfigItem("wGbTotal"      )->set_value( nodeset.granted().gbTotal );
    this->updateResourceItems();
    this->pendingRequest_ = NoPendingRequest;
}

void MainWindow::on_wPages_currentChanged(int index_current_page)
{
    LOG_AND_CHECK_IGNORESIGNAL( QString().setNum(index_current_page) );

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
        //if( this->previousPage_==0)
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
            bool ok = this->isUserAuthenticated();
            if( !ok ) {
                QMessageBox::warning(this,TITLE,"Since you are not authenticated, you have no access to the remote host. Remote functionality on this tab is disabled.");
            }
            this->ui->wRetrieveAllJobs    ->setEnabled(ok);
            this->ui->wRetrieveSelectedJob->setEnabled(ok);
            this->ui->wCheckDeleteRemoteJobFolder->setEnabled(ok);
            JobList joblist( this->getConfigItem("job_list")->value().toStringList() );
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

void MainWindow::on_wWalltimeUnit_currentTextChanged(const QString &arg1)
{
    LOG_AND_CHECK_IGNORESIGNAL( arg1 );

    this->getDataConnector("wWalltimeUnit")->value_widget_to_config();
    this->walltimeUnitDependencies(true);
}

void MainWindow::on_wWalltime_valueChanged(double arg1)
{
    LOG_AND_CHECK_IGNORESIGNAL( QString().setNum(arg1) );

    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;

    cfg::Item* walltimeSeconds = this->getConfigItem("walltimeSeconds");
    double seconds = arg1*this->walltimeUnitSeconds_;
    walltimeSeconds->set_value( formatWalltimeSeconds( seconds ) );
    this->getDataConnector("walltimeSeconds")->value_config_to_script();
    Log(1) <<"    walltime set to: " << seconds << "s.";
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
        this->getConfigItem("wNotifyAddress")->set_value(validated);

        IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
        dc::DataConnectorBase* link = this->getDataConnector("wNotifyAddress");
        if( link->value_config_to_widget() ) {
            link->value_config_to_script();
        }
    }
}

void MainWindow::update_abe( QChar c, bool checked )
{
    cfg::Item* ci_notifyWhen = this->getConfigItem("notifyWhen");
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

bool MainWindow::getPrivatePublicKeyPair()
{
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
    QString private_key;
    QString msg("You must provide a PRIVATE key file (openssh format).\n");
#ifdef Q_OS_MAC// Q_OS_WIN
    msg+="[Windows users should be aware that PuTTY keys are NOT in openssh format. Consult the VSC website for how to convert your PuTTY keys.]\n";
#endif
    msg+="Continue?";
    QMessageBox::StandardButton answer = QMessageBox::question(this, TITLE, msg, QMessageBox::Ok|QMessageBox::No,QMessageBox::Ok);
    if( answer==QMessageBox::Ok ) {
        private_key = QFileDialog::getOpenFileName( this, "Select your PRIVATE key file:", start_in );
        if( !QFileInfo( private_key ).exists() ) {
            this->statusBar()->showMessage( QString( "Private key '%1' not found.").arg(private_key) );
            return false;
        }
    } else {
        return false;
    }
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
    return true;
}

void MainWindow::on_wUsername_editingFinished()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    QString username = this->ui->wUsername->text().trimmed();
    username = validateUsername( username );

    if( username.isEmpty() )
    {// not a valid username
        this->ui->wUsername->setPalette(*this->paletteRedForeground_);
        this->statusBar()->showMessage("Invalid username");
     // reset the ssh session
        this->sshSession_.reset();
     // and forget the keys in the config file
        this->getConfigItem("privateKey")->set_value( QString() );
        this->getConfigItem("publicKey" )->set_value( QString() );
        this->activateAuthenticateButton(false);
    } else
    {// valid username
        this->ui->wUsername->setPalette( QApplication::palette(this->ui->wUsername) );
        cfg::Item* user = this->getConfigItem("wUsername");
        if( user->value().toString() == username )
        {// username is the same as in the config, hence it has not changed!
            if( this->isUserAuthenticated() )
            {// User is still authenticated, there is little to be done.
                this->activateAuthenticateButton(false,"authenticated");
                return;
            } else
            {// Authentication needed
                this->sshSession_.setUsername(username);
             // set private/public key pair from config
                if( this->sshSession_.setPrivatePublicKeyPair
                        ( this->getConfigItem("privateKey")->value().toString()
                        , this->getConfigItem("publicKey" )->value().toString()
                        , false
                        )
                  ) {
                    this->statusBar()->showMessage("Private/public key pair found in config file.");
                }
            }
        } else
        {// this is a new username, different from current config value:
         // store in config
            user->set_value(username);
         // clear config values for private/public key pair
            this->getConfigItem("privateKey")->set_value( QString() );
            this->getConfigItem("publicKey" )->set_value( QString() );
         // set the username for the ssh session
            this->sshSession_.setUsername(username);
        }
        this->activateAuthenticateButton(true);
        this->statusBar()->showMessage("Ready to authenticate");
    }
}

bool MainWindow::isUserAuthenticated() const
{
    return this->sshSession_.isAuthenticated();
}

void MainWindow::activateAuthenticateButton (bool activate, QString const& inactive_button_text )
{
    if( activate ) {
        this->ui->wAuthenticate->setText("authenticate...");
        this->ui->wAuthenticate->setEnabled(true);
    } else {
        this->ui->wAuthenticate->setText(inactive_button_text);
        this->ui->wAuthenticate->setEnabled(false);
    }
}


void MainWindow::on_wAuthenticate_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    this->setIgnoreSignals();
    int max_attempts = 3;
    int attempts = max_attempts;
    while( attempts )
    {
        try {
            this->sshSession_.open();
        }
        catch( ssh2::MissingUsername& e ) {
            QString username = /*dc::*/validateUsername( this->ui->wUsername->text() );
            if( username.isEmpty() ) {
                this->statusBar()->showMessage("Invalid username");
                this->activateAuthenticateButton(false,"authenticate...");
                break;
            }
            this->sshSession_.setUsername( username );
            if( this->getConfigItem("wUsername")->value().toString() == username ) {
                this->sshSession_.setPrivatePublicKeyPair
                        ( this->getConfigItem("privateKey")->value().toString()
                        , this->getConfigItem("publicKey" )->value().toString()
                        , false
                        );
            }
            continue;
        }
        catch( ssh2::MissingKey& e ) {
            if( !this->getPrivatePublicKeyPair() )
            {// failed to provide existing keys
                break;
            } else
            {// existing keys provided
                continue;
            }
        }
        catch( ssh2::PassphraseNeeded& e ) {
            QString msg = QString("Enter passphrase for unlocking key '%1':").arg( this->sshSession_.privateKey() );
            QString passphrase = QInputDialog::getText( 0, TITLE, msg, QLineEdit::Password );
            this->sshSession_.setPassphrase( passphrase );
            --attempts;
            continue;
        }
        catch( ssh2::WrongPassphrase& e ) {
            QString msg = QString("Could not unlock private key.\n%1 attempts left.\nEnter passphrase for unlocking key '%2':")
                          .arg(attempts).arg( this->sshSession_.privateKey() );
            QString passphrase = QInputDialog::getText( 0, "Passphrase needed", msg );
            this->sshSession_.setPassphrase( passphrase );
            --attempts;
            continue;
        }
        catch( std::runtime_error& e ) {
            QString msg( e.what() );
            if( msg.startsWith("getaddrinfo[8] :") ) {
                msg.append(" (verify your internet connection).");
            }
            this->statusBar()->showMessage(msg);
            Log(0) << msg.prepend("\n    ").C_STR();
         // remove keys from ssh session and from config
            this->sshSession_.setUsername( this->getConfigItem("wUsername")->value().toString() );
            this->getConfigItem("privateKey")->set_value( QString() );
            this->getConfigItem("publicKey" )->set_value( QString() );
            break;
        }
     // success
        this->activateAuthenticateButton(false,"authenticated");
        this->statusBar()->showMessage("Authentication succeeded.");
        this->getConfigItem("privateKey")->set_value( this->sshSession_.privateKey() );
        this->getConfigItem("publicKey" )->set_value( this->sshSession_. publicKey() );

        this->resolveRemoteFileLocations_();

        QString qs = this->ui->wRemote->currentText();
        if( this->remote_env_vars_.contains(qs) ) {
            this->ui->wRemote->setToolTip( this->remote_env_vars_[qs] );
        }
        QString cmd("__module_avail");
        this->execute_remote_command_(cmd);
        QStringList modules = this->sshSession_.qout().split("\n");
        this->ui->wSelectModule->clear();
        this->ui->wSelectModule->addItems(modules);

        QString modules_cluster = QString("modules_").append(this->getConfigItem("wCluster")->value().toString() );
        this->getConfigItem(modules_cluster)->set_choices(modules);

        break;
    }
    if( attempts==0 )
    {// remove the keys from the config and the session - maybe they were wrong
        this->sshSession_.setUsername( this->getConfigItem("wUsername")->value().toString() );
        this->getConfigItem("privateKey")->set_value( QString() );
        this->getConfigItem("publicKey" )->set_value( QString() );
        this->statusBar()->showMessage( QString("Authentication failed: %1 failed attempts to provide passphrase.").arg(max_attempts) );
    }
    if( !this->isUserAuthenticated() ) {
        QString modules_cluster = QString("modules_").append(this->getConfigItem("wCluster")->value().toString() );
        QStringList modules = this->getConfigItem(modules_cluster)->choices_as_QStringList();
        if( modules.isEmpty() ) {
            this->statusBar()->showMessage("List of modules not available for current cluster ");
        } else {
            this->ui->wSelectModule->clear();
            this->ui->wSelectModule->addItems(modules);
            this->statusBar()->showMessage("Using List of modules from a previous session. Authenticate to update it.");
        }
    }
    this->setIgnoreSignals(false);
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
            int rv = this->execute_remote_command_(cmd);
            bool is_finished = ( rv==0 );
            if( !is_finished ) {
                return false;
            }
         // It is possible but rare that finished.<jobid> exists, but the
         // epilogue is still running. To make sure:
         // Check the output of qstat -u username
            cmd = QString("qstat -u %1");
            rv = this->execute_remote_command_( cmd, this->sshSession_.username() );
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
        int rc = this->execute_remote_command_( cmd, vsc_data_destination );

        cmd = QString("cp -rv %1 %2"); // -r : recursive, -v : verbose
        rc = this->execute_remote_command_( cmd, job.remote_job_folder(), vsc_data_destination );
        job.status_ = Job::Retrieved;

        if( rc ) {/*keep compiler happy (rc unused variable)*/}
    }
    return job.status()==Job::Retrieved;
}

void MainWindow::resolveRemoteFileLocations_()
{
    this->remote_env_vars_.clear();
    for ( int i=0; i<this->ui->wRemote->count(); ++i ) {
        QString env = this->ui->wRemote->itemText(i);
        QString cmd("echo %1");
        try {
            this->execute_remote_command_( cmd, env );
            this->remote_env_vars_[env] = this->sshSession_.qout().trimmed();
        } catch(std::runtime_error &e )
        {}
   }
}

void MainWindow::on_wLocalFileLocationButton_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    QString directory = QFileDialog::getExistingDirectory( this, "Select local file location:", this->launcher_.homePath() );
    if( directory.isEmpty() ) return;

    this->getConfigItem("wLocal")->set_value(directory);

    this->getDataConnector("wLocal")->value_config_to_widget();

    this->getConfigItem   ("wSubfolder")->set_value("");
    this->getDataConnector("wSubfolder")->value_config_to_widget();

    this->ui->wSubfolder ->setToolTip("");
    this->ui->wSubfolder2->setToolTip("");
}

void MainWindow::on_wSubfolderButton_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    if( this->ui->wLocal->text().isEmpty() ) {
        QMessageBox::warning( this, TITLE, "You must select a local file location first.");
        return;
    }

    QString parent = this->ui->wLocal->text();
    QString folder = QFileDialog::getExistingDirectory( this, "Select subfolder:", parent );
    if( folder.isEmpty() ) {
        QMessageBox::warning( this, TITLE, "No directory selected.");
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

    this->getConfigItem   ("wSubfolder")->set_value( relative );
    this->getDataConnector("wSubfolder")->value_config_to_widget(); // also triggers wSubfolder2

    this->ui->wSubfolder ->setToolTip(this-> local_subfolder() );
    this->ui->wSubfolder2->setToolTip(this->remote_subfolder() );
    this->ui->wJobname   ->setToolTip("");
    this->ui->wJobname2  ->setToolTip("");
}

void MainWindow::on_wJobnameButton_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    if( this->ui->wLocal->text().isEmpty() ) {
        QMessageBox::warning( this, TITLE, "You must select a local file location first.");
        return;
    }
    if( this->ui->wSubfolder->text().isEmpty() ) {
        QMessageBox::warning( this, TITLE, "You must select a subfolder first.");
        return;
    }

    QString parent = this->local_subfolder();
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
    this->getConfigItem   ("wJobname")->set_value(selected_leaf);
    this->getDataConnector("wJobname")->value_config_to_widget(); // also triggers wJobname2

    this->ui->wJobname ->setToolTip(selected);
    QString remote_folder = this->remote_subfolder_jobname();
    this->ui->wJobname2->setToolTip(remote_folder);

    this->getConfigItem( "localFolder")->set_value(selected);
    this->getConfigItem("remoteFolder")->set_value(remote_folder);
    this->getDataConnector( "localFolder")->value_config_to_script();
    this->getDataConnector("remoteFolder")->value_config_to_script();

    this->lookForJobscript( selected );
}

void MainWindow::lookForJobscript( QString const& job_folder)
{
    QString script_file = QString(job_folder).append("/pbs.sh");
    if( QFile::exists(script_file) ) {
        QMessageBox::StandardButton answer =
            QMessageBox::question(this, TITLE
                                 , QString("The local job directory '%1' already contains a job script 'pbs.sh'.\n"
                                           "Do you want to load it?")
                                      .arg( job_folder )
                                 , QMessageBox::Yes|QMessageBox::No
                                 , QMessageBox::Yes
                                 );
        if( answer == QMessageBox::Yes ) {
            this->loadJobscript(script_file);
            this->statusBar()->showMessage( QString("Job script '%1' loaded.").arg(script_file) );
        } else {
            QMessageBox::warning( this, TITLE
                                , QString("The job script '%1' was not loaded. It will be overwritten if you save the current job script under this jobname.")
                                     .arg( script_file )
                                );
        }
    }
}

bool MainWindow::loadJobscript( QString const& filepath )
{
    Log(1) << QString("\n    loading job script '%1'.").arg(filepath).C_STR();
    try
    {
        Log(1) << "\n    reading the job script ... ";
        this->launcher_.script.read(filepath);
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
        QString abe = this->getConfigItem("notifyWhen")->value().toString();
        this->getConfigItem   ("wNotifyAbort")->set_value( abe.contains('a') );
        this->getConfigItem   ("wNotifyBegin")->set_value( abe.contains('b') );
        this->getConfigItem   ("wNotifyEnd"  )->set_value( abe.contains('e') );
        this->getDataConnector("wNotifyAbort")->value_config_to_widget();
        this->getDataConnector("wNotifyBegin")->value_config_to_widget();
        this->getDataConnector("wNotifyEnd"  )->value_config_to_widget();

        if( !this->ui->wUsername->text().isEmpty() ) {
            this->on_wUsername_editingFinished();
        }
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
        this->launcher_.script.write( filepath );
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
            QString backup = QString( this->local_subfolder_jobname() ).append("/~pbs.sh.").append( QString().setNum( qrand() ) );
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

void MainWindow::on_wSubfolder_textChanged(const QString &arg1)
{
    LOG_AND_CHECK_IGNORESIGNAL( arg1 );

    this->ui->wSubfolder2->setText(arg1);

    this->getConfigItem   ("wJobname" )->set_value("");
    this->getDataConnector("wJobname" )->value_config_to_widget();
}

void MainWindow::on_wJobname_textChanged(const QString &arg1)
{
    LOG_AND_CHECK_IGNORESIGNAL( arg1 );

    this->ui->wJobname2->setText(arg1);

    this->getConfigItem("wJobname" )->set_value( arg1 );
}


void MainWindow::on_wRemote_currentIndexChanged(const QString &arg1)
{
    LOG_AND_CHECK_IGNORESIGNAL( arg1 );

    this->getConfigItem("wRemote")->set_value(arg1);

    QString tooltip = arg1;
    if( this->remote_env_vars_.contains(arg1) ) {
        tooltip = this->remote_env_vars_[arg1];
    }
    this->ui->wRemote    ->setToolTip( tooltip );
    this->ui->wSubfolder2->setToolTip( this->remote_subfolder() );
    this->ui->wJobname2  ->setToolTip( this->remote_subfolder_jobname() );
}

void MainWindow::on_wSave_clicked()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    QString filepath = QString( this->local_subfolder_jobname() );
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

    QString script_file = QString( this->local_subfolder_jobname() ).append("/pbs.sh");
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

    if(  this->launcher_.script.has_unsaved_changes()
     || !this->launcher_.script.touch_finished_found()
      )
    {
        QString filepath = QString( this->local_subfolder_jobname() ).append("/pbs.sh");
        if( !this->saveJobscript(filepath) ) {
            return;
        }
    }
    if( !this->isUserAuthenticated() ) {
        QMessageBox::critical( this, TITLE, "You must authenticate first...");
        return;
    }
    QString cmd;
    if( this->ui->wEmptyRemoteFolder->isChecked() )
    {
        QMessageBox::Button answer = QMessageBox::question(this,TITLE,"OK to erase remote job folder contents?");
        if( answer==QMessageBox::Yes ) {
            cmd = QString("rm -rf %1/*");
            this->execute_remote_command_( cmd, this->remote_subfolder_jobname() );
            this->statusBar()->showMessage("Erased remote job folder.");
        } else {
            this->statusBar()->showMessage("Remote job folder not erased.");
        }
    }
    this->sshSession_.sftp_put_dir( this->local_subfolder_jobname(), this->remote_subfolder_jobname(), true );

    cmd = QString("cd %1 && qsub pbs.sh");
    this->execute_remote_command_( cmd, this->remote_subfolder_jobname() );
    QString job_id = this->sshSession_.qout().trimmed();
    this->statusBar()->showMessage(QString("Job submitted: %1").arg(job_id));
    cfg::Item* ci_job_list = this->getConfigItem("job_list");
    if( ci_job_list->value().type()==QVariant::Invalid )
        ci_job_list->set_value_and_type( QStringList() );
    QStringList job_list = ci_job_list->value().toStringList();
    QString remote = this->ui->wRemote->currentText();
    if( remote.startsWith('$') )
        remote = this->remote_env_vars_[remote];
    Job job( job_id, this->ui->wLocal->text(), remote, this->ui->wSubfolder->text(), this->ui->wJobname->text(), Job::Submitted );
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

    cfg::Item* ci_joblist = this->getConfigItem("job_list");
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

    cfg::Item* ci_joblist = this->getConfigItem("job_list");
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

    cfg::Item* ci_joblist = this->getConfigItem("job_list");
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
    QString username = this->ui->wUsername->text();
    if( !username.isEmpty() ) {
        QString cmd = QString("qstat -u %1");
        this->execute_remote_command_( cmd, username );
        if( !this->sshSession_.qout().isEmpty() ) {
            text.append("\n--- ").append( cmd ).append( this->sshSession_.qout() );
             // using '---' rather than '>>>' avoids that the user can
             // select the output of qstat as if it was a job entry.
        }
    }
    this->ui->wNotFinished->setText( text);

    this->ui->wRetrieved->setText( joblist.toString(Job::Retrieved) );

    this->getConfigItem("job_list")->set_value( joblist.toStringList(Job::Submitted|Job::Finished) );

    this->selected_job_.clear();
}

void MainWindow::deleteJob( QString const& jobid )
{
    QString msg = QString("Removing job : ").append(jobid).append(" ... ");
    this->statusBar()->showMessage(msg);

    cfg::Item* ci_job_list = this->getConfigItem("job_list");
    QStringList job_list = ci_job_list->value().toStringList();
    for( int i=0; i<job_list.size(); ++i ) {
        if( job_list[i].startsWith(jobid) )
        {
            Job job(job_list[i]);
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
                this->execute_remote_command_( cmd, job.remote_job_folder() );
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
    this->getConfigItem("wCheckCopyToDesktop")->set_value(checked);
}

void MainWindow::on_wCheckCopyToVscData_toggled(bool checked)
{
    LOG_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->getConfigItem("wCheckCopyToVscData")->set_value(checked);
}

void MainWindow::on_wCheckDeleteLocalJobFolder_toggled(bool checked)
{
    LOG_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->getConfigItem("wCheckDeleteLocalJobFolder")->set_value(checked);
}

void MainWindow::on_wCheckDeleteRemoteJobFolder_toggled(bool checked)
{
    LOG_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->getConfigItem("wCheckDeleteRemoteJobFolder")->set_value(checked);
}

SUBCLASS_EXCEPTION(BadLauncherHome,std::runtime_error)
SUBCLASS_EXCEPTION(BadDistribution,std::runtime_error)

QString copy_folder_recursively( QString const& source, QString const& destination, int level=1 )
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
 // create standard paths
//    LOG_CALLER <<"\n    creating standard paths ";
//    QDir launcher_home( this->launcher_.homePath() );
//    QStringList folders({"clusters","jobs"});
//    for ( QStringList::const_iterator iter=folders.cbegin()
//        ; iter!=folders.cend(); ++iter )
//    {
//        if( !launcher_home.mkpath(*iter) ) {
//            throw_<BadLauncherHome>("Cannot create directory '%1'.", launcher_home.absoluteFilePath(*iter) );
//        }
//        Log() << QString("\n        ").append( this->launcher_.homePath(*iter)).C_STR();
//    }

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
    for ( int i=0; i<folders.size(); ++i )
    {
        Log() << copy_folder_recursively( path_to_folders[i], this->launcher_.homePath( folders[i] ) ).C_STR();
    }
//        QString const& path = *iter;
//        Log() << QString("\n    Copying cluster info files from '%1'.").arg(path).C_STR();
//        dir = QDir(path, "*.info");
//    if( dir.entryInfoList().size()==0 ) {
//        QMessageBox::Button answer = QMessageBox::question(this,TITLE,"Your Launcher distribution has no clusters .info files.\nClick OK to select a clusters directory on your local file system, or Cancel to abort.", QMessageBox::Ok|QMessageBox::Cancel,QMessageBox::Cancel);
//        if( answer==QMessageBox::Ok) {
//            dir = QDir( this->get_path_to_clusters() );
//        } else {
//            throw_<NoClustersFound>("Aborting on user request.");
//        }
//    }
//    QFileInfoList clusters = dir.entryInfoList();
//    QString destination = this->launcher_.homePath("clusters");
//    for ( QFileInfoList::ConstIterator iter=clusters.cbegin()
//        ; iter!=clusters.cend(); ++iter )
//    {
//        Log() << QString("\n        ").append( iter->fileName() ).C_STR();
//        QString tmp = iter->absoluteFilePath();
//        QFile qFile( tmp );
//        tmp = QString(destination).append('/').append( iter->fileName() );
//        qFile.copy( tmp );
//    }
}

QString MainWindow::get_path_to_clusters()
{
 // look for standard clusters path
    QString path_to_clusters = Launcher::homePath("clusters");
    QDir dir( path_to_clusters, "*.info");
    if( dir.entryInfoList().size() > 0 ) return path_to_clusters;

 // look for non standard clusters path in config file:
    path_to_clusters = this->getConfigItem("path_to_clusters", QString() )->value().toString();
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

    if( arg1.startsWith('/') ) {

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
        this->statusBar()->showMessage( QString("Selected job ").append( this->selected_job_ ) );
    }
}

void MainWindow::on_wFinished_selectionChanged()
{
    LOG_AND_CHECK_IGNORESIGNAL(NO_ARGUMENT);

    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
    this->selected_job_ = this->selectedJob( this->ui->wFinished );
    this->statusBar()->showMessage( QString("Selected job ").append( this->selected_job_ ) );
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
