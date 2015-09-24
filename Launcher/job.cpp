#include "job.h"
#include <QStringList>


 //-----------------------------------------------------------------------------
    ssh2::Session* Job::sshSession = nullptr;
 //-----------------------------------------------------------------------------
    Job::
    Job
      ( QString const& job_id
      , QString const& local_subfolder
      , QString const& remote_subfolder
      , QString const& jobname
      , QString const& status
      )
      : job_id_(job_id)
      , local_subfolder_(local_subfolder)
      , remote_subfolder_(remote_subfolder)
      , jobname_(jobname)
      , status_(status)
    {}
 //-----------------------------------------------------------------------------
    Job::Job( QString const& string )
    {
        QStringList fields = string.split( QChar('\n') );
        this->job_id_           = fields.at(0);
        this-> local_subfolder_ = fields.at(1);
        this->remote_subfolder_ = fields.at(2);
        this->jobname_          = fields.at(3);
        this->status_           = fields.at(4);
    }
 //-----------------------------------------------------------------------------
    QString Job::toString() const
    {
        QString result = QString( this->job_id_          ).append('\n')
                         .append( this-> local_subfolder_).append('\n')
                         .append( this->remote_subfolder_).append('\n')
                         .append( this->jobname_         ).append('\n')
                         .append( this->status_          )
                         ;
        return result;
    }
 //-----------------------------------------------------------------------------
    QString Job::toStringFormatted() const
    {
        QString result = QString("\n>>> ").append( this->job_id_          )
                         .append("\n    local  subfolder : ").append( this-> local_subfolder_)
                         .append("\n    remote subfolder : ").append( this->remote_subfolder_)
                         .append("\n    jobname          : ").append( this->jobname_         )
                         .append("\n    status           : ").append( this->status_          )
                         .append("\n")
                         ;
        return result;
    }
 //-----------------------------------------------------------------------------
    QList<Job> toJobList( QStringList const& list )
    {
        QList<Job> result;
        for( QStringList::const_iterator iter=list.cbegin()
           ; iter!=list.cend(); ++iter )
        {
            result.append( Job(*iter) );
        }
        return result;
    }
 //-----------------------------------------------------------------------------
    QStringList toStringlist( QList<Job> const& list )
    {
        QStringList result;
        for( QList<Job>::const_iterator iter=list.cbegin()
           ; iter!=list.cend(); ++iter )
        {
            result.append( iter->toString() );
        }
        return result;
    }
 //-----------------------------------------------------------------------------
    QString Job::short_id() const
    {
       int dot_at = this->job_id_.indexOf('.');
       return this->job_id_.left(dot_at);
    }
 //-----------------------------------------------------------------------------
    bool Job::isFinished()
    {
        QString id = this->short_id();
        QString cmd = QString("ls ").append( this->remote_subfolder_ )
                                    .append('/' ).append(this->jobname_)
                                    .append(".o").append( this->short_id() )
                                    ;
        int rv = Job::sshSession->execute(cmd);
        bool is_finished = ( rv==0 );
        if( is_finished ) {
            this->status_ = "finished";
        }
        return is_finished;
    }
 //-----------------------------------------------------------------------------
    void Job::retrieve()
    {

    }

 //-----------------------------------------------------------------------------
