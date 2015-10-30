#!/bin/bash
set -e

revision=$(git describe)

if [ -e "revision.txt" ]
then
    echo 'revision.txt read'
    prev_revision=$(cat revision.txt)
else
    echo 'revision.txt not found'
    rm -f Launcher/version.h
    prev_revision=0
fi

echo 'revision='${prev_revision}

if [ "${revision}" == "${prev_revision}" ]
then
    echo ${revision} ' == ' ${prev_revision}
    echo 'Launcher/version.h is unchanged'
else
    echo ${revision} ' != ' ${prev_revision}
    echo 'writing revision.txt'
    echo ${revision} > revision.txt
    echo 'writing Launcher/version.h'

    echo '/*'                               >  Launcher/version.h
    echo ' * This file was generated by bash script git_revision.sh.' >> Launcher/version.h
    echo ' */'                              >> Launcher/version.h
    echo                                    >> Launcher/version.h
    echo '#ifndef LAUNCHER_VERSION_H'       >> Launcher/version.h
    echo '#define LAUNCHER_VERSION_H'       >> Launcher/version.h
    echo                                    >> Launcher/version.h
    echo '#define VERSION "'${revision}'"'  >> Launcher/version.h
    echo                                    >> Launcher/version.h
    echo '#endif//LAUNCHER_VERSION_H'     >> Launcher/version.h
fi
echo 'done'
exit 0