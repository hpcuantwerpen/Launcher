#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H


#ifndef MESSAGBOX_H
#define MESSAGBOX_H

#include <QMessageBox>
#include <QMap>

class MessageBox;
class Message
{
    friend class MessageBox;
public:
    Message() {}
    Message
      ( QString             msg
      , QMessageBox::StandardButtons buttons
      , QMessageBox::StandardButton  defaultButton
      )
      : msg_(msg)
      , buttons_(buttons)
      , defaultButton_(defaultButton)
      , remember_decision_(false)
      , answer_( (int)QMessageBox::NoButton )
    {}

private:
    QString msg_;
    QMessageBox::StandardButtons buttons_;
    QMessageBox::StandardButton  defaultButton_;
    bool                         remember_decision_;
    int answer_;
};

class MessageBox
{
public:
    MessageBox( QString const & title = QString() )
      : title_(title)
    {}

    void
    add
      ( QString             key
      , QString             msg
      , QMessageBox::StandardButtons buttons
      , QMessageBox::StandardButton  defaultButton
      );

    bool contains( QString const& key ) const { return messages.contains(key); }

//    QMessageBox::StandardButton
    int
    exec
      ( QString key
      , QString arg1 = QString()
      , QString arg2 = QString()
      , QString arg3 = QString()
      );

private:
    typedef QMap<QString,Message> Map_t_;
    Map_t_ messages;
    QString title_;
};

#endif // MESSAGBOX_H

#endif // MESSAGEBOX_H
