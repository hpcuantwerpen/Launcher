    #ifndef JOBSCRIPT_H
#define JOBSCRIPT_H

#include <QString>
//#include <QVector>
#include <QList>

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
    typedef QList<ShellCommand*>          ScriptLines_t;
 //=============================================================================
    class Script : public Text
    {
    public:
        Script(     QString const& filepath = QString()
              ,     QString const& text     = QString()
              , cfg::Config const& config   = cfg::Config()
              );
        ~Script();

        void read ( QString const& filepath,  bool additive=false );
        void write( QString const& filepath    = QString()
                  , bool warn_before_overwrite = true
                  , bool create_directories    = false ) const;

        void parse( QString const& text, bool additive=false );
        void add  ( QString const& line,bool hidden=false, bool parsing_=false );

        virtual void compose();
        int index( ShellCommand const* line ) const;
        int index( QString      const& line ) const;
        enum {not_found = -1};
        QString const& operator[]( QString const& key ) const;
        QString      & operator[]( QString const& key );
         // access the value associated with key

        void    add_parameters( QString const& key, Parameters_t const& parms );
        void remove_parameters( QString const& key, Parameters_t const& parms );

        void    add_features( QString const& key, Features_t const& features );
        void remove_features( QString const& key, Features_t const& features );

        ShellCommand* find_key( QString const& key );
         // find the script line associated with key

    private:
        ScriptLines_t lines_;
        QString filepath_;
        void insert_(ShellCommand* sc);
        void remove_( int i );
    };
 //=============================================================================
}// namespace pbs


#endif // JOBSCRIPT_H
