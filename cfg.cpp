#include "cfg.h"

#include <QFile>
#include <QIODevice>
#include <QDataStream>

#include <exception>

namespace cfg
{//-----------------------------------------------------------------------------
    Item::Item() {}
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
                ok = this->choices().first() <= value;
                ok&=                            value <= this->choices().last();
            }
        } else {
            ok = this->choices().contains(value);
        }
        if( trow ) {
            throw std::exception();
        }
        return ok;
    }
 //-----------------------------------------------------------------------------
    void Item::set_choices
      ( QList<QVariant> const& choices
      , bool                   is_range
      )
    {
        this->choices_ = choices;
        if( choices.empty() ) {
            this->choices_is_range_ = false;
        } else {
            this->choices_is_range_ = is_range;
            this->default_value_ = choices.at(0);
        }
    }
 //-----------------------------------------------------------------------------
    void Item::set_default_value(QVariant const& default_value)
    {
        if( this->is_valid(default_value,true) ) {
            this->default_value_ = default_value;
        }
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
}

