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
        UserComment( QString const& line, int ordinate=-1, types::Type type=types::UserComment);
        static UserComment* parse( QString const &line);
//        virtual void compose();
    private:
        static QString prefix_;
    };
 //=============================================================================
}// namespace pbs

#endif // USERCOMMENT_H
