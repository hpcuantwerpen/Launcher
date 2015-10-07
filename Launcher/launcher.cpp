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
    Launcher::~Launcher()
    {
    }
 //-----------------------------------------------------------------------------
    QString Launcher::homePath( QString const& sub )
    {
        QDir home = QDir::home();
        QString root = QDir::cleanPath( home.filePath("Launcher") );
        if( sub.isEmpty() ) {
            return root;
        } else {
            return QDir::cleanPath( QDir(root).filePath(sub) );
        }
    }
 //-----------------------------------------------------------------------------
    void Launcher::readClusterInfo( QString const& clusters_dir )
    {
        QDir dir( clusters_dir, "*.info");
        if( !dir.exists() ) {
            throw_<NoClustersFound>("Directory '%1' does not exist.", clusters_dir );
        }
        QFileInfoList clusters = dir.entryInfoList();
        if( clusters.size()==0 ) {
            throw_<NoClustersFound>("No .info files found in '%1'.", clusters_dir );
        }
        for ( QFileInfoList::const_iterator iter=clusters.cbegin()
            ; iter!=clusters.cend(); ++iter )
        {
            ClusterInfo cluster_i( iter->absoluteFilePath() );
            this->clusters[iter->baseName()] = cluster_i;
        }
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
