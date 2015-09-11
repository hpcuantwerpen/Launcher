#include "ssh2tools.h"

#ifdef Q_OS_WIN
    #include <windows.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #define closesocket close
#endif


#include <cstring>
#include <stdexcept>

#include <throw_.h>
#include <warn_.h>
#include <QFileInfo>

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
      : session_(nullptr)
      , sock_(-1)
    {}
 //-----------------------------------------------------------------------------
    bool Session::isInitializedLibssh_ = false;
 //-----------------------------------------------------------------------------
    void Session::setLoginNode( QString const& login ) {
        this->login_node_ = login.toStdString();
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
      , QString const& public_key
      , bool           must_throw
      )
    {
        this->private_key_.clear();
        this-> public_key_.clear();
        this-> passphrase_.clear();

        if( this->username_.empty() ) {
            if( must_throw ) { throw_<std::runtime_error>("Refused to set public/private key pair without username."); }
            else             { return false; }
        }

        if( !QFileInfo (private_key).exists() ) {
            if(must_throw) { throw_<std::runtime_error>("Private key '%1' does not exist.", private_key ); }
            else           { return false; }
        }

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
        return true;
    }
 //-----------------------------------------------------------------------------
    bool Session::setPassphrase( QString const & passphrase, bool must_throw )
    {
        if( this->username_.empty() ) {
            if( must_throw ) { throw_<std::runtime_error>("Refused to set password without username."); }
            else             { return false; }
        }
        if( this->private_key_.empty() || this->public_key_.empty() ) {
            if( must_throw ) { throw_<std::runtime_error>("Refused to set password without private/public key pair."); }
            else             { return false; }
        }
        this->passphrase_ = passphrase.toStdString();
    }

 //-----------------------------------------------------------------------------
    void Session::open()
    {
        if( this->isOpen() ) {
            return;
        }
        if( this->username_.empty() ) {
            throw_<MissingUsername>("Authentication error: Missing username.");
        }
        if( this->public_key_.empty() ) {
            throw_<MissingKey>( "Missing private/public key pair for user %1.", this->username_.c_str() );
        }

      /*const char *hostname    = "143.169.237.31";
        const char *username    = "vsc20170";
        const char *password    = "";
       */
        struct addrinfo hints, *servinfo, *p;
        int rv;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC; // use AF_INET6 to forve IPv6
        hints.ai_socktype = SOCK_STREAM;

        if( (rv=getaddrinfo( this->login_node_.c_str(), "22", &hints, &servinfo )) != 0 )
            throw_<std::runtime_error>("getaddrinfo[%1] : %2", rv, gai_strerror(rv) );

     // loop through all the results and connect to the first we can
        for( p=servinfo; p!=NULL; p=p->ai_next)
        {
            if( (this->sock_ = socket( p->ai_family, p->ai_socktype, p->ai_protocol )) == -1 ) {
                perror("this->sock_et");
                continue; // try next element in the list
            }
            if( connect( this->sock_, p->ai_addr, p->ai_addrlen ) == -1 ) {
                ::close( this->sock_ );
                perror("connect");
                continue; // try next element in the list
            }
            break; // if we get here, we must have connected successfully
        }
        if( p==NULL)
        {// looped off the end of the list with no connection
            freeaddrinfo(servinfo); // all done with this structure
            throw_<std::runtime_error>("Failed to connect.");
        }
     // libssh2 stuff...
        if( !Session::isInitializedLibssh_ )
        {
            if( (rv=libssh2_init(0)) != 0 ) {
                throw_<std::runtime_error>("Faild at initializing libssh2, exitcode = %1.", rv );
            }
            Session::isInitializedLibssh_ = true;
        }
        this->session_ = libssh2_session_init();
        if( !this->session_ ) {
            throw_<std::runtime_error>("Failed at initializing ssh2 session.");
        }
     // tell libssh2 we want it all done non-blocking
        libssh2_session_set_blocking(this->session_, 0);

     // ... start it up. This will trade welcome banners, exchange keys,
     // and setup crypto, compression, and MAC layers
        while( (rv = libssh2_session_handshake(this->session_, this->sock_)) == LIBSSH2_ERROR_EAGAIN);
        if( rv ) {
            throw_<std::runtime_error>("Failed at establishing SSH session, exitcode = %1>", rv );
        }
        LIBSSH2_KNOWNHOSTS* nh = libssh2_knownhost_init(this->session_);

        if( !nh )
        {// eeek, do cleanup here */
            throw_<std::runtime_error>("Failed at initializing known hosts");
        }
/*
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
*/
/*
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
            while( ( rv = libssh2_userauth_publickey_fromfile
                            ( this->session_
                            , this->username_   .c_str()
                            , this->public_key_ .c_str()
                            , this->private_key_.c_str()
                            , this->passphrase_   .c_str()
                            )
                    ) == LIBSSH2_ERROR_EAGAIN
                 );
            if( rv==0 ) {
                return;
            }
            this->close();
            if( rv==LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED ) {
                if( this->passphrase_.empty() ) {
                    throw_<PassphraseNeeded>("Passphrase needed to unlock key '%1'."   , this->private_key_.c_str() );
                } else {
                    throw_<WrongPassphrase>("Could not unlock key '%1' (wrong passphrase?).", this->private_key_.c_str() );
                }
            } else {
                throw_<std::runtime_error>("Failed at libssh2_userauth_publickey_fromfile, exitcode=%1.", rv );
            }
//      }
    }
 //-----------------------------------------------------------------------------
    bool Session::isOpen() const {
        return this->session_;
    }
 //-----------------------------------------------------------------------------
    bool Session::autoOpen = false;
    bool Session::autoClose= false;
 //-----------------------------------------------------------------------------
    void
    Session::
    exec( QString const & command_line, QString* qout, QString* qerr )
    {
        if( !this->isOpen() && Session::autoOpen ) {
            this->open();
        }

        int rv=0;
        char const* cmd = command_line.toStdString().c_str();

     // Exec non-blocking on the remove host
        LIBSSH2_CHANNEL *channel;
        while( (channel = libssh2_channel_open_session(this->session_)) == NULL
            && libssh2_session_last_error(this->session_,NULL,NULL,0) == LIBSSH2_ERROR_EAGAIN
             ) {
            waitsocket( this->sock_, this->session_ );
        }
        if( channel == NULL )
            throw_<std::runtime_error>("Error on libssh2_channel_open_session");

        while( (rv = libssh2_channel_exec(channel, cmd)) == LIBSSH2_ERROR_EAGAIN )
            waitsocket(this->sock_, this->session_);

        if( rv != 0 )
           throw_<std::runtime_error>("Error onlibssh2_channel_exec");
     //----
        int bytecount[2] = {0,0};
        QString* qxxx[2];
        qxxx[0]=qout;
        qxxx[1]=qerr;

        if( qout )
            qout->clear();
        if( qerr )
            qerr->clear();
        char buffer[0x4000];
        for( int iq=0;iq<2;++iq )
        {
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
                        bytecount[iq] += rv;
                        warn_("We read:");
                        for( int i=0; i < rv; ++i )
                            fputc( buffer[i], stderr);
                        warn_("");
                     //?buffer[rv] ='\0';
                        qxxx[iq]->append(buffer);
                    } else {
                        if( rv != LIBSSH2_ERROR_EAGAIN )
                        {// no need to output this for the EAGAIN case
                            warn_("libssh2_channel_read returned %1\n", rv);
                        }
                    }
                } while( rv > 0 );

             // this is due to blocking that would occur otherwise so we loop on this condition
                if( rv == LIBSSH2_ERROR_EAGAIN )
                    waitsocket(this->sock_, this->session_);
                else
                    break;
            }
        }
        int exitcode = 127;
        while( (rv = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN )
            waitsocket(this->sock_, this->session_);

        char *exitsignal=(char *)"none";
        if( rv == 0 ) {
            exitcode = libssh2_channel_get_exit_status( channel );
            libssh2_channel_get_exit_signal(channel, &exitsignal,NULL, NULL, NULL, NULL, NULL);
        }

        if( exitsignal )
            warn_("Got signal: %1\n", exitsignal);
        else
            warn_("EXIT: %1, bytecount[stdout]: %2, bytecount[stderr]: %3"
                 , exitcode, bytecount[0], bytecount[1]
                    );

        libssh2_channel_free(channel);
        channel = NULL;

        if( Session::autoClose ) {
            this->close();
        }
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
        this-> public_key_.clear();
        this-> passphrase_.clear();
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
        for( int i=0; i<in.size(); ++i )
            out.append('*');
        return out.toStdString();
    }
 //-----------------------------------------------------------------------------
    void Session::print( std::ostream& ostrm ) const
    {
        ostrm << "\nssh2 Session"
              << "\n    username    : "<< this->username_
              << "\n    private key : "<< this->private_key_
              << "\n    public  key : "<< this->public_key_
              << "\n    passphrase  : '"<< scramble(this->passphrase_)<<"'"
              << std::endl;
    }
 //-----------------------------------------------------------------------------
}// namespace ssh2
