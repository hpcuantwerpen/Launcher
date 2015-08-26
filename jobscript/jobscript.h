    #ifndef JOBSCRIPT_H
#define JOBSCRIPT_H

#include <QString>
#include <QVector>
#include <memory>

#include <cfg.h>
#include <toolbox.h>

#include "text.h"
#include "shellcommand.h"
#include "usercomment.h"
#include "shebang.h"
#include "launchercomment.h"
#include "pbsdirective.h"


namespace pbs
{//=============================================================================
 // ShellCommand
 //   +- UserComment
 //      +- Shebang
 //      +- LauncherComment
 //      +- PbsDirective
 //=============================================================================
    typedef std::shared_ptr<ShellCommand>  ScriptLine_t;
    typedef QVector<ScriptLine_t>          ScriptLines_t;
 //=============================================================================
    class Script : public Text
    {
    public:
        Script(       QString const& filepath = QString()
              ,       QString const& text    = QString()
              , cfg::Config_t const& config   = cfg::Config_t()
              );
        void read ( QString const& filepath,  bool additive=false );
        void write( QString const& filepath    = QString()
                  , bool warn_before_overwrite = true
                  , bool create_directories    = false ) const;

        void parse( QString const& text, bool additive=false );
        void add  ( QString const& line,bool hidden=false, bool parsing_=false );

        virtual void compose();
        int index( ScriptLine_t const& line ) const;
        int index( QString      const& line ) const;
        enum {not_found = -1};
        void remove( int i );
        QString const& operator[]( QString const& key ) const;
        QString      & operator[]( QString const& key );

        void    add_parameters( QString const& key, Parameters_t const& parms );
        void remove_parameters( QString const& key, Parameters_t const& parms );

        void    add_features( QString const& key, Features_t const& features );
        void remove_features( QString const& key, Features_t const& features );

    private:
        ScriptLines_t lines_;
        QString filepath_;
        void insert_(std::shared_ptr<ShellCommand> sc);
    };
 //=============================================================================
}// namespace pbs


#endif // JOBSCRIPT_H
