#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>

#include <cfg.h>
#include <jobscript.h>

#include "clusterinfo.h"

 //=============================================================================
    class Launcher
 //=============================================================================
    {
    public:
        Launcher();
        ~Launcher();
        void readClusterInfo( QString const& clusters_dir );
        static QString homePath( QString const& sub1 = QString() );

        void modifyScript( NodesetInfo const& nodeset );

    public: // data
        typedef QMap<QString,ClusterInfo> Clusters_t;

        cfg::Config config;
        Clusters_t  clusters;
        pbs::Script script;
    private:
    };

 //=============================================================================


#endif // LAUNCHER_H
