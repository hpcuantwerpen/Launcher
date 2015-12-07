#include "walltime.h"
#include <QRegularExpression>

Walltime::Walltime( QLineEdit* wWalltime, cfg::Item* ci )
  : wi_(wWalltime)
  , ci_(ci)
{

}

bool Walltime::validate( int seconds ) const {
    bool ok = seconds <= this->limit_;
    return ok;
}

bool Walltime::validate() const {
    return this->validate( this->seconds() );
}

int Walltime::seconds() const {
    int seconds = this->toInt( this->ci_->value().toString() );
    return seconds;
}

void Walltime::set_seconds( int seconds ) {
    if( this->validate(seconds) ) {
        QString hhmmss = this->toString(seconds);
        this->ci_->set_value(hhmmss);
        this->wi_->setText  (hhmmss);
    }
}

void Walltime::set_seconds( QString const& hhmmss ) {
    this->set_seconds( this->toInt( hhmmss ) );
}

QString Walltime::toString( int seconds )
{
    int h,m,s;
    h       = seconds/3600;
    seconds = seconds%3600;
    m       = seconds/60;
    s       = seconds%60;
    QString format("%1:%2:%3");
    QString formatted = format.arg(h).arg(m,2,10,QLatin1Char('0')).arg(s,2,10,QLatin1Char('0'));
    return formatted;
}

int Walltime::toInt( QString const& string )
{
    int h,m;
    int s = -1;
    QRegularExpression re;
    QRegularExpressionMatch rem;
 // "h+:mm:ss"
    re.setPattern("^(\\d+):(\\d+):(\\d+)");
    rem = re.match(string);
    if( rem.hasMatch() ) {
        try {
            h = rem.captured(1).toInt();
            m = rem.captured(2).toInt();
            s = rem.captured(3).toInt();
            s+= h*3600 + m*60;
        } catch(...) {
            s = -1;
        }
        return s;
    }
 // "<integer>unit" where unit is d|h|m|s (case insensitive)
    re.setPattern("^\\s*(\\d+)\\s*([dhmsDHMS]?)\\s*$");
    rem = re.match( string );
    if( rem.hasMatch() ) {
        QString number = rem.captured(1);
        QString unit   = rem.captured(2);
        try {
            s = number.toInt();
            if( s<0 ) s = -1;
            else if( unit=="d" ) s*=24*3600;
            else if( unit=="h" ) s*=3600;
            else if( unit=="m" ) s*=60;
            else if( unit=="s" ) s*=1;
            else s = -1;
        } catch(...) {
            s = -1;
        }
        return s;
    }
 // "h+:mm"
    re.setPattern("^\\s*(\\d+):(\\d+)\\s*$");
    rem = re.match( string );
    if( rem.hasMatch() ) {
        try {
            h = rem.captured(1).toInt();
            m = rem.captured(2).toInt();
            s = h*3600 + m*60;
        } catch(...) {
            s = -1;
        }
        return s;
    }
    return -1; //keep the compiler happy
}

void Walltime::config_to_widget() {
    this->wi_->setText( this->ci_->value().toString() );
}

void Walltime::widget_to_config() {
    this->ci_->set_value( this->wi_->text() );
}
