#include "shellcommand.h"
#include "usercomment.h"
#include <QTextStream>

namespace pbs
{//-----------------------------------------------------------------------------
    ShellCommand*
    ShellCommand::
    parse( QString const &line )
    {
        UserComment* parsed = UserComment::parse(line);
        if( parsed )
            return parsed;
        else
            return create<ShellCommand>(line);
    }
 //-----------------------------------------------------------------------------
    ShellCommand::
    ShellCommand( QString const& line, int ordinate, types::Type type )
      : Text(line)
      , ordinate_(ordinate)
      , type_(type)
    {}
 //-----------------------------------------------------------------------------
    void
    ShellCommand::
    compose()
    {
        if( this->hidden_ ) {
            return;
        }
        this->text_ = this->body_;
//        std::cout << "@@@ " <<this->text_.toStdString()<<std::endl;
        this->text_.append(this->comment_);
//        std::cout << "@@@ " <<this->text_.toStdString()<<std::endl;
        Text::staticCompose(this);
    }
 //-----------------------------------------------------------------------------
    bool ShellCommand::
    equals( ShellCommand const* rhs) const
    {
        if( rhs->type() == types::ShellCommand )
            return this->text() == rhs->text();
        return false;
    }
 //-----------------------------------------------------------------------------
    void ShellCommand::copyFrom( ShellCommand const* rhs )
    {
        if( this->parameters().isEmpty() ) {
            if( this->text_ != rhs->text_ ) {
                this->text_ = rhs->text_;
                this->set_is_modified();
            }
        }
    }
 //-----------------------------------------------------------------------------
    QString toString(Parameters_t parameters)
    {
        QString result("{ ");
        for ( Parameters_t::const_iterator iter=parameters.cbegin()
            ; iter!=parameters.cend(); ++iter )
        {
            result.append( QString("(%1:%2) ").arg(iter->first).arg(iter->second) );
        }
        result.append("}");
        return result;
    }
 //-----------------------------------------------------------------------------
    QString toString(Features_t features)
    {
        QString result("{ ");
        for ( Features_t::const_iterator iter=features.cbegin()
            ; iter!=features.cend(); ++iter )
        {
            result.append( QString("(%1) ").arg(*iter) );
        }
        result.append("}");
        return result;
    }
 //-----------------------------------------------------------------------------
    QString ShellCommand::toString( int verbose, bool refresh )
    {
        QString s = this->Text::toString(verbose,refresh);
        if( verbose )
        {
            QTextStream ts(&s);
            ts << "\nShellCommand::toString"
               << "\n->type_      [[" << this->type_                      << "]]"
               << "\n->value_     [[" << this->value_                     << "]]"
               << "\n->flag_      [[" << this->flag_                      << "]]"
               << "\n->parameters_[[" << pbs::toString( this->parameters_ ) << "]]"
               << "\n->features_  [[" << pbs::toString( this->features_   ) << "]]"
               << "\n->hidden_    [[" << (this->hidden_?1:0)              << "]]"
               ;
        }
        return s;
    }
 //-----------------------------------------------------------------------------
}//namespace pbs

