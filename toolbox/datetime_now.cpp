#include "datetime_now.h"
#include <ctime>

namespace toolbox
{//-----------------------------------------------------------------------------
    QString now()
    {
        time_t t = time(0);
        struct tm * tm_now = localtime( &t );
        QString qs_now = QString("%1-%2-%3:%4:%5:%6")
                .arg(tm_now->tm_year+1900)
                .arg(tm_now->tm_mon)
                .arg(tm_now->tm_mday)
                .arg(tm_now->tm_hour)
                .arg(tm_now->tm_min)
                .arg(tm_now->tm_sec)
                ;
        return qs_now;
    }
 //-----------------------------------------------------------------------------
}// namespace toolbox
