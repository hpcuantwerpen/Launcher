#include "log.h"

 //-----------------------------------------------------------------------------
    Log::Log( int level )
      : f_(nullptr), null_(nullptr)
    {
        if( level>=0 && level<=Log::verbosity ) {
            this->f_ = new std::ofstream( Log::filename, std::ios::out|std::ios_base::app );
        } else {
            this->null_ = new std::ostream(nullptr);
        }
    }
    //-----------------------------------------------------------------------------
    Log::~Log()
    {
        if( this->f_   ) delete this->f_;
        if( this->null_) delete this->null_;
    }
 //-----------------------------------------------------------------------------
    int Log::verbosity = 1;
 //-----------------------------------------------------------------------------
    std::string Log::filename("log.log");
 //-----------------------------------------------------------------------------
    qint64 Log::log_size()
    {
        qint64 size = QFileInfo( Log::filename.c_str() ).size();
        return size;
    }
 //-----------------------------------------------------------------------------
    void Log::clear()
    {
        std::ofstream log( Log::filename, std::ios::out|std::ios_base::trunc );
    }
 //-----------------------------------------------------------------------------

