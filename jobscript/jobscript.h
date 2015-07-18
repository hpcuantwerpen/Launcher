#ifndef JOBSCRIPT_H
#define JOBSCRIPT_H

#include <QString>
#include <QVector>
#include <QScopedPointer>

#include <cfg.h>
#include <toolbox.h>

#define NOT_AVALAIBLE_FOR_THIS_TYPE(MEMBER) \
    throw_<std::logic_error>("Member #1 is not available for type %2",MEMBER,this->typeName());

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
        int            ordinate() const { return this->ordinate_; }
        types::Type    type    () const { return this->type_;     }
        QString const& typeName() const { return this->typeName_;}
        virtual void compose() {}
        //QString const& parm_value( QString const & key ) const;

        virtual toolbox::OrderedMap const& parms() const { NOT_AVALAIBLE_FOR_THIS_TYPE("parms()") }
        virtual toolbox::OrderedMap      & parms()       { NOT_AVALAIBLE_FOR_THIS_TYPE("parms()") }

        virtual toolbox::OrderedSet const& feats() const { NOT_AVALAIBLE_FOR_THIS_TYPE("feats()") }
        virtual toolbox::OrderedSet      & feats()       { NOT_AVALAIBLE_FOR_THIS_TYPE("feats()") }

    protected:
        QString             text_;
    private:
        static ShellCommand* parse ( QString const &line);
        int         ordinate_;
        types::Type type_;
        QString     typeName_;
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
    public:
        virtual toolbox::OrderedMap const& parms() const { return parms_; }
        virtual toolbox::OrderedMap      & parms()       { return parms_; }
    protected:
        LauncherComment(QString const& line, int ordinate=1, types::Type type=types::LauncherComment);
        virtual void init();
        static LauncherComment* parse( QString const &line);
    private:
        toolbox::OrderedMap parms_;
        QString             value_;
    };
 //=============================================================================
    class PbsDirective : public UserComment
    {
        friend class UserComment;
        template <class T>
        friend T* create(QString const& line );
    public:
        virtual toolbox::OrderedMap const& parms() const { return parms_; }
        virtual toolbox::OrderedMap      & parms()       { return parms_; }
        virtual toolbox::OrderedSet const& feats() const { return feats_; }
        virtual toolbox::OrderedSet      & feats()       { return feats_; }
    protected:
        PbsDirective(QString const& line, int ordinate=2, types::Type type=types::PbsDirective);
        virtual void init();
        static PbsDirective* parse( QString const &line);
    private:
        toolbox::OrderedMap parms_;
        toolbox::OrderedSet feats_;
        QString             flag_;
        QString             value_;
        QString             comment_;
    };
 //=============================================================================
    typedef QVector<QString> Lines_t;
    class Script
    {
        Script(      QString  const* filepath = nullptr
              ,      Lines_t  const* lines    = nullptr
              , cfg::Config_t const* config   = nullptr
              );
    private:
        QVector<QScopedPointer<ShellCommand>> lines_;
        QString filepath_;
    };
 //=============================================================================
}// namespace pbs


#endif // JOBSCRIPT_H
