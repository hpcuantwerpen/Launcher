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
