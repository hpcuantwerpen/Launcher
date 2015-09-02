#ifndef SHELLCOMMAND_H
#define SHELLCOMMAND_H

#include <cfg.h>
#include <toolbox.h>

#include "text.h"

namespace pbs
{//=============================================================================
    namespace types {
        enum Type
        { ShellCommand
        , UserComment
        , Shebang
        , LauncherComment
        , PbsDirective
        };
    }
 //=============================================================================
    typedef toolbox::OrderedSet Features_t;
    typedef toolbox::OrderedMap Parameters_t;

 //=============================================================================
 // fwd declarations
    class ShellCommand;
    std::shared_ptr<ShellCommand> parse( QString const & line );
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
        friend std::shared_ptr<ShellCommand> parse( QString const & line );
        template <class T>
        friend T* create( QString const& line );
        friend class Script;
    protected:
        ShellCommand( QString const& line, int ordinate=10, types::Type type=types::ShellCommand );
        //virtual void init();
    public:
        virtual ~ShellCommand() {}
        int            ordinate() const { return this->ordinate_; }
        types::Type    type    () const { return this->type_;     }
        QString const& typeName() const { return this->typeName_;}
        //QString const& parm_value( QString const & key ) const;

        virtual bool equals( ShellCommand const* rhs) const;
        virtual bool copyFrom( ShellCommand const* rhs );

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

    private:
        static ShellCommand* parse ( QString const &line);
    private:
        int          ordinate_;
        types::Type  type_;
        QString      typeName_;
    protected:
        QString      flag_;
        QString      value_;
        QString      comment_;
        Parameters_t parameters_;
        Features_t   features_;
        bool         hidden_;
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
