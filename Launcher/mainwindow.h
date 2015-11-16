#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QPalette>
#include <QMap>
#include <QtDebug>

#include <launcher.h>
#include <ssh2tools.h>
#include <job.h>
#include <dataconnector.h>

#include <iostream>

#include "version.h"

namespace Ui {
    class MainWindow;
}

 //-----------------------------------------------------------------------------
    QString validateEmailAddress( QString const & address );
 //-----------------------------------------------------------------------------
    QString validateUsername( QString const & username );
 //-----------------------------------------------------------------------------
    QString
    copy_folder_recursively
      ( QString const& source
      , QString const& destination
      , int level=1
      );
 //-----------------------------------------------------------------------------


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    void new_session( QString const& path );
    void save_session_path( QString const& path );
    QString load_session_path();
    void open_session( QString const& path );
    void setup_session();
    void save_session( QString const& path );
private slots:
    void about();
    void verbose_logging();
    void new_session();
    void open_session();
    void save_session();

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

    void on_wSelectModule_currentIndexChanged(const QString &arg1);

    void on_wReload2_clicked();

    void on_wSave2_clicked();

    void on_wSubmit2_clicked();

    void on_wSingleJob_toggled(bool checked);

    void on_wNotFinished_selectionChanged();

    void on_wFinished_selectionChanged();

    void on_wClearSelection_clicked();

    void on_wCreateTemplate_clicked();

    void on_wSelectTemplate_clicked();

public:
    void setupHome();
    void setIgnoreSignals( bool ignore=true );
    bool getIgnoreSignals() const { return ignoreSignals_; }

    NodesetInfo const& nodesetInfo() const;
    ClusterInfo const& clusterInfo() const;

    void storeResetValues();
    QString check_script_unsaved_changes();
    void refreshJobs( JobList const& joblist );

    bool isFinished( Job const& job ) const;
     // check if the job is finished and update status_
    bool retrieve( Job const& job, bool local, bool vsc_data);
     // retrieve the job (if finished) and update status_
    void retrieveAll( bool local, bool vsc_data ) const;


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

    cfg::Item* getSessionConfigItem( QString const& name );
     // get config item by name and create it (empty) if it does not exist
    template <class T>
    cfg::Item* getSessionConfigItem( QString const& name, T default_value );
     // as above but sets a default value (and type) if the item did not exist before.

    QString local_subfolder();
    QString local_subfolder_jobname();
    QString remote_subfolder();
    QString remote_subfolder_jobname();

    void resolveRemoteFileLocations_();

    QString get_path_to_clusters();

    void createActions();
    void createMenus();

    void activateAuthenticateButton( bool activate, QString const& inactive_button_text="authenticate..." );

    QString selectedJob( QTextEdit* qTextEdit );
    void clearSelection( QTextEdit* qTextEdit );
    void deleteJob( QString const& jobid ); // from joblist

    QString select_template_location();

    void warn( QString const& msg1, QString const& msg2=QString() );
    // msg1 -> status bar
    // msg1+msg2 -> QMessageBox::warning

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
    int verbosity_;

    QAction *aboutAction_;
    QAction *verboseAction_;
    QAction *openSessionAction_;
    QAction *newSessionAction_;
    QAction *saveSessionAction_;

    QMenu *helpMenu_;
    QMenu *sessionMenu_;
    QMenu *extraMenu_;

    enum PendingRequest {
        NoPendingRequest=0
      , NodesAndCoresPerNode
      , CoresAndGbPerCore
    } pendingRequest_;

    QString selected_job_;
};


#endif // MAINWINDOW_H
