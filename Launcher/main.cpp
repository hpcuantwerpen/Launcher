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
        s.append( msg.mid(1) );
    } else {                    // create header line
        QString format;
        switch (type) {
        case QtDebugMsg:
            format = QString("\nDebug [%1,%2,%3]:\n");
            break;
        case QtInfoMsg:
            format = QString("\nInfo [%1,%2,%3]:\n");
            break;
        case QtWarningMsg:
            format = QString("\nWarning [%1,%2,%3]:\n");
            break;
        case QtCriticalMsg:
            format = QString("\nCritical [%1,%2,%3]:\n");
            break;
        case QtFatalMsg:
            format = QString("\nFATAL [%1,%2,%3]:\n");
            break;
        }
        s = format.arg(context.file).arg(context.line).arg(context.function);
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
