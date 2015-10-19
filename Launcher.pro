TEMPLATE = subdirs

#win32:{
#        DEFINES+= WINVER=0x0501
#        LIBS += -LC:/Qt/Tools/mingw492_32/i686-w64-mingw32/lib -lws2_32 -lwsock32
#}
SUBDIRS +=          \
    toolbox         \
    cfg             \
    Launcher        \
    ssh2tools       \
    jobscript

      cfg.depends = toolbox
ssh2tools.depends = toolbox
 Launcher.depends = jobscript toolbox cfg ssh2tools

BUILD_TESTS = 1
equals(BUILD_TESTS,1) {
    SUBDIRS +=  toolbox_test    \
                cfg_test        \
                ssh2tools_test  \
                jobscript_test

      toolbox_test.depends = toolbox
          cfg_test.depends = cfg
    ssh2tools_test.depends = ssh2tools toolbox
    jobscript_test.depends = jobscript toolbox cfg
}

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7

