#ifndef ORDEREDMAP_H
#define ORDEREDMAP_H

#include <QVector>
#include <QPair>
#include <QString>

#pragma message("orderedmap.h")
namespace toolbox
{//=============================================================================
    typedef QVector< QPair<QString,QString> > OrderedMapBase_t;
 //=============================================================================
    class OrderedMap : public OrderedMapBase_t
 // A QVector<QPair<QString,QString>> which also behaves as a map (key based
 // access}.
 //=============================================================================
    {
    public:
        QString const& operator[]( QString const& key ) const;
        QString      & operator[]( QString const& key );
        int            index     ( QString const& key ) const;
        using OrderedMapBase_t::operator[];
    private:
        enum {not_found = -1};
    };
 //=============================================================================

    typedef QVector<QString> OrderedSetBase_t;
 //=============================================================================
    class OrderedSet : public OrderedSetBase_t
 // A QVector<QString> which also behaves as a set (value can be added only once).
 //=============================================================================
    {
    public:
        void append( QString const& value );
        int index  ( QString const& value ) const;
    private:
        enum {not_found = -1};
    };
 //=============================================================================
}// namespace toolbox

#endif // ORDEREDMAP_H
