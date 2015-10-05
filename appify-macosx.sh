#!/bin/bash

BUILD_DIR=~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release
#   this is where the build resides

APP_DIR=$BUILD_DIR/distribute/macosx
#   this is where we create the distributable

################################################################################
# Copy the application bundle

rm -rf   $APP_DIR
mkdir -p $APP_DIR
cp -r $BUILD_DIR/Launcher/Launcher.app $APP_DIR

################################################################################
#Copy clusters file to distribution
cp -Rf ~/Launcher_pp/clusters $APP_DIR/Launcher.app/Contents/Resources

exit
################################################################################
#Add dependencies
dylibbundler -od -b -x $APP_DIR/Launcher.app/Contents/MacOS/Launcher -d $APP_DIR/Launcher.app/Contents/libs/
#   this adds the non standard libraries we depend on (Libssh2)
#   (but not Qt)

################################################################################
#Add Qt framework dependencies
#   @rpath/QtWidgets.framework/Versions/5/QtWidgets (compatibility version 5.5.0, current version 5.5.0)
#   @rpath/QtGui.framework/Versions/5/QtGui         (compatibility version 5.5.0, current version 5.5.0)
#   @rpath/QtCore.framework/Versions/5/QtCore       (compatibility version 5.5.0, current version 5.5.0)

rm -rf   $APP_DIR/Launcher.app/Contents/Frameworks/*
mkdir -p $APP_DIR/Launcher.app/Contents/Frameworks
#   we wil store them here

QT_FRAMEWORK_DIR=~/QtNew/5.5/clang_64/lib


for qt in QtWidgets QtGui QtCore
do
    echo
    echo Launcher depends on $qt
    echo
    echo cp -R $QT_FRAMEWORK_DIR/$qt.framework $APP_DIR/Launcher.app/Contents/Frameworks
    cp -R $QT_FRAMEWORK_DIR/$qt.framework $APP_DIR/Launcher.app/Contents/Frameworks
    echo
    echo install_name_tool -id     @executable_path/../Frameworks/$qt.framework/Versions/5/$qt $APP_DIR/Launcher.app/Contents/Frameworks/$qt.framework/Versions/5/$qt
    install_name_tool -id     @executable_path/../Frameworks/$qt.framework/Versions/5/$qt $APP_DIR/Launcher.app/Contents/Frameworks/$qt.framework/Versions/5/$qt
    echo
    echo install_name_tool -change @rpath/$qt.framework/Versions/5/$qt @executable_path/../Frameworks/$qt.framework/Versions/5/$qt $APP_DIR/Launcher.app/Contents/MacOs/Launcher
    install_name_tool -change @rpath/$qt.framework/Versions/5/$qt @executable_path/../Frameworks/$qt.framework/Versions/5/$qt      $APP_DIR/Launcher.app/Contents/MacOs/Launcher
done;

#Finally, the QtGui framework depends on QtCore, so we must remember to change the reference for QtGui:
echo
echo QtGui depends on QtCore
echo
echo install_name_tool -change @rpath/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore $APP_DIR/Launcher.app/Contents/Frameworks/QtGui.framework/Versions/5/QtGui
     install_name_tool -change @rpath/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore $APP_DIR/Launcher.app/Contents/Frameworks/QtGui.framework/Versions/5/QtGui

echo
echo QtWidgets depends on QtCore
echo
echo install_name_tool -change @rpath/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore $APP_DIR/Launcher.app/Contents/Frameworks/QtWidgets.framework/Versions/5/QtWidgets
     install_name_tool -change @rpath/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore $APP_DIR/Launcher.app/Contents/Frameworks/QtWidgets.framework/Versions/5/QtWidgets

echo
echo QtWidgets depends on QtGui
echo
echo install_name_tool -change @rpath/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui $APP_DIR/Launcher.app/Contents/Frameworks/QtWidgets.framework/Versions/5/QtWidgets
     install_name_tool -change @rpath/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui $APP_DIR/Launcher.app/Contents/Frameworks/QtWidgets.framework/Versions/5/QtWidgets

#verify:
echo
echo verify:
echo
echo otool -L $APP_DIR/Launcher.app/Contents/MacOs/Launcher
otool -L $APP_DIR/Launcher.app/Contents/MacOs/Launcher
echo
echo otool -L $APP_DIR/Launcher.app/Contents/Frameworks/QtGui.framework/Versions/5/QtGui
otool -L $APP_DIR/Launcher.app/Contents/Frameworks/QtGui.framework/Versions/5/QtGui
echo
echo otool -L $APP_DIR/Launcher.app/Contents/Frameworks/QtWidgets.framework/Versions/5/QtWidgets
otool -L $APP_DIR/Launcher.app/Contents/Frameworks/QtWidgets.framework/Versions/5/QtWidgets


