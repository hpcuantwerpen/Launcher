#ifndef CFG
#define CFG

#include <QMap>
#include <QString>
#include <QVariant>
#include <QList>

#include <stdexcept>
#include <throw_.h>

namespace cfg
{//=============================================================================
    class Item
 //=============================================================================
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
        bool isInitialized() const {
            return value_.type() != QVariant::Invalid;
        }

     // value
        QVariant const& value() const;
        bool set_value( QVariant const& value = QVariant() );
        void set_value_and_type( QVariant const& value = QVariant() );

     // default_value
        QVariant const& default_value() const;
        void set_default_value( QVariant const& default_value = QVariant() );

     // choices
        typedef QList<QVariant> Choices_t;
        Choices_t const& choices() const {
           return this->choices_;
        }
        QStringList choices_as_QStringList() const;

        void set_choices( Choices_t   const& choices = Choices_t(), bool is_range = false );
        void set_choices( QStringList const& choices              , bool is_range = false );
        template <class T>
        void set_choices( QList<T>    const& choices              , bool is_range = false ) {
            Choices_t choices2;
            for( int i=0; i<choices.size(); ++i ) {
                choices2.append( choices[i] );
            }
            set_choices( choices2, is_range );
        }

       bool choices_is_range() const {
           return this->choices_is_range_;
       }
       QVariant::Type type() const {
           return this->range_type_;
       }

    // test validity of a value
       bool is_valid(QVariant const& value, bool must_throw = false ) const;

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
    class Config : public QMap<QString,Item>
 //=============================================================================
    {
    public:
        Config() {}
        virtual ~Config() {}
        void setFilename( QString const & filename ) const;
        QString filename() const { return filename_; }
        void addItem( Item const& item );
        Item* getItem( QString const& itemName );
         // creates the item if it does not exist
        void load( QString const& filename );
        void save( QString const& filename=QString() ) const;
    private:
        mutable QString filename_;
    };
 //=============================================================================
    template <class T>
    QVariant
    cfg_get
      ( Config  const& config
      , QString const& key
      , T       const& default_value )
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
      ( Config  const& config
      , QString const& key
      , T       const& default_value
      )
    {
        return cfg_get(config,key,default_value).toString();
    }
 //=============================================================================
    SUBCLASS_EXCEPTION( InvalidConfigItemValue , std::runtime_error )
 //=============================================================================
}// namespace cfg

#endif // CFG

