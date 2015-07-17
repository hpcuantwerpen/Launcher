#include "orderedmap.h"

#include <stdexcept>
#include "throw_.h"

namespace toolbox
{//-----------------------------------------------------------------------------
    typedef QVector< QPair<QString,QString> > OrderedMapBase_t;
 //-----------------------------------------------------------------------------
    int
    OrderedMap::
    index( QString const& key ) const
    {
        int i = 0;
        int size = this->size();
        for( ; i<size; ++i ) {
            if( key == this->at(i).first )
                return i;
        }
        return this->not_found; // key not found
    }
//-----------------------------------------------------------------------------
    QString const&
    OrderedMap::
    operator[]( QString const& key ) const
    {
        int i = this->index(key);
        if( i==this->not_found)
            throw_<std::runtime_error>("key error: %1",key);
        return this->at(i).second;
    }
 //-----------------------------------------------------------------------------
    QString&
    OrderedMap::
    operator[]( QString const& key )
    {
        int i = this->index(key);
        if( i==this->not_found) {
            this->append( OrderedMapBase_t::value_type(key,QString()) );
            return this->last().second;
        } else {
            return (*this)[i].second;
        }
    }
 //-----------------------------------------------------------------------------
    int
    OrderedSet::
    index( QString const & value ) const
    {
        int i = 0;
        int size = this->size();
        for( ; i<size; ++i ) {
            if( value == (*this)[i] )
                return i;
        }
        return this->not_found; // key not found
    }
 //-----------------------------------------------------------------------------
    void
    OrderedSet::
    append(QString const& value )
    {
        int i = this->index(value);
        if( i==this->not_found )
            OrderedSetBase_t::append(value);
    }
 //-----------------------------------------------------------------------------
}// namespace toolbox
