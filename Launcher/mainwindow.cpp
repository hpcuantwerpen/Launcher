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


#define TITLE "Launcher 0.9"

 //-----------------------------------------------------------------------------
 // MainWindow::
 //-----------------------------------------------------------------------------
    MainWindow::MainWindow(QWidget *parent)
      : QMainWindow(parent)
      , ui(new Ui::MainWindow)
      , ignoreSignals_(false)
      , previousPage_(0)
      , verbosity_(1)
    {
        ui->setupUi(this);

        if( !QFile(this->launcher_.homePath("Launcher.cfg")).exists() )
        {// This must be the first time after installation...
            this->setupHome();
        }
        this->setWindowTitle("Launcher 0.9");

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
        ci = this->getConfigItem("wJobname", QString() );

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
        this->data_.append( dc::newDataConnector( this->ui->wGbPerCore     , "wGbPerCore"     , "Gb_per_core") );
        this->data_.append( dc::newDataConnector( this->ui->wGbTotal       , "wGbTotal"       , "Gb_total"   ) );
        this->data_.append( dc::newDataConnector( this->ui->wWalltime      , "wWalltime"      , ""           ) );

        this->storeResetValues();

        this->data_.append( dc::newDataConnector( this->ui->wUsername, "wUsername", "user" ) );
        bool can_authenticate = !this->getConfigItem("wUsername")->value().toString().isEmpty();
        this->ui->wAuthenticate->setEnabled( can_authenticate );

        ci = this->getConfigItem("wRemote");
        if( !ci->isInitialized() ) {
            ci->set_choices( QStringList({"$VSC_SCRATCH","$VSC_DATA"}) );
            ci->set_value("$VSC_SCRATCH");
        }
        this->data_.append( dc::newDataConnector( this->ui->wRemote, "wRemote", "") );

        ci = this->getConfigItem("wLocal");
        if( !ci->isInitialized() ) {
            this->getConfigItem("wJobname"  )->set_value_and_type("");
            this->getConfigItem("wSubFolder")->set_value_and_type("");
            ci                               ->set_value_and_type("");
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
        }
        this->on_wAuthenticate_clicked();

        createActions();
        createMenus();
    }

    void MainWindow::createActions()
    {
        aboutAction_ = new QAction(tr("&About"), this);
        connect(aboutAction_, SIGNAL(triggered()), this, SLOT(about()));
    }

    void MainWindow::createMenus()
    {
        helpMenu_ = menuBar()->addMenu("&Help");
        helpMenu_->addAction(aboutAction_);
    }

    void MainWindow::about()
    {
        this->statusBar()->showMessage("about");
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
#define PRINT_HEADER \
    QString header("[ MainWindow::%1, %2, %3 ]");\
    header = header.arg(__func__).arg(__FILE__).arg(__LINE__);\
    qInfo(header.toUtf8().data());\
    if( this->ignoreSignals_ ) return;
 //-----------------------------------------------------------------------------
    MainWindow::~MainWindow()
    {
        PRINT_HEADER

        qInfo("~Launcher closing down");
        for( QList<dc::DataConnectorBase*>::iterator iter = this->data_.begin(); iter!=this->data_.end(); ++iter ) {
            (*iter)->value_widget_to_config();
            delete (*iter);
        }

        QSize windowSize = this->size();
        this->getConfigItem("windowWidth" )->set_value( windowSize.width () );
        this->getConfigItem("windowHeight")->set_value( windowSize.height() );

        qInfo("~saving config file.");
        this->launcher_.config.save();

        if ( this->launcher_.script.has_unsaved_changes() ) {
            QMessageBox::Button answer = QMessageBox::question(this,TITLE,"The current script has unsaved changes.\nDo you want to save?");
            if(answer==QMessageBox::Yes) {
                qInfo( QString("~Calling on_wSave_clicked()").toUtf8().data() );
                this->on_wSave_clicked();
            }
        }

        delete ui;
        ssh2::Session::cleanup();

        delete this->paletteRedForeground_;
        qInfo("~Launcher closed.");
    }
 //-----------------------------------------------------------------------------
    void MainWindow::setIgnoreSignals( bool ignore )
    {
        if( this->verbosity_ > 1 ) {
            if( ignore ) {
                if(!this->ignoreSignals_ ) qInfo() << "    ignoring signals ... ";
            } else {
                if( this->ignoreSignals_ ) qInfo() << "    ... ignoring signals has stopped";
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
        }
     // try to authenticate to refresh the list available modules
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
        ts << "Checking unsaved changes : ";
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
        return msg;
    }
 //-----------------------------------------------------------------------------

 //-----------------------------------------------------------------------------
 // SLOTS
 //-----------------------------------------------------------------------------

#define IGNORE_SIGNALS_UNTIL_END_OF_SCOPE IgnoreSignals ignoreSignals(this)

#define PRINT_AND_CHECK_IGNORESIGNAL( msg ) \
    QString qmsg("[ MainWindow::%1, %2, %3 ]");\
    qmsg = qmsg.arg(__func__).arg(__FILE__).arg(__LINE__);\
    if( !QString((msg)).isEmpty() ) qmsg.append("\n    Function was called with argument : ").append(msg);\
    qmsg.append("\n    ").append( this->check_script_unsaved_changes() ) ;\
    if( this->ignoreSignals_ ) qmsg.append("\n    Ignoring signal");\
    else                       qmsg.append("\n    Executing signal");\
    std::string s = qmsg.toStdString();\
    char const* c = s.c_str();\
    qInfo(c);\
    if( this->ignoreSignals_ ) return;


#define FORWARDING \
    qInfo() << "    forwarding";

void MainWindow::on_wCluster_currentIndexChanged( QString const& arg1 )
{
    PRINT_AND_CHECK_IGNORESIGNAL( arg1 );

    this->launcher_.config["wCluster"].set_value(arg1);
    this->clusterDependencies(true);
}

void MainWindow::on_wNodeset_currentTextChanged(const QString &arg1)
{
    PRINT_AND_CHECK_IGNORESIGNAL( arg1 )

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
    PRINT_AND_CHECK_IGNORESIGNAL("")

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
}

void MainWindow::on_wAutomaticRequests_toggled(bool checked)
{
    PRINT_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )

    bool manual = !checked;
    this->ui->wRequestNodes->setEnabled(manual);
    this->ui->wRequestCores->setEnabled(manual);
}

void MainWindow::on_wNNodes_valueChanged(int arg1)
{
    PRINT_AND_CHECK_IGNORESIGNAL( QString().setNum(arg1) )

    if( this->ui->wAutomaticRequests->isChecked() ) {
        FORWARDING
        on_wRequestNodes_clicked();
    }
}

void MainWindow::on_wNCoresPerNode_valueChanged(int arg1)
{
    PRINT_AND_CHECK_IGNORESIGNAL( QString().setNum(arg1) )

    if( this->ui->wAutomaticRequests->isChecked() ) {
        FORWARDING
        on_wRequestNodes_clicked();
    }
}

void MainWindow::on_wNCores_valueChanged(int arg1)
{
    PRINT_AND_CHECK_IGNORESIGNAL( QString().setNum(arg1) )

    if( this->ui->wAutomaticRequests->isChecked() ) {
        FORWARDING
        on_wRequestCores_clicked();
    }
}

void MainWindow::on_wGbPerCore_valueChanged(double arg1)
{
    PRINT_AND_CHECK_IGNORESIGNAL( QString().setNum(arg1) )

    if( this->ui->wAutomaticRequests->isChecked() ) {
        FORWARDING
        on_wRequestCores_clicked();
    }
}

void MainWindow::on_wRequestCores_clicked()
{
    PRINT_AND_CHECK_IGNORESIGNAL("")

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
}

void MainWindow::on_wPages_currentChanged(int index)
{
    PRINT_AND_CHECK_IGNORESIGNAL( QString().setNum(index) )

    qInfo() << "    previous page: " << this->previousPage_;
    switch (index) {
    case 0:
        if( this->previousPage_==1 )
        {// read the script from wScript
            QString text = this->ui->wScript->toPlainText();
            if( this->launcher_.script.has_unsaved_changes() ) {
                this->launcher_.script.parse( text, false );
            }
//#ifdef QT_DEBUG
//            qInfo()<<text.toStdString()<<std::endl;
//            this->launcher_.script.print(qInfo(),1);
//#endif
        }
        break;
    case 1:        
        if( this->previousPage_==0)
        {
            for( QList<dc::DataConnectorBase*>::iterator iter=this->data_.begin(); iter!=this->data_.end(); ++iter )
            {
                if( (*iter)->value_widget_to_config() ) {
                    (*iter)->value_config_to_script();
                }
            }
            QString text = this->launcher_.script.text();
            IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;
            this->ui->wScript->setText(text);
//#ifdef QT_DEBUG
//            qInfo()<<text.toStdString()<<std::endl;
//            this->launcher_.script.print(qInfo(),1);
//#endif
        }
        break;
    case 2:
        {
            if( !Job::sshSession ) Job::sshSession = &this->sshSession_;

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
    this->previousPage_ = index;
}

void MainWindow::on_wEnforceNNodes_toggled(bool checked)
{
    PRINT_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )

    bool hide = !checked;
    this->launcher_.script.find_key("-W")->setHidden(hide);
}

void MainWindow::on_wWalltimeUnit_currentTextChanged(const QString &arg1)
{
    PRINT_AND_CHECK_IGNORESIGNAL( arg1 );

    this->getDataConnector("wWalltimeUnit")->value_widget_to_config();
    this->walltimeUnitDependencies(true);
}

void MainWindow::on_wWalltime_valueChanged(double arg1)
{
    PRINT_AND_CHECK_IGNORESIGNAL( QString().setNum(arg1) );

    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;

    cfg::Item* walltimeSeconds = this->getConfigItem("walltimeSeconds");
    double seconds = arg1*this->walltimeUnitSeconds_;
    walltimeSeconds->set_value( formatWalltimeSeconds( seconds ) );
    this->getDataConnector("walltimeSeconds")->value_config_to_script();
    qInfo() <<"    walltime set to: " << seconds << "s.";
}

void MainWindow::on_wNotifyAddress_editingFinished()
{
    PRINT_AND_CHECK_IGNORESIGNAL("");

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
    qInfo() << "    abe set to: '" << abe << "'.";

    this->getDataConnector("notifyWhen")->value_config_to_script();
}

void MainWindow::on_wNotifyAbort_toggled(bool checked)
{
    PRINT_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->update_abe('a', checked );
}

void MainWindow::on_wNotifyBegin_toggled(bool checked)
{
    PRINT_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->update_abe('b', checked );
}

void MainWindow::on_wNotifyEnd_toggled(bool checked)
{
    PRINT_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->update_abe('e', checked );
}

bool MainWindow::getPrivatePublicKeyPair()
{
    QString private_key = QFileDialog::getOpenFileName( this, "Select PRIVATE key file:", this->launcher_.homePath() );
    if( !QFileInfo( private_key ).exists() ) {
        this->statusBar()->showMessage( QString( "Private key '%1' not found.").arg(private_key) );
        return false;
    }
 // private key is provided, now public key
    QString public_key = private_key;
    public_key.append(".pub"); // make an educated guess...
    if( !QFileInfo( public_key ).exists() )
    {
        public_key = QFileDialog::getOpenFileName( this, "Select PUBLIC key file:", this->launcher_.homePath() );
        if( public_key.isEmpty() || !QFileInfo(public_key).exists() )
        {// not provided or inexisting
            this->statusBar()->showMessage(QString("Public key '%1' not found or not provided.").arg(public_key) );
            return false;
        }
    }
    this->statusBar()->showMessage("Private/public key pair found.");
    this->sshSession_.setPrivatePublicKeyPair( private_key, public_key );
    return true;
}

void MainWindow::on_wUsername_editingFinished()
{
    PRINT_AND_CHECK_IGNORESIGNAL("");

    QString username = validateUsername( this->ui->wUsername->text() );

    if( username.isEmpty() )
    {// not a valid username
        this->ui->wUsername->setPalette(*this->paletteRedForeground_);
        this->statusBar()->showMessage("Invalid username");
     // reset the ssh session
        this->sshSession_.reset();
     // deactivate the authenticate button
        this->ui->wAuthenticate->setText("authenticate...");
        this->ui->wAuthenticate->setEnabled(false);
    } else
    {// valid username
        this->ui->wUsername->setPalette( QApplication::palette(this->ui->wUsername) );
        cfg::Item* user = this->getConfigItem("wUsername");
        if( user->value().toString() != username )
        {// this is a new username, different from current config value
         // store in config
            user->set_value(username);
         // clear config values for private/public key pair
            this->getConfigItem("privateKey")->set_value( QString() );
            this->getConfigItem("publicKey" )->set_value( QString() );
         // set the username for the ssh session
            this->sshSession_.setUsername(username);
         // activate the authenticate button
            this->ui->wAuthenticate->setText("authenticate...");
            this->ui->wAuthenticate->setEnabled(true);
        } else
        {// username is the same as in the config
            this->sshSession_.setUsername(username);
         // set private/public key pair from config
            this->sshSession_.setPrivatePublicKeyPair
                    ( this->getConfigItem("privateKey")->value().toString()
                    , this->getConfigItem("publicKey" )->value().toString()
                    , false
                    );
         // activate the authenticate button
            this->ui->wAuthenticate->setText("authenticate...");
            this->ui->wAuthenticate->setEnabled(true);
        }
        this->statusBar()->showMessage("Ready to authenticate");
    }
}

bool MainWindow::isUserAuthenticated() const
{
    return this->ui->wAuthenticate->text() == "authenticated";
}

void MainWindow::on_wAuthenticate_clicked()
{
    PRINT_AND_CHECK_IGNORESIGNAL("");

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
             // deactivate the authenticate button
                this->ui->wAuthenticate->setText("authenticate...");
                this->ui->wAuthenticate->setEnabled(false);
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
            QString passphrase = QInputDialog::getText( 0, "Passphrase needed", msg );
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
            qInfo() << msg;
         // remove keys from ssh session and from config
            this->sshSession_.setUsername( this->getConfigItem("wUsername")->value().toString() );
            this->getConfigItem("privateKey")->set_value( QString() );
            this->getConfigItem("publicKey" )->set_value( QString() );
            break;
        }
     // success
        this->ui->wAuthenticate->setText("authenticated");
        this->ui->wAuthenticate->setEnabled(false);
        this->statusBar()->showMessage("Authentication succeeded.");
        this->getConfigItem("privateKey")->set_value( this->sshSession_.privateKey() );
        this->getConfigItem("publicKey" )->set_value( this->sshSession_. publicKey() );

        this->resolveRemoteFileLocations_();

        QString qs = this->ui->wRemote->currentText();
        if( this->remote_env_vars_.contains(qs) ) {
            this->ui->wRemote->setToolTip( this->remote_env_vars_[qs] );
        }
        this->sshSession_.execute( QString("module avail -t") );
        QStringList modules = this->sshSession_.qerr().split("\n");
        this->ui->wSelectModule->clear();
        this->ui->wSelectModule->addItems(modules);

        QString modules_cluster = QString("modules_").append(this->getConfigItem("wCluster")->value().toString() );
        this->getConfigItem(modules_cluster)->set_choices(modules);

#ifdef QT_DEBUG
        qDebug() << this->sshSession_.toString();
#endif
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

void MainWindow::resolveRemoteFileLocations_()
{
   for( int i=0; i<this->ui->wRemote->count(); ++i ) {
       QString qs = this->ui->wRemote->itemText(i);
       QString cmd = QString("echo ").append( qs );
       try {
           this->sshSession_.execute(cmd);
           this->remote_env_vars_[qs] = this->sshSession_.qout().trimmed();
       } catch(std::runtime_error &e ) {

       }
   }
}

void MainWindow::on_wLocalFileLocationButton_clicked()
{
    PRINT_AND_CHECK_IGNORESIGNAL("");

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
    PRINT_AND_CHECK_IGNORESIGNAL("");

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
    if( !folder.startsWith(parent) ) {
        QMessageBox::critical( this, TITLE, QString("The selected directory is not inside '%1.").arg(parent) );
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
    PRINT_AND_CHECK_IGNORESIGNAL("");

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
    if( !selected.startsWith(parent) ) {
        QMessageBox::critical( this, TITLE, QString("The selected directory is not inside '%1.").arg(parent) );
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
                                 , QString("The local job directory '%1' already contains a job script 'pbs.sh'.\nDo you want to load it?")
                                      .arg( job_folder )
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
    this->launcher_.script.read(filepath);
 // the order is important!
    QStringList first({"wCluster","wNodeset","wNNodes", "wNCoresPerNode"}) ;
    try
    {
        for(QStringList::const_iterator iter = first.cbegin(); iter!=first.cend(); ++iter ) {
            dc::DataConnectorBase* dataConnector = this->getDataConnector(*iter);
            if( dataConnector->value_script_to_config() ) {
                dataConnector->value_config_to_widget();
            }
        }
     // do the same for all other DataConnectors, for which the order does not matter.
     // the items in first are visited twice, but the second time has no effect.
        for( QList<dc::DataConnectorBase*>::Iterator iter = this->data_.begin(); iter!=this->data_.end(); ++iter ) {
            if( (*iter)->value_script_to_config() ) {
                (*iter)->value_config_to_widget();
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
        qInfo() << QString("ERROR: in 'void MainWindow:: loadJobscript( QString const& filepath /*=%1*/)'\n")
                        .arg(filepath)

                  << e.what()
                 ;
        return false;
    }
 // read from file, hence no unsaved changes sofar
//    this->launcher_.script.set_has_unsaved_changes(false);
    return true;
}

bool MainWindow::saveJobscript( QString const& filepath )
{
    qInfo() << "    writing script '" << filepath << "' ... ";
    try {
        this->launcher_.script.write( filepath );
        qInfo() << "done.";
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
            qInfo() << "done (overwritten, but made backup '"<<backup<<"').";
        } else {
            qInfo() << "aborted.";
            this->statusBar()->showMessage( QString("Job script NOT SAVED: '%1'").arg(filepath) );
            return false;
        }
    }
    this->statusBar()->showMessage( QString("Job script saved: '%1'").arg(filepath) );
 // just saved, hence no unsaved changes yet.
 //   this->launcher_.script.set_has_unsaved_changes(false);
    this->check_script_unsaved_changes();
    return true;
}

void MainWindow::on_wSubfolder_textChanged(const QString &arg1)
{
    PRINT_AND_CHECK_IGNORESIGNAL( arg1 );

    this->ui->wSubfolder2->setText(arg1);

    this->getConfigItem   ("wJobname" )->set_value("");
    this->getDataConnector("wJobname" )->value_config_to_widget();
}

void MainWindow::on_wJobname_textChanged(const QString &arg1)
{
    PRINT_AND_CHECK_IGNORESIGNAL( arg1 );

    this->ui->wJobname2->setText(arg1);

    this->getConfigItem("wJobname" )->set_value( arg1 );
}


void MainWindow::on_wRemote_currentIndexChanged(const QString &arg1)
{
    PRINT_AND_CHECK_IGNORESIGNAL( arg1 );

    this->getConfigItem("wRemote")->set_value(arg1);
    if( this->isUserAuthenticated() ) {
        QString qout;
        QString cmd = QString("echo ").append(arg1);
        this->sshSession_.execute( cmd );
        qInfo() << "\n" << cmd
                  << "\n''" << this->sshSession_.qout() <<"''";
    }

    if( !this->remote_env_vars_.contains(arg1) && this->isUserAuthenticated() ) {
        this->resolveRemoteFileLocations_();
    }
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
    PRINT_AND_CHECK_IGNORESIGNAL("");

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
    PRINT_AND_CHECK_IGNORESIGNAL("");

    QString script_file = QString( this->local_subfolder_jobname() ).append("/pbs.sh");
    QDir qDir(script_file);
    if( !qDir.exists() ) {
        this->statusBar()->showMessage("No job script found, hence not loaded.");
    } else {
        QMessageBox::StandardButton answer =
                QMessageBox::question( this, TITLE, "Unsaved changes to the current job script will be lost on reloading. Continue?");
        if( answer==QMessageBox::Yes ) {
            this->loadJobscript(script_file);
            this->statusBar()->showMessage("job script reloaded.");
        }
    }
}

void MainWindow::on_wSubmit_clicked()
{
    PRINT_AND_CHECK_IGNORESIGNAL("");

    QString filepath = QString( this->local_subfolder_jobname() ).append("/pbs.sh");
    if( this->launcher_.script.has_unsaved_changes() ) {
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
        QMessageBox::Button answer = QMessageBox::question(this,TITLE,"OK to erase remote job folder?");
        if( answer==QMessageBox::Yes ) {
                cmd = QString("rm -rf ").append( this->remote_subfolder_jobname() );
            this->sshSession_.execute(cmd);
            this->statusBar()->showMessage("Erased remote job folder.");
        } else {
            this->statusBar()->showMessage("Remote job folder not erased.");
        }
    }
    this->sshSession_.sftp_put_dir( this->local_subfolder_jobname(), this->remote_subfolder_jobname(), true );

    cmd = QString("cd ")
          .append( this->remote_subfolder_jobname() )
          .append(" && qsub pbs.sh");
    this->sshSession_.execute(cmd);
    QString job_id = this->sshSession_.qout().trimmed();
    this->statusBar()->showMessage(QString("Job submitted: %1").arg(job_id));
    cfg::Item* ci_job_list = this->getConfigItem("job_list");
    if( ci_job_list->value().type()==QVariant::Invalid )
        ci_job_list->set_value_and_type( QStringList() );
    QStringList job_list = ci_job_list->value().toStringList();
    Job job( job_id, this->ui->wLocal->text(), this->ui->wRemote->currentText(), this->ui->wSubfolder->text(), this->ui->wJobname->text(), Job::Submitted );
    job_list.append( job.toString() );
    ci_job_list->set_value( job_list );
}

void MainWindow::on_wScript_textChanged()
{
    PRINT_AND_CHECK_IGNORESIGNAL("");

    this->launcher_.script.set_has_unsaved_changes(true);
    this->check_script_unsaved_changes();
}

void MainWindow::on_wRefresh_clicked()
{
    PRINT_AND_CHECK_IGNORESIGNAL("");

    cfg::Item* ci_joblist = this->getConfigItem("job_list");
    JobList joblist( ci_joblist->value().toStringList() );
    this->refreshJobs(joblist);
}

void MainWindow::on_wRetrieveSelectedJob_clicked()
{
    PRINT_AND_CHECK_IGNORESIGNAL("");

    QString selection ;
    while( true )
    {
        QTextCursor cursor = this->ui->wFinished->textCursor();
        cursor.select(QTextCursor::LineUnderCursor);
        selection = cursor.selectedText();
        if( !selection.startsWith(">>> ") ) {
            int bn = cursor.blockNumber(); // = line number, starts at line 0, which is empty
            if( bn==0 ) {
                this->statusBar()->showMessage("No job Selected");
                return;
            }
            cursor.movePosition( QTextCursor::Up);
            this->ui->wFinished->setTextCursor(cursor);
        } else {
            this->ui->wFinished->setTextCursor(cursor);
            break;
        }
    }
    selection = selection.mid(4);
    //this->statusBar()->showMessage(selection);

    cfg::Item* ci_joblist = this->getConfigItem("job_list");
    JobList joblist( ci_joblist->value().toStringList() );
    Job const* job = joblist[selection];
    if( !job ) {
        this->statusBar()->showMessage( QString("job not found: %1").arg(selection) );
        return;
    }
    if( job->status()==Job::Retrieved ) {
        this->statusBar()->showMessage( QString("Job allready retrieved: %1").arg(selection) );
        return;
    }
    if( job->status()==Job::Submitted ) {
        this->statusBar()->showMessage( QString("Job not finished: %1").arg(selection) );
        return;
    }

    this->statusBar()->showMessage( QString("Retrieving job : %1").arg(selection) );

    bool toDesktop = this->ui->wCheckCopyToDesktop->isChecked();
    bool toVscData = this->ui->wCheckCopyToVscData->isChecked();
    if( job->retrieve( toDesktop, toVscData ) ) {
        ci_joblist->set_value( joblist.toStringList( Job::Submitted | Job::Finished ) );
        this->refreshJobs(joblist);
    }
}

void MainWindow::on_wRetrieveAllJobs_clicked()
{
    PRINT_AND_CHECK_IGNORESIGNAL("");

    cfg::Item* ci_joblist = this->getConfigItem("job_list");
    JobList joblist( ci_joblist->value().toStringList() );

    bool toDesktop = this->ui->wCheckCopyToDesktop->isChecked();
    bool toVscData = this->ui->wCheckCopyToVscData->isChecked();

    joblist.retrieveAll( toDesktop, toVscData );

    QStringList list = joblist.toStringList( Job::Submitted | Job::Finished );
    ci_joblist->set_value( list );
    this->refreshJobs(joblist);
}

void MainWindow::refreshJobs( JobList const& joblist )
{
    joblist.update(); // first check for finished jobs

    this->ui->wFinished->setText( joblist.toString(Job::Finished) );

    QString text = joblist.toString(Job::Submitted);
    QString username = this->ui->wUsername->text();
    if( !username.isEmpty() ) {
        QString cmd = QString("qstat -u ").append(username);
        this->sshSession_.execute( cmd );
        if( !this->sshSession_.qout().isEmpty() ) {
            text.append( cmd ).append('\n');
            text.append( this->sshSession_.qout() );
        }
    }
    this->ui->wNotFinished->setText( text);

    this->ui->wRetrieved->setText( joblist.toString(Job::Retrieved) );

    this->getConfigItem("job_list")->set_value( joblist.toStringList(Job::Submitted|Job::Finished) );
}

void MainWindow::on_wDeleteSelectedJob_clicked()
{
    PRINT_AND_CHECK_IGNORESIGNAL("");
    if( this->ui->wCheckDeleteLocalJobFolder->isChecked() ) {
        QDir qDir( this->local_subfolder_jobname() );
        qDir.removeRecursively();
        this->statusBar()->showMessage(QString("Removed: ").append( this->local_subfolder_jobname() ) );
    }
    if( this->ui->wCheckDeleteRemoteJobFolder->isChecked() ) {
        QString cmd = QString("rm -rf ").append( this->remote_subfolder_jobname() );
        this->sshSession_.execute(cmd);
        this->statusBar()->showMessage(QString("Removed: ").append( this->remote_subfolder_jobname() ) );
    }
}

void MainWindow::on_wCheckCopyToDesktop_toggled(bool checked)
{
    PRINT_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->getConfigItem("wCheckCopyToDesktop")->set_value(checked);
}

void MainWindow::on_wCheckCopyToVscData_toggled(bool checked)
{
    PRINT_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->getConfigItem("wCheckCopyToVscData")->set_value(checked);
}

void MainWindow::on_wCheckDeleteLocalJobFolder_toggled(bool checked)
{
    PRINT_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->getConfigItem("wCheckDeleteLocalJobFolder")->set_value(checked);
}

void MainWindow::on_wCheckDeleteRemoteJobFolder_toggled(bool checked)
{
    PRINT_AND_CHECK_IGNORESIGNAL( (checked?"true":"false") )
    this->getConfigItem("wCheckDeleteRemoteJobFolder")->set_value(checked);
}

SUBCLASS_EXCEPTION(BadLauncherHome,std::runtime_error)
SUBCLASS_EXCEPTION(BadDistribution,std::runtime_error)

void MainWindow::setupHome()
{
    PRINT_HEADER

 // create standard paths
    qInfo("~creating standard paths ");
    QDir launcher_home( this->launcher_.homePath() );
    if( !launcher_home.mkpath("clusters") ) {
        throw_<BadLauncherHome>("Cannot create directory '%1'.", launcher_home.absoluteFilePath("clusters") );
    }
    qInfo( QString("~    ").append( this->launcher_.homePath("clusters")).toUtf8().data() );
    if( !launcher_home.mkpath("jobs") ) {
        throw_<BadLauncherHome>("Cannot create directory '%1'.", launcher_home.absoluteFilePath("jobs") );
    }
    qInfo( QString("~    ").append( this->launcher_.homePath("jobs")).toUtf8().data() );

 // get the location of the application and derive the location of the clusters info files.
 // see http://stackoverflow.com/questions/4515602/how-to-get-executable-name-in-qt
    QString applicationFilePath = QCoreApplication::applicationFilePath();
    QDir dir = QFileInfo( applicationFilePath ).absoluteDir();
    QString path_to_clusters = dir.absoluteFilePath("../Resources/clusters");

    qInfo( QString("~copying cluster info files from '%1'.").arg(path_to_clusters).toUtf8().data() );
    dir = QDir(path_to_clusters, "*.info");
    if( dir.entryInfoList().size()==0 ) {
        QMessageBox::Button answer = QMessageBox::question(this,TITLE,"Your Launcher distribution has no clusters .info files.\nClick OK to select a clusters directory on your local file system, or Cancel to abort.", QMessageBox::Ok|QMessageBox::Cancel,QMessageBox::Cancel);
        if( answer==QMessageBox::Ok) {
            dir = QDir( this->get_path_to_clusters() );
        } else {
            throw_<NoClustersFound>("Aborting on user request.");
        }
    }
    QFileInfoList clusters = dir.entryInfoList();
    QString destination = this->launcher_.homePath("clusters");
    for ( QFileInfoList::ConstIterator iter=clusters.cbegin()
        ; iter!=clusters.cend(); ++iter )
    {
        qInfo( QString("~    ").append( iter->fileName() ).toUtf8().data() );
        QString tmp = iter->absoluteFilePath();
        QFile qFile( tmp );
        tmp = QString(destination).append('/').append( iter->fileName() );
        qFile.copy( tmp );
    }
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
