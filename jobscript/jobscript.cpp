#include "jobscript.h"
#include <datetime_now.h>

#include <QRegularExpression>
#include <QFile>
#include <QTextStream>

#include <cstddef>
#include <stdexcept>
#include <algorithm>

namespace pbs
{//-----------------------------------------------------------------------------
    std::shared_ptr<ShellCommand> parse(QString const & line) {
        return std::shared_ptr<ShellCommand>( ShellCommand::parse(line) );
    }
 //-----------------------------------------------------------------------------
    Script::
    Script
      (       QString const& filepath
      ,       QString const& text
      , cfg::Config_t const& config
      )
      : Text( QString() )
    {
        this->filepath_ = filepath;
     // create default lines. which store dummy parameters
        this->add( Shebang::default_value);
        this->add( QString("#LAU generated_on = %1") .arg( toolbox::now() ));
        this->add( QString("#LAU      cluster = %1") .arg( cfg_get_str(config,"cluster","unknown") ));
        this->add( QString("#LAU      nodeset = %1") .arg( cfg_get_str(config,"nodeset","unknown") ));

        this->add( QString("#PBS -l nodes=%1:ppn=%2").arg(cfg_get_str(config,"nodes",1) )
                                                     .arg(cfg_get_str(config,"ppn"  ,1) ) );

        this->add( QString("#PBS -l walltime=%1")    .arg(cfg_get_str(config,"walltime","1:00:00") ));

        this->add( QString("#PBS -M %1")             .arg(cfg_get_str(config,"notify_M","your.email@address.here") ));
        this->add( QString("#PBS -m %1")             .arg(cfg_get_str(config,"notify_m","e") ));

        this->add( QString("#PBS -W x=nmatchpolicy:exactnode"));

        this->add( QString("#PBS -N dummy"), true);

        this->add( QString("#"));
        this->add( QString("#--- shell commands below ").leftJustified(80,'-') );
        this->add( QString("cd $PBS_O_WORKDIR") );

        this->set_is_modified();
//        this->_unsaved_changes = True:
     // parse file, then lines
        this->read(filepath);
        this->parse(text);
    }
 //-----------------------------------------------------------------------------
    void
    Script::
    add(const QString &line, bool hidden, bool parsing_ )
    {
     // Note that directly calling add acts like calling parse() with additive=True
     // _parsing must be true when called by self.parse(), it affects the bookkeeping
     // of unsaved_changes
        std::shared_ptr<ShellCommand> p_new_script_line( pbs::parse(line) );
        p_new_script_line->setHidden(hidden);
        types::Type new_type = p_new_script_line->type();
        switch(new_type) {
        case types::ShellCommand:
        case types::UserComment:
            this->insert_(p_new_script_line);
            break;
        default:
         // Find the corresponding line in the script
            int i = this->index(p_new_script_line);
            if( i==this->not_found ) {
                this->insert_(p_new_script_line);
            } else {
                bool was_modified = this->lines_[i]->copyFrom( p_new_script_line.get() );
                if( was_modified) {// also mark the script as modified
                    this->set_is_modified();
                }
            }
        }
        //            if not _parsing:
        //                self._unsaved_changes = True
    }
 //-----------------------------------------------------------------------------
    void
    Script::
    insert_(std::shared_ptr<ShellCommand> new_line )
    {
     // set_is_modified(self)
        ScriptLines_t::iterator iter =
            std::find_if( this->lines_.begin(), this->lines_.end(),
                [&] ( ScriptLine_t const& current ) {
                   if( current->ordinate()==-1)
                       return false;
                   return current->ordinate() > new_line->ordinate();
                }
            );
        if( iter==this->lines_.end() ) {
            this->lines_.append(new_line);
        } else {
            this->lines_.insert(iter,new_line);
        }
    }
 //-----------------------------------------------------------------------------
    void Script::compose()
    {
        for( ScriptLines_t::iterator iter=this->lines_.begin(); iter < this->lines_.end(); ++iter ) {
            this->text_.append( (*iter)->text() );
        }
    }
 //-----------------------------------------------------------------------------
    void
    Script::
    read( QString const& filepath, bool additive)
    {
        if( filepath.isEmpty() )
            return;
        QFile file(filepath);
        if( !file.open(QIODevice::ReadOnly | QIODevice::Text) )
            return;
        this->filepath_ = filepath;
        QTextStream in(&file);

        QString text = in.readAll();
        this->parse( text );
    }
 //-----------------------------------------------------------------------------
    void
    Script::
    write( QString const& filepath, bool warn_before_overwrite, bool create_directories) const
    {
        QString new_filepath( filepath.isEmpty() ? this->filepath_ : filepath );
        if( new_filepath.isEmpty() )
            throw_<std::runtime_error>("Script::write() : no filepath provided.");
        /*
        TODO : convert to c++

        new_filepath = os.path.abspath(new_filepath)
        head,tail = os.path.split(new_filepath)
        if create_directories:
            my_makedirs(head)
        else:
            if not os.path.exists(head):
                raise InexistingParentFolder(head)

        if warn_before_overwrite and os.path.exists(new_filepath):
            raise AttemptToOverwrite(new_filepath)

        with open(new_filepath,'w+') as f:
            f.write(self.get_text())

        self.filepath = new_filepath

        self.set_unsaved_changes(False)
        */
    }
 //-----------------------------------------------------------------------------
    void Script::parse( QString const& text, bool additive )
    {
        if( text.isEmpty() )
             return;
        QStringList lines = text.split("\n");
        if( !additive )
        {// This case is when the text represents the full script - possibly containing new
         // information such as new lines, old lines with new parameter values or features...
         // Because we cannot know whether new lines of type UserComment and ShellComand
         // are equal to existing lines in the script as they may occur more than once.
         // Hence we first remove all UserComments and ShellComands from the current script:
            for( int i=this->lines_.size(); i>-1; --i)
            {
                ScriptLine_t const& sl = this->lines_[i];
                std::cout<<std::endl
                    <<sl->text()    .toStdString()<<std::endl
                    <<sl->typeName().toStdString()<<std::endl;
                if( sl->type()==types::ShellCommand
                 || sl->type()==types::UserComment ) {
                    this->lines_.remove(i);
                    std::cout<<"/tdeleted"<<std::endl;
                }
            }
        } else
        {// No special action required here:
         // All UserComment and ShellCommands are appended at the end, in the
         // order presented, thus allowing several occurrences of the same line.
        }
        std::for_each( lines.cbegin(), lines.cend(),
            [&] ( QString const& line ) {
                this->add(line);
            }
        );
    }
 //-----------------------------------------------------------------------------
    int Script::index( QString const& line ) const {
        int i = index( pbs::parse(line) );
        return i;
    }
 //-----------------------------------------------------------------------------
    void Script::remove( int i ) {
        if( i<0 || i>= this->lines_.size() )
            throw_<std::range_error>("Out of range: Script::remove(i), i=%1",i);
        this->lines_.remove(i);
    }
 //-----------------------------------------------------------------------------
    int Script::index( ScriptLine_t const& line ) const
    {
        for( int i=0; i<this->lines_.size(); ++i ) {
            bool b = ( line==this->lines_[i] );
            if( b )
                return i;
        }
        return this->not_found;
    }
 //-----------------------------------------------------------------------------
    QString const&
    Script::
    operator[]( QString const& key ) const
    {
        if( key[0]=='-' ) {
            ScriptLines_t::const_iterator iter =
                std::find_if( this->lines_.cbegin(), this->lines_.cend(),
                    [&] ( ScriptLine_t const& current ) {
                        bool b = ( current->flag()==key );
                        return b;
                    }
                );
            if( iter != this->lines_.cend() ) {
                return (*iter)->value();
            }
        } else {
            std::for_each( this->lines_.cbegin(), this->lines_.cend(),
                [&] ( ScriptLine_t const& current ) {
                    if( current->parameters().contains(key) ) {
                        QString const& val = current->parameters()[key];
                        return val;
                    }
                }
            );
        }
        throw_<std::range_error>("Key '%1' was not found in script.",key);
    }
 //-----------------------------------------------------------------------------
    QString &
    Script::
    operator[]( QString const& key )
    {
        if( key[0]=='-' ) {
            ScriptLines_t::iterator iter =
                std::find_if( this->lines_.begin(), this->lines_.end(),
                    [&] ( ScriptLine_t & current ) {
                        bool b = ( current->flag()==key );
                        return b;
                    }
                 );
            if( iter != this->lines_.end() )
            {// we can't verify if the value is really changed, but
             // as it is the purpose of this member function we assume
             // it has, hence we call set_is_modified() before the
             // is modified
                (*iter)->set_is_modified();
                return (*iter)->value_;
            }
        } else {
            std::for_each( this->lines_.begin(), this->lines_.end(),
                [&] ( ScriptLine_t & current ) {
                    if( current->parameters().contains(key) )
                    {// we can't verify if the value is really changed, but
                     // as it is the purpose of this member function we assume
                     // it has, hence we call set_is_modified() before the
                     // is modified
                        current->set_is_modified();
                        QString & val = current->parameters()[key];
                        return val;
                    }
                }
            );
        }
        throw_<std::range_error>("Key '%1' was not found in script.",key);
    }
 //-----------------------------------------------------------------------------
    void
    Script::
    add_parameters( QString const& key, Parameters_t const& parms )
    {
        if( key[0]=='-' ) {
            throw_<std::runtime_error>("Script line corresponding to key '%1' does not accept parameters.",key);
        } else {
            ScriptLine_t& sl_key = this->lines_[this->index(key)];
            switch(sl_key->type() ) {
            case(types::LauncherComment):
            case(types::PbsDirective):
                //TODO

                break;
            default:
                throw_<std::runtime_error>("Script line corresponding to key '%1' does not accept parameters.",key);
            }
        }
        /*
            for l in self.script_lines:
                l_parms = getattr(l,'parms',{})
                if l_parms:
                    if key in l_parms:
                        if remove:
                            for k in parms.iterkeys():
                                del l_parms[k]
                        else:
                            l_parms.update(parms)
                        set_is_modified(l)
                        set_is_modified(self)
                        return
            raise KeyError("Key '"+key+"' not found in script.")
      */
}
 //-----------------------------------------------------------------------------
    void
    Script::
    remove_parameters( QString const& key, Parameters_t const& parms )
    {
        if( key[0]=='-' ) {
            throw_<std::runtime_error>("Script line corresponding to key '%1' does not accept parameters.",key);
        } else {
            ScriptLine_t& sl_key = this->lines_[this->index(key)];
            switch(sl_key->type() ) {
            case(types::LauncherComment):
            case(types::PbsDirective):
                //TODO

                break;
            default:
                throw_<std::runtime_error>("Script line corresponding to key '%1' does not accept parameters.",key);
            }
        }

    }

 //-----------------------------------------------------------------------------
}// namespace pbs
