#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QTextStream>

#include <warn_.h>
#include <ssh2tools.h>

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
        this->data_.append( dc::newDataConnector( this->ui->wWalltimeUnit , "wWalltimeUnit"  , QString() ) );
        this->data_.append( dc::newDataConnector(                           "walltimeSeconds", "walltime" ) );
        this->data_.append( dc::newDataConnector( this->ui->wNotifyAddress, "wNotifyAddress" , "-M") );
        this->data_.append( dc::newDataConnector(                           "notifyWhen"     , "-m") );
        this->data_.append( dc::newDataConnector( this->ui->wNotifyAbort  , "wNotifyAbort"   , ""  ) );
        this->data_.append( dc::newDataConnector( this->ui->wNotifyEnd    , "wNotifyEnd"     , ""  ) );
        this->data_.append( dc::newDataConnector( this->ui->wNotifyBegin  , "wNotifyBegin"   , ""  ) );

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
        this->data_.append( dc::newDataConnector( this->ui->wWalltime      , "wWalltime"      , QString() ) );

        this->getDataConnector("wCluster")->config_to_widget();
        this->storeResetValues_();

        this->data_.append( dc::newDataConnector( this->ui->wUsername, "wUsername", "user" ) );
        bool can_authenticate = !this->getConfigItem("wUsername")->value().toString().isEmpty();
        this->ui->wAuthenticate->setEnabled( can_authenticate );

        this->setIgnoreSignals(false);

        this->ui->wAutomaticRequests->setChecked( true );

        this->paletteRedForground_ = new QPalette();
        this->paletteRedForground_->setColor(QPalette::Text,Qt::red);
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
            (*iter)->widget_to_config();
            delete (*iter);
        }

        QSize windowSize = this->size();
        this->getConfigItem("windowWidth" )->set_value( windowSize.width () );
        this->getConfigItem("windowHeight")->set_value( windowSize.height() );

        this->launcher_.config.save();

        delete ui;
        ssh2::Session::cleanup();
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

        IgnoreSignals ignoreSignals( this );

        QString cluster = this->getConfigItem("wCluster")->value().toString();
        ClusterInfo& clusterInfo = *(this->launcher_.clusters.find( cluster ) );
     // Initialize Config items that depend on the cluster
        cfg::Item* ci = this->getConfigItem("wNodeset");
        try {
            ci->set_choices( clusterInfo.nodesets().keys() );
        } catch( cfg::InvalidConfigItemValue& e ) {
            ci->set_default_value( clusterInfo.defaultNodeset() );
            ci->set_value        ( clusterInfo.defaultNodeset() );
        }

        if( updateWidgets ) {
            this->getDataConnector("wNodeset")->config_to_widget();
        }

        this->nodesetDependencies_     (updateWidgets);
        this->walltimeUnitDependencies_(updateWidgets);

        this->sshSession_.setLoginNode( clusterInfo.loginNodes()[0] );
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
         // This on has no slots connected - no need to ignore signals
         // It is also not connected to an cfg::Item by a DataConnector, hence
         // we do update it even if updateWidgets is false.

        ci = this->getConfigItem("wNCores");
        range.last() = nodesetInfo.nCoresAvailable();
        ci->set_choices( range, true );
        ci->set_value( nodesetInfo.nCoresPerNode() );

        ci = this->getConfigItem("wGbPerCore");
        range.clear();
        range.append( nodesetInfo.gbPerCoreAvailable() );
        range.append( nodesetInfo.gbPerNodeAvailable() );
        ci->set_choices( range, true );
        ci->set_value( nodesetInfo.gbPerCoreAvailable() );

        if( updateWidgets ) {
            IgnoreSignals ignoreSignals( this );
            this->getDataConnector("wNNodes"       )->config_to_widget();
            this->getDataConnector("wNCoresPerNode")->config_to_widget();
            this->getDataConnector("wNCores"       )->config_to_widget();
            this->getDataConnector("wGbPerCore"    )->config_to_widget();

            double gbTotal = this->ui->wGbPerNode->text().toDouble();
            gbTotal *= this->ui->wNNodes->value();
            this->ui->wGbTotal->setText( QString().setNum( gbTotal,'g',3) );
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
            this->getDataConnector("wWalltime")->config_to_widget();
        }
    }
 //-----------------------------------------------------------------------------

 //-----------------------------------------------------------------------------
 // SLOTS
 //-----------------------------------------------------------------------------
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
    IgnoreSignals ignoreSignals(this);

    QStringList itemList({"wNNodes", "wNCoresPerNode", "wNCores", "wGbPerCore", "wGbTotal"});
    for( QStringList::const_iterator iter=itemList.cbegin(); iter!=itemList.cend(); ++iter )
    {
        dc::DataConnectorBase* dataConnector = this->getDataConnector(*iter);
        dataConnector->config_to_widget();
        dataConnector->config_to_script();
    }
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
        break;
    case 1:
        if( this->previousPage_==0) {
            for( QList<dc::DataConnectorBase*>::iterator iter=this->data_.begin(); iter!=this->data_.end(); ++iter )
            {
                (*iter)->widget_to_config();
                (*iter)->config_to_script();
            }
            this->ui->wScript->setText( this->launcher_.script.text() );
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

    this->getDataConnector("wWalltimeUnit")->widget_to_config();
    this->walltimeUnitDependencies_(true);
}

void MainWindow::on_wWalltime_valueChanged(double arg1)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wWalltime_valueChanged(double arg1)", arg1 );

    IgnoreSignals ignoreSignals(this);

    cfg::Item* walltimeSeconds = this->getConfigItem("walltimeSeconds");
    double seconds = arg1*this->walltimeUnitSeconds_;
    walltimeSeconds->set_value( formatWalltimeSeconds( seconds ) );
    this->getDataConnector("walltimeSeconds")->config_to_script();
    std::cout <<"    walltime set to: " << seconds << "s." << std::endl;
}

void MainWindow::on_wNotifyAddress_editingFinished()
{
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wNotifyAddress_editingFinished()" );


    QString validated = validateEmailAddress( this->ui->wNotifyAddress->text() );
    if( validated.isEmpty() )
    {// not a valid address
        IgnoreSignals ignoreSignals(this);
        this->ui->wNotifyAddress->setPalette(*this->paletteRedForground_);
        this->statusBar()->showMessage("Invalid email address.");
    } else {
        this->ui->wNotifyAddress->setPalette( QApplication::palette(this->ui->wNotifyAddress) );
        this->statusBar()->clearMessage();
        this->getConfigItem("wNotifyAddress")->set_value(validated);

        IgnoreSignals ignoreSignals(this);
        dc::DataConnectorBase* link = this->getDataConnector("wNotifyAddress");
        link->config_to_widget();
        link->config_to_script();
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

    this->getDataConnector("notifyWhen")->config_to_script();
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
        this->ui->wUsername->setPalette(*this->paletteRedForground_);
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

bool
MainWindow::
isUserAuthenticated_() const
{
    return this->ui->wAuthenticate->text() == "authenticated";
}

void
MainWindow::
remoteExecute_( QString const& command_line, QString* qout, QString* qerr )
{
    if( !this->isUserAuthenticated_() ) {
        this->statusBar()->showMessage("You must authenticate first. (Press the authenticae button on the Resources page.) ");
    }
//    this->sshSession_.open();
//    if( !command_line.isEmpty() ) {
//        this->sshSession_.exec( command_line, qout, qerr );
//    }
//    if( close ) {
//        this->sshSession_.close();
//    }
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
        this->ui->wAuthenticate->setText("Authenticated");
        this->ui->wAuthenticate->setEnabled(false);
        this->statusBar()->showMessage("Authentication succeeded.");
        this->getConfigItem("privateKey")->set_value( this->sshSession_.privateKey() );
        this->getConfigItem("publicKey" )->set_value( this->sshSession_. publicKey() );
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
    this->setIgnoreSignals(false);
}
