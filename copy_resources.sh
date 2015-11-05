#/bin/bash
#copy resources from project directory to build directory

BUILD=build-Launcher-Desktop_Qt_5_5_0_clang_64bit

cp -rf ~/qws/Launcher/Launcher/clusters ~/qws/${BUILD}-Release/Launcher/Launcher.app/Contents/Resources
cp -rf ~/qws/Launcher/Launcher/jobs     ~/qws/${BUILD}-Release/Launcher/Launcher.app/Contents/Resources

cp -rf ~/qws/Launcher/Launcher/clusters ~/qws/${BUILD}-Debug/Launcher/Launcher.app/Contents/Resources
cp -rf ~/qws/Launcher/Launcher/jobs     ~/qws/${BUILD}-Debug/Launcher/Launcher.app/Contents/Resources
