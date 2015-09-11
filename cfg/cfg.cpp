#include "cfg.h"
#include <throw_.h>

#include <QFile>
#include <QIODevice>
#include <QDataStream>

namespace cfg
{//-----------------------------------------------------------------------------
    QString rangeToString( QList<QVariant> const& range, bool add_type = false )
    {
        QString qs = "[";
        qs.append( range.first().toString() );
        if( add_type )
            qs.append( " (").append(range.first().typeName()).append(")");
        for( int i=1; i<range.size(); ++i ) {
            qs.append(", ").append(range[i].toString());
            if( add_type )
                qs.append( " (").append(range[i].typeName()).append(")");
        }
        qs.append("]");
        return qs;
    }
 //-----------------------------------------------------------------------------
    Item::Item(const QString &name)
      : name_(name)
      , choices_is_range_(false)
      , range_type_(QVariant::Invalid)
    {}
 //-----------------------------------------------------------------------------
    Item::Item(Item const& rhs)
      : name_            (rhs.name_            )
      , value_           (rhs.value_           )
      , default_value_   (rhs.default_value_   )
      , choices_         (rhs.choices_         )
      , choices_is_range_(rhs.choices_is_range_)
    {}
 //-----------------------------------------------------------------------------
    bool Item::is_valid(const QVariant &value, bool trow) const
    {
        bool ok = true;
        if( this->choices_is_range_ ) {
            if( !choices_.empty() ) {
                ok = value.type()==this->range_type_
                  && this->choices().first() <= value
                  &&                            value <= this->choices().last();
                if( !ok && trow ) {
                    throw_<std::logic_error>("Value '%1' (%2) is not in %3 range %4."
                                            , value.toString()
                                            , value.typeName()
                                            , this->choices().first().typeName()
                                            , rangeToString( this->choices() )
                                            );
                }
            }
        } else {
            if( !choices_.empty() ) {
                ok = this->choices().contains(value);
                if( !ok && trow ) {
                    throw_<std::logic_error>("Value '%1' is not a valid choice. Valid choices are: %2."
                                            , value.toString()
                                            , rangeToString( this->choices() )
                                            );
                }
            } else {
                if( range_type_!=QVariant::Invalid ) {
                    ok = (value.type() == range_type_);
                    if( !ok && trow ) {
                        throw_<std::logic_error>("Value '%1' is not a valid choice. Valid choices are of type %2."
                                                , value.toString()
                                                , QVariant::typeToName(this->range_type_)
                                                );
                    }
                } else {// no type prescribed - anything goes
                    ok = true;
                }
            }
        }
        return ok;
    }
 //-----------------------------------------------------------------------------
    void Item::set_choices
      ( QList<QVariant> const& choices
      , bool                   is_range
      )
    {
        if( is_range )
        {// validate the range
            if( choices.size() > 2 ) {
                throw_<std::logic_error>("Ranges of more than 2 elements are invalid: range is: %1."
                                        , rangeToString( choices )
                                        );
            }
            if( choices.first().typeName() != choices.last().typeName() ) {
                throw_<std::logic_error>("The elements of a range must be of the same type: range is: %1."
                                        , rangeToString( choices )
                                        );
            }
            QVariant::Type range_type = choices.first().type();

            if( range_type != QVariant::Int
             && range_type != QVariant::Double ) {
                throw_<std::logic_error>("The elements of a range must be of type int or double: range is: %1."
                                        , rangeToString( choices )
                                        );
            }
            if( choices.first() > choices.last() ) {
                throw_<std::logic_error>("Empty range (the first element is larger than the last): range is: %1."
                                        , rangeToString( choices )
                                        );
            }
            this->range_type_ = range_type;
        } else {
            if( choices.size() > 0 ) {
                QVariant::Type range_type = choices.first().type();
                bool choices_of_same_type = true;
                for( int i=1; i<choices.size(); ++i ) {
                    if( choices[i].type() != range_type )
                        choices_of_same_type = false;
                }
                this->range_type_ = ( choices_of_same_type ? range_type : QVariant::Invalid );
            } else {
                if( this->value() != QVariant() )
                    this->range_type_ = QVariant::Invalid;
            }
        }
    // initialize *this
       this->choices_is_range_ = is_range;
       this->choices_ = choices;
    // adjust default value
       if( (this->default_value_==QVariant()) || (!this->is_valid(this->default_value_)) )
       {// there is no current default value or it is invalid
           if( choices.size() > 0 )
               this->default_value_ = choices.at(0);
       }
    // adjust value
//       if( (this->value_==QVariant()) || (!this->is_valid(this->value_)) )
//       {// there is no current value or it is invalid
//           this->value_ = this->default_value_;
//       }
    }
 //-----------------------------------------------------------------------------
    void  Item::set_choices
      ( QStringList const& choices
      , bool              is_range
      )
    {
        Choices_t choices2;
        for( int i=0; i<choices.size(); ++i ) {
            choices2.append( choices[i] );
        }
        set_choices( choices2, is_range );
    }
 //-----------------------------------------------------------------------------
    void Item::set_default_value(QVariant const& default_value)
    {
        if( this->is_valid(default_value,true) ) {
            this->default_value_ = default_value;
        }
//        if( this->value_==QVariant() ) {
//            this->value_ = this->default_value_;
//        } else if( !this->is_valid(this->value_) ) {
//            this->value_ = this->default_value_;
//        }
    }
 //-----------------------------------------------------------------------------
    QVariant const& Item::default_value() const {
        if( this->default_value_==QVariant() )
            if( !this->choices_.empty() )
                const_cast<Item*>(this)->default_value_ = this->choices().at(0);
        return this->default_value_;
    }
 //-----------------------------------------------------------------------------
   void Item::set_value( QVariant const& value )
   {
       if( value==QVariant() ){
           this->value_ = this->default_value();
       } else {
           if( this->is_valid(value,true) ) {
               this->value_ = value;
           }
       }
   }
 //-----------------------------------------------------------------------------
   void Item::set_value_and_type( QVariant const& value )
   {
       QVariant::Type type = value.type();
       if( this->range_type_==QVariant::Invalid )
           this->range_type_ = type;
       if( this->range_type_!= type ) {
           throw_<std::logic_error>("Cannot set type to %1. Item '%2' has already a type (%3)."
                                   , QVariant::typeToName(type)
                                   , this->name()
                                   , QVariant::typeToName(this->range_type_)
                                   );
       }
       this->set_value( value );
   }
 //-----------------------------------------------------------------------------
   QVariant const& Item::value() const {
       if( this->value_==QVariant() )
           return this->default_value();
       return this->value_;
   }
 //-----------------------------------------------------------------------------
    QDataStream &operator<<(QDataStream& ds, Item const& item)
    {// writing
        ds << item.name() << item.value() << item.default_value() << item.choices() << item.choices_is_range();
        return ds;
    }
 //-----------------------------------------------------------------------------
    QDataStream &operator>>(QDataStream& ds, Item & item)
    {
        QString         name;
        QVariant        value, default_value;
        QList<QVariant> choices;
        bool            choices_is_range;

        ds >> name >> value >> default_value >> choices >> choices_is_range;

        item.set_name(name);
        item.set_choices(choices,choices_is_range);
        item.set_default_value(default_value);
        item.set_value(value);

        return ds;
    }
 //-----------------------------------------------------------------------------
 // Config
 //-----------------------------------------------------------------------------
    void Config::setFilename( QString const& filename ) const
    {
        if( filename.isEmpty() ) {
            if( this->filename_.isEmpty() ) {
                throw_<std::runtime_error>("Filename missing.");
            }
        } else {
            this->filename_ = filename;
        }
    }
 //-----------------------------------------------------------------------------
    void Config::load( QString const& filename )
    {
        this->setFilename( filename );
        QFile file(this->filename_);
        file.open( QIODevice::ReadOnly );
        QDataStream ds(&file);
        ds >> *this;
    }
 //-----------------------------------------------------------------------------
    void Config::save( QString const& filename) const
    {
        this->setFilename( filename );
        QFile file(this->filename_);
        file.open( QIODevice::WriteOnly | QIODevice::Truncate );
        QDataStream ds(&file);
        ds << *this;
    }
 //-----------------------------------------------------------------------------
    void Config::addItem( Item const& item )
    {
        if( item.name().isEmpty() ) {
            throw_<std::logic_error>("Cannot add cfg::Item object with empty name.");
        }
        (*this)[item.name()] = item;
    }
 //-----------------------------------------------------------------------------
    Item* Config::getItem( QString const& itemName )
    {
        if( !this->contains( itemName ) ) {
            this->addItem( Item(itemName) );
        }
        return &(*( this->find(itemName) ));
    }
 //-----------------------------------------------------------------------------
}// namespace cfg

