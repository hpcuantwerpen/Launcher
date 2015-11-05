#ifndef LOG_H
#define LOG_H

#include <string>
#include <fstream>
#include <QFileInfo>

class Log;

template <class T> std::ostream& operator<<( Log const& log, T const& t );
 /* the Log argument is const& to allow for
  *    Log(2) << ...
  * which is more practical then
  *    Log log(2);
  *

  */

class Log
{/* For implementation details see
  * http://stackoverflow.com/questions/6240950/platform-independent-dev-null-in-c
  */

    template <class T> friend std::ostream& ::operator<<( Log const& log, T const& t );
public:
    Log( int level=0 );
    /* If level<=Log::verbosity the Log object behaves as a std::fstream for file Log::filename,
     * otherwise as an ostream that acts as a sink
     * If level<0 it always acts as a sink.
     * The default level acts as a log file, unless Log::verbosity is negative.
     */
    ~Log();
    static qint64 log_size();
     /* return size of log file in bytes.
      */
    static void clear();
     /* clear the log file.
      */
    static int verbosity;
     /* -1 = no logging (silent)
      */
    static std::string filename;

private:
    mutable std::ofstream* f_;
    mutable std::ostream * null_;
};

template <class T>
std::ostream& operator<<( Log const& log, T const& t )
{
    if( log.f_ ) {
        return *log.f_ << t;
    } else {
        return *log.null_ << t;
    }
}

#define INFO     "\nInfo"
#define DEBUG    "\nDebug"
#define WARNING  "\nWARNING"
#define CRITICAL "\nCRITICAL"
#define FATAL    "\n!!! FATAL !!!"

#define C_STR toUtf8().data
 // allows for Log() << some_QString.C_STR();

#endif // LOG_H
