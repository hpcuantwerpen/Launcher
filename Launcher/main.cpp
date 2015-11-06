#include "mainwindow.h"

#include <QApplication>
//#include <QFile>
#include <QDir>
#include <toolbox.h>
#include <qmessagebox.h>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
 // make sure we can create the Launcher home directory so that we can log to ${HOME}/Launcher/Launcher.log
    {
        QDir home(Launcher::homePath());
        home.cdUp();
        if( !home.mkpath("Launcher") ) {
            throw_<std::runtime_error>("aiaiaiaiaiaaaaai");
        }


        Log::filename = Launcher::homePath("Launcher.log").toStdString();
#ifdef QT_DEBUG
     // always clear the log file
        Log::clear();
        Log() << "Log file cleared (debug version running).";
#else
        qint64 const max_size_kB = 512;
        qint64 log_size_kB = Log::log_size()/1024;
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
                Log::clear();
                Log() << "Log file cleared.";
            }
        }
#endif
        Log()
            << "\n================================================================================"
            << "\nLauncher started " << toolbox::now().toStdString().c_str()
            << "\n================================================================================"
            ;
    }

    MainWindow w;
    w.show();

    return a.exec();
}
