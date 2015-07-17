### include file that specifies which libssh2 library is used.

macx
{## use libssh2 as installed by Homebrew
    LIBS += -L/usr/local/lib/ -lssh2
    INCLUDEPATH += /usr/local/include/
}
unix:!macx
{}
win32
{}
