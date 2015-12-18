#include "ssh2tools.h"
#include <log.h>

#include <libssh2_sftp.h>

#ifdef Q_OS_WIN
    #include <windows.h>
    #include <ws2tcpip.h>
    #define ssh2_gai_strerror gai_strerrorA
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #define closesocket close
    #define ssh2_gai_strerror gai_strerror
#endif

#include <cstring>
#include <stdexcept>

#include <QDir>
#include <QFileInfo>
#include <QTextStream>

#define LOG(VERBOSIITY) \
    QString _msg_("\n[ ssh2::Session::%1, %2, %3 ]");\
    _msg_ = _msg_.arg(__func__).arg(__FILE__).arg(__LINE__);\
    Log(VERBOSIITY) << INFO << _msg_.toStdString()
 // no terminating ';', hence can be used as
 //     LOG(1) << message << another_message;
 // caveat: because the macro has three stmts, this does NOT work
 //     if( condition )
 //         LOG(1) << "blablabla";
 // (compiler error undefined variable _msg_)
 // but this does:
 //     if( condition ) {
 //         LOG(1) << "blablabla";
 //     }
#define WARN \
    QString _msg_("\n[ ssh2::Session::%1, %2, %3 ]");\
    _msg_ = _msg_.arg(__func__).arg(__FILE__).arg(__LINE__);\
    Log(0) << WARNING << _msg_.toStdString()
 // no terminating ';', hence can be used as
 //     LOG(1) << message << another_message;

static int waitsocket(int socket_fd, LIBSSH2_SESSION *session)
{
    struct timeval timeout;
    int rc;
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir;

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    FD_ZERO(&fd);

    FD_SET(socket_fd, &fd);

    /* now make sure we wait in the correct direction */
    dir = libssh2_session_block_directions(session);


    if(dir & LIBSSH2_SESSION_BLOCK_INBOUND)
        readfd = &fd;

    if(dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
        writefd = &fd;

    rc = select(socket_fd + 1, readfd, writefd, NULL, &timeout);

    return rc;
}

namespace ssh2
{//-----------------------------------------------------------------------------
    Session::Session()
      : sock_(-1)
      , session_(nullptr)
      , isAuthenticated_(false)
      , execute_remote_command(this)
    {}
 //-----------------------------------------------------------------------------
    bool Session::isInitializedLibssh_ = false;
 //-----------------------------------------------------------------------------
    void Session::setLoginNode( QString const& login )
    {
        this->login_node_ = login.toStdString();
     // make sure that we re-connect and re-authenticate when the cluster is changed!
        this->isConnected_     = false;
        this->isAuthenticated_ = false;
    }
 //-----------------------------------------------------------------------------
    bool Session::setUsername( QString const& user )
    {
        this->reset();
        this->username_ = user.toStdString();
        return this->username_.empty();
    }
 //-----------------------------------------------------------------------------
    bool
    Session::
    setPrivatePublicKeyPair
      ( QString const& private_key
#   ifndef NO_PUBLIC_KEY_NEEDED
      , QString const& public_key
#   endif
      , bool           must_throw
      )
    {
        this->private_key_.clear();
#ifndef NO_PUBLIC_KEY_NEEDED
        this-> public_key_.clear();
#endif
        this-> passphrase_.clear();
        this->isAuthenticated_ = false;

        if( this->username_.empty() ) {
            if( must_throw ) { throw_<std::runtime_error>("Refused to set public/private key pair without username."); }
            else             { return false; }
        }

        if( !QFileInfo (private_key).exists() ) {
            if(must_throw) { throw_<std::runtime_error>("Private key '%1' does not exist.", private_key ); }
            else           { return false; }
        }
#ifdef NO_PUBLIC_KEY_NEEDED
        this->private_key_ = private_key.toStdString();
#else
        QString pub;
        if( public_key.isEmpty() ) {
            pub = private_key;
            pub.append(".pub");
        } else {
            pub = public_key;
        }
        if( !QFileInfo(pub).exists() ) {
            if(must_throw) { throw_<std::runtime_error>("Public key '%1' does not exist.", pub ); }
            else           { return false; }
        }
     // both files exist, store filenames and return true for success§
        this->private_key_ = private_key.toStdString();
        this-> public_key_ = pub        .toStdString();
#endif
        return true;
    }
 //-----------------------------------------------------------------------------
    bool Session::setPassphrase( QString const & passphrase, bool must_throw )
    {
        this->isAuthenticated_ = false;
        if( this->username_.empty() ) {
            if( must_throw ) { throw_<std::runtime_error>("Refused to set password without username."); }
            else             { return false; }
        }
        if( this->private_key_.empty()
          ) {
            if( must_throw ) { throw_<std::runtime_error>("Refused to set password without private key."); }
            else             { return false; }
        }
#ifndef NO_PUBLIC_KEY_NEEDED
        if( this->public_key_.empty()
          ) {
            if( must_throw ) { throw_<std::runtime_error>("Refused to set password without public key."); }
            else             { return false; }
        }
#endif
        this->passphrase_ = passphrase.toStdString();
        return true;
    }

 //-----------------------------------------------------------------------------
    void Session::open()
    {
        if( this->isOpen() ) {
            return;
        }

        this->connect();
        this->authenticate();
    }
 //-----------------------------------------------------------------------------
    void Session::connect()
    {
     // precondition
        if( this->login_node_.empty() ) {
            throw_<MissingLoginNode>("Connection error: Missing login node (don't know what to connect to.");
        }

        this->isConnected_     = false;
        this->isAuthenticated_ = false;
        int rv;
#ifdef Q_OS_WIN
     // see http://www.cplusplus.com/forum/windows/41339/
        WORD wVersionRequested;
        WSADATA wsaData;

     // Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h
        wVersionRequested = MAKEWORD(2, 2);
        rv = WSAStartup(wVersionRequested, &wsaData);
        if( rv!=0 ) {
            throw_<WinSockFailure>("WSAStartup failed with error %1.", rv);
        }

     /* Confirm that the WinSock DLL supports 2.2.
      * Note that if the DLL supports versions greater
      * than 2.2 in addition to 2.2, it will still return
      * 2.2 in wVersion since that is the version we
      * requested.
      */if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
        {   printf("Could not find a usable version of Winsock.dll\n");
            WSACleanup();
            throw_<WinSockFailure>("Could not find a usable version of Winsock.dll (version 2.2 needed).");
        }
#endif

        struct addrinfo hints, *servinfo, *p;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC; // use AF_INET6 to forve IPv6
        hints.ai_socktype = SOCK_STREAM;

        if( (rv=getaddrinfo( this->login_node_.c_str(), "22", &hints, &servinfo )) != 0 ) {
            throw_<NoAddrInfo>("getaddrinfo[%1] : %2", rv, ssh2_gai_strerror(rv) );
        }
     // loop through all the results and connect to the first we can
        for( p=servinfo; p!=NULL; p=p->ai_next)
        {
            if( (this->sock_ = socket( p->ai_family, p->ai_socktype, p->ai_protocol )) == -1 ) {
                WARN << "\n    socket(...):" << strerror(errno);
                continue; // try next element in the list
            }
            if( ::connect( this->sock_, p->ai_addr, p->ai_addrlen ) == -1 ) {
                ::close( this->sock_ );
                WARN << "\n    connect(...):"<<strerror(errno);
                continue; // try next element in the list
            }
            break; // if we get here, we must have connected successfully
        }
        if( p==NULL)
        {// looped off the end of the list with no connection
            freeaddrinfo(servinfo); // all done with this structure
            throw_<ConnectTimedOut>("Failed to connect.");
        }
     // libssh2 stuff...
        if( !Session::isInitializedLibssh_ )
        {
            if( (rv=libssh2_init(0)) != 0 ) {
                throw_<Libssh2Error>("Failed at initializing libssh2, exitcode = %1.", rv );
            }
            Session::isInitializedLibssh_ = true;
        }
        this->session_ = libssh2_session_init();
        if( !this->session_ ) {
            throw_<Libssh2Error>("Failed at initializing ssh2 session.");
        }
     // tell libssh2 we want it all done non-blocking
        libssh2_session_set_blocking(this->session_, 0);

     // ... start it up. This will trade welcome banners, exchange keys,
     // and setup crypto, compression, and MAC layers
        while( (rv = libssh2_session_handshake(this->session_, this->sock_)) == LIBSSH2_ERROR_EAGAIN);
        if( rv ) {
            throw_<Libssh2Error>("Failed at establishing SSH session, exitcode = %1>", rv );
        }
        this->isConnected_  = true;
/*
        LIBSSH2_KNOWNHOSTS* nh = libssh2_knownhost_init(this->session_);

        if( !nh )
        {// eeek, do cleanup here
            throw_<std::runtime_error>("Failed at initializing known hosts");
        }
     // read all hosts from here
        libssh2_knownhost_readfile(nh, "known_hosts", LIBSSH2_KNOWNHOST_FILE_OPENSSH);

     // store all known hosts to here
        libssh2_knownhost_writefile(nh, "dumpfile", LIBSSH2_KNOWNHOST_FILE_OPENSSH);

        size_t len;
        int type;
        const char *fingerprint = libssh2_session_hostkey(this->session_, &len, &type);
        if( fingerprint )
        {
            struct libssh2_knownhost *host;
#if LIBSSH2_VERSION_NUM >= 0x010206 // introduced in 1.2.6
            int check = libssh2_knownhost_checkp
                            ( nh, hostname, 22, fingerprint, len
                            , LIBSSH2_KNOWNHOST_TYPE_PLAIN|LIBSSH2_KNOWNHOST_KEYENC_RAW
                            , &host
                            );
#else // 1.2.5 or older
            int check = libssh2_knownhost_check
                            ( nh, hostname, fingerprint, len
                            , LIBSSH2_KNOWNHOST_TYPE_PLAIN|LIBSSH2_KNOWNHOST_KEYENC_RAW
                            , &host
                            );
#endif
            warn_("Host check: %1, key: %2\n"
                 , check
                 , (check <= LIBSSH2_KNOWNHOST_CHECK_MISMATCH) ? host->key : "<none>"
                 );
         // At this point, we could verify that 'check' tells us the key is fine or bail out.
        }
        else
        {// eeek, do cleanup here
            throw_<std::runtime_error>("No fingerprint");
        }
        libssh2_knownhost_free(nh);

        if ( strlen(password) != 0 )
        {// We could authenticate via password
            while( (rv = libssh2_userauth_password(this->session_, username, password)) == LIBSSH2_ERROR_EAGAIN );
            if (rv) {
                this->close();
                throw_<std::runtime_error>("\tAuthentication by password failed");
            }
        } else
        {// Or by public key
*/
    }
 //-----------------------------------------------------------------------------
    void Session::authenticate()
    {
        this->isAuthenticated_ = false;
        if( !this->isConnected() ) {
            throw_<NotConnected>("Attempt to authenticate without ssh2 connection.");
        }
        if( this->username().isEmpty() ) {
            throw_<MissingUsername>("Cannot authenticate without username.");
        }
        if( this->private_key_.empty() ) {
            throw_<MissingKey>("Cannot authenticate without private key.");
        }
#ifndef NO_PUBLIC_KEY_NEEDED
        if( this->public_key_.empty() ) {
            throw_<MissingKey>("Cannot authenticate without public key.");
        }
#endif
        int rv;
        while( ( rv = libssh2_userauth_publickey_fromfile
                        ( this->session_
                        , this->username_   .c_str()
#ifdef NO_PUBLIC_KEY_NEEDED
                        , nullptr
#else
                        , this->public_key_ .c_str()
#endif
                        , this->private_key_.c_str()
                        , this->passphrase_ .c_str()
                        )
                ) == LIBSSH2_ERROR_EAGAIN )
        {}

        if( rv!=0 ) {
            this->close();
            if( rv==LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED ) {
                if( this->passphrase_.empty() ) {
                    throw_<PassphraseNeeded>("Passphrase needed to unlock key '%1'."   , this->private_key_.c_str() );
                } else {
                    throw_<WrongPassphrase>("Could not unlock key '%1' (wrong passphrase?).", this->private_key_.c_str() );
                }
            } else {
                QString msg = QString("Failed at libssh2_userauth_publickey_fromfile, exitcode=%1.").arg(rv);
                if( rv==LIBSSH2_ERROR_FILE ) {
                    msg.append("\n(LIBSSH2_ERROR_FILE: The key file could not be read. Note that "
                               "key pairs generated with PuTTY Key Generator must be converted to "
                               "openssh format).");
                }
                if( rv==LIBSSH2_ERROR_AUTHENTICATION_FAILED ) {
                    msg.append("\n(LIBSSH2_ERROR_AUTHENTICATION_FAILED: possible username/key mismatch).");
                }
                throw_<Libssh2AuthError>( msg.C_STR() );
            }
        }
        this->isAuthenticated_ = true;
//      }
    }
 //-----------------------------------------------------------------------------
    bool Session::isOpen() const {
        return this->session_;
    }
 //-----------------------------------------------------------------------------
    bool Session::autoOpen = false;
    bool Session::autoClose= false;
    int  Session::verbose =
#ifdef QT_DEBUG
                            1;
#else
                            1;
#endif
 //-----------------------------------------------------------------------------
    void Session::exec_( QString const & command_line, QString* qout, QString* qerr )
    {
        if( !this->isOpen() ) {
            if( Session::autoOpen ) {
                this->open();
            } else {
                throw_<SshOpenError>("SshOpenError");
            }
        }
        int rv=0;
        std::string cmd = command_line.toStdString();

     // Exec non-blocking on the remote host
        LIBSSH2_CHANNEL *channel;
        while( (channel = libssh2_channel_open_session(this->session_)) == NULL
            && libssh2_session_last_error(this->session_,NULL,NULL,0) == LIBSSH2_ERROR_EAGAIN
             ) {
            waitsocket( this->sock_, this->session_ );
        }
        if( channel == NULL )
            throw_<std::runtime_error>("Error on libssh2_channel_open_session");

        while( (rv = libssh2_channel_exec( channel, cmd.c_str() )) == LIBSSH2_ERROR_EAGAIN )
            waitsocket(this->sock_, this->session_);

        if( rv != 0 )
           throw_<std::runtime_error>("Error onlibssh2_channel_exec");
     //----
        QString* qxxx[2];
        qxxx[0]=qout;
        qxxx[1]=qerr;
        std::string name[2] = {"stdout","stderr"};

        if( qout )
            qout->clear();
        if( qerr )
            qerr->clear();
        char buffer[0x4000];
        for( int iq=0;iq<2;++iq )
        {
            this->bytecount_[iq] = 0;
            if( !qxxx[iq] )
                continue;
            for( ;; )
            {// loop until we block
                do {
                    rv = ( iq==0
                         ? libssh2_channel_read       ( channel, buffer, sizeof(buffer) )
                         : libssh2_channel_read_stderr( channel, buffer, sizeof(buffer) )
                         );
                    if( rv > 0 )
                    {
                        this->bytecount_[iq] += rv;
                        if( Session::verbose > 1 ) {
                            std::cerr << '\n' << name[iq] << " buffer<<<";
                            for( int i=0; i < rv; ++i ) fputc( buffer[i], stderr);
                            std::cerr << ">>>";
                        }
                        buffer[rv] ='\0';
                        qxxx[iq]->append(buffer);
                    } else {
                        if( rv != LIBSSH2_ERROR_EAGAIN )
                        {// no need to output this for the EAGAIN case
//                            warn_("libssh2_channel_read returned %1\n", rv);
                        }
                    }
                } while( rv > 0 );

             // this is due to blocking that would occur otherwise so we loop on this condition
                if( rv == LIBSSH2_ERROR_EAGAIN ) {
                    waitsocket(this->sock_, this->session_);
                } else {
                    break;
                }
            }
        }
        while( (rv = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN )
            waitsocket(this->sock_, this->session_);

        this->cmd_exit_code_ = 127;
        this->cmd_exit_signal_ = nullptr;
        if( rv == 0 ) {
            this->cmd_exit_code_ = libssh2_channel_get_exit_status( channel );
            libssh2_channel_get_exit_signal(channel, &this->cmd_exit_signal_,NULL, NULL, NULL, NULL, NULL);
        } else {
            throw_<RemoteExecutionFailed>("Remote execution failed:\n    cmd: '%1'\n    rv :%2"
                                         ,command_line, rv );
        }

        libssh2_channel_free(channel);
        channel = nullptr;

        if( Session::autoClose ) {
            this->close();
        }
     }
 //-----------------------------------------------------------------------------
    int Session::execute( QString const& cmd )
    {
        try {
            if( Session::verbose ) {
                LOG(1) << "\nRemote execution of \n    command    : '" << cmd.C_STR() << "'";
            }
            this->exec_( cmd, &this->qout_, &this->qerr_ );

            if( Session::verbose || this->cmd_exit_code_ )
            {
                Log(1) << "\n    exit code  : " << this->cmd_exit_code_;
                if( this->cmd_exit_signal_ )
                Log(1) << "\n    exit signal: " << this->cmd_exit_signal_;
                if( !this->qout().isEmpty() ) {
                    Log(1) << "\n    stdout   :"
                              << "\n----------------\n"
                              << this->qout().C_STR()
                              << "\n----------------";
                }
                if( !this->qerr().isEmpty() ) {
                    Log(1) << "\n    stderr   :"
                              << "\n----------------\n"
                              << this->qerr().C_STR()
                              << "\n----------------";
                }
                Log(1) <<'\n';
            }
        } catch( std::runtime_error& e ) {
            std::cerr << e.what();
            return -9999;
        }
        return this->cmd_exit_code_;
    }
 //-----------------------------------------------------------------------------
#if defined SCP
    void
    Session::
    scp_put_file
      ( QString const&  q_local_filepath
      , QString const & q_remote_filepath )
    {
        std::string  local_filepath =  q_local_filepath.toStdString();
        std::string remote_filepath = q_remote_filepath.toStdString();
        char const*  p_local_filepath =  local_filepath.c_str();
        char const* p_remote_filepath = remote_filepath.c_str();
        if( Session::verbose ) {
            Log(1) << "\nFile transfer : local >> remote"
                      << "\n    local : '" << local_filepath << "'"
                      << "\n    remote: '" << remote_filepath << "'";
        }

        if( !this->isOpen() ) {
            if( Session::autoOpen ) {
                this->open();
            } else {
                throw_<SshOpenError>("SshOpenError");
            }
        }
     // adapted from http://www.libssh2.org/examples/scp_write_nonblock.html
        struct stat local_fileinfo;
        FILE *fh_local = fopen(p_local_filepath, "rb");
        if( !fh_local ) {
            throw_<FileOpenError>("Cannot open file '%1'.", q_remote_filepath );
        }

        stat(p_local_filepath, &local_fileinfo);

        LIBSSH2_CHANNEL *channel;
     // Send a file via scp. The mode parameter must only have permissions!
        do {
            channel = libssh2_scp_send( this->session_, p_remote_filepath, local_fileinfo.st_mode & 0777, (unsigned long)local_fileinfo.st_size );
            if( (!channel) && (libssh2_session_last_errno(this->session_) != LIBSSH2_ERROR_EAGAIN) ) {
                char *err_msg;
                libssh2_session_last_error( this->session_, &err_msg, NULL, 0);
                throw_<std::runtime_error>("p_local_filepath failed.\n    error: %1", err_msg );
            }
        } while( !channel );

        if( Session::verbose > 1 ) {
            fprintf(stderr, "SCP session waiting to send file\n");
        }
//        time_t start = time(NULL);
        size_t nread, prev;
        char mem[1024*100];
        char *ptr;
        long total = 0;
        int rc;
//        int duration;
        do {
            nread = fread( mem, 1, sizeof(mem), fh_local );
            if (nread <= 0) {// end of file
                break;
            }
            ptr = mem;
            total += nread;
            prev = 0;
            do {
                while( (rc = libssh2_channel_write(channel, ptr, nread)) == LIBSSH2_ERROR_EAGAIN ) {
                    waitsocket(this->sock_, this->session_);
                    prev = 0;
                }
                if (rc < 0) {
                    fprintf( stderr, "ERROR %d total %ld / %d prev %d\n", rc, total, (int)nread, (int)prev );
                    break;
                } else {
                    prev = nread;
                 // rc indicates how many bytes were written this time
                    nread -= rc;
                    ptr += rc;
                }
            } while( nread );
        } while( !nread ); /* only continue if nread was drained */

//        duration = (int)( time(NULL)-start );
//        fprintf(stderr, "%ld bytes in %d seconds makes %.1f bytes/sec\n", total, duration, total/(double)duration);
        if( Session::verbose > 1 ) {
            fprintf(stderr, "Sending EOF\n");
        }
        while (libssh2_channel_send_eof(channel) == LIBSSH2_ERROR_EAGAIN);
        if( Session::verbose > 1 ) {
            fprintf(stderr, "Waiting for EOF\n");
        }
        while (libssh2_channel_wait_eof(channel) == LIBSSH2_ERROR_EAGAIN);
        if( Session::verbose > 1 ) {
            fprintf(stderr, "Waiting for channel to close\n");
        }
        while (libssh2_channel_wait_closed(channel) == LIBSSH2_ERROR_EAGAIN);

        libssh2_channel_free(channel);
        channel = nullptr;

        if( Session::verbose ) {
            Log(1) << "\n    copied: " << (unsigned long)local_fileinfo.st_size << " bytes";
        }
    }
#endif//#ifdef SCP
 //-----------------------------------------------------------------------------
    size_t
    Session::
    sftp_put_file
      ( QString const&  q_local_filepath
      , QString const& q_remote_filepath )
    {
        std::string  local_filepath =  q_local_filepath.toStdString();
        std::string remote_filepath = q_remote_filepath.toStdString();
        if( Session::verbose ) {
            Log(1) << "\nFile transfer : local >> remote"
                    << "\n    local : '" <<  q_local_filepath.C_STR() << "'"
                    << "\n    remote: '" << q_remote_filepath.C_STR() << "'"
                    ;
        }

        if( !this->isOpen() ) {
            if( Session::autoOpen ) {
                this->open();
            } else {
                throw_<SshOpenError>("SshOpenError");
            }
        }

        int rc;
        LIBSSH2_SFTP *sftp_session;
        LIBSSH2_SFTP_HANDLE *sftp_handle;
        FILE *fh_local;
        char mem[512];

        fh_local = fopen( local_filepath.c_str(), "rb");
        if( !fh_local ) {
            throw_<FileOpenError>("Cannot open local file for reading: '%1'.", local_filepath.c_str() );
        }

        do {
            sftp_session = libssh2_sftp_init(this->session_);
            if( !sftp_session )
            {
                if( libssh2_session_last_errno(this->session_) == LIBSSH2_ERROR_EAGAIN ) {
                 // fprintf(stderr, "non-blocking init\n");
                    waitsocket( this->sock_, this->session_); /* now we wait */
                } else {
                    throw_<std::runtime_error>("Unable to init SFTP session.");
                }
            }
        } while( !sftp_session );

        do {
            sftp_handle = libssh2_sftp_open
                    ( sftp_session, remote_filepath.c_str()
                    , LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC
                    , LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH
                    );
            if( !sftp_handle )
            {
                if( libssh2_session_last_errno(this->session_) == LIBSSH2_ERROR_EAGAIN ) {
                 // fprintf(stderr, "non-blocking init\n");
                    waitsocket( this->sock_, this->session_); /* now we wait */
                } else {
                    char *err_msg;
                    int err = libssh2_session_last_error( this->session_, &err_msg, NULL, 0);
                    throw_<FileOpenError>("SFTP failed to open file for writing: '%1'.\n(%2): %3."
                                         , remote_filepath.c_str(), err, err_msg );
                }
            }
        } while( !sftp_handle );

        size_t nread;
        char *ptr;
        size_t total = 0;
        do {
            nread = fread(mem, 1, sizeof(mem), fh_local);
            if (nread <= 0) {// end of file
                break;
            }
            ptr = mem;
            do {// write data in a loop until we block
                while( ( rc = libssh2_sftp_write(sftp_handle, ptr, nread)) == LIBSSH2_ERROR_EAGAIN ) {
                    waitsocket(sock_, session_);
                }
                if(rc < 0)
                    break;
                ptr   += rc;
                nread -= rc;
            } while (nread);
            total += nread;
        } while (rc > 0);

        if( Session::verbose > 1 )
            fprintf(stderr, "SFTP upload done!\n");

        libssh2_sftp_shutdown(sftp_session);

        if(Session::autoClose) {
            this->close();
        }
        return total;
    }
 //-----------------------------------------------------------------------------
    size_t
    Session::
    sftp_get_file
      ( QString const&  q_local_filepath
      , QString const& q_remote_filepath )
    {
        std::string  local_filepath =  q_local_filepath.toStdString();
        std::string remote_filepath = q_remote_filepath.toStdString();
        if( Session::verbose ) {
            Log(1) << "\nFile transfer : remote >> local"
                      << "\n    remote: '" << q_remote_filepath.C_STR() << "'"
                      << "\n    local : '" <<  q_local_filepath.C_STR() << "'"
                     ;
        }

        if( !this->isOpen() ) {
            if( Session::autoOpen ) {
                this->open();
            } else {
                throw_<SshOpenError>("SshOpenError");
            }
        }

        int rc;
        LIBSSH2_SFTP       * sftp_session;
        LIBSSH2_SFTP_HANDLE* sftp_handle;
        FILE *fh_local;
        char mem[512];

        fh_local = fopen( local_filepath.c_str(), "wb");
        if(!fh_local) {
            throw_<FileOpenError>("Cannot open local file for writing: '%1'.", local_filepath.c_str() );
        }

        do {
            sftp_session = libssh2_sftp_init(this->session_);
            if( !sftp_session )
            {
                if( libssh2_session_last_errno(this->session_) == LIBSSH2_ERROR_EAGAIN ) {
                 // fprintf(stderr, "non-blocking init\n");
                    waitsocket( this->sock_, this->session_); /* now we wait */
                } else {
                    throw_<std::runtime_error>("Unable to init SFTP session.");
                }
            }
        } while( !sftp_session );

     // Request a file via SFTP */
        do {
            sftp_handle = libssh2_sftp_open( sftp_session, remote_filepath.c_str(), LIBSSH2_FXF_READ, 0);
            if (!sftp_handle) {
                if (libssh2_session_last_errno(this->session_) != LIBSSH2_ERROR_EAGAIN ) {
                    throw_<FileOpenError>("Unable to open file with SFTP: '%1'", remote_filepath.c_str() );
                } else
                {// fprintf(stderr, "non-blocking open\n");
                    waitsocket( this->sock_, this->session_ ); /* now we wait */
                }
            }
        } while( !sftp_handle);

        if( Session::verbose > 1 ) {
            fprintf(stderr, "libssh2_sftp_open() is done, now receive data!\n");
        }
        size_t total = 0;
        do {
            do
            {// read in a loop until we block
                rc = libssh2_sftp_read( sftp_handle, mem, sizeof(mem) );
                if( rc > 0 ) {
                    fwrite( mem, rc, 1, fh_local );
                    total +=rc;
                }
            } while( rc > 0 );
            if( rc == LIBSSH2_ERROR_EAGAIN ) {
                waitsocket(sock_, session_);
            } else {
                break; // error or end of file
            }

//            timeout.tv_sec = 10;
//            timeout.tv_usec = 0;

//            FD_ZERO( &fd );
//            FD_SET( this->sock_, &fd );

//         // wait for readable or writeable
//            rc = select( this->sock_+1, &fd, &fd, NULL, &timeout );
//            if( rc <= 0 ) {// negative is error, 0 is timeout
//                throw_<std::runtime_error>("SFTP download timed out: %1", rc );
//            }
        } while (1);

        libssh2_sftp_close(sftp_handle);

        fclose(fh_local);

        libssh2_sftp_shutdown(sftp_session);
        return total;
    }
 //-----------------------------------------------------------------------------
    bool
    Session::
    sftp_put_dir
      ( QString const&  local_dirpath
      , QString const& remote_dirpath
      , bool recurse
      , bool must_throw
      )
    {
        QString parent = QString(local_dirpath).append("/");
        QDir qDirLocal( local_dirpath );
        if( !qDirLocal.exists() ) {
            if( must_throw ) {
                throw_<FileOpenError>("Local directory does not exist: '%1'",local_dirpath);
            } else {
                return false;
            }
        }
        QFileInfoList qFileInfoListLocal = qDirLocal.entryInfoList( QStringList() );
        if( qFileInfoListLocal.isEmpty() ) {
            if( must_throw ) {
                throw_<FileOpenError>("Local directory is empty: '%1'",local_dirpath);
            } else {
                return false;
            }
        }

        QString cmd = QString("mkdir -p ").append(remote_dirpath);
        this->execute_remote_command(cmd);

        for ( QFileInfoList::const_iterator iter=qFileInfoListLocal.cbegin()
            ; iter!=qFileInfoListLocal.cend(); ++iter )
        {
            QFileInfo const& qFileInfo = *iter;
            QString local_iter = qFileInfo.absoluteFilePath();
            QString remote_iter= QString(remote_dirpath).append("/").append(qFileInfo.fileName());

            if( qFileInfo.isFile() ) {
                if( !local_iter.startsWith("~") )
                {// old versions of files are not copied
                    sftp_put_file( local_iter, remote_iter );
                }
            } else if( recurse && qFileInfo.isDir() ) {
                if( local_iter.startsWith(parent) ) {
                    this->sftp_put_dir( local_iter, remote_iter, true, false);
                }
            }
        }
        return true;
    }
 //-----------------------------------------------------------------------------
    bool
    Session::
    sftp_get_dir
      ( QString const&  local_dirpath // = target
      , QString const& remote_dirpath // = source
      , bool recurse
      , bool must_throw
      )
    {
        QDir().mkpath( local_dirpath );
        QString  local_parent =  local_dirpath;
        QString remote_parent = remote_dirpath;
        if( ! local_parent.endsWith('/') )  local_parent.append('/');
        if( !remote_parent.endsWith('/') ) remote_parent.append('/');

     // get list of remote files:
        QString cmd = QString("cd ")
                      .append(remote_dirpath)
                      .append(" && ls -1F");
                         // ls -1F : -1 puts every item on a new line,
                         //          -F adds a trailing / to directories
        int rc = this->execute_remote_command(cmd);
        if( rc ) {
            if( must_throw ) {
                throw_<FileOpenError>("Remote directory does not exist: '%1'.",remote_dirpath);
            } else {
                return false;
            }
        }

        QStringList entries = this->qout().split('\n', QString::SkipEmptyParts );
        if( entries.isEmpty() ) {
            if( must_throw ) {
                throw_<FileOpenError>("Remote directory is empty: '%1'.",remote_dirpath);
            } else {
                return false;
            }
        }

     // copy all files
        for ( QStringList::const_iterator iter = entries.cbegin()
            ; iter!=entries.cend(); ++iter )
        {
            QString const& entry = *iter;
            QString  local_child = QString( local_parent).append(entry);
            QString remote_child = QString(remote_parent).append(entry);
            if( entry.endsWith('/') )
            {// it's a directory
                if( recurse )
                    this->sftp_get_dir ( local_child, remote_child, recurse, false );
            } else {
                    this->sftp_get_file( local_child, remote_child );
            }
        }
        return true;
    }
 //-----------------------------------------------------------------------------
    void Session::
    close( bool keep_libssh_init )
    {
        if( this->session_ ) {
            libssh2_session_disconnect(this->session_,"Normal Shutdown, Thank you for playing");
            libssh2_session_free(this->session_);
            this->session_ = nullptr;
        }
        if( this->sock_ != -1) {
            ::close(this->sock_);
            this->sock_ = -1;
        }
        if( !keep_libssh_init && Session::isInitializedLibssh_ ) {
            libssh2_exit();
            Session::isInitializedLibssh_ = false;
        }
    }
 //-----------------------------------------------------------------------------
    void Session::
    reset()
    {
        this->close();

        this->   username_.clear();
        this->private_key_.clear();
#ifndef NO_PUBLIC_KEY_NEEDED
        this-> public_key_.clear();
#endif
        this-> passphrase_.clear();
        this->isAuthenticated_= false;
#ifdef Q_OS_WIN
        WSACleanup();
#endif
    }
 //-----------------------------------------------------------------------------
    Session::
    ~Session()
    {
        this->close();
    }
 //-----------------------------------------------------------------------------
    void Session::cleanup()
    {
        if( Session::isInitializedLibssh_ ) {
            libssh2_exit();
            Session::isInitializedLibssh_ = false;
        }
    }
 //-----------------------------------------------------------------------------
    std::string scramble( std::string in )
    {
        QString out;
        for( std::size_t i=0; i<in.size(); ++i )
            out.append('*');
        return out.toStdString();
    }
 //-----------------------------------------------------------------------------
    QString Session::toString() const
    {
        QString s;
        QTextStream ts(&s);
        ts << "\nssh2 Session"
           << "\n    login node  : " << QString( this->login_node_          .c_str() )
           << "\n    username    : " << QString( this->username_            .c_str() )
           << "\n    private key : " << QString( this->private_key_         .c_str() )
#ifndef NO_PUBLIC_KEY_NEEDED
           << "\n    public  key : " << QString( this->public_key_          .c_str() )
#endif
           << "\n    passphrase  : '"<< QString( scramble(this->passphrase_).c_str() )<<"'"
           ;
        return s;
    }
 //-----------------------------------------------------------------------------
    QString Session::get_env( QString const& env)
    {
        QString cmd("echo ");
        if( !env.startsWith('$') ) cmd.append('$');
        cmd.append(env);
        this->execute_remote_command(cmd);
        QString result = this->qout().trimmed();
        return result;
    }
 //-----------------------------------------------------------------------------
    int ExecuteRemoteCommand::execute(QString& cmd, const QString &arg1, const QString &arg2 ) const
    {
        if( cmd.startsWith("__") )
        {// this is a command key, get the corresponding command
            cmd = this->remote_commands_->value(cmd);
            if( cmd.isEmpty() )
                throw_<InexistingRemoteCommand>("RemoteCommand corresponding to '%1' was not found.",cmd);
        } else {
            QString wrapper = this->remote_commands_->value("wrapper");
            if( !wrapper.isEmpty() )
            {// wrap the command
                cmd = wrapper.arg(cmd);
            }
        }
        if( !arg1.isEmpty() ) {
            cmd = cmd.arg(arg1);
            if( !arg2.isEmpty() ) {
                cmd = cmd.arg(arg2);
            }
        }
        return this->ssh2_session_->execute(cmd);
    }
 //-----------------------------------------------------------------------------
}// namespace ssh2
