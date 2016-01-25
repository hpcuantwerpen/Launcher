#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#define LOCAL_REMOVE_REPO_WITH_RM

#include <QMainWindow>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QPalette>
#include <QMap>
#include <QStringList>
#include <QtDebug>
#include <QTime>
#include <QFileSystemModel>
#include <QLabel>
#include <QMessageBox>

#include <launcher.h>
//#include <ssh2tools.h>
#include <job.h>
#include <dataconnector.h>
#include <walltime.h>

#include <iostream>

#include "version.h"
#include "verify.h"
#include "log.h"

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
    explicit MainWindow
      ( toolbox::Log& log
      , QWidget *parent=0
      );
    ~MainWindow();

private:
    void log_call( int verbosity_level, QString const& callee, QString const& msg=QString() );

    void new_session( QString const& path );
    void save_session_path( QString const& path );
    QString load_session_path();
    void open_session( QString const& path );
    void setup_session();
    void save_session( QString const& path );
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

    void on_wNotifyAddress_editingFinished();

    void on_wNotifyAbort_toggled(bool checked);

    void on_wNotifyBegin_toggled(bool checked);

    void on_wNotifyEnd_toggled(bool checked);

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

    void on_wSingleJob_toggled(bool checked);

    void on_wFinished_selectionChanged();

    void on_wClearSelection_clicked();

    void aboutAction_triggered();
    void openSessionAction_triggered();
    void newSessionAction_triggered();
    void saveSessionAction_triggered();
    void saveAsSessionAction_triggered();
    void authenticateAction_triggered();
    void localFileLocationAction_triggered();
    void remoteFileLocationAction_triggered();
    void verboseAction_triggered();
    void selectTemplateAction_triggered();
    void createTemplateAction_triggered();
    void newJobscriptAction_triggered();
    void saveJobscriptAction_triggered();
    void saveAsJobscriptAction_triggered();
    void openJobscriptAction_triggered();
    void reloadJobscriptAction_triggered();
    void submitJobscriptAction_triggered();
    void showFileLocationsAction_triggered();
    void showLocalJobFolderAction_triggered();
    void showRemoteJobFolderAction_triggered();
    void useGitSynchronizationAction_triggered();
    void removeRepoAction_triggered();
    void createRepoAction_triggered();

    void on_wWalltime_editingFinished();

    void on_wNewJobscriptButton_clicked();

    void on_wRefreshLocalFileView_clicked();

    void on_wRefreshRemoteFileView_clicked();

public:
    void setupHome();
    void setIgnoreSignals( bool ignore=true );
    bool getIgnoreSignals() const { return ignoreSignals_; }

    NodesetInfo const& nodesetInfo() const;
    ClusterInfo const& clusterInfo() const;

    void storeResetValues();
    QString check_script_unsaved_changes();
    void script_text_to_config();
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
    bool authenticate( bool silent=true );
    bool actionRequiringAuthentication(const QString &action = QString() );

    void updateResourceItems();
    bool lookForJobscript(QString const& job_folder, bool ask_to_load=false, bool create_if_inexistent=false );
    bool loadJobscript( QString const& filepath );
    bool saveJobscript( QString const& filepath );
    bool remoteCopyJob();
    bool isScriptOpen() const;

    cfg::Item* getSessionConfigItem( QString const& name );
     // get config item by name and create it (empty) if it does not exist
    template <class T>
    cfg::Item* getSessionConfigItem( QString const& name, T default_value );
     // as above but sets a default value (and type) if the item did not exist before.

    QString local_file_location();
    QString project_folder();
    QString job_folder();
    QString remote_file_location();
    QString vscdata_file_location();

    enum PathTo {
        RootFolder, ProjectFolder, JobFolder, Script
    };
    QString  local_path_to( PathTo path_to, QString const& add_message=QString() );
    QString remote_path_to( PathTo path_to, bool resolve=true );
    bool resolve( QString & s );

    QString get_path_to_clusters();

    void createActions();
    void createMenus();

    QString selectedJob( QTextEdit* qTextEdit );
    void clearSelection( QTextEdit* qTextEdit );
    void deleteJob( QString const& jobid ); // from joblist

    QString select_template_location();

    void warn( QString const& msg1, QString const& msg2=QString() );

    QString username();
//    bool can_authenticate();
    void update_WindowTitle();
    void update_StatusbarWidgets();


    typedef QMessageBox::StandardButton (*message_box_t)( QWidget*, QString const&, QString const&, QMessageBox::StandardButtons, QMessageBox::StandardButton );
    QMessageBox::StandardButton message( QString msg, message_box_t message_box=&QMessageBox::warning );
    void error_message_missing_local_file_location( QString const& msg1=QString(), message_box_t message_box=QMessageBox::critical );
    void error_message_missing_project_folder     ( QString const& msg1=QString(), message_box_t message_box=QMessageBox::critical );
    void error_message_missing_job_folder         ( QString const& msg1=QString(), message_box_t message_box=QMessageBox::critical );
    void error_message_missing_job_script         ( QString const& msg1=QString(), message_box_t message_box=QMessageBox::critical );

    bool remove_repo_local ( QString* pmsg=nullptr );
    bool remove_repo_remote( QString* pmsg=nullptr );
    bool create_repo_local ( QString* pmsg=nullptr );
    bool create_repo_remote( QString* pmsg=nullptr );

private:
    toolbox::Log& log_;
    Ui::MainWindow *ui;
    Launcher launcher_;
    bool ignoreSignals_;
    int current_page_;
    QList<dc::DataConnectorBase*> data_;
    dc::DataConnectorBase* getDataConnector( QString const& name );
    QPalette* paletteRedForeground_;

//    ssh::Libssh2Impl sshSession_;
    toolbox::Ssh ssh;
    int verbosity_;

    QMenu* helpMenu_;
    QAction* aboutAction_;

    QMenu* jobMenu_;
    QAction* newJobscriptAction_;
    QAction* saveJobscriptAction_;
    QAction* saveAsJobscriptAction_;
    QAction* openJobscriptAction_;
    QAction* reloadJobscriptAction_;
    QAction* submitJobscriptAction_;
    QAction* showFileLocationsAction_;
    QAction* showLocalJobFolderAction_;
    QAction* showRemoteJobFolderAction_;
    QAction* removeRepoAction_;
    QAction* createRepoAction_;

    QMenu* templatesMenu_;
    QAction* selectTemplateAction_;
    QAction* createTemplateAction_;

    QMenu* sessionMenu_;
    QAction* newSessionAction_;
    QAction* openSessionAction_;
    QAction* saveSessionAction_;
    QAction* saveAsSessionAction_;

    QMenu* extraMenu_;
    QAction* authenticateAction_;
    QAction* localFileLocationAction_;
    QAction* remoteFileLocationAction_;
    QAction* verboseAction_;
    QAction* useGitSynchronizationAction_;

    QLabel *wAuthenticationStatus_
         , *wJobnameIndicator_
         , *wProjectIndicator_;
    Walltime *walltime_;

    QFileSystemModel* file_system_model;

    enum PendingRequest {
        NoPendingRequest=0
      , NodesAndCoresPerNode
      , CoresAndGbPerCore
    } pendingRequest_;

    QString selected_job_;
//    MessageBox messages_;
    QString is_uptodate_for_;
    bool proceed_offline_;
    bool use_git_sync_;
};

#endif // MAINWINDOW_H
