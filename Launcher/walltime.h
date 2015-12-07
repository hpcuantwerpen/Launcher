#ifndef WALLTIME_H
#define WALLTIME_H

#include <QLineEdit>

#include <cfg.h>

class Walltime
{
public:
    Walltime( QLineEdit* wWalltime, cfg::Item* ci );
    void config_to_widget();
    void widget_to_config();
    bool validate( int seconds ) const;
    bool validate() const;

    int seconds() const;
    void set_seconds( int seconds );
    void set_seconds( QString const& hhmmss );

    int limit() const { return limit_; }
    void set_limit( int limit ) {
        this->limit_ = limit;
    }
    static int toInt( QString const& string );
     /* string = "hh:mm:ss"
      *        | "hh:mm"
      *        | "<integer> d|h|m|s|D|H|M|S"
      * returns the number of seconds.
      * A negative number is returned in case of an uninterpretable string.
      */
    static QString toString( int seconds );
private:
    QLineEdit* wi_;
    cfg::Item* ci_;
    int limit_;
};

#endif // WALLTIME_H
