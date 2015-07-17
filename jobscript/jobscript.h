#ifndef JOBSCRIPT_H
#define JOBSCRIPT_H

#include <QString>
#include <toolbox.h>

class Jobscript
{

public:
    Jobscript();
};

namespace pbs
{//=============================================================================
    namespace types
    {
        enum Type
        { ShellCommand
        , UserComment
        , Shebang
        , LauncherComment
        , PbsDirective
        };
    }
 //=============================================================================
 // ShellCommand
 //   +- UserComment
 //      +- Shebang
 //      +- LauncherComment
 //      +- PbsDirective
 //=============================================================================

 //=============================================================================
 // fwd declarations
    class ShellCommand;
    ShellCommand * parse( QString const & line );
    template <class T> T* create( QString const& line );
 //=============================================================================
    class ShellCommand
    {
        friend ShellCommand * parse( QString const & line );
        template <class T>
        friend T* create( QString const& line );
    protected:
        ShellCommand(QString const& line, int ordinate=10, types::Type type=types::ShellCommand);
        virtual void init() {}
    public:
        virtual ~ShellCommand() {}
        int         ordinate() const { return this->ordinate_; }
        types::Type type    () const { return this->type_;     }
        virtual void compose() {}
    protected:
        QString          text_;
        toolbox::OrderedMap parms_;
    private:
        static ShellCommand* parse ( QString const &line);
        int         ordinate_;
        types::Type type_;
    };
 //=============================================================================
    class UserComment : public ShellCommand
    {
        friend class ShellCommand;
        template <class T>
        friend T* create(QString const& line );
    protected:
        UserComment(QString const& line, int ordinate=-1, types::Type type=types::UserComment);
        static UserComment* parse( QString const &line);
    };
 //=============================================================================
    class Shebang: public UserComment
    {
        friend class UserComment;
        template <class T>
        friend T* create(QString const& line );
    protected:
        Shebang(QString const& line, int ordinate=0, types::Type type=types::Shebang);
        static Shebang* parse( QString const &line);
    };
 //=============================================================================
    class LauncherComment : public UserComment
    {
        friend class UserComment;
        template <class T>
        friend T* create(QString const& line );
        virtual void compose();
    protected:
        LauncherComment(QString const& line, int ordinate=1, types::Type type=types::LauncherComment);
        virtual void init();
        static LauncherComment* parse( QString const &line);
    };
 //=============================================================================
    class PbsDirective : public UserComment
    {
        friend class UserComment;
        template <class T>
        friend T* create(QString const& line );
    protected:
        PbsDirective(QString const& line, int ordinate=2, types::Type type=types::PbsDirective);
        virtual void init();
        static PbsDirective* parse( QString const &line);
    };
 //=============================================================================
}

#endif // JOBSCRIPT_H
