#ifndef SHELLCOMMAND_H
#define SHELLCOMMAND_H

#include <cfg.h>
#include <toolbox.h>

#include "text.h"

namespace pbs
{//=============================================================================
    namespace types
    {
        enum Type
        { ShellCommandType = 0
        , UserCommentType
        , ShebangType
        , LauncherCommentType
        , PbsDirectiveType
        };

        enum Position
        { DefaultPosition         = -9999
        , ShebangPosition         =     0
        , LauncherCommentPosition =     1
        , PbsDirectivePosition    =     2
        , ShellCommandPosition    =    10
        , UserCommentPosition     =    -1 // => keep where inserted
        , LastPosition            =  9999
        };
    }
 //=============================================================================
    typedef toolbox::OrderedSet Features_t;
    typedef toolbox::OrderedMap Parameters_t;

 //=============================================================================
 // fwd declarations
    class ShellCommand;
    ShellCommand* parse( QString const & line );
    template <class T> T* create( QString const& line );
    class Script;

 //=============================================================================
    class ShellCommand : public Text
 /* Syntax
  *   script line:
  *     [prefix] [flag] value [comment]
  *   value:
  *     string
  *     key=value[:key=value]*[:feature]*
  */
 //=============================================================================
    {
        friend ShellCommand* parse( QString const & line );
        template <class T>
        friend T* create( QString const& line );
        friend class Script;
    protected:
        ShellCommand
          ( QString const& line
          , types::Position position = types::ShellCommandPosition
          , types::Type     type     = types::ShellCommandType
          );
        //virtual void init();
    public:
        virtual ~ShellCommand() {}
        types::Position position() const { return this->position_; }
        void setPosition( types::Position pos ) { this->position_ = pos; }
        types::Type     type    () const { return this->type_;     }
        //QString const& parm_value( QString const & key ) const;

        virtual bool equals( ShellCommand const* rhs) const;
        virtual void copyFrom( ShellCommand const* rhs );

     // accessors
        Parameters_t const& parameters() const { return this->parameters_; }
        Parameters_t      & parameters()       { return this->parameters_; }

        Features_t const& features() const { return this->features_; }
        Features_t      & features()       { return this->features_; }

        QString const& value() const  { return this->value_; }
        void setValue(QString const& value ) {
            this->value_ = value;
            this->set_is_modified();
        }

        bool hidden() const { return this->hidden_; }
        void setHidden( bool hidden ) {
            if( this->hidden() != hidden )
                this->set_is_modified();
            this->hidden_ = hidden;
        }

        virtual void compose();
        QString const& flag() const { return flag_; }

        virtual QString toString( int verbose=0 , bool refresh=false );

    private:
        static ShellCommand* parse( QString const &line);
    private:
        types::Position position_;
        types::Type     type_;
    protected:
        QString         flag_;
        QString         value_;
        Parameters_t    parameters_;
        Features_t      features_;
        bool            hidden_;
    };
 //=============================================================================
    template <class T>
    T* create(QString const& line )
    {
        T* ptrT = new T(line);
        try {
            ptrT->init();
        } catch( std::logic_error& e ) {
            delete ptrT;
            ptrT = nullptr;
            throw e;
        }
        return ptrT;
    }
 //-----------------------------------------------------------------------------
}//namespace pbs

#endif // SHELLCOMMAND_H
