#include "launcher.h"
#include <toolbox.h>

#include <QProcessEnvironment>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QMap>

 //-----------------------------------------------------------------------------
 // Launcher
 //-----------------------------------------------------------------------------
    Launcher::Launcher()
    {

    }
 //-----------------------------------------------------------------------------
    QString Launcher::homePath( QString const& sub )
    {
        QString home = QProcessEnvironment::systemEnvironment().value("HOME");
        QString root = QDir::cleanPath( QDir(home).filePath("Launcher_pp") );
        if( sub.isEmpty() ) {
            return root;
        } else {
            return QDir::cleanPath( QDir(root).filePath(sub) );
        }
    }
 //-----------------------------------------------------------------------------
    void Launcher::readClusterInfo()
    {
        QString pwd = QDir::currentPath();
        QDir dir( Launcher::homePath("clusters"), "*.info");
        QStringList clusters = dir.entryList();
        QDir::setCurrent( dir.absolutePath() );
        for (int i=0; i<clusters.size(); ++i) {
            ClusterInfo cluster_i( clusters[i] );
            this->clusters[cluster_i.name()] = cluster_i;
        }
        QDir::setCurrent( pwd );
    }
 //-----------------------------------------------------------------------------
    void Launcher::modifyScript( NodesetInfo const& nodeset )
    {
        if( this->script["nodes"].toInt() !=          nodeset.granted().nNodes ) {
            this->script["nodes"] = QString().setNum( nodeset.granted().nNodes );
        }
        if( this->script["ppn"].toInt() !=          nodeset.granted().nCoresPerNode ) {
            this->script["ppn"] = QString().setNum( nodeset.granted().nCoresPerNode );
        }
        std::cout << this->script.text().toStdString() <<std::endl;
    }
