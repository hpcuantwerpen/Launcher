#include "text.h"
#include <throw_.h>
#include <QRegularExpression>

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
            if( parent_ ) {
                parent_->set_is_modified( is_modified_ );
            }
            this->set_has_unsaved_changes(true);
        }
    }
 //-----------------------------------------------------------------------------
    void
    Text::
    staticCompose( Text* _this_)
    {
        if( !_this_->text_.endsWith('/n'))
            _this_->text_.append('\n');
    }
 //-----------------------------------------------------------------------------
    void Text::print( std::ostream &to, int verbose, bool refresh )
    {
        if( refresh && this->is_modified() ) {
            this->text();
        }
        if( verbose ) {
            to << "\nText::print"
               << "\n->text_    [[" << this->text_   .toStdString() << "]]"
               << "\n->body_    [[" << this->body_   .toStdString() << "]]"
               << "\n->comment_ [[" << this->comment_.toStdString() << "]]"
               << std::flush;
        } else {
            to << this->text_.toStdString() << std::flush;
        }
    }
 //-----------------------------------------------------------------------------
}//namespace pbs
