#include "log.h"
#include <cassert>

namespace toolbox
{//-----------------------------------------------------------------------------
    std::ostream Log::sink(nullptr);

    Log::Log( std::string const& log_to, int verbosity )
      : verbosity_(verbosity)
      , ostream_ptr_(nullptr)
      , newed_(nullptr)
    {
        this->set_log( log_to );
    }

    Log::Log( std::ostream* log_to, int verbosity )
      : verbosity_(verbosity)
      , ostream_ptr_(nullptr)
    {
        this->set_log(log_to);
    }

    void
    Log::
    set_log( std::string const& log_to )
    {
        this->log_ = log_to;
        if( log_to=="std::cout" ) {
            this->ostream_ptr_ = &std::cout;
        } else if( log_to=="std::cerr" ) {
            this->ostream_ptr_ = &std::cerr;
        } else if( log_to.empty() || (log_to=="Log::sink") ) {
            this->ostream_ptr_ = &Log::sink;
        } else
        {// this Log is logging to a file which will be opened before and closed after writing.
            this->ostream_ptr_ = nullptr;
        }
    }

    void
    Log::
    set_log( std::ostream* log_to )
    {
        if( log_to==&std::cout ) {
            this->log_ = "std::cout";
        } else if( log_to==&std::cerr ) {
            this->log_ = "std::cerr";
        } else if( log_to==&Log::sink ) {
            this->log_="Log::sink";
        } else {
            this->log_.clear();
        }
        this->ostream_ptr_ = log_to;
    }

    std::ostream* Log::ostream( int verbosity_level )
    {
        if( this->verbosity()<verbosity_level ) {
            return &Log::sink;
        }
        if( this->ostream_ptr_ ) {
            return this->ostream_ptr_;
        } else {
            {// try to open the log file
                assert( this->newed_==nullptr );
                 // if this fails the client didn't call cleanup() after writing to the log.
                 // we could call delete on it, but it may still be in use.
                std::ofstream* ofstrm = new std::ofstream( this->log_.c_str(), std::ios::out|std::ios_base::app );
                if( ofstrm->is_open() ) {// ok
                    this->newed_ = ofstrm;
                    return this->newed_;
                } else {// failed
                    delete ofstrm;
                    std::cerr << "\nClass Log could not open file '" << this->log_
                              << "'.\n(Logging to std::cerr instead).\n";
                    return &std::cerr;
                }
            }
        }
    }

    std::ostream* Log::ostream0( int verbosity_level )
    {
        std::ostream* po = this->ostream(verbosity_level);
        if( po==&Log::sink )
            po = nullptr;
        return po;
    }

    void Log::cleanup()
    {
        if( this->newed_ ) {
            delete this->newed_;
            this->newed_ = nullptr;
        }
    }


//    void Log::write( std::string const& s, int verbosity_level )
//    {
//        std::ostream* p_ostream = this->ostream(verbosity_level);

//        *p_ostream << s << std::flush;

//        this->cleanup();
//    }

    void Log::write(std::ostringstream const& oss, int verbosity_level )
    {
        std::string s = oss.str();
        this->write( s, verbosity_level );
    }

//    void Log::operator() ( std::string const& s, int verbosity_level ) {
//        this->write( s, verbosity_level );
//    }

    void Log::operator<< ( std::ostringstream const& oss ) {
        this->write( oss );
    }

    bool Log::clear()
    {
        if( !this->log_.empty()
         && !this->ostream_ptr_
          ) {
            std::ofstream of( this->log_.c_str(), std::ios::out|std::ios_base::trunc );
            if( of.is_open() ) {
                return true;
            } else {
                std::string s = std::string("\nError: Log::clear(): could not open file '")
                                    .append( this->log_ )
                                    .append("'.\n");
                this->write( s, WriteAlways );
            }
        } else {
            this->write( "\nWarning: Log::clear(): Log object is just holding a std::ostream pointer, cannot clear.\n", WriteAlways );
        }
        return false;
    }

    void operator<< ( Log& logger, std::ostringstream const& oss ) {
        logger.write( oss );
    }

//    qint64 filesize( Log const& log )
//    {
//        qint64 size = QFileInfo( log.filename().c_str() ).size();
//        return size;
//    }
 //-----------------------------------------------------------------------------
}// namespace toolbox
