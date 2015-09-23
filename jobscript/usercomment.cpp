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
    UserComment(QString const& line, int ordinate, types::Type type)
      : ShellCommand(line,ordinate,type)
    {}
 //-----------------------------------------------------------------------------
//    void
//    UserComment::
//    compose()
//    {
//        if( this->hidden_ ) {
//            return;
//        }
//        this->text_ = this->body_;
//        Text::staticCompose(this);
//    }
 //-----------------------------------------------------------------------------
}//namespace pbs
