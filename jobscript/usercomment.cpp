#include "usercomment.h"
#include "shebang.h"
#include "launchercomment.h"
#include "pbsdirective.h"

namespace pbs
{//-----------------------------------------------------------------------------
    UserComment*
    UserComment::
    parse( QString const &line)
    {
        if( !line.startsWith(UserComment::prefix_) )
            return nullptr;
        UserComment* parsed = Shebang::parse(line);
        if( parsed )
            return parsed;
        parsed = LauncherComment::parse(line);
        if( parsed )
            return parsed;
        parsed = PbsDirective::parse(line);
        if( parsed )
            return parsed;
        else
            return create<UserComment>(line);
    }
 //-----------------------------------------------------------------------------
    QString UserComment::prefix_ = "#";
 //-----------------------------------------------------------------------------
    UserComment::
    UserComment(QString const& line, types::Position position, types::Type type)
      : ShellCommand(line,position,type)
    {}
 //-----------------------------------------------------------------------------
}//namespace pbs
