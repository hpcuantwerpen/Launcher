TEMPLATE = subdirs


SUBDIRS +=          \
    toolbox         \
    toolbox_test    \
    cfg             \
    cfg_test        \
    Launcher        \
    ssh2tools       \
    ssh2tools_test  \
    jobscript       \
    jobscript_test

  toolbox_test.depends = toolbox
      cfg_test.depends = cfg
     ssh2tools.depends = toolbox
ssh2tools_test.depends = ssh2tools toolbox
jobscript_test.depends = jobscript toolbox cfg
      Launcher.depends = jobscript toolbox cfg ssh2tools

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7

