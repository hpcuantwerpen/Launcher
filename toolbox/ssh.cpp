#include "ssh.h"
#include "throw_.h"
#include "external.h"
#include <QDir>


namespace toolbox
{
    bool ssh_available( Log* log )
    {
        Execute x( nullptr, log );
        int rc = x("ssh -V",300,"Test availability of OS ssh command.");
        return rc==0;
    }

    bool git_available( Log *log )
    {
        Execute x( nullptr, log );
        int rc = x("git --version",300,"Test availability of git command.");
        return rc==0;
    }

 //=============================================================================
 // Ssh::
 //=============================================================================
    Ssh::Ssh( Log* log )
      : log_(log)
      , verbose_(false)
      , connected_to_internet_(false)
      , login_node_alive_(false)
      , authenticated_(false)
      , impl_(nullptr)
      , current_login_node_(-1)
    {
     // ping doesn't need impl_
        this->connected_to_internet_ = this->ping( "8.8.8.8", "test internet connection by pinging to google dns servers");
         // see http://etherealmind.com/what-is-the-best-ip-address-to-ping-to-test-my-internet-connection/
    }

    bool Ssh::set_login_nodes( QStringList const& login_nodes )
    {
        this->login_nodes_ = login_nodes;
        if( login_nodes.isEmpty() ) {
            this->current_login_node_ = -1;
        } else {
            this->set_login_node(0);
        }
        return true;
    }

    bool Ssh::set_login_node( int index )
    {
        if( index<0 || this->login_nodes().size()<=index ) {
            return false;
        } else {
            this->current_login_node_ = index;
            this->login_node_alive_ = false; // so it will be tested again.
            return true;
        }
    }

    bool Ssh::set_login_node( QString const& login_node )
    {
        int index = this->login_nodes().indexOf(login_node);
        if( index==-1 ) {
            return false;
        } else {
            this->current_login_node_ = index;
            this->login_node_alive_ = false;// so it will be tested again.
             // login node successfully set, not necessarily alive or reachable
            return true;
        }
    }

    QString Ssh::login_node() const
    {
        if( this->current_login_node_ == -1 ) {
            return QString();
        } else {
            return this->login_nodes_.at(this->current_login_node_);
        }
    }

    bool Ssh::ping( QString const& to, QString const& comment ) const
    {
#ifdef Q_OS_WIN
        QString ping_cmd("ping ");
#else
        QString ping_cmd("ping -c 1 ");
#endif
        ping_cmd.append( to );
        toolbox::Execute x( nullptr, this->log() );
        int rc = x( ping_cmd, 300, comment );
#ifdef Q_OS_WIN
        QString output = x.standardOutput();
        bool ok = !output.contains("Destination net unreachable");
        return ok;
#else
        return rc==0;
#endif
    }

    bool Ssh::test_login_nodes( QList<bool>* alive )
    {
        QList<bool> ok;
        for( int i=0; i<this->login_nodes_.size(); ++i ) {
            bool ok_i = this->ping( this->login_nodes_.at(i), "test login node");
            ok.append(ok_i);
        }
     // copy the individual results
        if( alive ) *alive = ok;
     // can we access cluster on the current login node?
        int index = this->current_login_node_;
        switch (index) {
        case 0: // generic login node
          { int n_alive = ok.count(true);
            this->login_node_alive_ = ( n_alive>1 );
          } break;
        default: // specific login node
          { bool ok_index = ok.at(index);
            this->login_node_alive_ = ok_index ;
          } break;
        }
        return this->login_node_alive();
    }

    bool Ssh::set_username( QString const& username )
    {
        this->username_ = username;
        this->private_key_.clear();
        this->passphrase_ .clear();
        this->authenticated_ = false;
     // cannot test at this point
        return true;
    }

    bool Ssh::set_private_key( QString const& filepath )
    {
        this->authenticated_ = false;
     // test existence
        if( !QFile(filepath).exists() ) {
            this->private_key_.clear();
            return false;
        } else {
            this->private_key_= filepath;
            return true;
        }
    }

    int Ssh::authenticate() const
    {
        const_cast<Ssh*>(this)->authenticated_ = false;
        if( !this->connected_to_internet() )
        {// retry
            const_cast<Ssh*>(this)->connected_to_internet_ = this->ping( "8.8.8.8", "test internet connection pinging to google dns servers");
            if( !this->connected_to_internet_ ) {
                this->log( QStringList() << "No internet connection."
                         , "Authentication failed.");
                return NO_INTERNET;
            }
        }
        if( this->login_node().isEmpty() ) {
            this->log( QStringList() << "No login node provided."
                     , "Authentication failed.");
            return MISSING_LOGINNODE;
        }
        if( !this->login_node_alive_ )
        {// retry
            bool ok = const_cast<Ssh*>(this)->test_login_nodes();
            if( !ok ) {
                this->log( QStringList() << "The login node is not alive."
                                         << QString("login node = ").append(this->current_login_node_)
                         , "Authentication failed.");
                return LOGINNODE_NOT_ALIVE;
            }
        }
        if( this->username_.isEmpty() ) {
            this->log( QStringList() << "No username provided."
                     , "Authentication failed.");
            return MISSING_USERNAME;
        }
        if( this->private_key_.isEmpty() ) {
            this->log( QStringList() << "The private key is missing."
                                     << QString("user=").append(this->username_).append('@').append(this->current_login_node_)
                     , "Authentication failed.");
            return MISSING_PRIVATEKEY;
        }
        int rc = this->execute("exit", 60, "Can authenticate?" );
        if( rc )
        {// failed
            this->log( QStringList() << "Wrong username/key/assphrase?"
                                     << QString("user=").append(this->username_).append('@').append(this->current_login_node_)
                                     << QString("private key: ").append(this->private_key_)
                                     << "Clearing username and private key."
                     , "Authentication failed.");

         // todo ? Under windows catch access denied errors due to keys with passprhases

         // clear the private key and the user name
            const_cast<Ssh*>(this)->private_key_.clear();
            const_cast<Ssh*>(this)->username_   .clear();
        } else {
            const_cast<Ssh*>(this)->authenticated_ = true;
        }
        return rc;
    }

    void Ssh::log( char const* s ) const
    {
        if( log_ ) {
            *log_ << s;
        }
    }

    void Ssh::log( QString const& s ) const
    {
        if( log_ ) {
            *log_ << s.toLatin1().constData();
        }
    }

    void Ssh::log( QStringList const& list, QString const& comment ) const
    {
        if( this->log_ ) {
            std::ostream* po = this->log_->ostream0();
            if( po ) {
                *po << "\n<<< [ "<< comment.toLatin1().constData() << " ]";
                for( int i=0; i < list.size(); ++i ) {
                    *po << "\n    " << list.at(i).toLatin1().constData();
                }
                *po << "\n>>>\n";
            }
            this->log_->cleanup();
        }
    }

    bool Ssh::set_impl( bool use_os )
    {
        if( this->impl_ ) {
            delete this->impl_;
            this->impl_ = nullptr;
        }
        if( use_os ) {
            bool ok  = ssh_available( this->log() );
                 ok &= git_available( this->log() );
            if( ok ) {
                this->impl_ = new SshImpl_os_ssh(this);
            }
            return ok;
        } else {
            return false;
        }
    }

    int Ssh::execute(QString const& remote_cmd, int msecs, QString const& comment , bool wrap) const
    {
        if( !this->impl_ ) {
            return IMPL_MEMBER_NOT_SET;
        }
        QString wrapped_cmd;
        if( wrap )
        {
            if( remote_cmd.startsWith("__") )
            {// this is a command key, get the corresponding command
                wrapped_cmd = this->remote_command_map_.value( remote_cmd );
                if( wrapped_cmd.isEmpty() ) {
                    return REMOTE_COMMAND_NOT_DEFINED_BY_CLUSTER;
                }
            } else {
                QString wrapper = this->remote_command_map_.value("wrapper");
                if( !wrapper.isEmpty() )
                {// wrap the command
                    wrapped_cmd = wrapper.arg(remote_cmd);
                } else {
                    wrapped_cmd = remote_cmd;
                }
            }
            this->log( QStringList() << QString("original: ").append(  remote_cmd )
                                     << QString("wrapped : ").append( wrapped_cmd )
                     , "remote command wrapped");
        } else {
            wrapped_cmd = remote_cmd;
        }
        int rc = this->impl_->remote_execute(wrapped_cmd,msecs,comment);
        return rc;
    }

    void Ssh::add_RemoteCommandMap( RemoteCommandMap_t const& map )
    {
        RemoteCommandMap_t::const_iterator iter = map.constBegin();
        while( iter != map.constEnd()) {
            this->remote_command_map_.insert( iter.key(), iter.value() );
            ++iter;
        }
    }

    void Ssh::set_RemoteCommandMap( RemoteCommandMap_t const& map )
    {
        this->remote_command_map_.clear();
        this->add_RemoteCommandMap( map );
    }

    int Ssh::local_save( QString const& local_jobfolder_path )
    {// forward to impl_
        return this->impl_->local_save(local_jobfolder_path);
    }

    int Ssh::local_sync_to_remote
      ( QString const&  local_jobfolder_path
      , QString const& remote_jobfolder_path
      , bool           save_first
      )
    {// forward to impl_
        return this->impl_->local_sync_to_remote(local_jobfolder_path,remote_jobfolder_path,save_first);
    }

    int Ssh::remote_save
      ( QString const&  local_jobfolder_path
      , QString const& remote_jobfolder_path )
    {// forward to impl_
        return this->impl_->remote_save(local_jobfolder_path,remote_jobfolder_path);
    }

    int Ssh::remote_sync_to_local
      ( QString const&  local_jobfolder_path
      , QString const& remote_jobfolder_path
      , bool           save_first
      )
    {// forward to impl_
        return this->impl_->remote_sync_to_local(local_jobfolder_path,remote_jobfolder_path,save_first);
    }

    bool Ssh::local_exists( QString const&  local_jobfolder_path )
    {// forward to impl_
        return this->impl_-> local_exists( local_jobfolder_path);
    }

    bool Ssh::remote_exists( QString const& remote_jobfolder_path )
    {// forward to impl_
        return this->impl_->remote_exists(remote_jobfolder_path);
    }

    int Ssh::remote_size( QString const& remote_jobfolder_path )
    {
        QString cmd = QString("du -s %1/.git").arg( remote_jobfolder_path );
        int rc = this->execute( cmd, 180, "Obtain repository size of remote job folder repository (Bytes).");
        if( rc==0 ) {
            QString output = this->standardOutput();
            rc = output.split('\t',QString::SkipEmptyParts)[0].trimmed().toInt();
        } else {
            rc = -1;
        }
        return rc;
    }
 //=============================================================================
 // SshImpl::
 //====b=========================================================================
    SshImpl::
    SshImpl()
      : super_(nullptr)
    {}

    SshImpl::
    ~SshImpl()
    {}

 //=============================================================================
 // SshImpl_os_ssh::
 //=============================================================================
    SshImpl_os_ssh::
    ~SshImpl_os_ssh()
    {}

    int
    SshImpl_os_ssh::
    remote_execute(QString const& remote_cmd, int msecs , const QString &comment)
    {// wrap the remote_cmd in a ssh command
        QString ssh_cmd = QString("ssh -i %1 %2@%3 \"%4\"")
                             .arg( this->super_->private_key() )
                             .arg( this->super_->username() )
                             .arg( this->super_->login_node() )
                             .arg( remote_cmd );

        toolbox::Execute x( nullptr, this->super_->log() );
        int rc = x( ssh_cmd, msecs, comment );
        this->super_->set_standardOutput( x.standardOutput() );
        this->super_->set_standardError( x.standardError () );
        return rc;
    }

    int
    SshImpl_os_ssh::
    local_save(QString const& local_jobfolder_path )
    {
        if(!this->local_exists(local_jobfolder_path) )
            this->local_create(local_jobfolder_path);

        toolbox::Execute x( nullptr, this->super_->log() );
        x.set_working_directory(local_jobfolder_path);
        int rc = 0;
        if( this->super_->verbose() ) {
            rc = x("git status",120,"local_save (git) step 1/3");
            if(rc) return rc;
        }
        rc = x("git add --verbose .",120,"local_save (git) step 2/3");
        if(rc) return rc;

        rc = x("git commit --all --message=\"local save\" ",120,"local_save (git) step 3/3");
        QString const& output( x.standardOutput() );
        if( rc && output.contains("nothing to commit") )
        {// this is ok too. (we cannot rely on "git allow -a --allow-empty -m 'blabla'"
         // because on hopper we have git 1.7.1 which does not support --allow-empty
            rc = 0;
        }
        return rc;
    }

    int
    SshImpl_os_ssh::
    local_sync_to_remote(QString const& local_jobfolder_path, QString const& remote_jobfolder_path, bool save_first )
    {
        int rc = 0;
        if ( save_first ) {
            rc = this->local_save( local_jobfolder_path );
            if( rc ) return rc;
        }

        if(!this->remote_exists( remote_jobfolder_path ) ) {
            rc = this->remote_create( local_jobfolder_path, remote_jobfolder_path );
            if(rc) return rc;
        }

        toolbox::Execute x( nullptr, this->super_->log() );
        x.set_working_directory(local_jobfolder_path);
        rc = x("git push origin master",3600,"local_sync_to_remote (git) step 1/2");
        if(rc) return rc;
        //todo : there must be a better way to do this than blocking! this may be a problem with large files.
        //the difficulty is the command below that requires the previous command to be finished

        QString cmd = QString("cd %1 && git reset --hard").arg(remote_jobfolder_path);
        rc = this->remote_execute(cmd,120,"local_sync_to_remote (git) step 2/2");
        return rc;
    }

    bool
    SshImpl_os_ssh::
    local_exists( QString const& local_jobfolder_path )
    {
        QDir jobfolder( local_jobfolder_path ); // assuming this exists
        bool exists = jobfolder.exists(".git");
        this->super_->log( QStringList() << ( exists ? "local repository exists." : "local repository does not exist." )
                         ,"local_exists (git) step 1/1");
        return exists;
    }

    int
    SshImpl_os_ssh::
    local_create( QString const& local_jobfolder_path )
    {
        toolbox::Execute x( nullptr, this->super_->log() );;
        x.set_working_directory(local_jobfolder_path);
        int rc = x("git init",120,"local_create (git) step 1/1");
        return rc;
    }

    int
    SshImpl_os_ssh::
    remote_save( const QString &local_jobfolder_path, QString const& remote_jobfolder_path )
    {
        int rc=0;
        if(!this->remote_exists( remote_jobfolder_path ) ) {
            rc = this->remote_create( remote_jobfolder_path, local_jobfolder_path );
            if(rc) return rc;
        }
        if( this->super_->verbose() ) {
            QString cmd = QString("cd %1 && git status").arg(remote_jobfolder_path);
            rc = this->remote_execute(cmd,300,"remote_save (git) step 1/3");
            if(rc) return rc;
        }

        QString cmd = QString("cd %1 && git add --verbose .").arg(remote_jobfolder_path);
        rc = this->remote_execute(cmd,300,"remote_save (git) step 2/3");
        if(rc) return rc;

        cmd = QString("cd %1 && git commit --all --message='remote save' ").arg(remote_jobfolder_path);
        rc = this->remote_execute(cmd,300,"remote_save (git) step 3/3");
        QString const& output( this->super_->standardOutput() );
        if( rc && output.contains("nothing to commit") )
        {// this is ok too. (we cannot rely on "git allow -a --allow-empty -m 'blabla'"
         // because on hopper we have git 1.7.1 which does not support --allow-empty
            rc = 0;
        }
        return rc;
    }

    int
    SshImpl_os_ssh::
    remote_sync_to_local(const QString &local_jobfolder_path, QString const& remote_jobfolder_path, bool save_first)
    {
        int rc = 0;
        if ( save_first ) {
            rc = this->remote_save( local_jobfolder_path, remote_jobfolder_path );
            if( rc ) return rc;
        }

        toolbox::Execute x( nullptr, this->super_->log() );
        x.set_working_directory(local_jobfolder_path);
        rc = x("git pull origin master",6000,"remote_sync_to_local (git) step 1/1");
        return rc;
        //todo : there must be a better way to do this than blocking! this may be a problem with large files.
        //the difficulty is the command below that requires the previous command to be finished
    }

    bool SshImpl_os_ssh::remote_exists( QString const& remote_jobfolder_path )
    {
        QString cmd = QString("ls %1/.git").arg( remote_jobfolder_path );
        int rc = this->remote_execute(cmd,60,"remote_exists (git) step 1/1");
        return rc==0;
    }

    int
    SshImpl_os_ssh::
    remote_create( const QString &local_jobfolder_path, QString const& remote_jobfolder_path )
    {
        int rc=-1;
        QString cmd = QString("mkdir -p ").append( remote_jobfolder_path );
        rc = this->remote_execute(cmd,120,"remote_create (git) step 1/3");
        if( rc ) return rc;

        cmd = QString("cd ")  .append( remote_jobfolder_path )
              .append(" && git init")
              .append(" && git config receive.denyCurrentBranch ignore")
              ;
        rc = this->remote_execute(cmd,120,"remote_create (git) step 2/3");
        if( rc ) return rc;

        toolbox::Execute x( nullptr, this->super_->log() );
        x.set_working_directory(local_jobfolder_path);
        cmd = QString("git remote add origin ssh://%1@%2:%3/%4")
                 .arg( this->super_->username() )
                 .arg( this->super_->login_node() )
                 .arg( remote_jobfolder_path )
                 .arg(".git")
                 ;
        rc = x(cmd,120,"remote_create (git) step 3/3");
        return rc;
    }

}// namespace toolbox
