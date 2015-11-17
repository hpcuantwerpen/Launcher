#include "messagebox.h"
#include <QCheckBox>

void
MessageBox::
add
  ( QString key
  , QString msg
  , QMessageBox::StandardButtons buttons
  , QMessageBox::StandardButton  defaultButton
  )
{
    this->messages[key] = Message(msg,buttons,defaultButton);
}

//QMessageBox::StandardButton
int
MessageBox::
exec
  ( QString key
  , QString arg1
  , QString arg2
  , QString arg3
  )
{
    Map_t_::iterator iter = this->messages.find(key);

    if( iter->remember_decision_ ) {
        return iter->answer_;
    }
    QString msg = QString( iter->msg_ );
    if( !arg1.isEmpty() ) msg = msg.arg(arg1);
    if( !arg2.isEmpty() ) msg = msg.arg(arg2);
    if( !arg3.isEmpty() ) msg = msg.arg(arg3);


    QMessageBox mb( QMessageBox::Question, this->title_, msg
                  , iter->buttons_
                  );
    mb.setDefaultButton(iter->defaultButton_);
    QCheckBox* cb = new QCheckBox( ( iter->buttons_==QMessageBox::Ok
                                   ? "Apply this action always."
                                   : "Don't show this message again.") );
    mb.setCheckBox(cb);
    mb.exec();
    iter->answer_            = mb.result();
    iter->remember_decision_ = cb->isChecked();
    return iter->answer_;
}
