#ifndef SSH2_H
#define SSH2_H

#include <libssh2.h>
#include <toolbox.h>

#include <QString>
#include <QMap>

namespace ssh2
{//=============================================================================
    typedef QMap<QString,QString> RemoteCommands_t;
    class Session;
 //=============================================================================
    class ExecuteRemoteCommand
 //=============================================================================
    {
    public:
        ExecuteRemoteCommand( Session* ssh2_session )
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
        Session* ssh2_session_;
        RemoteCommands_t const* remote_commands_;
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

        bool setPrivatePublicKeyPair
                ( QString const& private_key
                , QString const& public_key = QString()
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

        const char * privateKey() const { return this->private_key_.c_str(); }
        const char *  publicKey() const { return this-> public_key_.c_str(); }
         // read only accessors

        bool setPassphrase( QString const & passphrase, bool must_throw=true );
         // set the passphrase
         // throws a std::runtime_error if the username or private/public key pair
         // have not been successfully set before.

        void authenticate() { this->open(); }
        void open();
         /* tries to open a ssh session with the current username, private/public
          * key pair and passprhase. If unsuccessfull throws
          *   . MissingUsername
          *   . MissingKey
          *   . PassphraseNeeded
          *   . WrongPassphrase
          *   . std::runtime_error
          * If successfull, the session is left open if Session::autoClose is false,
          * and the session is ready to execute commands. If Session::autoClose is
          * true the session is closed.
          */
        bool isOpen() const;
         /* returns true if the session was successfully opened, and is ready to
          * execute commands (exec())
          */
        bool isAuthenticated() const { return this->isAuthenticated_; }
         /* returns true if the session was successfully opened, and is ready to
          * execute commands (exec())
          */

        void close( bool keep_libssh_init=true );
        void reset();
         // close and clear authentication members

        static bool autoOpen;
        static bool autoClose;
        static int  verbose;

        static void cleanup();
        //void print( std::ostream& ostrm = std::cout ) const;
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
                  , passphrase_
                  , private_key_
                  , public_key_
                  ;
     // set by Session::execute()
        QString qout_, qerr_;
     // set by Session::exec_
        int   cmd_exit_code_;
        char* cmd_exit_signal_;
        int bytecount_[2];
        bool isAuthenticated_;
    public:
        ssh2::ExecuteRemoteCommand execute_remote_command;

    private:
        void exec_( QString const& command_line
                  , QString* qout=nullptr
                  , QString* qerr=nullptr
                  );
    };
 //=============================================================================
    SUBCLASS_EXCEPTION( MissingLoginNode       , std::runtime_error )
    SUBCLASS_EXCEPTION( MissingUsername        , std::runtime_error )
    SUBCLASS_EXCEPTION( MissingKey             , std::runtime_error )
    SUBCLASS_EXCEPTION( PassphraseNeeded       , std::runtime_error )
    SUBCLASS_EXCEPTION( WrongPassphrase        , std::runtime_error )
    SUBCLASS_EXCEPTION( RemoteExecutionFailed  , std::runtime_error )
    SUBCLASS_EXCEPTION( FileOpenError          , std::runtime_error )
    SUBCLASS_EXCEPTION( SshOpenError           , std::runtime_error )
    SUBCLASS_EXCEPTION( ConnectTimedOut        , std::runtime_error )
    SUBCLASS_EXCEPTION( NoAddrInfo             , std::runtime_error )
    SUBCLASS_EXCEPTION( InexistingRemoteCommand, std::runtime_error )
#ifdef Q_OS_WIN
    SUBCLASS_EXCEPTION( WinSockFailure         , std::runtime_error )
#endif
 //=============================================================================
}// namespace ssh2

#endif // SSH2_H
