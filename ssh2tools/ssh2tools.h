#ifndef SSH2_H
#define SSH2_H

#include <libssh2.h>
#include <toolbox.h>

#include <QString>
#include <QMap>

namespace ssh
{//=============================================================================
    typedef QMap<QString,QString> RemoteCommands_t;
    class Libssh2Impl;
 //=============================================================================
    class ExecuteRemoteCommand
 //=============================================================================
    {
    public:
        ExecuteRemoteCommand( Libssh2Impl* ssh2_session )
          : ssh2_session_(ssh2_session)
          , remote_commands_(nullptr)
        {}
        void set_remote_commands( RemoteCommands_t const* remote_commands ) {
            this->remote_commands_ = remote_commands;
        }
        int execute( QString& cmd, QString const& arg1=QString(), QString const& arg2=QString() ) const;
         /* - If cmd starts with '__" it is treated as a key to the remote_commands_ map.
          *   InexistingRemoteCommand is thrown if the key is not found,
          * - Otherwise, the command is wrapped, if remote_commands_ contains a wrapper
          *   entry. (no warning is given if no wrapper is found).
          * - Next, If the arguments are not null, they are pasted into the command.
          * - Finally, the command is executed and the return code is returned.
          *   Output can be retrieved from the ssh2_session_
          * note that cmd is a QString&, not a QString const&, on return it contains
          * the command that was actually executed.
          */
        int operator() ( QString& cmd, QString const& arg1=QString(), QString const& arg2=QString() ) const {
            return this->execute( cmd, arg1, arg2 );
        }
    private:
        Libssh2Impl* ssh2_session_;
        RemoteCommands_t const* remote_commands_;
    };

 // base class
    class SessionImpl
    {

    };
 //=============================================================================
    class Session
 //=============================================================================
    {
    public:
        Session();
        ~Session();

        QString loginNode() const & { return this->login_node_.c_str(); }
        void setLoginNode( QString const& loginNode );

        QString username() const { return QString( username_.c_str() ); }
        bool setUsername( QString const& username );
         // returns true for non empty username
         // Automatically clears the private/public key pair and the passphrase.

        bool setPrivateKey( QString const& filepath );
         // set private key.
         // For the function to succeed the username must have been set
         // successfully (setUsername()) and the files private_key and
         // public_key must exist.
         // If public_key is the empty QString, the public key filename is
         // constructed by appending ".pub" to the private key filename.
         // Returns true in case of success.
         // If the function fails it returns false or throws if must_throw is true.

        QString const& privateKey() const { return this->private_key_.c_str(); }
         // read only accessors

        bool setPassphrase( QString const & passphrase, bool must_throw=true );
         // set the passphrase
         // throws a std::runtime_error if the username or private/public key pair
         // have not been successfully set before.
        bool hasPassphrase() const { return !this->passphrase_.empty(); }

    private:
       QString login_node_;
       QString username_;
       QString private_key_;
       QString passphrase_;
//       bool isAuthenticated_;

       SessionImpl* impl_;
    };

 //=============================================================================
    class ExternalSshImpl : public SessionImpl
 // This SessionImpl uses the OS's ssh command to access the cluster
 // on windows it is most easily provided by git
 //=============================================================================
    {

    };


 //=============================================================================
    class Libssh2Impl : public SessionImpl
 // This SessionImpl uses libssh2 to access the cluster
 //=============================================================================
    {
    public:
        Libssh2Impl();
        virtual ~Libssh2Impl();

        QString loginNode() const & { return this->login_node_.c_str(); }
        void setLoginNode( QString const& loginNode );

        QString username() const { return QString( username_.c_str() ); }
        bool setUsername( QString const& username );
         // returns true for non empty username
         // Automatically clears the private/public key pair and the passphrase.

        bool setPrivatePublicKeyPair
                ( QString const& private_key
#           ifndef NO_PUBLIC_KEY_NEEDED
                , QString const& public_key = QString()
#           endif
                , bool must_throw = true
                );
         // set public/private key pair (filenames).
         // For the function to succeed the username must have been set
         // successfully (setUsername()) and the files private_key and
         // public_key must exist.
         // If public_key is the empty QString, the public key filename is
         // constructed by appending ".pub" to the private key filename.
         // Returns true in case of success.
         // If the function fails it returns or throws if must_throw is true.

#ifndef NO_PUBLIC_KEY_NEEDED
        const char *  publicKey() const { return this-> public_key_.c_str(); }
#endif
        const char * privateKey() const { return this->private_key_.c_str(); }
         // read only accessors

        bool setPassphrase( QString const & passphrase, bool must_throw=true );
         // set the passphrase
         // throws a std::runtime_error if the username or private/public key pair
         // have not been successfully set before.
        bool hasPassphrase() const { return !this->passphrase_.empty(); }

        void open();
         /* tries to make ssh connection and authenticate
          *     connect()
          *     authenticate()
          */
        bool isOpen() const;

        void connect();
         /* tries to make a ssh connection to the current login node
          * If unsuccessful throws
          *   . MissingLoginNode
          *   . WinSockFailure (on windows)
          *   . NoAddrInfo
          *   . ConnectTimedOut
          *   . Libssh2Error
          * if successful isConnected() will return true;
          */
        bool isConnected() const { return isConnected_; }

        void authenticate();
         /* tries twith the current username, private/public
          * key pair and passprhase. If unsuccessful throws
          *   . MissingUsername
          *   . MissingKey
          *   . PassphraseNeeded
          *   . WrongPassphrase
          *   . std::runtime_error
          * If successful, the session is left open if Session::autoClose is false,
          * and the session is ready to execute commands. If Session::autoClose is
          * true the session is closed.
          */
        bool isAuthenticated() const { return this->isAuthenticated_; }

        void close( bool keep_libssh_init=true );
        void reset();
         // close() and clear authentication members

        static bool autoOpen;
        static bool autoClose;
        static int  verbose;

        static void cleanup();

        QString toString() const;

        int execute(QString const& cmd );
         /* Remotely execute the command cmd.
          * Output (stdout and stderr) is copied to the QString s returned
          * by qout() and qerr().
          * Throws if sth went wrong, also if the command returned a nonzero
          * exit code.
          */
        QString const& qout() const {
            return this->qout_;
        }
        QString const& qerr() const {
            return this->qerr_;
        }
//#define SCP
#ifdef SCP
        void scp_put_file( QString const& local_filepath, QString const& remote_filepath );
         /* scp file transfer of a single file, local >> remote.
          */
#endif
        size_t sftp_put_file( QString const&  local_filepath
                            , QString const& remote_filepath );
        size_t sftp_get_file( QString const&  local_filepath
                            , QString const& remote_filepath );
         /* sftp file transfer of a single file,
          *  - put implies local >> remote
          *  - get implies remote >> local
          */

        bool sftp_put_dir( QString const&  local_filepath
                         , QString const& remote_filepath
                         , bool           recurse    = true
                         , bool           must_throw = true
                         );
        bool sftp_get_dir( QString const&  local_filepath
                         , QString const& remote_filepath
                         , bool           recurse    = true
                         , bool           must_throw = true
                         );
         /* sftp file transfer of a complete directory and all files contained in it,
          *  - put implies local >> remote
          *  - get implies remote >> local
          * if recurse is true the function is applied recursively to all subdirectories of the
          * source directory.
          */

        QString get_env( QString const& env) ;
         /* Get the value of a remote environment variable
          */
     private:
        int sock_;
        LIBSSH2_SESSION *session_;
        static bool isInitializedLibssh_;
        std::string login_node_
                  , username_
                  , private_key_
#ifndef NO_PUBLIC_KEY_NEEDED
                  , public_key_
#endif
                  , passphrase_
                  ;
     // set by Session::execute()
        QString qout_, qerr_;
     // set by Session::exec_
        int   cmd_exit_code_;
        char* cmd_exit_signal_;
        int bytecount_[2];
        bool isAuthenticated_
           , isConnected_;
    public:
        ssh::ExecuteRemoteCommand execute_remote_command;

    private:
        void exec_( QString const& command_line
                  , QString* qout=nullptr
                  , QString* qerr=nullptr
                  );
    };
 //=============================================================================
    SUBCLASS_EXCEPTION( MissingLoginNode       , std::runtime_error )
#ifdef Q_OS_WIN
    SUBCLASS_EXCEPTION( WinSockFailure         , std::runtime_error )
#endif
    SUBCLASS_EXCEPTION( NoAddrInfo             , std::runtime_error )
    SUBCLASS_EXCEPTION( ConnectTimedOut        , std::runtime_error )
    SUBCLASS_EXCEPTION( Libssh2Error           , std::runtime_error )

    SUBCLASS_EXCEPTION( NotConnected           , std::runtime_error )
    SUBCLASS_EXCEPTION( MissingUsername        , std::runtime_error )
    SUBCLASS_EXCEPTION( MissingKey             , std::runtime_error )
    SUBCLASS_EXCEPTION( PassphraseNeeded       , std::runtime_error )
    SUBCLASS_EXCEPTION( WrongPassphrase        , std::runtime_error )
    SUBCLASS_EXCEPTION( Libssh2AuthError       , std::runtime_error )

    SUBCLASS_EXCEPTION( RemoteExecutionFailed  , std::runtime_error )

    SUBCLASS_EXCEPTION( FileOpenError          , std::runtime_error )
    SUBCLASS_EXCEPTION( SshOpenError           , std::runtime_error ) // used by exec_
    SUBCLASS_EXCEPTION( InexistingRemoteCommand, std::runtime_error )
 //=============================================================================
}// namespace ssh2

#endif // SSH2_H
