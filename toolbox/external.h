#ifndef EXTERNAL_H
#define EXTERNAL_H

#include <QObject>
#include <QProcess>
#include <QDateTime>

#include "property.h"
#include "log.h"

namespace toolbox
{
//    class OstreamLogWrapper
//    {/*
//      * A wrapper that holds astd::ostream* or a toolbox::Log*
//      * This allows toolbox::Execute and toolbox::Ssh to use both types
//      * to log their activities.
//      */
//    public:
//        OstreamLogWrapper( std::ostream* p_ostream=nullptr );
//        OstreamLogWrapper(          Log* p_log     );
//        void set_ostream ( std::ostream* p_ostream );
//        void set_ostream (          Log* p_log     );


//        std::ostream* ostream( int verbosity_level=1 );
//         /* if OstreamLogWrapper holds a raw ostream* it is dereferenced and returned.
//          * if it holds a Log* it returns the Log's ostream
//          * if it holds nothing, it returns a Log::sink;
//          * if the client is logging to a file, he must call cleanup()
//          * before calling ostream() again.
//          */
//        void cleanup();
//        /* if the client is logging to a file, he must call cleanup()
//         * before calling ostream() again. It is good practice to call
//         * cleanup() always after writing to ostream().
//         */
//        std::ostream* ostream0( int verbosity_level=1 );
//         /* As ostream(), but if ostream() would return a sink, ostream0()
//          * returns nullptr.
//          * if the client is logging to a file, he must call cleanup()
//          * before calling ostream() again.
//          */


//    private:
//        std::ostream* p_ostream_;
//        Log* p_log_;
//        std::ostream* p_log_stream_;

//    };

    enum
    {
        TimedOut=-9999
      , Running =-9998
      , NoTimeOut = -1
    };

    namespace Helper
    {
        class ExecuteHelperBase: public QObject
        {
        public:
            ExecuteHelperBase( QObject* parent=nullptr, std::ostream* log_stream=nullptr )
              : QObject(parent)
              , log_stream_(log_stream)
            {}

            QString timestamp() const {
                return this->started_.toString("yyyy-mm-dd_hh:mm:ss.zzz");
            }
            QString duration() const;
            QString starting() const;
            QString summary() const;

            void addLogStream( std::ostream& log_stream ) {
                this->log_stream_ = &log_stream;
            }

            PROPERTY_RW(QString,working_directory,public,public,protected)
        protected:
            QDateTime started_, stopped_;
            QString cmd_;
            QProcess p_;
            bool isBlocking_;
            mutable QByteArray stdout_,stderr_;
            std::ostream* log_stream_;
        };

        class ExecuteHelperBlocking: public ExecuteHelperBase
        {
            Q_OBJECT
        public:
            ExecuteHelperBlocking( QObject* parent=nullptr, std::ostream* log_stream=nullptr);

            int operator() ( QString const& cmd, int secs=30, QString const& comment=QString() );

            QByteArray const& standardOutput() const { return this->stdout_; }
            QByteArray const& standardError() const { return this->stderr_; }
        };

        class ExecuteHelperNonBlocking : public ExecuteHelperBase
        {/*
          * Note that ExecuteNpnBlocking objects must live at least as long as the task
          * they are supposed to carry out. Terminating a ExecuteNpnBlocking object also
          * terminates (kill) its task.
          *
          * You can't use an ExecuteHelperNonBlocking object to execute a second task if the first
          * isn't finished.
          */
            Q_OBJECT
        public:
            ExecuteHelperNonBlocking( QObject* parent=nullptr, std::ostream* log_stream=nullptr );
            virtual ~ExecuteHelperNonBlocking();

            int operator() ( QString const& cmd, QString const& comment=QString() );

            void done( int exitcode, QProcess::ExitStatus exitstatus );

            PROPERTY_RO( bool, isDone )
        private:
            QString comment_;
        };
    }// namespace Helper

    class Execute: public QObject
    {
    public:
        Execute( QObject* parent=nullptr, Log* log=nullptr );

        int operator()( QString const& cmd, int secs=30, QString const& comment=QString() );
         /* Times out after secs. (blocking)
          * Use secs=NoTimeout to execute the command non-blocking
          */
        QByteArray const& standardOutput() const { return stdout_; }
        QByteArray const& standardError () const { return stderr_; }
         /* Return the standard output (resp., standard error) for the last finished
          * blocking command.
          * For non-blocking commands the output is not accessible in the program.
          */
        PROPERTY_RW( int, exit_code, public, public, private )
        QString error() const;
         /* read the error on the last command
          */
        PROPERTY_RW( QString, working_directory, public, public, protected)
        PROPERTY_RW( Log*   , log              , public, public, protected)
//        static void static_set_log_stream( std::ostream* logstream );
    private:
        QByteArray stdout_,stderr_;
//        static OstreamLogWrapper static_log_stream_;
    };

}// namespace toolbox
#endif // EXTERNAL_H
