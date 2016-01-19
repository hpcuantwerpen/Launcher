#include "external.h"
#include <iostream>
#include <cassert>

namespace toolbox
{
/*
    OstreamLogWrapper::OstreamLogWrapper( std::ostream* p_ostream )
      : p_ostream_   (nullptr)
      , p_log_       (nullptr)
      , p_log_stream_(nullptr)
    {
        if( p_ostream )
            this->set_ostream( p_ostream );
    }

    OstreamLogWrapper::OstreamLogWrapper( Log* p_log )
      : p_ostream_   (nullptr)
      , p_log_       (nullptr)
      , p_log_stream_(nullptr)
    {
        this->set_ostream( p_log );
    }

    void OstreamLogWrapper::set_ostream ( std::ostream* p_ostream )
    {
        this->p_ostream_ = p_ostream;
        this->p_log_     = nullptr;
        this->cleanup();
    }

    void OstreamLogWrapper::set_ostream ( Log* p_log )
    {
        this->p_ostream_ = nullptr;
        this->p_log_     = p_log;
        this->cleanup();
    }

    std::ostream *OstreamLogWrapper::ostream( int verbosity_level )
    {
        std::ostream* po = nullptr;
        if( this->p_ostream_ ) {
            po = this->p_ostream_;
        } else if( this->p_log_ ) {
            assert( this->p_log_stream_==nullptr );
             // if this fails the client didn't call cleanup() after writing to the log.
            bool is_newed;
            po = this->p_log_->get_ostream( verbosity_level, is_newed );
            if( is_newed )
            {// store the pointer so that we can call delete on it in cleanup()
                this->p_log_stream_ = po;
            }
        }
        return po;
    }

    std::ostream *OstreamLogWrapper::ostream0( int verbosity_level )
    {
        std::ostream* po = this->ostream(verbosity_level);
        if( po==&Log::sink )
            po = nullptr;
        return po;
    }

    void
    OstreamLogWrapper::
    cleanup()
    {
        if( p_log_stream_ )
            delete p_log_stream_;
    }
*/

    namespace Helper
    {
        QString
        ExecuteHelperBase::
        duration() const
        {
            QString d("%1");
            qint64 msecs = this->started_.msecsTo( this->stopped_ );
            d = ( msecs > 99999
                ? d.arg(msecs/1000).append("s")
                : d.arg(msecs     ).append("ms")
                );
            return d;
        }

        QString
        ExecuteHelperBase::
        starting() const
        {
            QString s = QString("Started: '%1'\n"
                                "     on: %2 (%3)\n"
                                "     in: %4\n")
                           .arg( this->cmd_ )
                           .arg( this->timestamp() ).arg( (this->isBlocking_ ? "blocking" : "non-blocking") )
                           .arg( (this->working_directory().isEmpty() ? QString(".") : this->working_directory() ) )
                           ;
            return s;
        }

        QString
        ExecuteHelperBase::
        summary() const
        {
            ExecuteHelperBase* non_const_this = const_cast<ExecuteHelperBase* >(this);
            stdout_ = non_const_this->p_.readAllStandardOutput();
            stderr_ = non_const_this->p_.readAllStandardError();

            QString s = QString("Completed  : '%1'\n"
                                "  exitcode : %2\n"
                                "  started  : %3\n"
                                "  duration : %4\n"
                                "  stdout   :\n"
                                "%5\n"
                                "  stderr   :\n"
                                "%6")
                           .arg( this->cmd_)
                           .arg( this->p_.exitCode() )
                           .arg( this->timestamp() )
                           .arg( this->duration() )
                           .arg( (stdout_.size() ? stdout_.constData() : "[none]") )
                           .arg( (stderr_.size() ? stderr_.constData() : "[none]") )
                           ;
            return s;
        }

        ExecuteHelperBlocking::ExecuteHelperBlocking( QObject *parent , std::ostream *log_stream )
          : ExecuteHelperBase(parent,log_stream)
        {
            this->isBlocking_ = true;
        }

        int
        ExecuteHelperBlocking::
        operator()
          ( QString const& cmd
          , int secs
          , QString const& comment
          )
        {
            this->started_ = QDateTime::currentDateTime();
            this->cmd_       = cmd;
            this->p_.setWorkingDirectory(this->working_directory());
            this->p_.start(cmd);
         // block, wait for completion of the command
            if( log_stream_ ) {
                *log_stream_ << "<<< [" << comment.toLatin1().constData() << "]\n"
                            << this->starting().toLatin1().constData()
                            << std::endl;
            }
            int rc = TimedOut;
            if( this->p_.waitForFinished(secs*1000) ) {
                rc = this->p_.exitCode();
            }
            this->stopped_ = QDateTime::currentDateTime();
            if( log_stream_ ) {
                *log_stream_ << this->summary().toLatin1().constData()
                        << "\n>>>\n" << std::endl;
            }
            this->p_.close();
            return rc;
        }

        ExecuteHelperNonBlocking::
        ExecuteHelperNonBlocking( QObject *parent, std::ostream *log_stream)
          : ExecuteHelperBase(parent,log_stream)
          , isDone_(true)
        {
            connect( &this->p_, static_cast< void(QProcess::*)(int, QProcess::ExitStatus) >( &QProcess::finished )
                   , this     , &ExecuteHelperNonBlocking::done
                   );
            this->isBlocking_ = false;
        }

        ExecuteHelperNonBlocking::
        ~ExecuteHelperNonBlocking()
        {// disconnect all signals from our QProcess object
            disconnect( &this->p_, 0, 0, 0);
        }

        int
        ExecuteHelperNonBlocking::
        operator() ( QString const& cmd, QString const& comment )
        {
            if(!this->isDone() ) {
                return Running;
            }
            this->started_ = QDateTime::currentDateTime();
            this->cmd_     = cmd;
            this->isDone_  = false;
            this->p_.setWorkingDirectory(this->working_directory());
            this->p_.start(cmd);
            if( log_stream_ ) {
                this->comment_ = comment;
                *log_stream_ << "<<< [" << comment.toLatin1().constData() << "]\n"
                             << this->starting().toLatin1().constData()
                             << "\n>>>\n" << std::endl;
            }
            return 0;
        }

        void
        ExecuteHelperNonBlocking::
        done( int /*exitcode*/, QProcess::ExitStatus /*exitstatus*/ )
        {
            this->stopped_ = QDateTime::currentDateTime();
            if( log_stream_ ) {
                *log_stream_ << "<<< [" << this->comment_.toLatin1().constData() << "]\n"
                             << this->summary().toLatin1().constData()
                             << "\n>>>\n" << std::endl;
                this->comment_.clear();
            }
            this->p_.close();
            this->isDone_ = true;
        }
    }


    Execute::
    Execute( QObject *parent, Log* log )
      : QObject(parent)
    {
        this->set_log(log);
    }

//    std::ostream* const&
//    Execute::
//    log_stream() const
//    {
//        if( this->log_stream_ ) {
//            return this->log_stream_;
//        } else {
//            return Execute::static_log_stream;
//        }
//    }

//    std::ostream* Execute::static_log_stream = nullptr;

    int
    Execute::
    operator()( QString const& cmd, int secs, QString const& comment )
    {
        std::ostream* ostrm = ( this->log_ ? this->log_->ostream0() : nullptr );
        if( secs==NoTimeOut ) {
            Helper::ExecuteHelperNonBlocking* x = new Helper::ExecuteHelperNonBlocking( this, ostrm );
            x->set_working_directory(this->working_directory());
            this->exit_code_ = (*x)( cmd, comment );
            this->stdout_ = QByteArray();
            this->stderr_ = QByteArray();
        } else {
            Helper::ExecuteHelperBlocking x( this, ostrm );
            x.set_working_directory(this->working_directory());
            this->exit_code_ = x( cmd, secs, comment );
            this->stdout_ = x.standardOutput();
            this->stderr_ = x.standardError();
        }
        if( ostrm ) {
            this->log_->cleanup();
        }
        return this->exit_code_;
    }

//    int
//    Execute::
//    test(QString const& cmd, int secs, QString const& comment )
//    {
//        if( secs==NoTimeOut ) {
//            throw std::runtime_error("");
//        } else {
//            Helper::ExecuteHelperBlocking x(nullptr,Execute::static_log_stream_);
//            int rc = x(cmd,secs,comment);
//            return rc;
//        }
//    }

    QString
    Execute::
    error() const
    {
        if( this->exit_code_ ) {
            return QString( this->standardError() );
        } else {
            return QString();
        }
    }

}// namespace toolbox
