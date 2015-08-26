#include "orderedmap.h"

#include <stdexcept>
#include "throw_.h"

namespace toolbox
{//-----------------------------------------------------------------------------
 // OrderedMap
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
    bool
    OrderedMap::
    contains( QString const& key ) const
    {
        int i = this->index(key);
        return i != this->not_found;
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
    OrderedMap::
    add( OrderedMap const & om_rhs )
    {
        std::for_each( om_rhs.cbegin(), om_rhs.cend(),
                      [&] ( OrderedMapBase_t::value_type const& v )
                      {   (*this)[v.first] = v.second;
                      }
                     );
        return om_rhs.size();
    }
 //-----------------------------------------------------------------------------
    int
    OrderedMap::
    remove( OrderedMap const & om_rhs )
    {
        int removed = 0;
        std::for_each( om_rhs.cbegin(), om_rhs.cend(),
                      [&] ( OrderedMapBase_t::value_type const& v )
                      {   int iv = this->index(v.first);
                          if( iv != this->not_found ) {
                              this->removeAt(iv);
                              ++removed;
                          }
                      }
                     );
        return removed;
    }
 //-----------------------------------------------------------------------------
 // OrderedSet
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
    int
    OrderedSet::
    add( OrderedSet const& os_rhs )
    {
        int added = 0;
        std::for_each( os_rhs.cbegin(), os_rhs.cend(),
                       [&] ( OrderedSetBase_t::value_type const& feature )
                       {   int i = this->index(feature) ;
                           if( i != this->not_found ) {
                               this->append(feature);
                               ++added;
                           }
                        }
                     );
        return added;
    }
 //-----------------------------------------------------------------------------
    int
    OrderedSet::
    remove( OrderedSet const& os_rhs )
    {
        int removed = 0;
        std::for_each( os_rhs.cbegin(), os_rhs.cend(),
                       [&] ( OrderedSetBase_t::value_type const& feature )
                       {   int i = this->index(feature) ;
                           if( i != this->not_found ) {
                               this->removeAt(i);
                               ++removed;
                           }
                        }
                     );
        return removed;
    }
 //-----------------------------------------------------------------------------
}// namespace toolbox
