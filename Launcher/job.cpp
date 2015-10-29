#include "job.h"
#include <QStringList>
#include <iostream>

 //-----------------------------------------------------------------------------
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
//        if( remote_location.startsWith('$') ) {
//            remote_location = this->sshSession->get_env( remote_location );
//        }
        QString result = this->append_subfolder_jobname_(remote_location);
        return result;
    }
 //-----------------------------------------------------------------------------
    QString Job::vsc_data_job_parent_folder(QString const& vsc_data) const {
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
        for ( List_t::const_iterator iter=this->job_list_.cbegin()
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
        for ( List_t::const_iterator iter=this->job_list_.cbegin()
            ; iter!=this->job_list_.cend(); ++iter )
        {
            if( ( select & iter->status() ) == iter->status() ) {
                string.append( iter->toStringFormatted() );
            }
        }
        return string;
    }
 //-----------------------------------------------------------------------------
    Job const* JobList::operator[]( QString const& job_id ) const
    {
        for ( List_t::const_iterator iter=this->job_list_.cbegin()
            ; iter!=job_list_.cend(); ++iter )
        {
            if( iter->long_id()==job_id )
                return &(*iter);
        }
        return nullptr;
    }
 //-----------------------------------------------------------------------------
