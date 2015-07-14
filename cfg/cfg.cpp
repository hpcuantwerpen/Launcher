#include "cfg.h"

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
    Item::Item(const QString &name) : name_(name) {}
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
                    QString qs = "Value ";
                    qs.append( value.toString() )
                      .append(" (")
                      .append( value.typeName() )
                      .append(") is not in ")
                      .append( this->choices().first().typeName() )
                      .append(" range ")
                      .append( rangeToString(this->choices()) )
                      .append(".\n")
                      ;
                    throw std::logic_error(qs.toStdString());
                }
            }
        } else {
            if( !choices_.empty() ) {
                ok = this->choices().contains(value);
                if( !ok && trow ) {
                    QString qs = "Value ";
                    qs.append( value.toString() )
                      .append(" is not a valid choice. Valid choices are: ")
                      .append( rangeToString( this->choices() ) )
                      .append(".\n")
                      ;
                    throw std::logic_error(qs.toStdString());
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
                QString qs = "Ranges of more than 2 elements are invalid: range is ";
                qs.append( rangeToString( choices ) ).append(".\n");
                throw std::logic_error(qs.toStdString());
            }
            if( choices.first().typeName() != choices.last().typeName() ) {
                QString qs = "The elements of a range must be of the same type: range is ";
                qs.append( rangeToString( choices, true ) ).append(".\n");
                throw std::logic_error(qs.toStdString());
            }
            QVariant::Type range_type = choices.first().type();
            if( range_type != QVariant::Int
             && range_type != QVariant::Double ) {
                QString qs = "The elements of a range must be of type int or double: range is ";
                qs.append( rangeToString( choices, true ) ).append(".\n");
                throw std::logic_error(qs.toStdString());
            }
            if( choices.first() > choices.last() ) {
                QString qs = "Empty range (the first element is larger than the last): range is ";
                qs.append( rangeToString( choices ) ).append(".\n");
                throw std::logic_error(qs.toStdString());
            }
            this->range_type_ = range_type;
        } else {
            this->range_type_ = QVariant::Invalid;
        }
    // initialize *this
       this->choices_is_range_ = is_range;
       this->choices_ = choices;
    // adjust default value
       if( (this->default_value_==QVariant()) || (!this->is_valid(this->default_value_)) )
       {// there is no current default value or it is invalid
           this->default_value_ = choices.at(0);
       }
    // adjust value
       if( (this->value_==QVariant()) || (!this->is_valid(this->value_)) )
       {// there is no current value or it is invalid
           this->value_ = this->default_value_;
       }
    }
 //-----------------------------------------------------------------------------
    void Item::set_default_value(QVariant const& default_value)
    {
        if( this->is_valid(default_value,true) ) {
            this->default_value_ = default_value;
        }
        if( this->value_==QVariant() ) {
            this->value_ = this->default_value_;
        } else if( !this->is_valid(this->value_) ) {
            this->value_ = this->default_value_;
        }
    }
 //-----------------------------------------------------------------------------
    QVariant const& Item::default_value() const {
        if( this->default_value_==QVariant() )
            if( !this->choices_.empty() )
                const_cast<Item*>(this)->default_value_ = this->choices().at(0);
        return this->default_value_;
    }
 //-----------------------------------------------------------------------------
   void Item::set_value(QVariant const& value)
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
   QVariant const& Item::value() const {
       if( this->value_==QVariant() )
           const_cast<Item*>(this)->value_ = this->default_value();
       return this->value_;
   }
 //-----------------------------------------------------------------------------
    QDataStream &operator<<(QDataStream& ds, Item const& item)
    {// writing
        ds << item.name() << item.value() << item.default_value() << item.choices() << item.choices_is_range();
        return ds;
    }
 //-----------------------------------------------------------------------------
    QDataStream &operator>>(QDataStream& ds, Item      & item)
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
    void load(QString const& filename, Config_t& config)
    {
        QFile file(filename);
        file.open(QIODevice::ReadOnly);
        QDataStream ds(&file);
        ds >> config;
    }
 //-----------------------------------------------------------------------------
    void save(QString const& filename, Config_t const& config)
    {
        QFile file(filename);
        file.open(QIODevice::WriteOnly|QIODevice::Truncate);
        QDataStream ds(&file);
        ds << config;
    }
 //-----------------------------------------------------------------------------
}// namespace cfg

