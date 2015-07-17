# specify a library to depend on
# arguments:
#   arg_path : relative path to the lib subproject's parent
#   arg_lib  : the name of the lib subproject

INCLUDEPATH += $${arg_path}/$${arg_lib}/
message(depend_on_lib.pri : INCLUDEPATH $${INCLUDEPATH})
DEPENDPATH += $${INCLUDEPATH} # force rebuild if the headers change

unix {_PATH_TO_LIB = $${arg_path}/$${arg_lib}/lib$${arg_lib}.a}
message(depend_on_lib.pri : library     $${_PATH_TO_LIB})
LIBS += $${_PATH_TO_LIB}
PRE_TARGETDEPS += $${_PATH_TO_LIB}
