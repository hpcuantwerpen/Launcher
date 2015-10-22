#!/bin/bash

################################################################################
# process command line arguments
################################################################################
if [ -z "$1" ];
then
    GIT_BRANCH=master
else
    GIT_BRANCH-$1
fi

################################################################################
# retrieve git revision info
################################################################################
cd ~/qws/Launcher
git checkout ${GIT_BRANCH}
git describe ${GIT_BRANCH} >

################################################################################
# build release version:
################################################################################
#cd into build directory
cd ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release
#run qmake
~/QtNew/5.5/clang_64/bin/qmake ~/qws/Launcher/Launcher.pro -r -spec macx-clang CONFIG+=x86_64
make

################################################################################
# copy Launcher.app to build_directory/distribute/macosx
################################################################################
mkdir -p ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/distribute/macosx/
rm -rf   ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/distribute/macosx/*

cp -R ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/Launcher/Launcher.app ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/distribute/macosx/

echo Running macdeployqt
~/QtNew/5.5/clang_64/bin/macdeployqt ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/distribute/macosx/Launcher.app -dmg

echo cat qt.conf
cat ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/distribute/macosx/Launcher.app/Contents/resources/qt.conf

. ./rename_dmg.sh
