### include file that specifies which libssh2 library is used.

macx
{## use libssh2 as installed by Homebrew
    LIBS += -L/usr/local/lib/ -lssh2
    INCLUDEPATH += /usr/local/include/
#    LIBS += -L /Users/etijskens/ews-2/libssh2/use_me/lib/ -lssh2
#    INCLUDEPATH += /Users/etijskens/ews-2/libssh2/include/

}
unix:!macx
{}
win32
{}
