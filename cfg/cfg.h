#ifndef CFG
#define CFG

#include <QMap>
#include <QString>
#include <QVariant>
#include <QList>

#include <stdexcept>

namespace cfg
{//=============================================================================
    class Item;
 //=============================================================================
    typedef QMap<QString,Item> Config_t;
 //=============================================================================
    class Item
    {
        Q_PROPERTY(QString         name_             READ name                                 )
        Q_PROPERTY(QVariant        value_            READ value         WRITE set_value        )
        Q_PROPERTY(QVariant        default_value_    READ default_value WRITE set_default_value)
        Q_PROPERTY(QList<QVariant> choices_          READ choices       WRITE set_choices      )
        Q_PROPERTY(bool            choices_is_range_                                           )
    public:
        explicit Item(QString const& name = QString());
        Item(Item const& rhs);
        //virtual ~Item() {}

     // accessor functions:
     // name
        QString const& name() const {
            return this->name_;
        }
        void set_name(QString const& name) {
            this->name_ = name;
        }

     // value
        QVariant const& value() const;
        void set_value(QVariant const& value = QVariant());

     // default_value
        QVariant const& default_value() const;
        void set_default_value(QVariant const& default_value = QVariant());

     // choices
        typedef QList<QVariant> Choices_t;
        Choices_t const& choices() const {
           return this->choices_;
        }
        void set_choices( Choices_t   const& choices = Choices_t(), bool is_range = false );
        void set_choices( QStringList const& choices              , bool is_range = false );
        template <class T>
        void set_choices( QList<T>    const& choices              , bool is_range = false
          ) {
            Choices_t choices2;
            for( int i=0; i<choices.size(); ++i ) {
                choices2.append( choices[i] );
            }
            set_choices( choices2, is_range );
        }

       bool choices_is_range() const {
           return this->choices_is_range_;
       }

    // test validity of a value
       bool is_valid(QVariant const& value, bool trow = false ) const;

    private:
        QString         name_;              // item name
        QVariant        value_;             // item current value
        QVariant        default_value_;     // item default value
        QList<QVariant> choices_;           // list or range of allowed values
        bool            choices_is_range_;  // true if choices_ represents a range, false otherwise
        QVariant::Type  range_type_;        // the type of the range/list (int, float, string)
    };
 //=============================================================================
    QDataStream &operator<<( QDataStream& ds, Item const& item );
    QDataStream &operator>>( QDataStream& ds, Item      & item );
 //=============================================================================
    void load( Config_t      & config, QString const& filename );
    void save( Config_t const& config, QString const& filename );
 //=============================================================================
    template <class T>
    QVariant
    cfg_get
      ( Config_t const& config
      , QString  const& key
      , T        const& default_value )
    {
        if (config.contains(key) ) {
            return config[key].value();
        } else {
            QVariant q(default_value);
            return q;
        }
    }
 //=============================================================================
    template <class T>
    QString
    cfg_get_str
      ( Config_t const& config
      , QString  const& key
      , T        const& default_value )
    {
        return cfg_get(config,key,default_value).toString();
    }
 //=============================================================================
}// namespace cfg

#endif // CFG

