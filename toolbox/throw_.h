#ifndef THROW_H
#define THROW_H

#include <cstdlib>
#include <iostream>

#include <QString>

template<class E> // e.g. std::runtime_error
void throw_( char const* msg ) {
    throw E( msg );
}

template <class E, class A1>
void throw_( char const* msg, A1 const& a1 ) {
    QString errmsg = QString(msg).arg(a1);
    throw_<E>( errmsg.toStdString().c_str() );
}

template <class E, class A1, class A2>
void throw_(char const* msg, A1 const& a1, A2 const& a2 ) {
    QString errmsg = QString(msg).arg(a1).arg(a2);
    throw_<E>( errmsg.toStdString().c_str() );
}

#endif // THROW_H

