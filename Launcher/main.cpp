#include "mainwindow.h"

#include <QApplication>
//#include <QFile>
#include <QDir>
#include <toolbox.h>
#include <QMessageBox>


#ifdef QT_DEBUG
# define LOG_TO_STDOUT
#endif
int main(int argc, char *argv[])
{
    toolbox::Log log;
 // make sure we can create the Launcher home directory so that we can log to ${HOME}/Launcher/Launcher.log
    QApplication a(argc, argv); // must be created first before we can execute the code below.
    {
        QDir home(Launcher::homePath());
        home.cdUp();
        if( !home.mkpath("Launcher") ) {
            throw_<std::runtime_error>("aiaiaiaiaiaaaaai");
        }

#if defined(QT_DEBUG) && defined(LOG_TO_STDOUT)
        log.set_log("std::cout");
#else
        log.set_log( Launcher::homePath("Launcher.log").toStdString() );
#   ifdef QT_DEBUG
     // always clear the log file
        log.clear();
        log<<"Log file cleared (debug version running).";
#   else
        qint64 const max_size_kB = 512;
        qint64 log_size_kB = QFileInfo( log.filename().c_str() ).size()/1024;
        if( log_size_kB > max_size_kB )
        {
            QString msg = QString("The size of the log file exceeds %1 kB (%2 kB). "
                                  "Unless you want to inspect the log file for "
                                  "troubleshooting, it can safely be cleared."
                                  "\nClear the log file now?")
                             .arg(max_size_kB)
                             .arg(log_size_kB);
            QMessageBox::Button answer = QMessageBox::question(nullptr,"Launcher",msg);
            if( answer==QMessageBox::Yes ) {
                log.clear();
                log("Log file cleared on demand.");
            }
        }
#   endif
#endif

#ifdef QT_DEBUG
        log.set_verbosity(10);
#else
        log.set_verbosity(1);
#endif
        std::string s = std::string("\n================================================================================")
                            .append("\nLauncher started ").append( toolbox::now().toStdString() )
                            .append("\n================================================================================");
        log << s;
    }

    MainWindow w( log );
    w.show();

    return a.exec();
}
