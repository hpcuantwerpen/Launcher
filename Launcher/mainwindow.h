#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QPalette>
#include <QMap>

#include <launcher.h>
#include <ssh2tools.h>
#include <job.h>
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

    void on_wLocalFileLocationButton_clicked();

    void on_wSubfolder_textChanged(const QString &arg1);

    void on_wJobname_textChanged(const QString &arg1);

    void on_wSubfolderButton_clicked();

    void on_wJobnameButton_clicked();

    void on_wRemote_currentIndexChanged(const QString &arg1);

    void on_wSave_clicked();

    void on_wReload_clicked();

    void on_wSubmit_clicked();

    void on_wScript_textChanged();

    void on_wRefresh_clicked();

    void on_wRetrieveSelectedJob_clicked();

    void on_wRetrieveAllJobs_clicked();

    void on_wDeleteSelectedJob_clicked();

    void on_wCheckCopyToDesktop_toggled(bool checked);

    void on_wCheckCopyToVscData_toggled(bool checked);

    void on_wCheckDeleteLocalJobFolder_toggled(bool checked);

    void on_wCheckDeleteRemoteJobFolder_toggled(bool checked);

public:
    void setIgnoreSignals( bool ignore=true );
    bool getIgnoreSignals() const { return ignoreSignals_; }

    NodesetInfo const& nodesetInfo() const;
    ClusterInfo const& clusterInfo() const;

    void print_signature( std::string const& signature ) const;

    template <class T>
    void print_signature( std::string const& signature, T arg1 ) const {
        this->print_signature(signature);
        std::cout << "    arg1 = " << arg1 << std::endl;
    }

    void storeResetValues();
    void check_script_unsaved_changes();
    void refreshJobs( JobList const& joblist );

    void      clusterDependencies( bool updateWidgets );
    void      nodesetDependencies( bool updateWidgets );
    void walltimeUnitDependencies( bool updateWidgets );

    void update_abe( QChar c, bool checked );
    bool getPrivatePublicKeyPair();
    void updateResourceItems();
    bool isUserAuthenticated() const;

    void lookForJobscript( QString const& job_folder );
    bool loadJobscript( QString const& filepath );
    bool saveJobscript( QString const& filepath );
    bool remoteCopyJob();

    cfg::Item* getConfigItem( QString const& name );
     // get config item by name and create it (empty) if it does not exist
    template <class T>
    cfg::Item* getConfigItem( QString const& name, T default_value );
     // as above but sets a default value if the item did not exist before.

    QString local_subfolder();
    QString local_subfolder_jobname();
    QString remote_subfolder();
    QString remote_subfolder_jobname();

    void resolveRemoteFileLocations_();

private:
    Ui::MainWindow *ui;
    Launcher launcher_;
    bool ignoreSignals_;
    int previousPage_;
    QList<dc::DataConnectorBase*> data_;
    dc::DataConnectorBase* getDataConnector( QString const& name );
    int walltimeUnitSeconds_; // #seconds in walltime unit as given by this->ui->wWalltimeUnit
    QPalette* paletteRedForeground_;

    ssh2::Session sshSession_;
    QMap<QString,QString> remote_env_vars_;

};

#define PRINT0_AND_CHECK_IGNORESIGNAL( signature ) \
    this->print_signature( signature ); \
    this->check_script_unsaved_changes();\
    if( this->ignoreSignals_ ) return;

#define PRINT1_AND_CHECK_IGNORESIGNAL( signature, arg1 ) \
    this->print_signature( signature, (arg1) ); \
    this->check_script_unsaved_changes();\
    if( this->ignoreSignals_ ) return;

#define FORWARDING \
    std::cout << "    forwarding" << std::flush;


#endif // MAINWINDOW_H
