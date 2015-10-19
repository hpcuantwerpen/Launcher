#ifndef SHEBANG_H
#define SHEBANG_H

#include "usercomment.h"

namespace pbs
{//=============================================================================
    class Shebang: public UserComment
 //=============================================================================
    {
        friend class UserComment;
        template <class T>
        friend T* create(QString const& line );
    public:
        static QString default_value;
        virtual bool equals( ShellCommand const* rhs) const;
    protected:
        Shebang
          ( QString const&  line     = default_value
          , types::Position position = types::ShebangPosition
          , types::Type     type     = types::ShebangType
          );
        static Shebang* parse( QString const &line);
    private:
        static QString prefix_;
    };
//=============================================================================
}// namespace pbs

#endif // SHEBANG_H
