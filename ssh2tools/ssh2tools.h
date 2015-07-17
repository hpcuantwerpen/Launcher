#ifndef SSH2_H
#define SSH2_H

#include <libssh2.h>
#include <toolbox.h>

#include <QString>

namespace ssh2
{//=============================================================================
    class Session
    {
    public:
        Session();
        void open();
        void exec( QString const& command_line
                 , QString* qout=nullptr
                 , QString* qerr=nullptr
                 );
        void close();
    private:
        int sock_;
        LIBSSH2_SESSION *session_;
    };
 //=============================================================================
}// namespace ssh2

#endif // SSH2_H
