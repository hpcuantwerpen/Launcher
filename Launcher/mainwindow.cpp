#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QTextStream>

#include <warn_.h>
#include <ssh2tools.h>

namespace dc
{//-----------------------------------------------------------------------------
 // DataConnector::
 //-----------------------------------------------------------------------------
    DataConnector::DataConnector
      ( QWidget      * widget
      , QString const& itemName
      , cfg::Config  * config
      , pbs::Script  * script
      , QString const& key
      )
      : widget_   (widget)
      , itemName_ (itemName)
      , config_   (config)
      , script_   (script)
      , scriptKey_(key)
    {// create the item if it does not already exist
        if( !this->itemName_.isEmpty() ) {
            cfg::Item* item = config->getItem( itemName );
        }
    }
 //-----------------------------------------------------------------------------
    DataConnector::~DataConnector() {}
 //-----------------------------------------------------------------------------
    cfg::Item* DataConnector::item() {
        return &(*( this->config_->find( this->itemName_ ) ));
    }
 //-----------------------------------------------------------------------------
    void DataConnector::setScriptFromConfig()
    {
        if( this->script_ )
        {
            cfg::Item* item = this->item();
            if( item->type()==QVariant::Bool ) {
                this->script_->find_key(this->scriptKey_)->setHidden (item->value().toBool() );
            } else {
//                QString value = item->value().toString();
                QString value = this->formattedItemValue();
                (*this->script_)[this->scriptKey_] = value;
            }
        }
    }
 //-----------------------------------------------------------------------------
 // NoWidgetDataConnector::
 //-----------------------------------------------------------------------------
    NoWidgetDataConnector::
    NoWidgetDataConnector
        ( QString const& itemName
        , cfg::Config  * config
        , pbs::Script  * script
        , QString const& key
        )
        : DataConnector( nullptr, itemName, config, script, key )
    {
        this->setWidgetFromConfig();
    }
 //-----------------------------------------------------------------------------
 // QComboBoxDataConnector::
 //-----------------------------------------------------------------------------
    QComboBoxDataConnector::
    QComboBoxDataConnector
        ( QWidget      * widget
        , QString const& itemName
        , cfg::Config  * config
        , pbs::Script  * script
        , QString const& key
        )
        : DataConnector( widget, itemName, config, script, key )
    {
        this->setWidgetFromConfig();
    }
 //-----------------------------------------------------------------------------
    void QComboBoxDataConnector::setWidgetFromConfig()
    {
        Widget_t* w = this->widget();
        w->clear();
        cfg::Item* item = this->config_->getItem(this->itemName_);

        for( int i=0; i<item->choices().size(); ++i ) {
            w->addItem( item->choices()[i].toString() );
        }
        w->setCurrentText( item->value().toString() );
    }
 //-----------------------------------------------------------------------------
    void QComboBoxDataConnector::setConfigFromWidget() {
        this->config_->getItem(this->itemName_)->set_value( this->widget()->currentText() );
    }
 //-----------------------------------------------------------------------------
 // QDoubleSpinBoxDataConnector::
 //-----------------------------------------------------------------------------
    QDoubleSpinBoxDataConnector::
    QDoubleSpinBoxDataConnector
        ( QWidget      * widget
        , QString const& itemName
        , cfg::Config  * config
        , pbs::Script  * script
        , QString const& key
        )
        : DataConnector( widget, itemName, config, script, key )
    {
        this->setWidgetFromConfig();
    }
 //-----------------------------------------------------------------------------
    void QDoubleSpinBoxDataConnector::setWidgetFromConfig()
    {
        cfg::Item* item = this->config_->getItem(this->itemName_);

        if( item->choices().size() ) {
            QDoubleSpinBox* w = this->widget();
            w->setRange (item->choices().first().toDouble(), item->choices().last().toDouble() );
            w->setValue( item->value().toDouble() );
            w->setSingleStep(0.5);
        }
    }
 //-----------------------------------------------------------------------------
    void QDoubleSpinBoxDataConnector::setConfigFromWidget() {
        this->config_->getItem(this->itemName_)->set_value( this->widget()->value() );
    }
 //-----------------------------------------------------------------------------
    QString QDoubleSpinBoxDataConnector::formattedItemValue() {
        double v = this->item()->value().toDouble();
        QString formatted;
        formatted.setNum(v,'g', this->widget()->decimals() );
        return formatted;
    }
 //----------------------------------------------------------------------------
 // QSpinBoxDataConnector::
 //-----------------------------------------------------------------------------
    QSpinBoxDataConnector::
    QSpinBoxDataConnector
        ( QWidget      * widget
        , QString const& itemName
        , cfg::Config  * config
        , pbs::Script  * script
        , QString const& key
        )
        : DataConnector( widget, itemName, config, script, key )
    {
        this->setWidgetFromConfig();
    }
 //-----------------------------------------------------------------------------
    void QSpinBoxDataConnector::setWidgetFromConfig()
    {
        cfg::Item* item = this->config_->getItem(this->itemName_);

        if( item->choices().size() ) {
            QSpinBox* w = this->widget();
            w->setRange (item->choices().first().toInt(), item->choices().last().toInt() );
            w->setValue( item->value().toInt() );
        }
    }
 //-----------------------------------------------------------------------------
    void QSpinBoxDataConnector::setConfigFromWidget() {
        this->config_->getItem(this->itemName_)->set_value( this->widget()->value());
    }

 //-----------------------------------------------------------------------------
 // QLineEditDataConnector::
 //-----------------------------------------------------------------------------
    QLineEditDataConnector::
    QLineEditDataConnector
        ( QWidget      * widget
        , QString const& itemName
        , cfg::Config  * config
        , pbs::Script  * script
        , QString const& key
        )
        : DataConnector( widget, itemName, config, script, key )
    {
        this->setWidgetFromConfig();
    }
 //-----------------------------------------------------------------------------
    void QLineEditDataConnector::setWidgetFromConfig()
    {
        cfg::Item* item = this->config_->getItem(this->itemName_);
        Widget_t* w = this->widget();
        w->setText( item->value().toString() );
    }
 //-----------------------------------------------------------------------------
    void QLineEditDataConnector::setConfigFromWidget() {
        this->config_->getItem(this->itemName_)->set_value( this->widget()->text() );
    }

 //-----------------------------------------------------------------------------
 // QCheckBoxDataConnector::
 //-----------------------------------------------------------------------------
    QCheckBoxDataConnector::
    QCheckBoxDataConnector
        ( QWidget      * widget
        , QString const& itemName
        , cfg::Config  * config
        , pbs::Script  * script
        , QString const& key
        )
        : DataConnector( widget, itemName, config, script, key )
    {
        this->setWidgetFromConfig();
    }
 //-----------------------------------------------------------------------------
    void QCheckBoxDataConnector::setWidgetFromConfig()
    {
        cfg::Item* item = this->config_->getItem(this->itemName_);
        Widget_t* w = this->widget();
        w->setChecked( item->value().toBool() );
    }
 //-----------------------------------------------------------------------------
    void QCheckBoxDataConnector::setConfigFromWidget() {
        this->config_->getItem(this->itemName_)->set_value( this->widget()->isChecked() );
    }

 //-----------------------------------------------------------------------------
 //-----------------------------------------------------------------------------
    DataConnector*
    getDataConnector( QList<DataConnector*> list, QString const& widgetName )
    {
        for( QList<DataConnector*>::iterator iter=list.begin(); iter!=list.end(); ++iter ) {
            if( (*iter)->name() == widgetName ) {
                return (*iter);
            }
        }
        throw_<std::logic_error>("Widget '%1' not found in QList<DataConnector*>.", widgetName );
    }
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
}// namespace dc



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
        this->setIgnoreSignals();
        cfg::Item* item = nullptr;
     // wWalltimeUnit
        item = this->launcher_.config.getItem("wWalltimeUnit");
        if( item->choices().size()==0 ) {
            QStringList units;
            units.append("seconds");
            units.append("minutes");
            units.append("hours");
            units.append("days");
            units.append("weeks");
            item->set_choices(units);
//            item->set_default_value("hours");
            item->set_value        ("hours");
//            this->walltimeUnitSeconds_ = nSecondsPerUnit( item->value().toString() );
        }
        item = this->launcher_.config.getItem(("wWalltime"));
        this->data_.append( new dc::     QComboBoxDataConnector( this->ui->wWalltimeUnit, "wWalltimeUnit"  , &this->launcher_.config ) );
        this->data_.append( new dc::QDoubleSpinBoxDataConnector( this->ui->wWalltime    , "wWalltime"      , &this->launcher_.config ) );
        this->data_.append( new dc::      NoWidgetDataConnector(                          "walltimeSeconds", &this->launcher_.config, &this->launcher_.script, "walltime" ) );

     // this->ui->wNotifyAddress
        item = this->launcher_.config.getItem("notifyWhen");
        this->data_.append( new dc::QLineEditDataConnector( this->ui->wNotifyAddress, "wNotifyAddress", &this->launcher_.config, &this->launcher_.script, "-M" ) );
        this->data_.append( new dc:: NoWidgetDataConnector(                           "notifyWhen"    , &this->launcher_.config, &this->launcher_.script, "-m" ) );
        this->data_.append( new dc::QCheckBoxDataConnector( this->ui->wNotifyAbort  , "wNotifyAbort"  , &this->launcher_.config ) );
        this->data_.append( new dc::QCheckBoxDataConnector( this->ui->wNotifyEnd    , "wNotifyEnd"    , &this->launcher_.config ) );
        this->data_.append( new dc::QCheckBoxDataConnector( this->ui->wNotifyBegin  , "wNotifyBegin"  , &this->launcher_.config ) );

     // this->ui->wCluster
        this->launcher_.readClusterInfo(); // this throws if there are no clusters/*.info files
        item = this->launcher_.config.getItem("wCluster");
        item->set_choices( this->launcher_.clusters.keys() );

        this->data_.append( new dc::     QComboBoxDataConnector( this->ui->wCluster       , "wCluster"       , &this->launcher_.config, &this->launcher_.script, "cluster"    ) );
        this->data_.append( new dc::     QComboBoxDataConnector( this->ui->wNodeset       , "wNodeset"       , &this->launcher_.config, &this->launcher_.script, "nodeset"    ) );
        this->data_.append( new dc::      QSpinBoxDataConnector( this->ui->wNNodes        , "wNNodes"        , &this->launcher_.config, &this->launcher_.script, "nodes"      ) );
        this->data_.append( new dc::      QSpinBoxDataConnector( this->ui->wNCoresPerNode , "wNCoresPerNode" , &this->launcher_.config, &this->launcher_.script, "ppn"        ) );
        this->data_.append( new dc::      QSpinBoxDataConnector( this->ui->wNCores        , "wNCores"        , &this->launcher_.config, &this->launcher_.script, "n_cores"      ) );
        this->data_.append( new dc::QDoubleSpinBoxDataConnector( this->ui->wGbPerCore     , "wGbPerCore"     , &this->launcher_.config, &this->launcher_.script, "Gb_per_core") );
        this->data_.append( new dc::     QLineEditDataConnector( this->ui->wGbTotal       , "wGbTotal"       , &this->launcher_.config, &this->launcher_.script, "Gb_total"   ) );

        dc::getDataConnector(this->data_,"wCluster")->setWidgetFromConfig();
        this->clusterDependencies_();
        this->storeResetValues_();

        this->data_.append( new dc::QLineEditDataConnector( this->ui->wUsername, "wUsername" , &this->launcher_.config, &this->launcher_.script, "user" ) );
        bool can_authenticate = !this->launcher_.config.getItem("wUsername" )->value().toString().isEmpty();
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
        for( QList<dc::DataConnector*>::iterator iter = this->data_.begin(); iter!=this->data_.end(); ++iter ) {
            (*iter)->setConfigFromWidget();
            delete (*iter);
        }

        QSize windowSize = this->size();
        this->launcher_.config.getItem("windowWidth" )->set_value( windowSize.width () );
        this->launcher_.config.getItem("windowHeight")->set_value( windowSize.height() );

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
    ClusterInfo const&
    MainWindow::clusterInfo() const
    {
        QString            clusterName = this->ui->wCluster->currentText();
        ClusterInfo const& clusterInfo = *( this->launcher_.clusters.find( clusterName ) );
        return clusterInfo;
    }
 //-----------------------------------------------------------------------------
    NodesetInfo const&
    MainWindow::nodesetInfo() const
    {
        QString nodesetName = this->ui->wNodeset->currentText();
        QString clusterName = this->ui->wCluster->currentText();
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
    void MainWindow::clusterDependencies_()
    {
        IgnoreSignals ignoreSignals( this );

        QString cluster = this->ui->wCluster->currentText();
        ClusterInfo& clusterInfo = *(this->launcher_.clusters.find( cluster ) );
     // Initialize Config items that depend on the cluster
        cfg::Item* item = this->launcher_.config.getItem("wNodeset");
        item->set_choices      ( clusterInfo.nodesets().keys() );
//        item->set_default_value( clusterInfo.defaultNodeset() );
        item->set_value        ( clusterInfo.defaultNodeset() );
        dc::getDataConnector( this->data_, "wNodeset")->setWidgetFromConfig();

        this->nodesetDependencies_();

        this->walltimeUnitDependencies_();
        this->sshSession_.setLoginNode( clusterInfo.loginNodes()[0] );
    }
 //-----------------------------------------------------------------------------
    void MainWindow::nodesetDependencies_()
    {
        IgnoreSignals ignoreSignals( this );

        NodesetInfo const& nodesetInfo = this->nodesetInfo();

        cfg::Item* item = this->launcher_.config.getItem("wNNodes");
        QList<QVariant> range;
        range.append( 1 );
        range.append( nodesetInfo.nNodes() );
        item->set_choices( range, true );
        dc::getDataConnector( this->data_, "wNNodes")->setWidgetFromConfig();

        item = this->launcher_.config.getItem("wNCoresPerNode");
        range.last() = nodesetInfo.nCoresPerNode();
        item->set_choices( range, true );
        item->set_value( range.last() );
        dc::getDataConnector( this->data_, "wNCoresPerNode")->setWidgetFromConfig();

        this->ui->wGbPerNode->setText( QString().setNum( nodesetInfo.gbPerNodeAvailable(),'g',3) );

        item = this->launcher_.config.getItem("wNCores");
        range.last() = nodesetInfo.nCoresAvailable();
        item->set_choices( range, true );
        item->set_value( nodesetInfo.nCoresPerNode() );
        dc::getDataConnector( this->data_, "wNCores")->setWidgetFromConfig();

        item = this->launcher_.config.getItem("wGbPerCore");
        range.clear();
        range.append( nodesetInfo.gbPerCoreAvailable() );
        range.append( nodesetInfo.gbPerNodeAvailable() );
        item->set_choices( range, true );
        item->set_value( nodesetInfo.gbPerCoreAvailable() );
        dc::getDataConnector( this->data_, "wGbPerCore")->setWidgetFromConfig();

        double gbTotal = this->ui->wGbPerNode->text().toDouble();
        gbTotal *= this->ui->wNNodes->value();
        this->ui->wGbTotal->setText( QString().setNum( gbTotal,'g',3) );
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
    void MainWindow::walltimeUnitDependencies_()
    {
        IgnoreSignals ignoreSignals( this );

        this->walltimeUnitSeconds_ = nSecondsPerUnit( this->ui->wWalltimeUnit->currentText() );

        double walltime_limit = (double) this->clusterInfo().walltimeLimit() / this->walltimeUnitSeconds_;
        QList<QVariant> range;
        range.append( 0. );
        range.append( walltime_limit );
        cfg::Item*  item = this->launcher_.config.getItem("wWalltime");
        item->set_choices(range,true);
        if( item->value().toDouble() == 0.0 ) {
            item->set_value( 1.0 );
            this->launcher_.config.getItem("walltimeSeconds")
                    ->set_value( formatWalltimeSeconds( this->walltimeUnitSeconds_) );
        } else {
            item->set_value( (double)
                unformatWalltimeSeconds( this->launcher_.config.getItem("walltimeSeconds")->value().toString() )
                 / this->walltimeUnitSeconds_
                           );
        }
        dc::getDataConnector( this->data_, "wWalltime")->setWidgetFromConfig();
    }
 //-----------------------------------------------------------------------------
    dc::DataConnector* MainWindow::getData_( QString const& itemName ) {
        return dc::getDataConnector( this->data_, itemName );
    }
 //-----------------------------------------------------------------------------

 //-----------------------------------------------------------------------------
 // SLOTS
 //-----------------------------------------------------------------------------
void MainWindow::on_wCluster_currentIndexChanged( QString const& arg1 )
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wCluster_currentIndexChanged( QString const& arg1 )", arg1.toStdString() )

    this->launcher_.config["wCluster"].set_value(arg1);
    this->clusterDependencies_();
 // todo effects on script
}

void MainWindow::on_wNodeset_currentTextChanged(const QString &arg1)
{
    PRINT1_AND_CHECK_IGNORESIGNAL( "void MainWindow::on_wNodeset_currentTextChanged(const QString &arg1)", arg1.toStdString() )

    this->launcher_.config.getItem("wNodeset")->set_value(arg1);
    this->nodesetDependencies_();
}

void MainWindow::updateResourceItems_()
{
    QStringList itemList({"wNNodes", "wNCoresPerNode", "wNCores", "wGbPerCore", "wGbTotal"});
    for( QStringList::const_iterator iter=itemList.cbegin(); iter!=itemList.cend(); ++iter )
    {
        dc::DataConnector* link = dc::getDataConnector( this->data_, *iter );
        link->setConfigFromWidget();
        link->setScriptFromConfig();
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
    IgnoreSignals ignoreSignals(this);// will be reset at end of scope
    this->ui->wNCores   ->setValue( nodeset.granted().nCores );
    this->ui->wGbPerCore->setValue( nodeset.granted().gbPerCore );
    this->ui->wGbTotal  ->setText ( QString().setNum( nodeset.granted().gbTotal ) );
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
    IgnoreSignals ignoreSignals(this);// will be reset at end of scope
    this->ui->wNNodes       ->setValue( nodeset.granted().nNodes );
    this->ui->wNCoresPerNode->setValue( nodeset.granted().nCoresPerNode );
    this->ui->wNCores       ->setValue( nodeset.granted().nCores );
    this->ui->wGbPerCore    ->setValue( nodeset.granted().gbPerCore );
    this->ui->wGbTotal      ->setText ( QString().setNum( nodeset.granted().gbTotal ) );
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
//            this->launcher_.modifyScript( this->nodesetInfo() );
            for( QList<dc::DataConnector*>::const_iterator iter=this->data_.cbegin(); iter!=this->data_.cend(); ++iter )
            {
                dc::DataConnector* link = *iter;
                link->setConfigFromWidget();
                link->setScriptFromConfig();
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

    this->walltimeUnitDependencies_();

    //        item = this->launcher_.config.getItem("wWalltime");

}

void MainWindow::on_wWalltime_valueChanged(double arg1)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wWalltime_valueChanged(double arg1)", arg1 );

    cfg::Item* walltimeSeconds = this->launcher_.config.getItem("walltimeSeconds");
    double seconds = arg1*this->walltimeUnitSeconds_;
    walltimeSeconds->set_value( formatWalltimeSeconds( seconds ) );
    std::cout <<"    walltime set to: " << seconds << "s." << std::endl;
}

void MainWindow::on_wNotifyAddress_editingFinished()
{
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wNotifyAddress_editingFinished()" );

    IgnoreSignals ignoreSignals(this);

    QString validated = dc::validateEmailAddress( this->ui->wNotifyAddress->text() );
    if( validated.isEmpty() )
    {// not a valid address
        IgnoreSignals ignoreSignals(this);
        this->ui->wNotifyAddress->setPalette(*this->paletteRedForground_);
        this->statusBar()->showMessage("Invalid email address.");
    } else {
        this->ui->wNotifyAddress->setText( validated );
        this->ui->wNotifyAddress->setPalette( QApplication::palette(this->ui->wNotifyAddress) );
        this->launcher_.config.getItem("wNotifyAddress")->set_value(validated);
        this->statusBar()->clearMessage();

        dc::DataConnector* link = dc::getDataConnector( this->data_, "wNotifyAddress");
        link->setConfigFromWidget();
        link->setScriptFromConfig();
    }
}

void MainWindow::update_abe_( QChar c, bool checked )
{
    cfg::Item* notify_abe = this->launcher_.config.getItem("notifyWhen");
    QString abe = notify_abe->value().toString();
    if( checked && !abe.contains(c) ) {
        abe.append(c);
    } else if( !checked && abe.contains(c) ) {
        int i = abe.indexOf(c);
        abe.remove(i,1);
    }
    notify_abe->set_value( abe );
    std::cout << "    abe set to: '" << abe.toStdString() << "'." << std::endl;

    dc::DataConnector* link = dc::getDataConnector( this->data_, "notifyWhen");
    link->setScriptFromConfig();
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
    QString private_key = QFileDialog::getOpenFileName( nullptr, "Select PRIVATE key file:", this->launcher_.homePath() );
    if( !QFileInfo( private_key ).exists() ) {
        this->statusBar()->showMessage( QString( "Private key '%1' not found.").arg(private_key) );
        return false;
    }
 // private key is provided, now public key
    QString public_key = private_key;
    public_key.append(".pub"); // make an educated guess...
    if( !QFileInfo( public_key ).exists() )
    {
        public_key = QFileDialog::getOpenFileName( nullptr, "Select PUBLIC key file:", this->launcher_.homePath() );
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

    QString username = dc::validateUsername( this->ui->wUsername->text() );

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
        cfg::Item* user = this->launcher_.config.getItem("wUsername");
        if( user->value().toString() != username )
        {// this is a new username, different from current config value
         // store in config
            user->set_value(username);
         // clear config values for private/public key pair
            this->launcher_.config.getItem("privateKey")->set_value( QString() );
            this->launcher_.config.getItem("publicKey" )->set_value( QString() );
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
                    ( this->launcher_.config.getItem("privateKey")->value().toString()
                    , this->launcher_.config.getItem("publicKey" )->value().toString()
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
            QString username = dc::validateUsername( this->ui->wUsername->text() );
            if( username.isEmpty() ) {
                this->statusBar()->showMessage("Invalid username");
             // deactivate the authenticate button
                this->ui->wAuthenticate->setText("authenticate...");
                this->ui->wAuthenticate->setEnabled(false);
                break;
            }
            this->sshSession_.setUsername( username );
            if( this->launcher_.config.getItem("wUsername")->value().toString() == username ) {
                this->sshSession_.setPrivatePublicKeyPair
                        ( this->launcher_.config.getItem("privateKey")->value().toString()
                        , this->launcher_.config.getItem("publicKey" )->value().toString()
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
            this->sshSession_.setUsername( this->launcher_.config.getItem("wUsername")->value().toString() );
            this->launcher_.config.getItem("privateKey")->set_value( QString() );
            this->launcher_.config.getItem("publicKey" )->set_value( QString() );
            break;
        }
     // success
        this->ui->wAuthenticate->setText("Authenticated");
        this->ui->wAuthenticate->setEnabled(false);
        this->statusBar()->showMessage("Authentication succeeded.");
        this->launcher_.config.getItem("privateKey")->set_value( this->sshSession_.privateKey() );
        this->launcher_.config.getItem("publicKey" )->set_value( this->sshSession_. publicKey() );
#ifdef QT_DEBUG
        this->sshSession_.print();
#endif
        break;
    }
    if( attempts==0 )
    {// remove the keys from the config and the session - maybe they were wrong
        this->sshSession_.setUsername( this->launcher_.config.getItem("wUsername")->value().toString() );
        this->launcher_.config.getItem("privateKey")->set_value( QString() );
        this->launcher_.config.getItem("publicKey" )->set_value( QString() );
        this->statusBar()->showMessage( QString("Authentication failed: %1 failed attempts to provide passphrase.").arg(max_attempts) );
    }
    this->setIgnoreSignals(false);
}
