TEMPLATE = subdirs

SUBDIRS += \
    cfg \
    cfg-test \
    Launcher

cfg-test.depends = cfg
