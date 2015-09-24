#ifndef JOB_H
#define JOB_H

#include <QString>
#include <QList>

#include <ssh2tools.h>

class Job
{
public:
    Job
      ( QString const& job_id
      , QString const&  local_subfolder
      , QString const& remote_subfolder
      , QString const& jobname
      , QString const& status
      );
    Job( QString const& string );

    QString toString() const;
    QString toStringFormatted() const;
    bool isFinished();
     // check if the job is finished and update status_
    void retrieve();
    // retrieve the job and update status_
    QString short_id() const;
    QString long_id () const {
        return this->job_id_;
    }

    static ssh2::Session* sshSession;
private:
    QString job_id_;
    QString local_subfolder_;
    QString remote_subfolder_;
    QString jobname_;
    QString status_;
};

QList<Job>  toJobList   ( QStringList const& list );
QStringList toStringlist( QList<Job>  const& list );

#endif // JOB_H
