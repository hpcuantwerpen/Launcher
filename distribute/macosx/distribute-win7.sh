#!/bin/bash

################################
# run this in an MSYS terminal #
################################

cd /c/Users/etijskens/qws/Launcher
cd ../build-Launcher-Desktop_Qt_5_5_0_MinGW_32bit-Release/Launcher/release
mkdir -p        ../../distribute/win7
cp Launcher.exe ../../distribute/win7
cd              ../../distribute/win7

/c/Qt/5.5/mingw492_32/bin/windeployqt Launcher.exe
ls -l

cd /c/Users/etijskens/qws/Launcher

