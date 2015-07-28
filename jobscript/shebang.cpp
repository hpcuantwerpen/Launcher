#include "shebang.h"

namespace pbs
{//-----------------------------------------------------------------------------
    Shebang*
    Shebang::
    parse( QString const &line)
    {
        if( !line.startsWith(Shebang::prefix_) )
            return nullptr;
        else
            return create<Shebang>(line);
    }
 //-----------------------------------------------------------------------------
    QString Shebang        ::prefix_ = "#!";
    QString Shebang::default_value = "#!/bin/bash";
 //-----------------------------------------------------------------------------
    Shebang::
    Shebang( QString const &line, int ordinate, types::Type type)
      : UserComment(line,ordinate,type)
    {}
 //-----------------------------------------------------------------------------
    bool Shebang::
    equals( ShellCommand const* rhs) const
    {
        return ( rhs->type() == types::Shebang );
    }
 //-----------------------------------------------------------------------------
}//namespace pbs

