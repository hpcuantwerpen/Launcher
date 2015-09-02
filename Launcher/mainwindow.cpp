#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QAbstractListModel>


#include <warn_.h>

namespace models
{//-----------------------------------------------------------------------------
    class ClusterModel : public QAbstractListModel
    {
    public:
        ClusterModel( Launcher & launcher )
          : launcher_(&launcher)
          , choices_( launcher.clusters.keys() )
          , wname_("wCluster")
        {
            if( !this->launcher_->config.contains(wname_) )
            {// add a configuration item if none exists
                cfg::Item item(wname_);
                cfg::Item::Choices_t choices;
                for( int i=0; i<choices_.size(); ++i ) {
                    choices.append(choices_[i]);
                }
                item.set_choices(choices);
                this->launcher_->config[wname_] = item;
            } else
            {// ensure that all clusters appear in the wCluster configuration item
                cfg::Item::Choices_t choices = this->launcher_->config[wname_].choices();
                int added = 0;
                for( int i=0; i<choices_.size(); ++i ) {
                    if( !choices.contains(choices_[i]) ) {
                        choices.append(choices_[i]);
                        ++added;
                    }
                }
                if( added ) {
                    this->launcher_->config[wname_].set_choices(choices);
                }
            }
        }
        int rowCount(const QModelIndex & parent = QModelIndex()) const {
            int n = this->launcher_->clusters.size();
            return n;
        }
        QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const
        {
            if( index.isValid() && role == Qt::DisplayRole ) {
                int r = index.row();
                return choices_.at(r);
            } else {
                return QVariant();
            }
        }
    private:
        Launcher* launcher_;
        QList<QString> choices_;
        QString wname_;
    };
 //-----------------------------------------------------------------------------
    class NodesetModel : public QAbstractListModel
    {
    public:
        NodesetModel( Launcher & launcher )
          : launcher_(&launcher)
          , wname_("wNodeset")
        {}
        QString selectCluster( QString cluster)
         // returns the nodeset that was selected in a previous session
        {
            if( cluster_!=cluster )
            {
                cluster_ = cluster;
                this->choices_ = launcher_->clusters[cluster].nodesets().keys();
                if( !this->launcher_->config.contains(wname_) )
                {// create and add a new configuration item
                    cfg::Item item(wname_);
                    cfg::Item::Choices_t choices;
                    for( int i=0; i<choices_.size(); ++i ) {
                        choices.append(choices_[i]);
                    }
                    item.set_choices(choices);
                    if( !launcher_->clusters[cluster].defaultNodeset().isEmpty() ) {
                        item.set_default_value( launcher_->clusters[cluster].defaultNodeset() );
                        item.set_value( item.default_value() );
                    }
                    this->launcher_->config[wname_] = item;

                    return item.value().toString();
                } else {
                    cfg::Item& item = *( this->launcher_->config.find(wname_) );
//                    if( this->launcher_->isConstructingMainWindow )
//                    {// don't modify the configuration during the construction of the MainWindow
//                        return item.value().toString();
//                    }
                 // cfg::Item stores its choices as a QList<QVariant>, which cannot be compared to
                 // a QStringList. We create a temporary cfg::Item to do the comparison
                    cfg::Item item2;
                    item2.set_choices(choices_);
                    if( item.choices() != item2.choices() ) {
                        item.set_choices( choices_ );
                        if( !launcher_->clusters[cluster].defaultNodeset().isEmpty() ) {
                            item.set_default_value( launcher_->clusters[cluster].defaultNodeset() );
                            item.set_value( item.default_value() );
                        }
                        return item.value().toString();
                    } else {
                        return item.value().toString();
                    }
                }
            }
        }
        int rowCount(const QModelIndex & parent = QModelIndex()) const {
            int n = this->choices_.size();
            return n;
        }
        QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const
        {
            if( index.isValid() && role == Qt::DisplayRole ) {
                int r = index.row();
                return choices_.at(r);
            } else {
                return QVariant();
            }
        }
    private:
        Launcher* launcher_;
        QList<QString> choices_;
        QString wname_;
        QString cluster_;
    };
 //-----------------------------------------------------------------------------
}// namespace models

 //-----------------------------------------------------------------------------
    MainWindow::MainWindow(QWidget *parent)
      : QMainWindow(parent)
      , ui(new Ui::MainWindow)
      , ignoreSignals_(false)
      , previousPage_(0)
    {
        ui->setupUi(this);

        this->launcher_.readClusterInfo();

        QString launcher_cfg( Launcher::homePath("Launcher.cfg") );
        if( QFileInfo(launcher_cfg).exists() ) {
            cfg::load(this->launcher_.config,launcher_cfg);
        }

        this->setIgnoreSignals();
        models::ClusterModel* wClusterModel = new models::ClusterModel(this->launcher_);
        this->ui->wCluster->setModel(wClusterModel);

        this->setIgnoreSignals(false);

        models::NodesetModel* wNodesetModel = new models::NodesetModel(this->launcher_);
        this->ui->wNodeset->setModel(wNodesetModel);
        QString text = dynamic_cast<models::NodesetModel*>( ui->wNodeset->model() )->selectCluster( ui->wCluster->currentText() );
        ui->wNodeset->setCurrentText( text );


        QString cfg_value;
        cfg_value = this->launcher_.config["wCluster"].value().toString();
        this->ui->wCluster->setCurrentText( cfg_value );
        cfg_value = this->launcher_.config["wNodeset"].value().toString();
        this->ui->wNodeset->setCurrentText( cfg_value );

        this->ui->wAutomaticRequests->setChecked( true );
    }
 //-----------------------------------------------------------------------------
    MainWindow::~MainWindow()
    {
        cfg::save( this->launcher_.config, Launcher::homePath("Launcher.cfg") );
        delete ui;
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
    NodesetInfo const&
    MainWindow::nodesetInfo() const
    {
        QString nodesetName = this->ui->wNodeset->currentText();
        QString clusterName = this->ui->wCluster->currentText();
        NodesetInfo const& nodeset = *( this->launcher_.clusters[clusterName].nodesets().find( nodesetName ) );
        return nodeset;
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
// slots
//------------------------------------------------------------------------------
void MainWindow::on_wCluster_currentIndexChanged( QString const& arg1 )
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wCluster_currentIndexChanged( QString const& arg1 )", arg1.toStdString() )

    this->launcher_.config["wCluster"].set_value(arg1);
    QString text = dynamic_cast<models::NodesetModel*>( ui->wNodeset->model() )->selectCluster( arg1 );
    ui->wNodeset->setCurrentText( text );
 // todo effects on script
}

void MainWindow::on_wNodeset_currentTextChanged(const QString &arg1)
{
    PRINT1_AND_CHECK_IGNORESIGNAL( "void MainWindow::on_wNodeset_currentTextChanged(const QString &arg1)", arg1.toStdString() )

    NodesetInfo const &nodeset = this->nodesetInfo();

    this->launcher_.config["wNodeset"].set_value(arg1);
    this->ui->wNNodes->setRange(1,nodeset.nNodes());

    this->ui->wNCoresPerNode->setRange(1,nodeset.nCoresPerNode());
    this->ui->wNCoresPerNode->setValue(nodeset.nCoresPerNode());

    //QString v;
    this->ui->wGbPerNode->setText( QString().setNum( nodeset.gbPerNodeAvailable() ) );

    this->ui->wNCores->setRange(1,nodeset.nNodes()*nodeset.nCoresPerNode());
    this->ui->wNCores->setValue(nodeset.nCoresPerNode());

    this->ui->wGbPerCore->setRange(0,nodeset.gbPerNodeAvailable());
    this->ui->wGbPerCore->setValue(nodeset.gbPerNodeAvailable()/nodeset.nCoresPerNode());

    this->ui->wGbTotal->setText( QString().setNum(nodeset.gbPerNodeAvailable()*this->ui->wNNodes->value(),'g',3));
}

void MainWindow::on_wRequestNodes_clicked()
{
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wRequestNodes_clicked()")

    NodesetInfo const& nodeset = this->nodesetInfo();
    try {
        nodeset.requestNodesAndCores( this->ui->wNNodes->value(), this->ui->wNCoresPerNode->value() );
    } catch( std::runtime_error & e ) {
        warn_( e.what() );
        return;
    }
    this->ui->wNCores   ->setValue( nodeset.granted().nCores );
    this->ui->wGbPerCore->setValue( nodeset.granted().gbPerCore );
    this->ui->wGbTotal  ->setText ( QString().setNum( nodeset.granted().gbTotal ) );
}

void MainWindow::on_wRequestCores_clicked()
{
    PRINT0_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wRequestCores_clicked()")

    NodesetInfo const& nodeset = this->nodesetInfo();
    try {
        nodeset.requestCoresAndMemory( this->ui->wNCores->value(), this->ui->wGbPerCore->value() );
    } catch( std::runtime_error & e ) {
        warn_( e.what() );
        return;
    }

    this->ui->wNNodes       ->setValue( nodeset.granted().nNodes );
    this->ui->wNCoresPerNode->setValue( nodeset.granted().nCoresPerNode );
    this->ui->wNCores       ->setValue( nodeset.granted().nCores );
    this->ui->wGbPerCore    ->setValue( nodeset.granted().gbPerCore );
    this->ui->wGbTotal      ->setText ( QString().setNum( nodeset.granted().gbTotal ) );
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

void MainWindow::on_wPages_currentChanged(int index)
{
    PRINT1_AND_CHECK_IGNORESIGNAL("void MainWindow::on_wPages_currentChanged(int index)", index )

    std::cout << "    previous page: " << this->previousPage_ << std::endl;
    switch (index) {
    case 0:
        break;
    case 1:
        if( this->previousPage_==0) {
            this->launcher_.modifyScript( this->nodesetInfo() );
            this->ui->wScript->setText( this->launcher_.script.text() );
        }
        break;
    default:
        break;
    }
    this->previousPage_ = index;
}
