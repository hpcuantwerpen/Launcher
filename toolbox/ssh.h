#ifndef SSH_H
#define SSH_H

#include "external.h"
#include "property.h"
#include <iostream>
#include <QString>
#include <QMap>

// #define DONT_PING_TO_GENERIC_LOGIN_NODE
// #define DONT_PING_TO_INDIVIDUAL_LOGIN_NODES
   #define NO_TESTS_JUST_SSH // run no tests, authenticate rightaway.

namespace toolbox
{
    class Ssh;
    class SshImpl
    {
    public:
        SshImpl();
        virtual ~SshImpl();
        virtual int remote_execute( QString const& remote_cmd, int secs, QString const& comment=QString() ) = 0;
        virtual int local_save          ( QString const& local_jobfolder_path ) = 0;
        virtual int local_sync_to_remote( QString const& local_jobfolder_path, QString const& remote_jobfolder_path, bool save_first=true ) = 0;
        virtual int remote_save         ( QString const& local_jobfolder_path, QString const& remote_jobfolder_path ) = 0;
        virtual int remote_sync_to_local( QString const& local_jobfolder_path, QString const& remote_jobfolder_path, bool save_first=true ) = 0;
        virtual bool local_exists       ( QString const& local_jobfolder_path ) = 0;
        virtual bool remote_exists      (                                      QString const& remote_jobfolder_path ) = 0;
        virtual void adjust_homedotsshconfig( QString const& /*cluster*/ ) {}
        QString const& host() const { return host_; }

    protected:
        Ssh* super_;
        QString host_;
    };

    class SshImpl_os_ssh : public SshImpl
    {
    public:
        SshImpl_os_ssh( Ssh* super ) { super_ = super; }
        virtual ~SshImpl_os_ssh();
        virtual int remote_execute( QString const& remote_cmd, int secs,QString const& comment=QString() );

        virtual int local_save          ( QString const& local_jobfolder_path );
        virtual int local_sync_to_remote( QString const& local_jobfolder_path, QString const& remote_jobfolder_path, bool save_first=true );
        virtual int remote_save         ( QString const& local_jobfolder_path, QString const& remote_jobfolder_path );
        virtual int remote_sync_to_local( QString const& local_jobfolder_path, QString const& remote_jobfolder_path, bool save_first=true );
    protected:
        virtual bool local_exists       ( QString const& local_jobfolder_path );
                 int local_create       ( QString const& local_jobfolder_path );
        virtual bool remote_exists      (                                      QString const& remote_jobfolder_path );
                 int remote_create      ( QString const& local_jobfolder_path, QString const& remote_jobfolder_path );
        virtual void adjust_homedotsshconfig( QString const& cluster );
    };

    class Ssh
    {
    public:
        Ssh( Log* log=nullptr );

        enum AuthenticationError
        {// ERROR CODES
            MISSING_USERNAME                        = -100000
          , MISSING_LOGINNODE                       = -100001
          , MISSING_PRIVATEKEY                      = -100002
          , NO_INTERNET                             = -100003
          , LOGINNODE_NOT_ALIVE                     = -100004
          , CONNECTION_FAILED                       = -100005
          , SSH_KEY_ERROR                           = -100006
          , IMPL_MEMBER_NOT_SET                     = -100100
          , REMOTE_COMMAND_NOT_DEFINED_BY_CLUSTER   = -100200
        };
        enum AuthenticationStatus
        {   AUTHENTICATION_FAILED = -1
          , NOT_AUTHENTICATED     =  0
          , AUTHENTICATED         =  1
        };
     // properties
        PROPERTY_Rw( QStringList, login_nodes, public, public, private )
        /* Get/set list of login nodes for the current cluster.
         * The setter also selects the first login node in the list as
         * the current login node. This is expected to be the generic
         * login node.
         */
      #ifndef NO_TESTS_JUST_SSH
        PROPERTY_RW( int, login_port, public, public, private )
      #endif//NO_TESTS_JUST_SSH

        QString login_node() const;
        bool set_login_node( QString const& login_node );
        bool set_login_node( int select );
         /* Get/set login_node to be used to access the cluster
          */
        PROPERTY_Rw( QString, username   , public, public, private )
        PROPERTY_Rw( QString, private_key, public, public, private )
        PROPERTY_Rw( QString, passphrase , public, public, private )

        PROPERTY_RW( Log*   , log        , public, public, private )
        PROPERTY_RW( bool   , verbose    , public, public, private )

      #ifndef NO_TESTS_JUST_SSH
        PROPERTY_RO( bool, connected_to_internet ) // internet connection ok? can ping 8.8.8.8
        PROPERTY_RO( bool, login_node_alive      ) // can ping to login_node?
      #endif //#ifndef NO_TESTS_JUST_SSH
        PROPERTY_RO( AuthenticationStatus, authentication_status )
        bool authenticated() const {
            return authentication_status_ == AUTHENTICATED;
        }
        PROPERTY_RW( bool, authenticated_before, public, public, private )
        PROPERTY_RO( SshImpl*, impl  )

        PROPERTY_RW( QString , standardError, public, public, private )
        PROPERTY_RW( QString , standardOutput, public, public, private )

      #ifndef NO_TESTS_JUST_SSH
        PROPERTY_RW( bool, telnet_available, public, protected, protected )
      #endif//NO_TESTS_JUST_SSH

        typedef QMap<QString,QString> RemoteCommandMap_t;
        void add_RemoteCommandMap( RemoteCommandMap_t const& map );
        void set_RemoteCommandMap( RemoteCommandMap_t const& map );

    public:
      #ifndef NO_TESTS_JUST_SSH
        bool test_login_nodes( QList<bool>* alive=nullptr );
         /* tests (ping) if all the login nodes can be reached and optionally
          * copies the result to *alive.
          * Returns true iff the cluster can be reached with the currently
          * selected login node, that is,
          *   . if the currently selected login node is the generic node,
          *     at least one specific login node must be alive,
          *   . if the currently selected login node is a specific node,
          *     that specific login node must be alive.
          * Failure to ping to the generic login node typically indicates
          * that there is no internet connection. (In very rare cases the site
          * is down).
          * Failure to ping to a specific node arises
          *   - if there is no internet connection
          *   - if there is internet connection, but you are off-campus and
          *     vpn is not connected
          *   - if the login node is down
          */
      #endif//NO_TESTS_JUST_SSH
        bool ping  ( QString const& to,           QString const& comment=QString() ) const;
        bool telnet( QString const& to, int port, QString const& comment=QString() ) const;

        AuthenticationError authenticate( QString& msg, QString& details ) const;
        bool set_impl( bool use_os );

        int execute( QString const& remote_cmd, int secs, QString const& comment=QString(), bool wrap=true ) const;

        int local_save( QString const& local_jobfolder_path );
        int local_sync_to_remote
          ( QString const&  local_jobfolder_path
          , QString const& remote_jobfolder_path
          , bool           save_first = true
          );
        int remote_save
          ( QString const&  local_jobfolder_path
          , QString const& remote_jobfolder_path
          );
        int remote_sync_to_local
          ( QString const&  local_jobfolder_path
          , QString const& remote_jobfolder_path
          , bool           save_first = true
          );
        bool  local_exists( QString const&  local_jobfolder_path );
        bool remote_exists( QString const& remote_jobfolder_path );

        int remote_size( QString const& remote_jobfolder_path );

        void adjust_homedotsshconfig( QString const& cluster );
         /* Set up home/.ssh/config for git to use the correct key
          * for pushing to the remote repository.
          */
        QString const& host() const;

        void log( QString const& s ) const;
        void log( QString const& comment, QStringList const& lst ) const;
        void log( char    const* s ) const;

    private:
        RemoteCommandMap_t remote_command_map_;
        int current_login_node_;
    };

    bool ssh_available( Log *log=nullptr );
    bool git_available( Log *log=nullptr );

}// namespace toolbox
#endif // SSH_H
