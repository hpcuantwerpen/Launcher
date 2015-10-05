#include "warn_.h"
#include <iostream>
#include <QtDebug>

void warn_( char const* msg ) {
    qWarning() << msg;
}
