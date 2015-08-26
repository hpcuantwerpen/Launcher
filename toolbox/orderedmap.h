#ifndef ORDEREDMAP_H
#define ORDEREDMAP_H

#include <QVector>
#include <QPair>
#include <QString>

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
        bool           contains  ( QString const& key ) const;
        using OrderedMapBase_t::operator[];
        enum {not_found = -1};
        int add( OrderedMap const & om_rhs );
         // add all parameters in om_rhs to *this (just like the python
         // dictionary update method)
         // returns the number of changed entries
        int remove( OrderedMap const & om_rhs );
         // remove all parameters in om_rhs from *this
         // returns the number of removed entries
    };
 //=============================================================================

    typedef QVector<QString> OrderedSetBase_t;
 //=============================================================================
    class OrderedSet : public OrderedSetBase_t
 // A QVector<QString> which also behaves as a set (value can be added only once).
 //=============================================================================
    {
    public:
        int index  ( QString const& value ) const;
        enum {not_found = -1};
        int add( OrderedSet const& os_rhs );
         // append all features in os_rhs to *this
         // returns the number of added entries
        int remove( OrderedSet const& os_rhs );
         // remove all features in os_rhs from *this
         // returns the number of removed entries
    };
 //=============================================================================
}// namespace toolbox

#endif // ORDEREDMAP_H
