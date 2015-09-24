#include "dataconnector.h"

namespace dc
{
    cfg::Config* DataConnectorBase::config = nullptr;
    pbs::Script* DataConnectorBase::script = nullptr;

    template <>
    QString toValue( QVariant const& qv ) {
        return qv.toString();
    }
    template <>
    int toValue( QVariant const& qv ) {
        if( qv.type() != QVariant::Int )
            throw_<std::runtime_error>("Using 'toValue<int>( QVariant const& qv )' on qv of type %1.", qv.typeName() );
        return qv.toInt();
    }
    template <>
    double toValue( QVariant const& qv ) {
        if( qv.type() != QVariant::Double )
            throw_<std::runtime_error>("Using 'toValue<double>( QVariant const& qv )' on qv of type %1.", qv.typeName() );
        return qv.toDouble();
    }
    template <>
    bool toValue( QVariant const& qv ) {
//        if( qv.type() == QVariant::Invalid )
//            return false;
        if( qv.type() != QVariant::Bool )
            throw_<std::runtime_error>("Using 'toValue<bool>( QVariant const& qv )' on qv of type '%1'.", qv.typeName() );
        return qv.toBool();
    }

    QString toValue( QString const& qs, QWidget       * /*w*/, Line_t* /*line */) { return qs;             }
//  QString toValue( QString const& qs, QComboBox     * /*w*/, Line_t* /*line*/ ) { return qs;             }
//  QString toValue( QString const& qs, QLineEdit     * /*w*/, Line_t* /*line */) { return qs;             }
    int     toValue( QString const& qs, QSpinBox      * /*w*/, Line_t* /*line*/ ) { return qs.toInt();     }
    double  toValue( QString const& qs, QDoubleSpinBox* /*w*/, Line_t* /*line */) { return qs.toDouble();  }
    bool    toValue( QString const& qs, QCheckBox     * /*w*/, Line_t*   line   ) { return line->hidden(); }

    DataConnectorBase* newDataConnector( QString const& configItemName, QString const& scriptFlag ) {
        return new DataConnector<QWidget>( nullptr, configItemName, scriptFlag );
    }


    QString toString( QVariant const& qv ) {
        switch( qv.type() ) {
        case QVariant::Bool:
            return ( qv.toBool() ? "bool(true)" : "bool(false)");
        case QVariant::Double:
            return QString().setNum( qv.toDouble(), 'g', 3 );
        default:
            return qv.toString();
        }
    }


}// namespace dc
