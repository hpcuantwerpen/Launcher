#include "jobscript.h"
#include <datetime_now.h>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>

#include <cstddef>
#include <stdexcept>
#include <algorithm>

namespace pbs
{//-----------------------------------------------------------------------------
    ShellCommand* parse(QString const & line) {
        return ShellCommand::parse(line);
    }
 //-----------------------------------------------------------------------------
    Script::
    Script
      (     QString const& filepath
      ,     QString const& text
      , cfg::Config const& config
      )
      : Text( QString() )
    {
        this->filepath_ = filepath;
     // create default lines. which store dummy parameters
        this->add( Shebang::default_value );
        this->add( QString("#LAU generated_on = %1") .arg( toolbox::now() ) );
        this->add( QString("#LAU         user = %1") .arg( cfg_get_str(config,"wUsername" ,"unknown") ) );
        this->add( QString("#LAU      cluster = %1") .arg( cfg_get_str(config,"wCluster"  ,"unknown") ) );
        this->add( QString("#LAU      nodeset = %1") .arg( cfg_get_str(config,"wNodeset"  ,"unknown") ) );
        this->add( QString("#LAU      n_cores = %1") .arg( cfg_get_str(config,"wNCores"   ,"unknown") ) );
        this->add( QString("#LAU  Gb_per_core = %1") .arg( cfg_get_str(config,"wGbPerCore","unknown") ) );
        this->add( QString("#LAU     Gb_total = %1") .arg( cfg_get_str(config,"wGbTotal"  ,"unknown") ) );


        this->add( QString("#PBS -l nodes=%1:ppn=%2").arg(cfg_get_str(config,"wNNodes"       ,1) )
                                                     .arg(cfg_get_str(config,"wNCoresPerNode",1) ) );

        this->add( QString("#PBS -l walltime=%1")    .arg(cfg_get_str(config,"walltime","1:00:00") ) );

        this->add( QString("#PBS -M %1")             .arg(cfg_get_str(config,"notify_M","your.email@address.here") ), true );
        this->add( QString("#PBS -m %1")             .arg(cfg_get_str(config,"notify_m","e") ), true );

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
        ShellCommand* p_new_script_line( pbs::parse(line) );
        p_new_script_line->setHidden(hidden);
        p_new_script_line->set_parent( this );
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
                this->lines_[i]->copyFrom( p_new_script_line );
                delete p_new_script_line;
            }
        }
        //            if not _parsing:
        //                self._unsaved_changes = True
    }
 //-----------------------------------------------------------------------------
    void
    Script::
    insert_(ShellCommand * new_line )
    {
        new_line->set_parent( this );

        if( new_line->ordinate() == -1 ) {
            this->lines_.append(new_line);
        } else {
            ScriptLines_t::iterator iter =
                std::find_if( this->lines_.begin(), this->lines_.end(),
                    [&] ( ShellCommand const* current ) {
                       return current->ordinate() > new_line->ordinate();
                    }
                );
            if( iter==this->lines_.end() ) {
                this->lines_.append(new_line);
            } else {
                this->lines_.insert(iter,new_line);
            }
        }
     // set_is_modified(self)
    }
 //-----------------------------------------------------------------------------
    void Script::compose()
    {
        QString script_text;
        for( ScriptLines_t::iterator iter=this->lines_.begin(); iter < this->lines_.end(); ++iter ) {
            ShellCommand* line = *iter;
            QString line_text = line->text();
            if( !line->hidden() ) {
                script_text.append( line_text );
            }
            std::cout<<"\n%%%%\n"<<this->text_.toStdString()<<std::endl;
        }
        this->text_ = script_text;
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
    struct WarnBeforeOverwrite: public std::runtime_error {
        WarnBeforeOverwrite( const char * what ) : std::runtime_error(what) {}
    };
 //-----------------------------------------------------------------------------
    void
    Script::
    write( QString const& filepath, bool warn_before_overwrite, bool create_directories) const
    {
        QString new_filepath( filepath.isEmpty() ? this->filepath_ : filepath );
        if( new_filepath.isEmpty() )
            throw_<std::runtime_error>("Script::write() : no filepath provided.");
        QFileInfo fileinfo( new_filepath );
        fileinfo.makeAbsolute();
        QDir dir( fileinfo.absolutePath() );
        if( create_directories ) {
            dir.mkpath("");
        } else {
            if( !dir.exists() ) {
                throw_<std::runtime_error>("Script::write() : inexisting folder: '%1'",fileinfo.filePath() );
            }
        }
        if( fileinfo.exists() && warn_before_overwrite ) {
            throw_<WarnBeforeOverwrite>("Warn before overwrite: '%1'",fileinfo.filePath() );
        }
        QFile f(fileinfo.filePath());
        f.open(QIODevice::Truncate|QIODevice::Text|QIODevice::WriteOnly);
        QTextStream out(&f);
        QString const& txt = this->text();
        out << txt;
        const_cast<Script*>(this)->filepath_ = new_filepath;
      //self.set_unsaved_changes(False)
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
            for( int i=this->lines_.size()-1; i>-1; --i)
            {
                ShellCommand const* sl = this->lines_[i];
//                std::cout<<std::endl
//                    <<sl->text()    .toStdString()<<std::endl
//                    <<sl->typeName().toStdString()<<std::endl;
                switch( sl->type() ) {
                  case types::ShellCommand:
                  case types::UserComment :
                    this->remove_(i);
//                    std::cout<<"/tdeleted"<<std::endl;
                    break;
                }
            }
        } else
        {// No special action required here:
         // All UserComment and ShellCommands are appended at the end, in the
         // order presented, thus allowing several occurrences of the same line.
        }
        std::for_each( lines.cbegin(), lines.cend(),
            [&] ( QString const& line )
            {   if( !line.isEmpty() )
                    this->add(line);
            }
        );
    }
 //-----------------------------------------------------------------------------
    int Script::index( QString const& line ) const
    {
        ShellCommand* ptr = pbs::parse(line);
        int i = index( ptr );
        delete ptr;
        return i;
    }
 //-----------------------------------------------------------------------------
    void Script::remove_( int i ) {
        if( i<0 || i>= this->lines_.size() )
            throw_<std::range_error>("Out of range: Script::remove(i), i=%1",i);
        ShellCommand* ptr = this->lines_[i];
        delete ptr;
        this->lines_.removeAt(i);
    }
 //-----------------------------------------------------------------------------
    int Script::index( ShellCommand const* line ) const
    {
        for( int i=0; i<this->lines_.size(); ++i ) {
            bool b = ( line->equals( this->lines_[i] ) );
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
        ShellCommand* line = const_cast<Script*>(this)->find_key(key);
        if( key[0]=='-' ) {
            return line->value_;
        } else {
            line->set_is_modified();
            QString & val = line->parameters()[key];
            return val;
        }
    }
 //-----------------------------------------------------------------------------
    QString &
    Script::
    operator[]( QString const& key )
    {
        ShellCommand* line = this->find_key(key);
        line->set_is_modified();
        if( key[0]=='-' ) {
            return line->value_;
        } else {
            QString & val = line->parameters()[key];
            return val;
        }
    }
 //-----------------------------------------------------------------------------
    void
    Script::
    add_parameters( QString const& key, Parameters_t const& parms )
    {
        if( key[0]=='-' ) {
            throw_<std::runtime_error>("Script line corresponding to key '%1' does not accept parameters.",key);
        } else {
            ShellCommand* line = this->find_key(key);
            switch(line->type() ) {
            case(types::LauncherComment):
            case(types::PbsDirective):
                if( line->parameters_.add( parms ) ) {
                    line->set_is_modified();
                }
                break;
            default:
                throw_<std::runtime_error>("Script line corresponding to key '%1' does not accept parameters.",key);
            }
        }
    }
 //-----------------------------------------------------------------------------
    void
    Script::
    remove_parameters( QString const& key, Parameters_t const& parms )
    {
        if( key[0]=='-' ) {
            throw_<std::runtime_error>("Script line corresponding to key '%1' does not accept parameters.",key);
        } else {
            ShellCommand* line = this->find_key(key);
            switch(line->type() ) {
            case(types::LauncherComment):
            case(types::PbsDirective):
                if( line->parameters_.remove( parms ) ) {
                    line->set_is_modified();
                }
                break;
            default:
                throw_<std::runtime_error>("Script line corresponding to key '%1' does not accept parameters.",key);
            }
        }
    }
 //-----------------------------------------------------------------------------
    void
    Script::
    add_features( QString const& key, Features_t const& features )
    {
        if( key[0]=='-' ) {
            throw_<std::runtime_error>("Script line corresponding to key '%1' does not accept features.",key);
        } else {
            ShellCommand* line = this->find_key(key);
            switch(line->type() ) {
            case(types::LauncherComment):
            case(types::PbsDirective):
                if( line->features_.add( features ) ) {
                    line->set_is_modified();
                }
                break;
            default:
                throw_<std::runtime_error>("Script line corresponding to key '%1' does not accept features.",key);
            }
        }
    }
 //-----------------------------------------------------------------------------
    void
    Script::
    remove_features( QString const& key, Features_t const& features )
    {
        if( key[0]=='-' ) {
            throw_<std::runtime_error>("Script line corresponding to key '%1' does not accept features.",key);
        } else {
            ShellCommand* line = this->find_key(key);
            switch(line->type() ) {
            case(types::LauncherComment):
            case(types::PbsDirective):
                if( line->features_.remove( features ) ) {
                    line->set_is_modified();
                }
                break;
            default:
                throw_<std::runtime_error>("Script line corresponding to key '%1' does not accept features.",key);
            }
        }
    }
 //-----------------------------------------------------------------------------
    Script::~Script()
    {
        for( int i=0; i<this->lines_.size(); ++i ) {
            ShellCommand* ptr = this->lines_[i];
            delete ptr;
        }
    }
 //-----------------------------------------------------------------------------
    ShellCommand*
    Script::
    find_key( QString const& key )
    {
        if( key[0]=='-' ) {
            ScriptLines_t::iterator iter =
                std::find_if( this->lines_.begin(), this->lines_.end(),
                    [&] ( ShellCommand* current ) {
                        bool b = ( current->flag()==key );
                        return b;
                    }
                 );
            if( iter != this->lines_.end() ) {
                return (*iter);
            }
        } else {
            for( int i=0; i<this->lines_.size(); ++i ) {
                ShellCommand* current = this->lines_[i];
                if( current->parameters().contains(key) ) {
                    return current;
                }
            }
        }
        throw_<std::range_error>("Key '%1' was not found in script.",key);
    }

 //-----------------------------------------------------------------------------
}// namespace pbs
