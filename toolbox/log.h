#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <limits>

#include "property.h"

namespace toolbox
{//-----------------------------------------------------------------------------
    class Log
    {
    public:
        enum {
            Silent=0
          , WriteAlways = std::numeric_limits<int>::min()
        };

        Log( std::ostream* log_to, int verbosity=1 );
        Log( std::string const& log_to = std::string(), int verbosity=1 );
         /* Create a Log with the specified verbosity. All messages with
          * verbosity_level <= verbosity that are  sent to the log are
          * written to the log, and messages with verbosity_level > verbosity
          * are ignored.
          */

        void set_log( std::string const& log_to = std::string() );
        /* Specify where to log to. Valid strings are
         * .  "std::cout"
         * .  "std::cerr"
         * .  "Log::sink" (or empty string)
         * .  a filename
         */

        void set_log( std::ostream* po );
        /* Specify where to log to.
         */

        template <class T>
        void write( T const& t, int verbosity_level=1 ) {
            std::ostream* o = this->ostream( verbosity_level );
            *o << t << std::flush;
            this->cleanup();
        }

        template <class T>
        void operator<< ( T const& t ) {
            this->write(t);
        }
//        void write      ( std::string const& s         , int verbosity_level = 1 );
//        void operator() ( std::string const& s         , int verbosity_level = 1 );

        void write      ( std::ostringstream const& oss, int verbosity_level = 1 );
        void operator<< ( std::ostringstream const& oss );
         /* Write a message to the log if the verbosity_level is <= than the Log's
          * verbosity. Messages with a higher verbosity_level than the Log's verbosity
          * are ignored.
          * When logging to a file, the file is first opened, then the message is
          * appened, and finally the file is closed again. This avoids that the log
          * is corrupt after a program crash (except if it happens during logging).
          * Messages with verbosity_level == WriteAlways are always written,
          * even if the Log is silent (verbosity==0).
          */

        std::ostream* ostream ( int verbosity_level=1 );
        std::ostream* ostream0( int verbosity_level=1 );
        /* if OstreamLogWrapper holds a raw ostream* it is dereferenced and returned.
         * if it holds a Log* it returns the Log's ostream
         * if it holds nothing, ostream () returns a Log::sink,
         *                      ostream0() returns nullptr
         * if the client is logging to a file, he must call cleanup()
         * before calling ostream() again.
         */
        void cleanup();
        /* if the client is logging to a file, he must call cleanup()
         * before calling ostream() again. It is good practice to call
         * cleanup() always after writing to ostream().
         */

        PROPERTY_RW( int, verbosity, public, public, private)
         /* Verbosity of the log.
          * if verbosity <= 0
          *   the log acts as a sink
          * if verbosity == n
          *   only log messages with verbosity_level <=n are written,
          *   and messages with a higher verbosity_level are sinked.
          * The default value is 1.
          */

//        PROPERTY_Rw( std::string, log, public, public, private )

        bool clear();
         /* Truncate the log file.
          * (If Log is not logging to a file it does nothing).
          */

        static std::ostream sink;

    private:
        std::string log_;
        std::ostream* ostream_ptr_;
        std::ostream* newed_;
    };

    void operator<< ( Log& logger, std::ostringstream const& oss );
     /* Send amessage with default verbosity_level (=1).
      * use as
      *     Log log(...);
      */



 //-----------------------------------------------------------------------------
}// namespace toolbox
#endif // LOG_H
