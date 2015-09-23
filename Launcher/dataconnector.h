#ifndef DATACONNECTOR
#define DATACONNECTOR

#include <tuple>

#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QCheckBox>

#include <cassert>

#include <cfg.h>
#include <jobscript.h>

namespace dc
{/* ****************************************************************************
  * Classes for communicating data values between widget, config file and script.
  * Data items are stored at three different places
  *  - in the script, where it is accessible through a key via operator[]
  *  - in the gui, as the value of a widget (how it is accessed depends on the
  *    widget type)
  *  - in the config file as a cfg::Item, accessed through the item name
  *
  * As indicated in the table below widgets and cfg::Items do have a type
  * (int, double, QString, bool) and optionally a list or range of admissible
  * values. The script does not have typed values, as it converts everything to
  * QString. The script also does not store a list of admissible values.
  *
  *             Widget  Config  Script
  *     value   +       +       +
  *     Value_t +       +       - (QString)
  *     choices +       +       -
  *
  * Initially all data are either entered by the user and read from the
  * <cluster>.info files.
  * When the user modifies a value through a widget, it is always communnicated
  * to the corresponding
  * ****************************************************************************/

 /* ****************************************************************************
  * Base class for connecting a widget, a cfg::Item and a script value. This is
  * the base class for class DataConnector<W>, where W is a QComboBox, QLineEdit,
  * QSpinBox, QDoubleSpinBox, QCheckBox, or a QWidget (which acts as a dummy
  * type for the case where only a cfg::Item and a script value and no widget
  * are connected.
  * If the widget is a QCheckBox, it is not connected to a script value. Instead
  * it tells whether the corresponding script line is visible or not.
  * ****************************************************************************/
    class DataConnectorBase
    {
    public:
     /* The class stores a weak pointer to the cfg::Config object and the script
      * (the application handles a single Config object and script.)
      * Initially, they are null pointers, so if you forget to initialize them
      * you get an exception.
      */
        static cfg::Config* config;
        static pbs::Script* script;

     /* The ctor stores information on the widget, the cfg::Item name and the key
      * of the the data value in the script. Derived objects are meant to be
      * constructed through the newDataConnector() function
      * The configItemName is mandatory, but the widget or the scriptKey can be
      * omitted.
      */
        DataConnectorBase( QWidget* qw, QString const& configItemName, QString const& scriptKey )
          : qw_(qw)
          , configItemName__(configItemName)
          , scriptKey__(scriptKey)
        {// assertions to make sure that the script and the Config object are accessible.
            assert( DataConnectorBase::config != nullptr );
            assert( DataConnectorBase::script != nullptr );

            this->configItemName_ = ( configItemName.isEmpty() ? nullptr : &this->configItemName__ );
            this->scriptKey_      = ( scriptKey     .isEmpty() ? nullptr : &this->scriptKey__      );
        }

     // Get this DataConnector's cfg::item
        cfg::Item* configItem() {
            return config->getItem(*this->configItemName_);
        }

     /* Virtual member functions for communicating the value between cfg::Item,
      * widget, and script.
      * These return true if the target was changed, and false otherwise.
      */
        virtual bool value_config_to_widget()=0;
        virtual bool value_widget_to_config()=0;
        virtual bool value_config_to_script()=0;
        virtual bool value_script_to_config()=0;

     /* Virtual member functions for communicating the list or range of
      * admissible values between cfg::Item, widget, and script.
      */
        virtual void choices_config_to_widget()=0;
        virtual void choices_widget_to_config()=0;

     /* Get the name of the cfg::Item.
      * The general policy is that the Item's name is the name of the widget's
      * variable name in the MainWindow's user interface.
      */
        QString const& name() {
            return configItemName__;
        }

    protected:
        QWidget* qw_;
        QString*configItemName_;
        QString*scriptKey_;
    private:
        QString configItemName__;
        QString scriptKey__;
    };

 // Templated versions of QVariants converstion functions
 // generic version, specialisations for V = QString, double, int, bool
    template <class V>
    V toValue( QVariant const& qv ) {
        throw_<std::logic_error>("Not Implemented toValue<V>( QVariant const& qv ).");
    }
    template <> QString toValue( QVariant const& qv );
    template <> double  toValue( QVariant const& qv );
    template <> int     toValue( QVariant const& qv );
    template <> bool    toValue( QVariant const& qv );

    typedef pbs::ShellCommand Line_t;

 // helper function to convert script parameter or value  to config value
    QString toValue( QString const& script_value, QWidget       * w, Line_t* line );
//  QString toValue( QString const& script_value, QComboBox     * w, Line_t* line ); // covered by QWidget overload
//  QString toValue( QString const& script_value, QLineEdit     * w, Line_t* line ); // covered by QWidget overload
    double  toValue( QString const& script_value, QDoubleSpinBox* w, Line_t* line );
    int     toValue( QString const& script_value, QSpinBox      * w, Line_t* line );
    bool    toValue( QString const& script_value, QCheckBox     * w, Line_t* line );

    typedef QList<QVariant> Choices_t;

 /* ****************************************************************************
  * WidgetAccessor<W> provides a unified interface for getting and setting
  * a QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit, QCheckBox.
  * A spicalization for QWidget is alos provided (to keep the compiler happy).
  * ****************************************************************************/
 // generic version
    template <class W> struct WidgetAccessor {};
 // specializations
    template<> struct WidgetAccessor<QComboBox>
    {
        typedef QComboBox       Widget_t;
        typedef QString         Value_t;

        static Value_t get_value( Widget_t const* w ) {
            return w->currentText();
        }
        static bool set_value( Widget_t* w, Value_t const& value ) {
            if( w->currentText() == value ) {
                return false;
            } else{
                w->setCurrentText( value );
                return true;
            }
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
        static bool set_value( Widget_t* w, Value_t value ) {
            if( w->value()==value ) {
                return false;
            } else {
                w->setValue( value );
                return true;
            }
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
        static bool set_value( Widget_t* w, Value_t value ) {
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
        static bool set_value( Widget_t* w, Value_t const & value ) {
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
        static bool set_value( Widget_t* w, Value_t value ) {
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
        typedef QString Value_t; // dummy

        static Value_t get_value( Widget_t* w )                         { return QString(); }
        static bool set_value( Widget_t* w, Value_t const& /*dummy*/ )  {/*nothing to do*/}
        static Choices_t get_choices( Widget_t* w )                     { return Choices_t(); }
        static void set_choices( Widget_t* w, Choices_t const& list  )  {/*nothing to do*/}
    };

 // not entirely the same as QVariant::toString()
    QString toString( QVariant const& qv);

 /* ****************************************************************************
  * DataConnector<W> with W some concrete widget. For documentation see the
  * base class DataConnectorBase.
  * The member functions are implemented in terms of WidgetAccessor<W>
  * ****************************************************************************/
    template<class W>
    class DataConnector : public DataConnectorBase
    {
        typedef WidgetAccessor<W> wa;
    public:
        DataConnector( W* qw, QString const& configItemName, QString const& scriptKey )
          : DataConnectorBase( qw, configItemName, scriptKey )
        {}
        W* widget() {
            return dynamic_cast<W*>(this->qw_);
        }

        virtual bool value_config_to_widget()
        {
            if( !this->qw_ || !this->configItemName_ ) return false;

            cfg::Item* config_item = this->configItem();
            typename wa::Value_t v = toValue<typename wa::Value_t>( config_item->value() );
            return wa::set_value( this->widget(), v );
        }

        virtual bool value_widget_to_config()
        {
            if( !this->qw_ || !this->configItemName_ ) return false;

            cfg::Item* config_item = this->configItem();
            Choices_t c = wa::get_choices( this->widget() );
            typename wa::Value_t v = wa::get_value( this->widget() );
            return config_item->set_value( v );
        }

        virtual bool value_config_to_script()
        {
            if( !this->scriptKey_ ) return false;

            cfg::Item* config_item = this->configItem();
            QString v = toString( config_item->value() );
            Line_t* line = this->script->find_key( *this->scriptKey_ );
            if( v=="bool(true)" ) {// checked -> unhide
                if( line->hidden() ) {
                    line->setHidden(false);
                    return true;
                } else {// already not hidden
                    return false;
                }
            } else if( v=="bool(false)" ) {//unchecked -> hide
                if( line->hidden() ) {// already hidden
                    return false;
                } else {
                    line->setHidden(true);
                    return true;
                }
            } else if( this->scriptKey_->startsWith("-") ) {
                if( (*this->script)[*this->scriptKey_] == v ) {
                    return false;
                } else {
                    this->script->find_key( *this->scriptKey_ )->setHidden( v.isEmpty() );
                    (*this->script)[*this->scriptKey_] = v;
                    return true;
                }
            } else {
                if( (*this->script)[*this->scriptKey_] == v ) {
                    return false;
                } else {
                    (*this->script)[*this->scriptKey_] = v;
                    return true;
                }
            }
        }

        virtual bool value_script_to_config()
        {
            if( !this->qw_ || !this->scriptKey_ ) return false;

            Line_t* line = this->script->find_key(*this->scriptKey_);
            QString qs = (*this->script)[*this->scriptKey_];
            typename wa::Value_t v = toValue( qs, dynamic_cast<W*>( this->widget() ), line );
            return this->configItem()->set_value(v);
        }

        virtual void choices_config_to_widget()
        {
            if( !this->qw_ || !this->configItemName_ ) return;

            wa::set_choices( this->widget(), this->configItem()->choices() );
        }

        virtual void choices_widget_to_config()
        {
            if( !this->qw_ || !this->configItemName_ ) return;

            cfg::Item* config_item = this->configItem();
            Choices_t c = wa::get_choices( this->widget() );
            config_item->set_choices( c, config_item->choices_is_range() );
        }
    };

 /* ****************************************************************************
  * Create a new DataConnector<W> for a widget of type W.
  * The object is created on the heap and must be deleted if it is no longer
  * needed
  * ****************************************************************************/
    template <class W>
    DataConnectorBase* newDataConnector( W* qw, QString const& configItemName, QString const& scriptKey )
    {
        DataConnectorBase* dataConnector = new DataConnector<W>( qw, configItemName, scriptKey );
        dataConnector->choices_config_to_widget();
        dataConnector->  value_config_to_widget();
        dataConnector->  value_config_to_script();
        return dataConnector;
    }

 /* ****************************************************************************
  * Convenience function to create a DataConnector<W> without a widget,
  * connecting a cfg::Item and a script value.
  * ****************************************************************************/
    DataConnectorBase* newDataConnector( QString const& configItemName, QString const& scriptKey );

}// namespace dc

#endif // DATACONNECTOR

