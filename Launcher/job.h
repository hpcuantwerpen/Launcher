#ifndef JOB_H
#define JOB_H

#include <QString>
#include <QStringList>

//#include <ssh2tools.h>
#include <job.h>

class MainWindow;

class Job
{
    friend class MainWindow;
public:
    static unsigned const Submitted = (1u<<0);
    static unsigned const Finished  = (1u<<1);
    static unsigned const Retrieved = (1u<<2);
//    static ssh2::Session* sshSession;
    Job
      ( QString const& job_id
      , QString const&  local_location
      , QString const& remote_location
      , QString const& subfolder
      , QString const& jobname
      , int            status
      );
    Job( QString const& string );
     // construct Job object from JobList string in config

    QString toString() const;
    QString toStringFormatted() const;
    QString short_id() const;
    QString  long_id() const { return this->job_id_; }
    unsigned status() const { return this->status_; }
    QString status_text() const;
    QString    local_job_folder() const;
    QString   remote_job_folder() const;
    QString vsc_data_job_parent_folder(QString const& vsc_data) const;

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
    typedef QList<Job> List_t;
    typedef List_t::const_iterator const_iterator;

    List_t const& jobs() const { return job_list_; }
    List_t      & jobs()       { return job_list_; }
    List_t::const_iterator cbegin() const { return job_list_.cbegin(); }
    List_t::const_iterator cend  () const { return job_list_.cend  (); }
private:
    List_t job_list_;
    //QStringList job_string_list_;
};


//QStringList toStringlist( QList<Job>  const& list );
//Job findJob( QString const& job_id,  QStringList const& job_list );

#endif // JOB_H
