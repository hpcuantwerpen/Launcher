#ifndef PBSDIRECTIVE_H
#define PBSDIRECTIVE_H

#include "usercomment.h"

namespace pbs
{//=============================================================================
    class PbsDirective : public UserComment
 //=============================================================================
    {
        friend class UserComment;
        template <class T>
        friend T* create(QString const& line );
    public:
        virtual bool equals  ( ShellCommand const* rhs ) const;
        virtual void copyFrom( ShellCommand const* rhs );
        virtual void compose();
        virtual QString const& value() const;
        virtual void setValue( QString const& value );
    protected:
        PbsDirective(QString const& line, int ordinate=2, types::Type type=types::PbsDirective);
        virtual void init();
        static PbsDirective* parse( QString const &line);
    private:
        static QString prefix_;
    };
 //=============================================================================
}// namespace pbs

#endif // PBSDIRECTIVE_H
