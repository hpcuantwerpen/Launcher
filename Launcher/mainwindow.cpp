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

#include <time.h>       /* time */


 //-----------------------------------------------------------------------------
 // MainWindow::
 //-----------------------------------------------------------------------------
    MainWindow::MainWindow(QWidget *parent)
      : QMainWindow(parent)
      , ui(new Ui::MainWindow)
      , ignoreSignals_(false)
      , previousPage_(0)
    {
        ui->setupUi(this);

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
            units.append("seconds");
            units.append("minutes");
            units.append("hours");
            units.append("days");
            units.append("weeks");
            ci->set_choices(units);
            ci->set_value("hours");
        }

        ci = this->getConfigItem("walltimeSeconds");
        if( !ci->isInitialized() ) {
            ci->set_value("1:00:00");
        }
        ci = this->getConfigItem("wNotifyAddress");
        if( !ci->isInitialized() ) {
            ci->set_value("");
        }
        ci = this->getConfigItem("notifyWhen");
        if( !ci->isInitialized() ) {
            ci->set_value("");
            this->getConfigItem("wNotifyAbort")->set_value(false);
            this->getConfigItem("wNotifyBegin")->set_value(false);
            this->getConfigItem("wNotifyEnd"  )->set_value(false);
        }
        ci = this->getConfigItem("wJobname");
        if( !ci->isInitialized() ) {
            ci->set_value("");
        }

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
        this->launcher_.readClusterInfo(); // this throws if there are no clusters/*.info files
        ci = this->getConfigItem("wCluster");
        try {
            ci->set_choices( this->launcher_.clusters.keys() );
        } catch( cfg::InvalidConfigItemValue & e) {
            ci->set_value( ci->default_value() );
        }
        if( !ci->isInitialized() )
            ci->set_value( ci->default_value() );
        this->clusterDependencies_(false);

        this->data_.append( dc::newDataConnector( this->ui->wCluster       , "wCluster"       , "cluster"    ) );
        this->data_.append( dc::newDataConnector( this->ui->wNodeset       , "wNodeset"       , "nodeset"    ) );
        this->data_.append( dc::newDataConnector( this->ui->wNNodes        , "wNNodes"        , "nodes"      ) );
        this->data_.append( dc::newDataConnector( this->ui->wNCoresPerNode , "wNCoresPerNode" , "ppn"        ) );
        this->data_.append( dc::newDataConnector( this->ui->wNCores        , "wNCores"        , "n_cores"    ) );
        this->data_.append( dc::newDataConnector( this->ui->wGbPerCore     , "wGbPerCore"     , "Gb_per_core") );
        this->data_.append( dc::newDataConnector( this->ui->wGbTotal       , "wGbTotal"       , "Gb_total"   ) );
        this->data_.append( dc::newDataConnector( this->ui->wWalltime      , "wWalltime"      , ""           ) );

//        this->getDataConnector("wCluster")->config_to_widget();
        this->storeResetValues_();

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

        this->setIgnoreSignals(false);

        this->ui->wAutomaticRequests->setChecked( true );

        this->paletteRedForeground_ = new QPalette();
        this->paletteRedForeground_->setColor(QPalette::Text,Qt::red);

        if( !this->getConfigItem("wJobname")->value().toString().isEmpty() ) {
            this->lookForJobscript( local_subfolder_jobname );
        }
        this->on_wAuthenticate_clicked();
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
    void MainWindow::storeResetValues_()
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
        for( QList<dc::DataConnectorBase*>::iterator iter = this->data_.begin(); iter!=this->data_.end(); ++iter ) {
            (*iter)->value_widget_to_config();
            delete (*iter);
        }

        QSize windowSize = this->size();
        this->getConfigItem("windowWidth" )->set_value( windowSize.width () );
        this->getConfigItem("windowHeight")->set_value( windowSize.height() );

        this->launcher_.config.save();

        delete ui;
        ssh2::Session::cleanup();

        delete this->paletteRedForeground_;
    }
 //-----------------------------------------------------------------------------
    void MainWindow::setIgnoreSignals( bool ignore )
    {
        if( ignore ) {
            if( !this->ignoreSignals_ )
                std::cout << "\n<<< ignoring signals >>>" << std::endl;
        } else {
            if( this->ignoreSignals_ )
                std::cout << "\n<<< stopping ignoring signals >>>" << std::endl;
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
 //-----------------------------------------------------------------------------
    void MainWindow::print_( std::string const& signature ) const
    {
        if( this->ignoreSignals_ ) {
            std::cout << "\nIgnoring signal:\n";
        } else {
            std::cout << "\nExecuting slot:\n";
        }
        std::cout << signature << std::endl;
    }
 //------------------------------------------------------------------------------
    void MainWindow::clusterDependencies_( bool updateWidgets )
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

            this->nodesetDependencies_     (updateWidgets);
            this->walltimeUnitDependencies_(updateWidgets);

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
    void MainWindow::nodesetDependencies_( bool updateWidgets )
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
    void MainWindow::walltimeUnitDependencies_( bool updateWidgets )
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
        double current = ci_wWalltime->value().toDouble();
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
    void MainWindow::check_script_unsaved_changes()
    {
        std::cout << "\nChecking unsaved changes : ";
        if( this->launcher_.script.has_unsaved_changes() ) {
            std::cout << "true." << std::endl;
            this->ui->wSave->setText("*** Save ***");
            this->ui->wSave->setToolTip("Save the current script locally (there are unsaved changes).");
        } else {
            std::cout << "false." << std::endl;
            this->ui->wSave->setText("Save");
            this->ui->wSave->setToolTip("Save the current script locally.");
        }
    }
 //-----------------------------------------------------------------------------

 //-----------------------------------------------------------------------------
 // SLOTS
 //-----------------------------------------------------------------------------

#define IGNORE_SIGNALS_UNTIL_END_OF_SCOPE; IgnoreSignals ignoreSignals(this)

void MainWindow::on_wCluster_currentIndexChanged( QString const& arg1 )
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wCluster_currentIndexChanged( QString const& arg1 )", arg1.toStdString() )

    this->launcher_.config["wCluster"].set_value(arg1);
    this->clusterDependencies_(true);
}

void MainWindow::on_wNodeset_currentTextChanged(const QString &arg1)
{
    PRINT1_AND_CHECK_IGNORESIGNAL( "void MainWindow::on_wNodeset_currentTextChanged(const QString &arg1)", arg1.toStdString() )

    this->getConfigItem("wNodeset")->set_value(arg1);
    this->nodesetDependencies_(true);
}

void MainWindow::updateResourceItems_()
{
    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;;

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
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wRequestNodes_clicked()")

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
    this->updateResourceItems_();
}

void MainWindow::on_wAutomaticRequests_toggled(bool checked)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void on_wAutomaticRequests_toggled(bool checked)", (checked?"true":"false") )

    bool manual = !checked;
    this->ui->wRequestNodes->setEnabled(manual);
    this->ui->wRequestCores->setEnabled(manual);
}

void MainWindow::on_wNNodes_valueChanged(int arg1)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wNNodes_valueChanged(int arg1)", arg1 )

    if( this->ui->wAutomaticRequests->isChecked() ) {
        FORWARDING
        on_wRequestNodes_clicked();
    }
}

void MainWindow::on_wNCoresPerNode_valueChanged(int arg1)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wNCoresPerNode_valueChanged(int arg1)", arg1 )

    if( this->ui->wAutomaticRequests->isChecked() ) {
        FORWARDING
        on_wRequestNodes_clicked();
    }
}

void MainWindow::on_wNCores_valueChanged(int arg1)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wNCores_valueChanged(int arg1)", arg1 )

    if( this->ui->wAutomaticRequests->isChecked() ) {
        FORWARDING
        on_wRequestCores_clicked();
    }
}

void MainWindow::on_wGbPerCore_valueChanged(double arg1)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wGbPerCore_valueChanged(double arg1)", arg1 )

    if( this->ui->wAutomaticRequests->isChecked() ) {
        FORWARDING
        on_wRequestCores_clicked();
    }
}

void MainWindow::on_wRequestCores_clicked()
{
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wRequestCores_clicked()")

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
    this->updateResourceItems_();
}

void MainWindow::on_wPages_currentChanged(int index)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wPages_currentChanged(int index)", index )

    std::cout << "    previous page: " << this->previousPage_ << std::endl;
    switch (index) {
    case 0:
        if( this->previousPage_==1 )
        {// read the script from wScript
            QString text = this->ui->wScript->toPlainText();
#ifdef QT_DEBUG
            std::cout<<text.toStdString()<<std::endl;
#endif
            this->launcher_.script.parse( text, false );
#ifdef QT_DEBUG
            this->launcher_.script.print(std::cout,1);
#endif
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
#ifdef QT_DEBUG
            std::cout<<text.toStdString()<<std::endl;
#endif
            this->ui->wScript->setText(text);
#ifdef QT_DEBUG
            this->launcher_.script.print(std::cout,1);
#endif
        }
        break;
    default:
        break;
    }
    this->previousPage_ = index;
}

void MainWindow::on_wEnforceNNodes_toggled(bool checked)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wEnforceNNodes_toggled(bool checked)", checked );

    bool hide = !checked;
    this->launcher_.script.find_key("-W")->setHidden(hide);
}

void MainWindow::on_wWalltimeUnit_currentTextChanged(const QString &arg1)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wWalltimeUnit_currentTextChanged(const QString &arg1)", arg1.toStdString() );

    this->getDataConnector("wWalltimeUnit")->value_widget_to_config();
    this->walltimeUnitDependencies_(true);
}

void MainWindow::on_wWalltime_valueChanged(double arg1)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wWalltime_valueChanged(double arg1)", arg1 );

    IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;;

    cfg::Item* walltimeSeconds = this->getConfigItem("walltimeSeconds");
    double seconds = arg1*this->walltimeUnitSeconds_;
    walltimeSeconds->set_value( formatWalltimeSeconds( seconds ) );
    this->getDataConnector("walltimeSeconds")->value_config_to_script();
    std::cout <<"    walltime set to: " << seconds << "s." << std::endl;
}

void MainWindow::on_wNotifyAddress_editingFinished()
{
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wNotifyAddress_editingFinished()" );


    QString validated = validateEmailAddress( this->ui->wNotifyAddress->text() );
    if( validated.isEmpty() )
    {// not a valid address
        IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;;
        this->ui->wNotifyAddress->setPalette(*this->paletteRedForeground_);
        this->statusBar()->showMessage("Invalid email address.");
    } else {
        this->ui->wNotifyAddress->setPalette( QApplication::palette(this->ui->wNotifyAddress) );
        this->statusBar()->clearMessage();
        this->getConfigItem("wNotifyAddress")->set_value(validated);

        IGNORE_SIGNALS_UNTIL_END_OF_SCOPE;;
        dc::DataConnectorBase* link = this->getDataConnector("wNotifyAddress");
        if( link->value_config_to_widget() ) {
            link->value_config_to_script();
        }
    }
}

void MainWindow::update_abe_( QChar c, bool checked )
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
    std::cout << "    abe set to: '" << abe.toStdString() << "'." << std::endl;

    this->getDataConnector("notifyWhen")->value_config_to_script();
}

void MainWindow::on_wNotifyAbort_toggled(bool checked)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wNotifyAbort_toggled(bool checked)", checked );
    this->update_abe_('a', checked );
}

void MainWindow::on_wNotifyBegin_toggled(bool checked)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wNotifyBegin_toggled(bool checked)", checked );

    this->update_abe_('b', checked );
}

void MainWindow::on_wNotifyEnd_toggled(bool checked)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wNotifyend_toggled(bool checked)", checked );

    this->update_abe_('e', checked );
}

bool MainWindow::getPrivatePublicKeyPair_()
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
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wUsername_editingFinished()" );

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

bool MainWindow::isUserAuthenticated_() const
{
    return this->ui->wAuthenticate->text() == "authenticated";
}

void MainWindow::on_wAuthenticate_clicked()
{
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wAuthenticate_clicked()" );

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
            if( !this->getPrivatePublicKeyPair_() )
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
            std::cerr << msg.toStdString() << std::endl;
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
        this->sshSession_.print();
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
    if( !this->isUserAuthenticated_() ) {
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
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wLocalFileLocationButton_clicked()" );

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
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wSubfolderButton_clicked()" );

    if( this->ui->wLocal->text().isEmpty() ) {
        QMessageBox::warning( this, "Warning", "You must select a local file location first.");
        return;
    }

    QString parent = this->ui->wLocal->text();
    QString folder = QFileDialog::getExistingDirectory( this, "Select subfolder:", parent );
    if( folder.isEmpty() ) {
        QMessageBox::warning( this, "Warning", "No directory selected.");
        return;
    }
    if( !folder.startsWith(parent) ) {
        QMessageBox::critical( this, "Error", QString("The selected directory is not inside '%1.").arg(parent) );
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
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wJobnameButton_clicked()" );

    if( this->ui->wLocal->text().isEmpty() ) {
        QMessageBox::warning( this, "Warning", "You must select a local file location first.");
        return;
    }
    if( this->ui->wSubfolder->text().isEmpty() ) {
        QMessageBox::warning( this, "Warning", "You must select a subfolder first.");
        return;
    }

    QString parent = this->local_subfolder();
    QString selected = QFileDialog::getExistingDirectory( this, "Select subfolder:", parent );
    if( selected.isEmpty() ) {
        QMessageBox::warning( this, "Warning", "No directory selected.");
        return;
    }
    if( !selected.startsWith(parent) ) {
        QMessageBox::critical( this, "Error", QString("The selected directory is not inside '%1.").arg(parent) );
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
            QMessageBox::question(this, "Job script found ..."
                                 , QString("The local job directory '%1' already contains a job script 'pbs.sh'.\nDo you want to load it?")
                                      .arg( job_folder )
                                 );
        if( answer == QMessageBox::Yes ) {
            this->loadJobscript(script_file);
            this->statusBar()->showMessage( QString("Job script '%1' loaded.").arg(script_file) );
        } else {
            QMessageBox::warning( this, "Warning"
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
    }
    catch( std::runtime_error& e )
    {
        std::cerr << QString("ERROR: in 'void MainWindow:: loadJobscript( QString const& filepath /*=%1*/)'\n")
                        .arg(filepath)
                        .toStdString()
                  << e.what()
                  << std::endl;
        return false;
    }
 // read from file, hence no unsaved changes sofar
    this->launcher_.script.set_has_unsaved_changes(false);
    return true;
}

bool MainWindow::saveJobscript( QString const& filepath )
{
    std::cout << "    writing script '" << filepath.toStdString() << "' ... "<< std::flush;
    try {
        this->launcher_.script.write( filepath );
        std::cout << "done."<< std::endl;
    } catch( pbs::WarnBeforeOverwrite &e ) {
        QMessageBox::StandardButton answer =
                QMessageBox::question( this, "Ok to overwrite?"
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
            std::cout << "done (overwritten, but made backup '"<<backup.toStdString()<<"')."<< std::endl;
        } else {
            std::cout << "aborted."<< std::endl;
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
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wSubfolder_textChanged(const QString &arg1)", arg1.toStdString() );

    this->ui->wSubfolder2->setText(arg1);

    this->getConfigItem   ("wJobname" )->set_value("");
    this->getDataConnector("wJobname" )->value_config_to_widget();
}

void MainWindow::on_wJobname_textChanged(const QString &arg1)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wJobname_textChanged(const QString &arg1)", arg1.toStdString() );

    this->ui->wJobname2->setText(arg1);

    this->getConfigItem("wJobname" )->set_value( arg1 );
}


void MainWindow::on_wRemote_currentIndexChanged(const QString &arg1)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wRemote_currentIndexChanged(const QString &arg1)", arg1.toStdString() );

    this->getConfigItem("wRemote")->set_value(arg1);
    this->remote_ = arg1;
    if( this->isUserAuthenticated_() ) {
        QString qout;
        QString cmd = QString("echo ").append(this->remote_);
        this->sshSession_.execute( cmd );
        std::cout << "\n" << cmd.toStdString()
                  << "\n''" << this->sshSession_.qout().toStdString() <<"''" <<std::endl;
    }

    if( !this->remote_env_vars_.contains(arg1) && this->isUserAuthenticated_() ) {
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
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wSave_clicked()");

    QString filepath = QString( this->local_subfolder_jobname() ).append("/pbs.sh");
    this->saveJobscript( filepath );
}

void MainWindow::on_wReload_clicked()
{
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wReload_clicked()");

    QString script_file = QString( this->local_subfolder_jobname() ).append("/pbs.sh");
    QDir qDir(script_file);
    if( !qDir.exists() ) {
        this->statusBar()->showMessage("No job script found, hence not loaded.");
    } else {
        QMessageBox::StandardButton answer = QMessageBox::question( this, "Reload job script?", "Unsaved changes to the current job script will be lost. Continue?");
        if( answer==QMessageBox::Yes ) {
            this->loadJobscript(script_file);
            this->statusBar()->showMessage("job script reloaded.");
        }
    }
}

void MainWindow::on_wSubmit_clicked()
{
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wSubmit_clicked()");

    QString filepath = QString( this->local_subfolder_jobname() ).append("/pbs.sh");
    if( this->launcher_.script.has_unsaved_changes() ) {
        if( !this->saveJobscript(filepath) ) {
            return;
        }
    }
    if( !this->isUserAuthenticated_() ) {
        QMessageBox::critical( this, "Error", "You must authenticate first...");
    }
    this->sshSession_.sftp_put_dir( this->local_subfolder_jobname(), this->remote_subfolder_jobname(), true );

    QString cmd = QString("cd ")
                  .append( this->remote_subfolder_jobname() )
                  .append(" && qsub pbs.sh");
    this->sshSession_.execute(cmd);
}
