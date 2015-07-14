TEMPLATE = subdirs

SUBDIRS += \
    cfg \
    cfg-test \
    Launcher \
    ssh2 \
    ssh2-test \
    jobscript \
    jobscript-test

cfg-test.depends = cfg
ssh2-test.depends = ssh2
jobscript-test.depends = jobscript
