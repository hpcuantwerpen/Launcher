#include "shellcommand.h"
#include "usercomment.h"

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
        this->text_ += this->value_;
        this->text_ += this->comment_;
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
    bool
    ShellCommand::
    copyFrom( ShellCommand const* rhs )
    {
        if( this->parameters().isEmpty() )
            this->text_ = rhs->text_;
        return this->is_modified();
    }
 //-----------------------------------------------------------------------------
}//namespace pbs

