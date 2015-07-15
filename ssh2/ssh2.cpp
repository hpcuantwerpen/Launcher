#include "ssh2.h"

#include <libssh2.h>

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
#include <cstdio>
#include <stdexcept>

#include <QString>

namespace ssh2
{//=============================================================================
    Session::Session()
    {}
 //=============================================================================

    void Session::open()
    {
        int sockfd;
        struct addrinfo hints, *servinfo, *p;
        int rv;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
        hints.ai_socktype = SOCK_STREAM;

        if( ( rv = getaddrinfo("login.hpc.uantwerpen.be", "22", &hints, &servinfo ) ) != 0 ) {
            QString errmsg = QString("getaddrinfo: %1\n").arg( gai_strerror(rv) );
            throw std::runtime_error( errmsg.toStdString() )
        }

     // loop through all the results and connect to the first we can
        for( p=servinfo; p!=NULL; p=p->ai_next)
        {
            if( (sockfd = socket( p->ai_family, p->ai_socktype, p->ai_protocol )) == -1 ) {
                perror("socket");
                continue; // try next element in the list
            }
            if( connect( sockfd, p->ai_addr, p->ai_addrlen ) == -1 ) {
                ::close( sockfd );
                perror("connect");
                continue; // try next element in the list
            }
            break; // if we get here, we must have connected successfully
        }
        if( p==NULL)
        {// looped off the end of the list with no connection
            freeaddrinfo(servinfo); // all done with this structure
            throw std::runtime_error ("Failed to connect.\n")
        }
     // libssh2 stuff...
        rv = libssh2_init(0);
        if( rvc!=0 ) {
           QString errmsg("libssh2 initialization failed (%1)\n");
           errmsg.arg(rv);
           throw std::runtime_error( errmsg.toStdString() )
        }
        LIBSSH2_SESSION *session;
        session = libssh2_session_init();
        if (libssh2_session_startup(session, sockfd)) {
            fprintf(stderr, "Failure establishing SSH session\n");
            return ;
        }

    }
 //=============================================================================
    void Session::close()
    {}
 //=============================================================================
}// namespace ssh2
