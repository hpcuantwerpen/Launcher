#include "dataconnector.h"

namespace dc2
{
    cfg::Config* DataConnectorBase::config = nullptr;
    pbs::Script* DataConnectorBase::script = nullptr;

 // QComboBox accessor functions
    QString get_value( QComboBox* w )
    {
        return w->currentText();
    }

    void set_value(QComboBox* w, QString const & value )
    {
        return w->setCurrentText( value );
    }

    QStringList get_choices( QComboBox* w )
    {
        QStringList result;
        for( int i=0; i<w->count(); ++i ) {
            result.append( w->itemText(i) );
        }
        return result;
     }

    void set_choices( QComboBox* w, QStringList const & list )
    {
        w->clear();
        w->addItems(list);
    }

 // QSpinBox accessor functions
    int get_value( QSpinBox* w )
    {
        return w->value();
    }

    void set_value( QSpinBox* w, int value )
    {
         w->setValue( value );
    }

    std::tuple<int,int,int> get_choices( QSpinBox* w )
    {
        return std::make_tuple( w->minimum(), w->maximum(), w->singleStep() );
    }

    void set_choices( QSpinBox* w, std::tuple<int,int,int> const& choices )
    {
        w->setRange( std::get<0>(choices), std::get<1>(choices) );
        w->setSingleStep( std::get<2>(choices) );
    }

 // QDoubleSpinBox accessor functions
    double get_value( QDoubleSpinBox* w )
    {
        return w->value();
    }

    void set_value( QDoubleSpinBox* w, double value )
    {
        w->setValue( value );
    }

    std::tuple<double,double,double> get_choices( QDoubleSpinBox* w )
    {
        return std::make_tuple( w->minimum(), w->maximum(), w->singleStep() );
    }

    void set_choices( QDoubleSpinBox* w, std::tuple<double,double,double> const& choices )
    {
        w->setRange( std::get<0>(choices), std::get<1>(choices) );
        w->setSingleStep( std::get<2>(choices) );
    }

 // QLineEdit accessor functions
    QString get_value( QLineEdit* w )
    {
        return w->text();
    }

    void set_value( QLineEdit* w, QString const & value )
    {
        w->setText( value );
    }

    QStringList get_choices( QLineEdit* w )
    {// empty list on purpose
        return QStringList();
    }

    void set_choices( QLineEdit* w, QStringList const& list )
    {// nothing to do
        if( list.size() ) {
            throw_<std::runtime_error>("Non empty list is unexpected in:\n    void set_choices( QLineEdit* w, QStringList const& list )\n");
        }
    }

 // dummies for no widget. theses are not actually called. their only purpose is to satisfy the compiler..
    QString get_value( QWidget* w )
    {
       if( w ) {
           throw_<std::runtime_error>("Nonzero widgetis unexpected in:\n    QString get_value( QWidget* w )\n");
       }
       return QString(); // empty string on purpose
    }

    void set_value( QWidget* w, QString const & value )
    {
        if( w ) {
            throw_<std::runtime_error>("Nonzero widgetis unexpected in:\n    QString set_value( QWidget* w, QString const & value )\n");
        }
    }

    QStringList get_choices( QWidget* w )
    {
        if( w ) {
            throw_<std::runtime_error>("Nonzero widgetis unexpected in:\n    QString set_value( QWidget* w, QString const & value )\n");
        }
        return QStringList(); // empty list on purpose
    }

    void set_choices( QWidget* w, QStringList const& list )
    {// nothing to do
        if( list.size() ) {
            throw_<std::runtime_error>("Non empty list is unexpected in:\n    void set_choices( QWidget* w, QStringList const& list )\n");
        }
    }



}// namespace dc2
