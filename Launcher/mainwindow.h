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
#include <dataconnector.h>

#include <iostream>


namespace Ui {
    class MainWindow;
}

 //-----------------------------------------------------------------------------
    QString validateEmailAddress( QString const & address );
 //-----------------------------------------------------------------------------
    QString validateUsername( QString const & username );
 //-----------------------------------------------------------------------------



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
    void      clusterDependencies_( bool updateWidgets );
    void      nodesetDependencies_( bool updateWidgets );
    void walltimeUnitDependencies_( bool updateWidgets );

    void update_abe_( QChar c, bool checked );
    bool getPrivatePublicKeyPair_();
    void updateResourceItems_();
    bool isUserAuthenticated_() const;

    void remoteExecute_( QString const& command_line, QString* qout=nullptr, QString* qerr=nullptr );


    Ui::MainWindow *ui;
    Launcher launcher_;
    bool ignoreSignals_;
    int previousPage_;
    QList<dc::DataConnectorBase*> data_;
    dc::DataConnectorBase* getDataConnector( QString const& name );
    cfg::Item* getConfigItem( QString const& name );
    int walltimeUnitSeconds_; // #seconds in walltime unit as given by this->ui->wWalltimeUnit
    QPalette* paletteRedForground_;
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
