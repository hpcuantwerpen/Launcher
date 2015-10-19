#ifndef USERCOMMENT_H
#define USERCOMMENT_H

#include "shellcommand.h"

namespace pbs
{//=============================================================================
    class UserComment : public ShellCommand
 //=============================================================================
    {
        friend class ShellCommand;
        template <class T>
        friend T* create(QString const& line );
    protected:
        UserComment
          ( QString const&  line
          , types::Position position = types::UserCommentPosition
          , types::Type     type     = types::UserCommentType
          );
        static UserComment* parse( QString const &line);
    private:
        static QString prefix_;
    };
 //=============================================================================
}// namespace pbs

#endif // USERCOMMENT_H
