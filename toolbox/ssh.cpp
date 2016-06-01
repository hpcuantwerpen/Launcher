#include "ssh.h"
#include "throw_.h"
#include "external.h"
#include <QDir>
#include <QRegularExpression>

#define ONE_MINUTE 60
namespace toolbox
{
    bool is_ssh_available( Log* log )
    {
        Execute x( nullptr, log );
        int rc = x("ssh -V",ONE_MINUTE,"Test availability of OS ssh command.");
        return rc==0;
    }

    bool is_git_available( Log *log )
    {
        Execute x( nullptr, log );
        int rc = x("git --version",ONE_MINUTE,"Test availability of git command.");
        return rc==0;
    }

    bool is_rm_available( Log *log )
    {
        Execute x( nullptr, log );
        int rc = x("rm --help",ONE_MINUTE,"Test availability of git command.");
        return rc==0;
    }

#ifndef NO_TESTS_JUST_SSH
    bool is_telnet_available( Log *log )
    {
        Execute x( nullptr, log );
        int rc = x("echo \"^]close\" | telnet 127.0.0.1 22",ONE_MINUTE,"Test availability of telnet command.");
        return rc==0;
    }
#endif //#ifndef NO_TESTS_JUST_SSH


 //=============================================================================
 // Ssh::
 //=============================================================================
    Ssh::Ssh( Log* log )
      : log_(log)
      , verbose_(false)
    #ifndef NO_TESTS_JUST_SSH
      , connected_to_internet_(false)
      , login_node_alive_(false)
    #endif//NO_TESTS_JUST_SSH
      , authentication_status_(NOT_AUTHENTICATED)
      , authenticated_before_ (false)
      , impl_(nullptr)
      , current_login_node_(-1)
    {
      #ifndef NO_TESTS_JUST_SSH
        this->telnet_available_ = is_telnet_available( this->log() );
        if( this->telnet_available_ )
        {//
            this->connected_to_internet_ = true;
        } else
        {// (ping doesn't need impl_)
            this->connected_to_internet_ = this->ping( "8.8.8.8", "test internet connection by pinging to google dns servers");
             // see http://etherealmind.com/what-is-the-best-ip-address-to-ping-to-test-my-internet-connection/
        }
      #endif//NO_TESTS_JUST_SSH
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
          #ifndef NO_TESTS_JUST_SSH
            this->login_node_alive_ = false; // so it will be tested again.
          #endif//NO_TESTS_JUST_SSH
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
          #ifndef NO_TESTS_JUST_SSH
            this->login_node_alive_ = false;// so it will be tested again.
             // login node successfully set, not necessarily alive or reachable
          #endif//NO_TESTS_JUST_SSH
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
        int rc = x( ping_cmd, 0.5*ONE_MINUTE, comment );
      #ifdef Q_OS_WIN
        QString output = x.standardOutput();
        bool ok = !output.contains("Destination net unreachable");
        return ok && (rc==0);
      #else
        return rc==0;
      #endif
    }

    bool Ssh::telnet( QString const& to, int port, QString const& comment ) const
    {
        QString telnet_cmd("echo \"^]close\" | telnet %1 %2");
        telnet_cmd = telnet_cmd.arg( to ).arg( port );
        toolbox::Execute x( nullptr, this->log() );
        int rc = x( telnet_cmd, 0.5*ONE_MINUTE, comment );
        return rc==0;
    }

#ifndef NO_TESTS_JUST_SSH
 // todo fix or remove this this
    bool Ssh::test_login_nodes( QList<bool>* alive )
    {
        QList<bool> ok;
        if( this->telnet_available() )
        {// telnet to generic login node
            this->telnet( this->login_nodes_.at(0), this->login_port_, "test login node");
            for( int i=1; i<this->login_nodes_.size(); ++i ) {
               bool ok_i = this->telnet( this->login_nodes_.at(0), this->login_port_, "test login node");
               ok.append(ok_i);
           }
        } else
        {// try ping instead of telnet
         // ping to generic login node
            ok.append(
                #ifdef DONT_PING_TO_GENERIC_LOGIN_NODE
                       true
                #else
                       this->ping( this->login_nodes_.at(0), "test login node")
                #endif
                     );
            for( int i=1; i<this->login_nodes_.size(); ++i ) {
                bool ok_i =
                  #ifdef DONT_PING_TO_INDIVIDUAL_LOGIN_NODES
                        true; // assumed
                  #else
                        this->ping( this->login_nodes_.at(i), "test login node");
                  #endif
                ok.append(ok_i);
            }
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
#endif//NO_TESTS_JUST_SSH

    bool Ssh::set_username( QString const& username )
    {
        this->username_ = username;
        this->private_key_.clear();
        this->passphrase_ .clear();
        this->authentication_status_ = NOT_AUTHENTICATED;
        return !username.isEmpty();
    }

    bool Ssh::set_private_key( QString const& filepath )
    {
        this->authentication_status_ = NOT_AUTHENTICATED;
     // test existence
        if( !QFile(filepath).exists() ) {
            this->private_key_.clear();
            return false;
        } else {
            this->private_key_= filepath;
            return true;
        }
    }

    Ssh::AuthenticationError
    Ssh::authenticate( QString& msg, QString& details ) const
    {
        const_cast<Ssh*>(this)->authentication_status_ = NOT_AUTHENTICATED;
      #ifndef NO_TESTS_JUST_SSH
        if( !this->connected_to_internet() )
        {// retry
            const_cast<Ssh*>(this)->connected_to_internet_ = this->ping( "8.8.8.8", "test internet connection pinging to google dns servers");
            if( !this->connected_to_internet_ ) {
                this->log("Authentication failed:", QStringList() << "No internet connection." );
                return NO_INTERNET;
            }
        }
      #endif//NO_TESTS_JUST_SSH
        if( this->login_node().isEmpty() ) {
            msg = "Not authenticated: no login node provided.";
            this->log("Insufficient information for authentication:", QStringList() << "No login node provided.");
            return MISSING_LOGINNODE;
        }
      #ifndef NO_TESTS_JUST_SSH
        if( !this->login_node_alive_ )
        {// retry
            bool ok = const_cast<Ssh*>(this)->test_login_nodes();
            if( !ok ) {
                this->log("Authentication failed:"
                         , QStringList() << "The login node is not alive."
                                         << QString("login node = ").append(this->login_node() )
                         );
                return LOGINNODE_NOT_ALIVE;
            }
        }
      #endif//NO_TESTS_JUST_SSH
        if( this->username_.isEmpty() ) {
            msg = "Insufficient information for authentication: no username provided.";
            this->log("Insufficient information for authentication:", QStringList() << "No username provided.");
            return MISSING_USERNAME;
        }
        if( this->private_key_.isEmpty() ) {
            msg = "Insufficient information for authentication: no private key provided.";
            this->log("Insufficient information for authentication:", QStringList() << "No private key provided.");
            return MISSING_PRIVATEKEY;
        }
        int rc = this->execute("exit", 0.5*ONE_MINUTE, "Can authenticate?" );
        if( rc )
        {// failed
            const_cast<Ssh*>(this)->authentication_status_ = AUTHENTICATION_FAILED;
            QStringList lst;
         // Attempt to find out why ...
            if( this->standardError().contains("ssh: Could not resolve hostname") )
            {// no connection
                details = "Authentication failed:";
                lst << QString("Failed connecting to login-node '%1'.").arg(this->login_node())
                    << "\nPossible causes:"
                    << "\nVerify your internet connection, e.g.:"
                    << "    $> ping 8.8.8.8"
                    << "\nVerify that the login-nodes are up and running, e.g.:"
                    << QString("    $> ping %1").arg(this->login_node())
                    ;
                msg = "Authentication failed: connection could not be established.";
                rc = CONNECTION_FAILED;
            } else if( this->standardError().contains("debug1: Connecting to") )
            {// no connection
                details = "Authentication failed:";
                lst << QString("Failed connecting to login-node '%1'.").arg(this->login_node())
                    << "\nPossible causes:"
                    << "\nSome VSC clusters cannot be reached without a VPN connection when you are off-campus."
                    << "\nVerify that the login-nodes are up and running, e.g.:"
                    << QString("    $> ping %1").arg(this->login_node())
                    ;
                msg = "Authentication failed: connection could not be established.";
                rc = CONNECTION_FAILED;
            } else
            {// incorrect key use
                details = "Authentication failed : there is a problem with the private ssh key.";
                msg = "Authentication failed: there is a problem with the ssh key.";
                lst << QString("\nFailed connecting user=%1@%2").arg(this->username_).arg(this->login_node())
                    << QString("with private key: '%1'.").arg(this->private_key_)
                    << "\nPossible causes:"
                    << "\nThe wrong key was specified."
                  #ifdef Q_OS_WIN
                    << "\nWindows users must use a key without a passphrase."
                    << "\nThe key is not in openssh format (Putty keys must be converted first)."
                  #else
                    << "\nThe passphrase is wrong."
                  #endif
                    ;
                if( !this->authenticated_before() )
                {// clear the private key and the user name
                    lst << "\n(Clearing username and private key).";
                    const_cast<Ssh*>(this)->private_key_.clear();
                    const_cast<Ssh*>(this)->username_   .clear();
                }
                rc = SSH_KEY_ERROR;
            }
            this->log("Authentication failed:", lst );
            for( int i=0; i < lst.size(); ++i ) {
                details.append("\n").append( lst.at(i) );
            }
        } else {
            const_cast<Ssh*>(this)->authentication_status_ = AUTHENTICATED;
        }
        return static_cast<AuthenticationError>( rc );
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

    void Ssh::log( QString const& comment, QStringList const& lst ) const
    {
        if( this->log_ ) {
            std::ostream* po = this->log_->ostream0();
            if( po ) {
                *po << "\n<<< [ "<< comment.toLatin1().constData() << " ]";
                for( int i=0; i < lst.size(); ++i ) {
                    *po << "\n    " << lst.at(i).toLatin1().constData();
                }
                *po << "\n>>>\n" << std::flush;
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
        if( use_os )
        {// on mac osx and unix rm and ssh are usually preinstalled, and git
         // is installed easily.
         // on windows git comes with its own version of ssh and rm in its
         // Git\user\bin directory
            bool ok  = is_ssh_available( this->log() );
                 ok &= is_git_available( this->log() );
#ifdef Q_OS_WIN
                 ok &=  is_rm_available( this->log() );
                  // supposedly ok on mac osx and linux
#endif
            if( ok ) {
                this->impl_ = new SshImpl_os_ssh(this);
            }
            return ok;
        } else {
            return false;
        }
    }

    int Ssh::execute(QString const& remote_cmd, int secs, QString const& comment , bool wrap) const
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
            this->log("Remote command wrapped:"
                     , QStringList() << QString("original: ").append(  remote_cmd )
                                     << QString("wrapped : ").append( wrapped_cmd )
                     );
        } else {
            wrapped_cmd = remote_cmd;
        }
        int rc = this->impl_->remote_execute(wrapped_cmd,secs,comment);
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
        int rc = this->execute( cmd, 3*ONE_MINUTE, "Obtain repository size of remote job folder repository (Bytes).");
        if( rc==0 ) {
            QString output = this->standardOutput();
            rc = output.split('\t',QString::SkipEmptyParts)[0].trimmed().toInt();
        } else {
            rc = -1;
        }
        return rc;
    }

    void Ssh::adjust_homedotsshconfig( QString const& cluster ) {
        this->impl_->adjust_homedotsshconfig( cluster );
    }

    QString const& Ssh::host() const {
        return this->impl_->host();
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
    remote_execute(QString const& remote_cmd, int secs , const QString &comment)
    {// wrap the remote_cmd in a ssh command
        QString ssh_cmd = QString("ssh -v -i %1 %2@%3 \"%4\"")
                             .arg( this->super_->private_key() )
                             .arg( this->super_->username() )
                             .arg( this->super_->login_node() )
                             .arg( remote_cmd );

        toolbox::Execute x( nullptr, this->super_->log() );
        int rc = x( ssh_cmd, secs, comment );
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
            rc = x("git status",2*ONE_MINUTE,"local_save (git) step 1/3");
            if(rc) return rc;
        }
        rc = x("git add --verbose .",2*ONE_MINUTE,"local_save (git) step 2/3");
        if(rc) return rc;

        rc = x("git commit --all --message=\"local save\" ",2*ONE_MINUTE,"local_save (git) step 3/3");
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
        rc = x("git push --verbose origin master",3600,"local_sync_to_remote (git) step 1/2");
        if(rc) return rc;
        //todo : there must be a better way to do this than blocking! this may be a problem with large files.
        //the difficulty is the command below that requires the previous command to be finished

        QString cmd = QString("cd %1 && git reset --hard").arg(remote_jobfolder_path);
        rc = this->remote_execute(cmd,2*ONE_MINUTE,"local_sync_to_remote (git) step 2/2");
        return rc;
    }

    bool
    SshImpl_os_ssh::
    local_exists( QString const& local_jobfolder_path )
    {
        QDir jobfolder( local_jobfolder_path ); // assuming this exists
        bool exists = jobfolder.exists(".git");
        this->super_->log("local_exists (git) step 1/1"
                         , QStringList() << ( exists ? "local repository exists." : "local repository does not exist." )
                         );
        return exists;
    }

    int
    SshImpl_os_ssh::
    local_create( QString const& local_jobfolder_path )
    {
        toolbox::Execute x( nullptr, this->super_->log() );;
        x.set_working_directory(local_jobfolder_path);
        int rc = x("git init",2*ONE_MINUTE,"local_create (git) step 1/1");
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
            rc = this->remote_execute(cmd,ONE_MINUTE,"remote_save (git) step 1/3");
            if(rc) return rc;
        }

        QString cmd = QString("cd %1 && git add --verbose .").arg(remote_jobfolder_path);
        rc = this->remote_execute(cmd,ONE_MINUTE,"remote_save (git) step 2/3");
        if(rc) return rc;

        cmd = QString("cd %1 && git commit --all --message='remote save' ").arg(remote_jobfolder_path);
        rc = this->remote_execute(cmd,ONE_MINUTE,"remote_save (git) step 3/3");
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
        rc = x("git pull origin master",10*ONE_MINUTE,"remote_sync_to_local (git) step 1/1");
        return rc;
        //todo : there must be a better way to do this than blocking! this may be a problem with large files.
        //the difficulty is the command below that requires the previous command to be finished
    }

    bool SshImpl_os_ssh::remote_exists( QString const& remote_jobfolder_path )
    {
        QString cmd = QString("ls %1/.git").arg( remote_jobfolder_path );
        int rc = this->remote_execute(cmd, ONE_MINUTE, "remote_exists (git) step 1/1");
        return rc==0;
    }

    int
    SshImpl_os_ssh::
    remote_create( const QString &local_jobfolder_path, QString const& remote_jobfolder_path )
    {
        int rc=-1;
        QString cmd = QString("mkdir -p ").append( remote_jobfolder_path );
        rc = this->remote_execute(cmd,2*ONE_MINUTE,"remote_create (git) step 1/3");
        if( rc ) return rc;

        cmd = QString("cd ")  .append( remote_jobfolder_path )
              .append(" && git init")
              .append(" && git config receive.denyCurrentBranch ignore")
              ;
        rc = this->remote_execute(cmd,2*ONE_MINUTE,"remote_create (git) step 2/3");
        if( rc ) return rc;

        toolbox::Execute x( nullptr, this->super_->log() );
        x.set_working_directory(local_jobfolder_path);
        cmd = QString("git remote add origin ssh://%1@%2:%3/%4")
                 .arg( this->super_->username() )
                 .arg( this->host() )
                 .arg( remote_jobfolder_path )
                 .arg(".git")
                 ;
        rc = x(cmd,2*ONE_MINUTE,"remote_create (git) step 3/3");
        return rc;
    }

    void SshImpl_os_ssh::adjust_homedotsshconfig( QString const & cluster )
    {
        QDir dir( QDir::homePath() );
        dir.mkdir(".ssh");
        dir.cd(".ssh");
        this->host_ = QString("Launcher-%1-%2").arg(cluster).arg( this->super_->username() );
        QString new_entry =
            QString("Host %1\n"
                        "  HostName %2\n"
                        "  User %3\n"
                        "  IdentityFile %4\n"
                        "  IdentitiesOnly yes\n")
               .arg( this->host() )
               .arg( this->super_->login_node() )
               .arg( this->super_->username() )
               .arg( this->super_->private_key() )
               ;
        if( dir.exists("config") ){
            QFile dotssh_config(dir.filePath("config"));
            dotssh_config.open(QIODevice::ReadOnly);
            QString s = dotssh_config.readAll();
            dotssh_config.close();
            QStringList entries;
            bool found = false;
            QRegularExpression re("(\\w+)\\s+(.+)\\n(\\s+.+\\n)*");
            QRegularExpressionMatchIterator i = re.globalMatch(s);
            while (i.hasNext()) {
                QRegularExpressionMatch m = i.next();
                QString c0 = m.captured(0);
                QString c1 = m.captured(1);
                QString c2 = m.captured(2);
                if( c1.toLower()== "host" && c2==this->host() ) {
                    entries << new_entry;
                    found = true;
                } else {
                    entries << c0;
                }
            }
            if( !found ) {
                entries << new_entry;
            }
            dotssh_config.open(QIODevice::WriteOnly);
            for( int i=0; i<entries.size(); ++i) {
                dotssh_config.write( entries.at(i).toLatin1() );
            }
            dotssh_config.close();
        } else {
            QFile dotssh_config(dir.filePath("config"));
            dotssh_config.open(QIODevice::WriteOnly);
            dotssh_config.write( new_entry.toLatin1() );
            dotssh_config.close();
        }

    }

}// namespace toolbox
