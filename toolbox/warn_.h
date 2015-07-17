#ifndef WARN_H
#define WARN_H

#include <QString>

void warn_( char const* msg );

template <class A1>
void warn_( char const* msg, A1 const& a1 ) {
    QString errmsg = QString(msg).arg(a1);
    warn_( errmsg.toStdString().c_str() );
}

template <class A1, class A2>
void warn_(char const* msg, A1 const& a1, A2 const& a2 ) {
    QString errmsg = QString(msg).arg(a1).arg(a2);
    warn_( errmsg.toStdString().c_str() );
}

template <class A1, class A2, class A3>
void warn_(char const* msg, A1 const& a1, A2 const& a2, A3 const& a3 ) {
    QString errmsg = QString(msg).arg(a1).arg(a2).arg(a3);
    warn_( errmsg.toStdString().c_str() );
}
#endif // WARN_H

