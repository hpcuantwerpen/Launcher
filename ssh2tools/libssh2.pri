### include file that specifies which libssh2 library is used.

USE_SYSTEM_LIBSSH2 = 0

macx:{
    equals(USE_SYSTEM_LIBSSH2,1) {
      # use libssh2 as installed by Homebrew
        LIBS += -L/usr/local/lib/ -lssh2
        INCLUDEPATH += /usr/local/include/
        message("using libssh2 as installed by Homebrew")
    } else {
      # use libssh2 as built by me
        SSH2_DEP_DIR = "/Users/etijskens/qws/libssh2+dependencies/"
        LIBS += $${SSH2_DEP_DIR}libssh2/build/lib/libssh2.a
        LIBS += $${SSH2_DEP_DIR}openssl-1.0.2d/build/lib/libssl.a
        LIBS += $${SSH2_DEP_DIR}openssl-1.0.2d/build/lib/libcrypto.a
        LIBS += $${SSH2_DEP_DIR}zlib-1.2.8/libz.a
        message(">>> $$LIBS")
        INCLUDEPATH += $${SSH2_DEP_DIR}libssh2/build/include/
        message(">>>  $$INCLUDEPATH")
        message("using self build libssh2")
    }
}
unix:!macx:{
}
win32:{
      # use libssh2 as built by me
        DEFINES+= WINVER=0x0501
        SSH2_DEP_DIR = "/Users/etijskens/qws/libssh2_dependencies/"
        LIBS += $${SSH2_DEP_DIR}libssh2/build/lib/libssh2.a
        LIBS += $${SSH2_DEP_DIR}openssl-1.0.2d/build/lib/libssl.a
        LIBS += $${SSH2_DEP_DIR}openssl-1.0.2d/build/lib/libcrypto.a
        LIBS += $${SSH2_DEP_DIR}zlib-1.2.8/libzlibstatic.a
        message(">>> $$LIBS")
        LIBS += -LC:/Qt/Tools/mingw492_32/i686-w64-mingw32/lib -lws2_32 -lwsock32 -lgdi32
        INCLUDEPATH += $${SSH2_DEP_DIR}libssh2/build/include/
        message(">>>  $$INCLUDEPATH")
        message("using self build libssh2")
}
