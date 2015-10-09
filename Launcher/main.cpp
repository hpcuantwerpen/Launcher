#include "mainwindow.h"

#include <QApplication>
//#include <QFile>
#include <QDir>
#include <toolbox.h>


int main(int argc, char *argv[])
{
 // make sure we can create the Launcher home directory so that we can log to ${HOME}/Launcher/Launcher.log
    {
        QDir home(Launcher::homePath());
        home.cdUp();
        if( !home.mkpath("Launcher") ) {
            throw_<std::runtime_error>("aiaiaiaiaiaaaaai");
        }
        Log::filename = Launcher::homePath("Launcher.log").toStdString();
        Log()
            << "\n================================================================================"
            << "\nLauncher started " << toolbox::now().toStdString().c_str()
            << "\n================================================================================"
            ;
    }
    QApplication a(argc, argv);

    MainWindow w;
    w.show();

    return a.exec();
}
