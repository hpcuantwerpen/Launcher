#/bin/bash
#copy resources from project directory to build directory

BUILD=build-Launcher-Desktop_Qt_5_5_0_clang_64bit

echo
echo "copying to "${BUILD}-Release
cp -rvf ~/qws/Launcher/Launcher/clusters ~/qws/${BUILD}-Release/Launcher/Launcher.app/Contents/Resources
cp -rvf ~/qws/Launcher/Launcher/jobs     ~/qws/${BUILD}-Release/Launcher/Launcher.app/Contents/Resources
echo
echo "copying to "${BUILD}-Debug
cp -rvf ~/qws/Launcher/Launcher/clusters ~/qws/${BUILD}-Debug/Launcher/Launcher.app/Contents/Resources
cp -rvf ~/qws/Launcher/Launcher/jobs     ~/qws/${BUILD}-Debug/Launcher/Launcher.app/Contents/Resources
echo
if [ -d "${HOME}/Launcher" ]; then
echo "copying to ${HOME}/Launcher"
cp -rvf ~/qws/Launcher/Launcher/clusters ~/Launcher
cp -rvf ~/qws/Launcher/Launcher/jobs     ~/Launcher
fi
