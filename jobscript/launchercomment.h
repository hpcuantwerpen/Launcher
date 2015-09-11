#ifndef LAUNCHERCOMMENT_H
#define LAUNCHERCOMMENT_H

#include "usercomment.h"

namespace pbs
{//=============================================================================
    class LauncherComment : public UserComment
 //=============================================================================
    {
        friend class UserComment;
        template <class T>
        friend T* create(QString const& line );
        virtual void compose();
    public:
        virtual bool equals( ShellCommand const* rhs) const;
        virtual void copyFrom( ShellCommand const* rhs );
    protected:
        LauncherComment(QString const& line, int ordinate=1, types::Type type=types::LauncherComment);
        virtual void init();
        static LauncherComment* parse( QString const &line);
    private:
        static QString prefix_;
    };
 //=============================================================================
}// namespace pbs


#endif // LAUNCHERCOMMENT_H
