#ifndef DATACONNECTOR
#define DATACONNECTOR

#include <tuple>

#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QCheckBox>

#include <cfg.h>
#include <jobscript.h>

namespace dc
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

        QString const& name() {
            return configItemName__;
        }

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


 // generic version, specialisations for V = QString, double, int, bool
    template <class V>
    V toValue( QVariant const& qv ) {
        throw_<std::logic_error>("Not Implemented toValue<V>.");
    }
    template <> QString toValue( QVariant const& qv );
    template <> double  toValue( QVariant const& qv );
    template <> int     toValue( QVariant const& qv );
    template <> bool    toValue( QVariant const& qv );

    typedef QList<QVariant> Choices_t;

    // generic version, specialisations for W = QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit, QCheckBox, QWidget
    template <class W> struct WidgetAccessor {};

    template<> struct WidgetAccessor<QComboBox>
    {
        typedef QComboBox       Widget_t;
        typedef QString         Value_t;

        static Value_t get_value( Widget_t const* w ) {
            return w->currentText();
        }
        static void set_value( Widget_t* w, Value_t const& value ) {
            return w->setCurrentText( value );
        }
        static Choices_t get_choices( Widget_t* w ) {
            Choices_t choices;
            for( int i=0; i<w->count(); ++i ) {
                choices.append( w->itemText(i) );
            }
            return choices;
        }
        static void set_choices( Widget_t* w, Choices_t const& choices )  {
            w->clear();
            for( Choices_t::const_iterator iter=choices.cbegin(); iter!=choices.cend(); ++iter )
                w->addItem( iter->toString() );
        }
    };
    template<> struct WidgetAccessor<QSpinBox>
    {
        typedef QSpinBox   Widget_t;
        typedef int        Value_t;

        static Value_t get_value( Widget_t const* w ) {
            return w->value();
        }
        static void set_value( Widget_t* w, Value_t value ) {
             w->setValue( value );
        }
        static Choices_t get_choices( Widget_t* w )
        {
            Choices_t choices;
            choices.append( w->minimum()    );
            choices.append( w->maximum()    );
            choices.append( w->singleStep() );
            return choices;
        }
        static void set_choices( Widget_t* w, Choices_t const& choices ) {
            w->setRange( toValue<Value_t>( choices.at(0) ), toValue<Value_t>( choices.at(1) ) );
            if( choices.size()==3 ) {
                w->setSingleStep( toValue<Value_t>( choices.at(2) ) );
            }
        }
    };
    template<> struct WidgetAccessor<QDoubleSpinBox>
    {
        typedef QDoubleSpinBox Widget_t;
        typedef double         Value_t;

        static Value_t get_value( Widget_t const* w ) {
            return w->value();
        }
        static void set_value( Widget_t* w, Value_t value ) {
             w->setValue( value );
        }
        static Choices_t get_choices( Widget_t* w )
        {
            Choices_t choices;
            choices.append( w->minimum()    );
            choices.append( w->maximum()    );
            choices.append( w->singleStep() );
            return choices;
        }
        static void set_choices( Widget_t* w, Choices_t const& choices ) {
            w->setRange( toValue<Value_t>( choices.at(0) ), toValue<Value_t>( choices.at(1) ) );
            if( choices.size()==3 ) {
                w->setSingleStep( toValue<Value_t>( choices.at(2) ) );
            }
        }
    };
    template<> struct WidgetAccessor<QLineEdit>
    {
        typedef QLineEdit Widget_t;
        typedef QString   Value_t;

        static Value_t get_value ( Widget_t* w ) {
            return w->text();
        }
        static void set_value( Widget_t* w, Value_t const & value ) {
            w->setText( value );
        }
        static Choices_t get_choices( Widget_t* w ) {
            return Choices_t();
        }
        static void set_choices( Widget_t* w, Choices_t const& /*dummy*/  )
        {// nothing to do
        }
    };
    template<> struct WidgetAccessor<QCheckBox>
    {
        typedef QCheckBox Widget_t;
        typedef bool      Value_t;

        static Value_t get_value( Widget_t* w ) {
            return w->isChecked();
        }
        static void set_value( Widget_t* w, Value_t value ) {
            w->setChecked(value);
        }
        static Choices_t get_choices( Widget_t* w ){
            return Choices_t();
        }
        static void set_choices( Widget_t* w, Choices_t const& /*dummy*/  )
        {// nothing to do
        }
    };
 // dummies for no widget. theses are not actually called. their only purpose is to satisfy the compiler..
    template<> struct WidgetAccessor<QWidget>
    {
        typedef QWidget Widget_t;
        typedef int     Value_t; // dummy

        static Value_t get_value( Widget_t* w )                         { return 0; }
        static void set_value( Widget_t* w, Value_t const& /*dummy*/ )  {/*nothing to do*/}
        static Choices_t get_choices( Widget_t* w )                     { return Choices_t(); }
        static void set_choices( Widget_t* w, Choices_t const& list  )  {/*nothing to do*/}
    };

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

    template <class W>
    void
    DataConnector<W>::
    config_to_widget()
    {
        if( !this->qw_ || !this->configItemName_ ) return;

        cfg::Item* config_item = this->configItem();
        wa::set_choices( this->widget(), config_item->choices() );
        typename wa::Value_t v = toValue<typename wa::Value_t>( config_item->value() );
        wa::set_value( this->widget(), v );
    }

    template <class W>
    void
    DataConnector<W>::
    widget_to_config()
    {
        if( !this->qw_ || !this->configItemName_ ) return;

        cfg::Item* config_item = this->configItem();
        Choices_t c = wa::get_choices( this->widget() );
        typename wa::Value_t v = wa::get_value( this->widget() );
        config_item->set_choices( c, config_item->choices_is_range() );
        config_item->set_value  ( v );
    }

    QString toString( QVariant const& qv);

    template <class W>
    void
    DataConnector<W>::
    config_to_script()
    {
        if( !this->scriptFlag_ ) return;

        cfg::Item* config_item = this->configItem();
        QString v = toString( config_item->value() );
        if( v=="bool(true)" ) {
            this->script->find_key( *this->scriptFlag_ )->setHidden(true);
        } else if( v=="bool(true)" ) {
            this->script->find_key( *this->scriptFlag_ )->setHidden(false);
        } else if( this->scriptFlag_->startsWith("-") ) {
            this->script->find_key( *this->scriptFlag_ )->setHidden( v.isEmpty() );
            (*this->script)[*this->scriptFlag_] = v;
        } else {
            (*this->script)[*this->scriptFlag_] = v;
        }
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
        DataConnectorBase* dataConnector = new DataConnector<W>( qw, configItemName, scriptFlag );
        dataConnector->config_to_widget();
        dataConnector->config_to_script();
        return dataConnector;
    }

    DataConnectorBase* newDataConnector( QString const& configItemName, QString const& scriptFlag );

}// namespace dc

#endif // DATACONNECTOR

