#ifndef SSH_H
#define SSH_H

#include "external.h"
#include "property.h"
#include <iostream>
#include <QString>
#include <QMap>

namespace toolbox
{
    class Ssh;
    class SshImpl
    {
    public:
        SshImpl();
        virtual ~SshImpl();
        virtual int remote_execute( QString const& remote_cmd, int msecs, QString const& comment=QString() ) = 0;
        virtual int local_save          ( QString const& local_jobfolder_path ) = 0;
        virtual int local_sync_to_remote( QString const& local_jobfolder_path, QString const& remote_jobfolder_path, bool save_first=true ) = 0;
        virtual int remote_save         ( QString const& local_jobfolder_path, QString const& remote_jobfolder_path ) = 0;
        virtual int remote_sync_to_local( QString const& local_jobfolder_path, QString const& remote_jobfolder_path, bool save_first=true ) = 0;
    protected:
        Ssh* super_;
    };

    class SshImpl_os_ssh : public SshImpl
    {
    public:
        SshImpl_os_ssh( Ssh* super ) { super_ = super; }
        virtual ~SshImpl_os_ssh();
        virtual int remote_execute( QString const& remote_cmd, int msecs,QString const& comment=QString() );

        virtual int local_save          ( QString const& local_jobfolder_path );
        virtual int local_sync_to_remote( QString const& local_jobfolder_path, QString const& remote_jobfolder_path, bool save_first=true );
        virtual int remote_save         ( QString const& local_jobfolder_path, QString const& remote_jobfolder_path );
        virtual int remote_sync_to_local( QString const& local_jobfolder_path, QString const& remote_jobfolder_path, bool save_first=true );
    protected:
               bool local_exists        ( QString const& local_jobfolder_path );
                int local_create        ( QString const& local_jobfolder_path );
               bool remote_exists       (                                      QString const& remote_jobfolder_path );
                int remote_create       ( QString const& local_jobfolder_path, QString const& remote_jobfolder_path );
    };

    class Ssh
    {
    public:
        Ssh( Log* log=nullptr );

        enum
        {// ERROR CODES
            MISSING_USERNAME                        = -100000
          , MISSING_LOGINNODE                       = -100001
          , MISSING_PRIVATEKEY                      = -100002
          , NO_INTERNET                             = -100003
          , LOGINNODE_NOT_ALIVE                     = -100004
          , IMPL_MEMBER_NOT_SET                     = -100100
          , REMOTE_COMMAND_NOT_DEFINED_BY_CLUSTER   = -100200
        };
     // properties
        PROPERTY_Rw( QString, login_node , public, public, private )
        PROPERTY_Rw( QString, username   , public, public, private )
        PROPERTY_Rw( QString, private_key, public, public, private )
        PROPERTY_Rw( QString, passphrase , public, public, private )

        PROPERTY_RW( Log*   , log        , public, public, private )
        PROPERTY_RW( bool   , verbose    , public, public, private )

        PROPERTY_RO( bool, connected_to_internet ) // internet connection ok? can ping 8.8.8.8
        PROPERTY_RO( bool, login_node_alive      ) // can ping to login_node?
        PROPERTY_RO( bool, authenticated         ) // successfully authenticated before?
        PROPERTY_RO( SshImpl*, impl  )

        PROPERTY_RW( QString , standardError, public, public, private )
        PROPERTY_RW( QString , standardOutput, public, public, private )

        typedef QMap<QString,QString> RemoteCommandMap_t;
        void add_RemoteCommandMap( RemoteCommandMap_t const& map );
        void set_RemoteCommandMap( RemoteCommandMap_t const& map );

    public:
        bool ping(QString const& to , const QString &comment= QString()     ) const;
        int authenticate() const;
        bool set_impl( bool use_os );
        int execute( QString const& remote_cmd, int msecs, QString const& comment=QString(), bool wrap=true ) const;
        int local_save          ( QString const& local_jobfolder_path )                                                             { return this->impl_->local_save          (local_jobfolder_path); }
        int local_sync_to_remote( QString const& local_jobfolder_path, QString const& remote_jobfolder_path, bool save_first=true ) { return this->impl_->local_sync_to_remote(local_jobfolder_path,remote_jobfolder_path,save_first); }
        int remote_save         ( QString const& local_jobfolder_path, QString const& remote_jobfolder_path )                       { return this->impl_->remote_save         (local_jobfolder_path,remote_jobfolder_path); }
        int remote_sync_to_local( QString const& local_jobfolder_path, QString const& remote_jobfolder_path, bool save_first=true ) { return this->impl_->remote_sync_to_local(local_jobfolder_path,remote_jobfolder_path,save_first); }

        void log( QString const& s ) const;
        void log( QStringList const& s, QString const& comment=QString() ) const;
        void log( char    const* s ) const;
    private:
        RemoteCommandMap_t remote_command_map_;
    };

    bool ssh_available( Log *log=nullptr );
    bool git_available( Log *log=nullptr );

}// namespace toolbox
#endif // SSH_H
