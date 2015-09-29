#ifndef JOB_H
#define JOB_H

#include <QString>
#include <QList>

#include <ssh2tools.h>

class Job
{
public:
    static unsigned const Submitted = (1u<<0);
    static unsigned const Finished  = (1u<<1);
    static unsigned const Retrieved = (1u<<2);
    static ssh2::Session* sshSession;

    Job
      ( QString const& job_id
      , QString const&  local_location
      , QString const& remote_location
      , QString const& subfolder
      , QString const& jobname
      , int            status
      );
    Job( QString const& string );

    QString toString() const;
    QString toStringFormatted() const;
    bool isFinished() const;
     // check if the job is finished and update status_
    bool retrieve(bool local, bool vsc_data) const;
    // retrieve the job and update status_
    QString short_id() const;
    QString  long_id() const { return this->job_id_; }
    unsigned status() const { return this->status_; }
    QString status_text() const;
    QString    local_job_folder() const;
    QString   remote_job_folder() const;
    QString vsc_data_job_parent_folder() const;

private:
    QString job_id_;
    QString  local_location_;
    QString remote_location_;
    QString subfolder_;
    QString jobname_;
    mutable unsigned status_;

    QString append_subfolder_jobname_( QString const& location ) const;
    QString append_subfolder_        ( QString const& location ) const;
};

class JobList
{
public:
    JobList( QStringList const& list=QStringList() );

    void set_list( QStringList const& list );
    QStringList toStringList( unsigned select=0 ) const;
    QString     toString    ( unsigned select=0 ) const;

    Job const* operator[]( QString const& job_id ) const;
    void retrieveAll( bool local, bool vsc_data ) const;

    void update() const;
     // check for jobs that might have finished in the mean time, and update their status.
    typedef QList<Job> JobList_t;
    JobList_t const& jobs() const { return job_list_; }
    JobList_t      & jobs()       { return job_list_; }
private:
    JobList_t job_list_;
    //QStringList job_string_list_;
};


//QStringList toStringlist( QList<Job>  const& list );
//Job findJob( QString const& job_id,  QStringList const& job_list );

#endif // JOB_H
