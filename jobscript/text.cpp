#include "text.h"
#include <throw_.h>
#include <QRegularExpression>

namespace pbs
{//-----------------------------------------------------------------------------
    Text::Text( QString const& txt)
      : text_(txt )
    {}
 //-----------------------------------------------------------------------------
    void Text::init()
    {
        QRegularExpression re("^(#*[^#]*\\b)((\\s*)(#.*)?)$");
        QRegularExpressionMatch m = re.match(this->text_);
        if( m.hasMatch() ) {
//#ifdef QT_DEBUG
//            std::cout<<this->text_.toStdString()<<std::endl;
//            for(int i=0; i<=m.lastCapturedIndex();++i)
//                std::cout<<i<<" : "<<m.captured(i).toStdString()<<std::endl;
//            std::cout<<std::endl;
//#endif//ifdef QT_DEBUG
            this->body_    = m.captured(1);
            this->comment_ = m.captured(2);
        } else if( this->text_.startsWith('#') ) {
            this->body_   = this->text_;
            this->comment_.clear();
        } else {
            throw_<std::runtime_error>("Malformed script line:'%1'\n\tin Text::init_()", this->text_ );
        }
        Text::staticCompose(this);
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
        if( is_modified_ )
            this->text_.clear();
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
}//namespace pbs
