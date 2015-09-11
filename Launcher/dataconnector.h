#ifndef DATACONNECTOR
#define DATACONNECTOR

#include <tuple>

#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>

#include <cfg.h>
#include <jobscript.h>

namespace dc2
{
    class DataConnectorBase
    {
    public:
        DataConnectorBase( QWidget* qw, QString const& configItemName, QString const& scriptFlag )
          : qw_(qw)
          , configItemName__(configItemName)
          , scriptFlag__(scriptFlag)
        {
            this->configItemName_ = ( configItemName.isEmpty() ? nullptr : &this->configItemName__ );
            this->scriptFlag_     = ( scriptFlag    .isEmpty() ? nullptr :&this->scriptFlag__      );
        }
        cfg::Item* configItem() {
            return config->getItem(*this->configItemName_);
        }
        virtual void config_to_widget()=0;
        virtual void widget_to_config()=0;
        virtual void config_to_script()=0;
        virtual void script_to_config()=0;

        static cfg::Config* config;
        static pbs::Script* script;
    protected:
        QWidget* qw_;
        QString*configItemName_;
        QString*scriptFlag_;
    private:
        QString configItemName__;
        QString scriptFlag__;
    };

    template <class W>
    struct WidgetAccessor
    {};

    template<> struct WidgetAccessor<QComboBox>
    {
        typedef QComboBox Widget_t;
        typedef QString   Value_t;
        typedef QStringList Choices_t;

        static   Value_t get_value  ( Widget_t* w );
        static      void set_value  ( Widget_t* w, Value_t  const & value );
        static Choices_t get_choices( Widget_t* w );
        static      void set_choices( Widget_t* w, Choices_t const& list  );

        static Value_t toValue( QVariant const& qv ) { return qv.toString(); }
    };
    template<> struct WidgetAccessor<QSpinBox>
    {
        typedef QSpinBox                Widget_t;
        typedef int                     Value_t;
        typedef std::tuple<int,int,int> Choices_t;

        static   Value_t get_value  ( Widget_t* w );
        static      void set_value  ( Widget_t* w, Value_t  const & value );
        static Choices_t get_choices( Widget_t* w );
        static      void set_choices( Widget_t* w, Choices_t const& list  );

        static Value_t toValue( QVariant const& qv ) { return qv.toInt(); }
    };
    template<> struct WidgetAccessor<QDoubleSpinBox>
    {
        typedef QDoubleSpinBox                   Widget_t;
        typedef double                           Value_t;
        typedef std::tuple<double,double,double> Choices_t;

        static   Value_t get_value  ( Widget_t* w );
        static      void set_value  ( Widget_t* w, Value_t  const & value );
        static Choices_t get_choices( Widget_t* w );
        static      void set_choices( Widget_t* w, Choices_t const& list  );

        static Value_t toValue( QVariant const& qv ) { return qv.toDouble(); }
    };
    template<> struct WidgetAccessor<QLineEdit>
    {
        typedef QLineEdit            Widget_t;
        typedef QString              Value_t;
        typedef cfg::Item::Choices_t Choices_t;

        static   Value_t get_value  ( Widget_t* w );
        static      void set_value  ( Widget_t* w, Value_t  const & value );
        static Choices_t get_choices( Widget_t* w )                         { return Choices_t(); }
        static      void set_choices( Widget_t* w, Choices_t const& list  ) {}

        static Value_t toValue( QVariant const& qv ) { return qv.toString(); }
    };
    template<> struct WidgetAccessor<QWidget>
    {
        typedef QWidget              Widget_t;
        typedef QString              Value_t;
        typedef cfg::Item::Choices_t Choices_t;

        static   Value_t get_value  ( Widget_t* w )                         { return Value_t(); }
        static      void set_value  ( Widget_t* w, Value_t  const & value ) {}
        static Choices_t get_choices( Widget_t* w )                         { return Choices_t(); }
        static      void set_choices( Widget_t* w, Choices_t const& list  ) {}

        static Value_t toValue( QVariant const& qv ) { return Value_t(); }
    };


           // dummies for no widget. theses are not actually called. their only purpose is to satisfy the compiler..
    QString get_value( QWidget* w );
       void set_value( QWidget* w, QString const & value );

    QStringList get_choices( QWidget* w );
           void set_choices( QWidget* w, QStringList const& list );

    template<class W>
    class DataConnector : public DataConnectorBase
    {
        typedef WidgetAccessor<W> wa;
    public:
        DataConnector( W* qw, QString const& configItemName, QString const& scriptFlag )
          : DataConnectorBase( qw, configItemName, scriptFlag )
        {}
        W* widget() {
            return dynamic_cast<W*>(this->qw_);
        }
        virtual void config_to_widget();
        virtual void widget_to_config();
        virtual void config_to_script();
        virtual void script_to_config();
    };

    //QStringList makeStringList( cfg::Item::Choices_t const & choices ) { return QStringList(); }

    template <class W>
    void
    DataConnector<W>::
    config_to_widget()
    {
        if( !this->qw_ || !this->configItemName_ ) return;

        cfg::Item* config_item = this->configItem();
        wa::set_choices( this->widget(), config_item->choices() );
        typename wa::Value_t v = wa::toValue( config_item->value() );
        wa::set_value  ( this->widget(), v );
    }

    template <class W>
    void
    DataConnector<W>::
    widget_to_config()
    {
        if( !this->qw_ || !this->configItemName_ ) return;

        cfg::Item* config_item = this->configItem();
        typename wa::Choices_t c = wa::get_choices( this->widget() );
        typename wa::Value_t   v = wa::get_value  ( this->widget() );
        config_item->set_choices( c );
        config_item->set_value  ( v );
    }

    template <class W>
    void
    DataConnector<W>::
    config_to_script()
    {
        if( !this->qw_ || !this->scriptFlag_ ) return;

        cfg::Item* config_item = this->configItem();
        typename wa::Value_t v = wa::get_value( this->widget() );
        (*this->script)[*this->scriptFlag_] = v;
    }

    template <class W>
    void
    DataConnector<W>::
    script_to_config()
    {
        if( !this->qw_ || !this->scriptFlag_ ) return;
    }



    template <class W>
    DataConnectorBase* newDataConnector( W* qw, QString const& configItemName, QString const& scriptFlag )
    {
        return new DataConnector<W>( qw, configItemName, scriptFlag );
    }

    DataConnectorBase* newDataConnector( QString const& configItemName, QString const& scriptFlag )
    {
        return new DataConnector<QWidget>( nullptr, configItemName, scriptFlag );
    }


}// namespace dc2

#endif // DATACONNECTOR

