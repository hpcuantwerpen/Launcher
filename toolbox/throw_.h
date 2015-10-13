#ifndef THROW_H
#define THROW_H

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <QString>

// preprocessor macro to subclass an exception class
#define SUBCLASS_EXCEPTION(DERIVED,BASE)                \
    struct DERIVED : public BASE {                      \
        DERIVED( char const* what ) : BASE( what ) {}   \
    };

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

template <class E, class A1, class A2, class A3>
void throw_(char const* msg, A1 const& a1, A2 const& a2, A3 const& a3 ) {
    QString errmsg = QString(msg).arg(a1).arg(a2).arg(a3);
    throw_<E>( errmsg.toStdString().c_str() );
}

template <class E, class A1, class A2, class A3, class A4>
void throw_(char const* msg, A1 const& a1, A2 const& a2, A3 const& a3, A4 const& a4 ) {
    QString errmsg = QString(msg).arg(a1).arg(a2).arg(a3).arg(a4);
    throw_<E>( errmsg.toStdString().c_str() );
}
#endif // THROW_H

