#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QDir>

void messageHandler( QtMsgType type, QMessageLogContext const& context, QString const& msg )
{/* You can suppresse the header line with file/line number/calling function by starting the
  * message with a '~'.
  */
    QString s;
    if( msg.startsWith('~') ) { // no header line
        s.append("    ")
         .append( msg.mid(1) );
    } else {                    // create header line
        switch (type) {
        case QtDebugMsg:
            s= QString("\nDebug ");
            break;
        case QtInfoMsg:
            s= QString("\nInfo ");
            break;
        case QtWarningMsg:
            s= QString("\nWarning ");
            break;
        case QtCriticalMsg:
            s= QString("\nCritical ");
            break;
        case QtFatalMsg:
            s= QString("\nFATAL ");
            break;
        }
        s.append(msg);
    }
    QDir().mkpath(Launcher::homePath());
    QString log = Launcher::homePath("Launcher.log");
    QFile outFile(log);
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream ts(&outFile);
    ts << s << endl;
}

int main(int argc, char *argv[])
{
  qInstallMessageHandler(messageHandler);
    qInfo()
        << "\n================================================================================"
        << "\nLauncher started " << toolbox::now().toStdString().c_str()
        << "\n================================================================================"
        ;

    QApplication a(argc, argv);

    MainWindow w;
    w.show();

    return a.exec();
}
