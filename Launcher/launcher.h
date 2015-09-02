#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>

#include <cfg.h>

#include "clusterinfo.h"

 //=============================================================================
    class Launcher
 //=============================================================================
    {
    public:
        Launcher();
        void readClusterInfo();
        static QString homePath( QString const& sub1 = QString() );

    public: // data
        cfg::Config_t config;
        QMap<QString,ClusterInfo> clusters;
//        bool isConstructingMainWindow;
    private:
    };

 //=============================================================================


#endif // LAUNCHER_H
