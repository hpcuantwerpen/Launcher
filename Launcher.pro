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
ssh2tools_test.depends = ssh2tools toolbox
jobscript-test.depends = jobscript toolbox cfg
#      Launcher.depends = jobscript toolbox cfg sshtools
