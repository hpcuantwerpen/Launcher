#include "job.h"
#include <QStringList>
#include <iostream>

 //-----------------------------------------------------------------------------
    ssh2::Session* Job::sshSession = nullptr;
 //-----------------------------------------------------------------------------
    Job::
    Job
      ( QString const& job_id
      , QString const& local_location
      , QString const& remote_location
      , QString const& subfolder
      , QString const& jobname
      , int            status
      )
      : job_id_(job_id)
      ,  local_location_( local_location)
      , remote_location_(remote_location)
      , subfolder_(subfolder)
      , jobname_(jobname)
      , status_(status)
    {}
 //-----------------------------------------------------------------------------
    Job::Job( QString const& string )
    {
        QStringList fields = string.split( QChar('\n') );
        this->job_id_          = fields.at(0);
        this-> local_location_ = fields.at(1);
        this->remote_location_ = fields.at(2);
        this->subfolder_       = fields.at(3);
        this->jobname_         = fields.at(4);
        this->status_          = fields.at(5).toUInt();
    }
 //-----------------------------------------------------------------------------
    QString Job::toString() const
    {
        QString result = QString( this->job_id_         ).append('\n')
                         .append( this-> local_location_).append('\n')
                         .append( this->remote_location_).append('\n')
                         .append( this->subfolder_      ).append('\n')
                         .append( this->jobname_        ).append('\n')
                         .append( QString().setNum(this->status_) )
                         ;
        return result;
    }
 //-----------------------------------------------------------------------------
    QString Job::toStringFormatted() const
    {
        QString result = QString("\n>>> ").append( this->job_id_          )
                         .append("\n    local  location : ").append( this-> local_location_)
                         .append("\n    remote location : ").append( this->remote_location_)
                         .append("\n    subfolder       : ").append( this->subfolder_      )
                         .append("\n    jobname         : ").append( this->jobname_        )
                         .append("\n    status          : ").append( this->status_text()   )
                         .append("\n")
                         ;
        return result;
    }
 //-----------------------------------------------------------------------------
    QString Job::short_id() const
    {
       int dot_at = this->job_id_.indexOf('.');
       return this->job_id_.left(dot_at);
    }
 //-----------------------------------------------------------------------------
    bool Job::isFinished() const
    {
        if( this->status_ == Job::Submitted )
        {// test if the file pbs.sh.o<short_id> exists, if so, the job is finished.

         /* TODO make sure this also works if the #PBS -o redirect_stdout is used
          * and pbs.sh.oXXXXXX is replaced by redirect_stdout.
          * On second thought NOT a good idea. If this option is exposed to the
          * user he will probably use it to put a fixed file name and it will
          * become impossible to judge whether the file is there because the job
          * is finished, or whether it is a left over from an older run that was
          * not cleaned up

mn01.hopper.antwerpen.vsc:
                                                                                  Req'd    Req'd       Elap
Job ID                  Username    Queue    Jobname          SessID  NDS   TSK   Memory   Time    S   Time
----------------------- ----------- -------- ---------------- ------ ----- ------ ------ --------- - ---------
128279.mn.hopper.antwe  vsc20170    q1h      a_simple_job        --      1     20    --   01:00:00 Q       --
          */
            try
            {// Test for the existence fo finished.<jobid>
             // The job cannot be finished if finished.<jobid> does not exist.
                QString cmd = QString("ls ")
                    .append( this->remote_location_ )
                    .append('/' ).append(this->subfolder_)
                    .append('/' ).append(this->jobname_)
                    .append("/finished.").append( this->short_id() );
                int rv = Job::sshSession->execute(cmd);
                bool is_finished = ( rv==0 );
                if( !is_finished ) {
                    return false;
                }
             // It is possible but rare that finished.<jobid> exists, but the
             // epilogue is still running. To make sure:
             // Check the output of qstat -u username
                cmd = QString("qstat -u ").append( Job::sshSession->username() );
                rv = Job::sshSession->execute(cmd);
                QStringList qstat_lines = Job::sshSession->qout().split('\n',QString::SkipEmptyParts);
                for ( QStringList::const_iterator iter=qstat_lines.cbegin()
                    ; iter!=qstat_lines.cend(); ++iter ) {
                    if( iter->startsWith(this->job_id_)) {
                        if ( iter->at(101)=='C' ) {
                            this->status_ = Job::Finished;
                            return true;
                        } else {
                            return false;
                        }
                    }
                }
             // it is no longer in qstat so it must be finished.
                this->status_ = Job::Finished;
                return true;
            } catch( std::runtime_error& e ) {
                std::cout << e.what() << std::endl;
            }
            return false; // never reached, but keep the compiler happy
        } else {
            return true;
        }
    }
 //-----------------------------------------------------------------------------
    bool Job::retrieve( bool local, bool vsc_data ) const
    {
        if( this->status()==Job::Retrieved ) return true;

        if( local )
        {
            QString target = this-> local_job_folder();
            QString source = this->remote_job_folder();
            this->sshSession->sftp_get_dir(target,source);
            this->status_ = Job::Retrieved;
        }
        if( vsc_data && this->remote_location_!="$VSC_DATA")
        {
            QString cmd = QString("mkdir -p ").append( this->vsc_data_job_parent_folder() );
            int rc = this->sshSession->execute(cmd);
            if( rc ) {/*keep compiler happy (rc unused variable)*/}
            cmd = QString("cp -rv ").append( this->remote_job_folder() )
                                    .append(" ").append( this->vsc_data_job_parent_folder() );
            rc = this->sshSession->execute(cmd);
            this->status_ = Job::Retrieved;
        }
        return this->status()==Job::Retrieved;
    }
 //-----------------------------------------------------------------------------
    QString Job::local_job_folder() const {
        QString result = this->append_subfolder_jobname_( QString(this->local_location_) );
        return result;
    }
 //-----------------------------------------------------------------------------
    QString Job::status_text() const
    {
        switch( this->status() ) {
        case Job::Finished:
            return "Finished";
        case Job::Submitted:
            return "Submitted";
        case Job::Retrieved:
            return "Retrieved";
        default:
            return "";
        }
    }
 //-----------------------------------------------------------------------------
    QString Job::remote_job_folder() const {
        QString remote_location = this->remote_location_;
        if( remote_location.startsWith('$') ) {
            remote_location = this->sshSession->get_env( remote_location );
        }
        QString result = this->append_subfolder_jobname_(remote_location);
        return result;
    }
 //-----------------------------------------------------------------------------
    QString Job::vsc_data_job_parent_folder() const {
        QString vsc_data = this->sshSession->get_env("$VSC_DATA");
        QString result = this->append_subfolder_(vsc_data);
        return result;
    }
 //-----------------------------------------------------------------------------
    QString Job::append_subfolder_jobname_( QString const& location ) const {
        return QString(location).append('/').append(this->subfolder_)
                                .append('/').append(this->jobname_);
    }
 //-----------------------------------------------------------------------------
    QString Job::append_subfolder_( QString const& location ) const {
        return QString(location).append('/').append(this->subfolder_);
    }
 //-----------------------------------------------------------------------------

 //-----------------------------------------------------------------------------
 // JobList::
 //-----------------------------------------------------------------------------
    JobList::JobList( QStringList const& list )
    {
        this->set_list(list);
    }
 //-----------------------------------------------------------------------------
    void JobList::set_list( QStringList const& list )
    {
        this->job_list_.clear();
        for( QStringList::const_iterator iter=list.cbegin()
           ; iter!=list.cend(); ++iter )
        {
            this->job_list_.append( Job(*iter) );
        }
    }
 //-----------------------------------------------------------------------------
    QStringList JobList::toStringList(unsigned select ) const
    {
        QStringList strings;
        for ( JobList_t::const_iterator iter=this->job_list_.cbegin()
            ; iter!=this->job_list_.cend(); ++iter )
        {
            if( ( select & iter->status() ) == iter->status() ) {
                strings.append( iter->toString() );
            }
        }
        return strings;
    }
 //-----------------------------------------------------------------------------
    QString JobList::toString(unsigned select ) const
    {
        QString string;
        for ( JobList_t::const_iterator iter=this->job_list_.cbegin()
            ; iter!=this->job_list_.cend(); ++iter )
        {
            if( ( select & iter->status() ) == iter->status() ) {
                string.append( iter->toStringFormatted() );
            }
        }
        return string;
    }
 //-----------------------------------------------------------------------------
    void JobList::retrieveAll( bool local, bool vsc_data ) const
    {
        for ( JobList_t::const_iterator iter=this->job_list_.cbegin()
            ; iter!=job_list_.cend(); ++iter )
        {
            iter->retrieve( local, vsc_data );
        }
    }
 //-----------------------------------------------------------------------------
    Job const* JobList::operator[]( QString const& job_id ) const
    {
        for ( JobList_t::const_iterator iter=this->job_list_.cbegin()
            ; iter!=job_list_.cend(); ++iter )
        {
            if( iter->long_id()==job_id )
                return &(*iter);
        }
        return nullptr;
    }
 //-----------------------------------------------------------------------------
    void JobList::update() const
    {
        for ( JobList_t::const_iterator iter=this->job_list_.cbegin()
            ; iter!=job_list_.cend(); ++iter )
        {
            iter->isFinished();
        }
    }
