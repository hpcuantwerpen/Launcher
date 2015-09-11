#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QPalette>

#include <launcher.h>
#include <ssh2tools.h>

#include <iostream>


namespace Ui {
    class MainWindow;
}
namespace dc
{//-----------------------------------------------------------------------------
    class DataConnector
 //-----------------------------------------------------------------------------
    {
    public:
        DataConnector
            ( QWidget      * widget
            , QString const& itemName
            , cfg::Config  * config
            , pbs::Script  * script = nullptr
            , QString const & key   = QString()
            );
        virtual ~DataConnector();
        virtual void setWidgetFromConfig()=0;
                void setScriptFromConfig();
        virtual void setConfigFromWidget()=0;
        virtual QString formattedItemValue() {
            return this->item()->value().toString();
        }

        cfg::Item * item();

        QString const& name() const {
            return itemName_;
        }
    protected:
        QWidget     * widget_;
        QString       itemName_;
        cfg::Config * config_;
        pbs::Script * script_;
        QString       scriptKey_;
    };
 //-----------------------------------------------------------------------------
    class NoWidgetDataConnector : public DataConnector
 //-----------------------------------------------------------------------------
    {
    public:
        NoWidgetDataConnector
            ( QString const& itemName
            , cfg::Config  * config
            , pbs::Script  * script = nullptr
            , QString const & key   = QString()
            );
        virtual ~NoWidgetDataConnector() {}
        virtual void setWidgetFromConfig() {}
        virtual void setConfigFromWidget() {}
        QWidget* widget() { return nullptr; }
    };
 //-----------------------------------------------------------------------------
    class QComboBoxDataConnector : public DataConnector
 //-----------------------------------------------------------------------------
    {
    public:
        QComboBoxDataConnector
            ( QWidget      * widget
            , QString const& itemName
            , cfg::Config  * config
            , pbs::Script  * script = nullptr
            , QString const & key   = QString()
            );
        virtual ~QComboBoxDataConnector() {}
        virtual void setWidgetFromConfig();
        virtual void setConfigFromWidget();
        typedef QComboBox Widget_t;
        Widget_t* widget() {
            return dynamic_cast<Widget_t*>(this->widget_);
        }
    };
 //-----------------------------------------------------------------------------
    class QDoubleSpinBoxDataConnector : public DataConnector
 //-----------------------------------------------------------------------------
    {
    public:
        QDoubleSpinBoxDataConnector
            ( QWidget      * widget
            , QString const& itemName
            , cfg::Config  * config
            , pbs::Script  * script = nullptr
            , QString const & key   = QString()
            );
        virtual ~QDoubleSpinBoxDataConnector() {}
        virtual void setWidgetFromConfig();
        virtual void setConfigFromWidget();
        virtual QString formattedItemValue();
        typedef QDoubleSpinBox Widget_t;
        Widget_t* widget() {
            return dynamic_cast<Widget_t*>(this->widget_);
        }
    };
 //-----------------------------------------------------------------------------
    class QSpinBoxDataConnector : public DataConnector
 //-----------------------------------------------------------------------------
    {
    public:
        QSpinBoxDataConnector
            ( QWidget      * widget
            , QString const& itemName
            , cfg::Config  * config
            , pbs::Script  * script = nullptr
            , QString const & key   = QString()
            );
        virtual ~QSpinBoxDataConnector() {}
        virtual void setWidgetFromConfig();
        virtual void setConfigFromWidget();
        typedef QSpinBox Widget_t;
        Widget_t* widget() {
            return dynamic_cast<Widget_t*>(this->widget_);
        }
    };
 //-----------------------------------------------------------------------------
    class QLineEditDataConnector : public DataConnector
 //-----------------------------------------------------------------------------
    {
    public:
        QLineEditDataConnector
            ( QWidget      * widget
            , QString const& itemName
            , cfg::Config  * config
            , pbs::Script  * script = nullptr
            , QString const & key   = QString()
            );
        virtual ~QLineEditDataConnector() {}
        virtual void setWidgetFromConfig();
        virtual void setConfigFromWidget();
        typedef QLineEdit Widget_t;
        Widget_t* widget() {
            return dynamic_cast<Widget_t*>(this->widget_);
        }
    };
 //-----------------------------------------------------------------------------
    class QCheckBoxDataConnector : public DataConnector
 //-----------------------------------------------------------------------------
    {
    public:
        QCheckBoxDataConnector
            ( QWidget      * widget
            , QString const& itemName
            , cfg::Config  * config
            , pbs::Script  * script = nullptr
            , QString const & key   = QString()
            );
        virtual ~QCheckBoxDataConnector() {}
        virtual void setWidgetFromConfig();
        virtual void setConfigFromWidget();
        typedef QCheckBox Widget_t;
        Widget_t* widget() {
            return dynamic_cast<Widget_t*>(this->widget_);
        }
    };
 //-----------------------------------------------------------------------------
    DataConnector* getDataConnector( QList<DataConnector*> list, QString const& widgetName );
 //-----------------------------------------------------------------------------
    QString validateEmailAddress( QString const & address );
 //-----------------------------------------------------------------------------
    QString validateUsername( QString const & username );
 //-----------------------------------------------------------------------------
}// namespace dc




class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void setIgnoreSignals( bool ignore=true );
    bool getIgnoreSignals() const { return ignoreSignals_; }
    NodesetInfo const& nodesetInfo() const;
    ClusterInfo const& clusterInfo() const;

private:
    void print_( std::string const& signature ) const;
    template <class T>
    void print_( std::string const& signature, T arg1 ) const {
        this->print_(signature);
        std::cout << "    arg1 = " << arg1 << std::endl;
    }
    void storeResetValues_();

private slots:
    void on_wCluster_currentIndexChanged(const QString &arg1);

    void on_wNodeset_currentTextChanged(const QString &arg1);

    void on_wRequestNodes_clicked();

    void on_wRequestCores_clicked();

    void on_wAutomaticRequests_toggled(bool checked);

    void on_wNNodes_valueChanged(int arg1);

    void on_wNCoresPerNode_valueChanged(int arg1);

    void on_wPages_currentChanged(int index);

    void on_wNCores_valueChanged(int arg1);

    void on_wGbPerCore_valueChanged(double arg1);

    void on_wEnforceNNodes_toggled(bool checked);

    void on_wWalltimeUnit_currentTextChanged(const QString &arg1);

    void on_wWalltime_valueChanged(double arg1);

    void on_wNotifyAddress_editingFinished();

    void on_wNotifyAbort_toggled(bool checked);

    void on_wNotifyBegin_toggled(bool checked);

    void on_wNotifyEnd_toggled(bool checked);

    void on_wUsername_editingFinished();

    void on_wAuthenticate_clicked();

private:
    void clusterDependencies_();
    void nodesetDependencies_();
    void walltimeUnitDependencies_();
    void update_abe_( QChar c, bool checked );
    bool getPrivatePublicKeyPair_();
    void updateResourceItems_();
    dc::DataConnector* getData_( QString const& itemName );
    bool isUserAuthenticated_() const;

    void remoteExecute_( QString const& command_line, QString* qout=nullptr, QString* qerr=nullptr );

    Ui::MainWindow *ui;
    Launcher launcher_;
    bool ignoreSignals_;
    int previousPage_;
    QList<dc::DataConnector*> data_;
    int walltimeUnitSeconds_; // #seconds in walltime unit as given by this->ui->wWalltimeUnit
    QPalette* paletteRedForground_;
//    QString privateKey_;
//    QString publicKey_;
//    QString passphrase_;
    ssh2::Session sshSession_;
};

#define PRINT0_AND_CHECK_IGNORESIGNAL( signature ) \
    this->print_( signature ); \
    if( this->ignoreSignals_ ) return;

#define PRINT1_AND_CHECK_IGNORESIGNAL( signature, arg1 ) \
    this->print_( signature, (arg1) ); \
    if( this->ignoreSignals_ ) return;

#define FORWARDING \
    std::cout << "    forwarding" << std::flush;


#endif // MAINWINDOW_H
