#include "text.h"
#include <throw_.h>
#include <QRegularExpression>
#include <QTextStream>

namespace pbs
{//-----------------------------------------------------------------------------
    Text::Text(QString const& txt, Text *parent )
      : text_(txt )
      , parent_(parent)
      , has_unsaved_changes_(true)
    {}
 //-----------------------------------------------------------------------------
    void Text::init()
    {
        int comment_at = this->text_.indexOf('#');
        if( comment_at==-1) {
            this->body_ = this->text_;
        } else {
            this->body_   = this->text_.left(comment_at);
            this->comment_= this->text_.mid (comment_at);
        }
        Text::staticCompose(this);
    }
 //-----------------------------------------------------------------------------
    void Text::set_parent( Text* parent ) {
        this->parent_ = parent;
    }

 //-----------------------------------------------------------------------------
    Text::~Text() {}
 //-----------------------------------------------------------------------------
    QString const&
    Text::
    text() const
    {
        if( is_modified() )
            const_cast<Text*>(this)->compose();
        return this->text_;
    }
 //-----------------------------------------------------------------------------
    bool
    Text::
    is_modified() const {
        return this->text_.isEmpty();
    }
 //-----------------------------------------------------------------------------
    void
    Text::
    set_is_modified( bool is_modified_ ) {
        if( is_modified_ ) {
            this->text_.clear();
            if( this->parent_ ) {
                this->parent_->set_is_modified( is_modified_ );
#ifdef QT_DEBUG
                if( !this->parent_->has_unsaved_changes() )
                    std::cout<<"\nToggling unsaved changes: "<<this->text().toStdString()<<std::flush;
#endif
            }
            this->set_has_unsaved_changes(true);

        }
    }
 //-----------------------------------------------------------------------------
    void
    Text::
    staticCompose( Text* _this_)
    {
        if( !_this_->text_.endsWith('\n'))
            _this_->text_.append('\n');
    }
 //-----------------------------------------------------------------------------
    QString Text::toString( int verbose, bool refresh )
    {
        QString s;
        if( refresh && this->is_modified() ) {
            this->text();
        }
        QTextStream ts(&s);
        if( verbose ) {
            ts << "\nText::toString"
               << "\n->text_    [[" << this->text_    << "]]"
               << "\n->body_    [[" << this->body_    << "]]"
               << "\n->comment_ [[" << this->comment_ << "]]"
              ;
        } else {
            ts << this->text_;
        }
        return s;
    }
 //-----------------------------------------------------------------------------
}//namespace pbs
