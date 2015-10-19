echo Copying Launcher.app
mkdir -p ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/distribute/macosx/
rm -rf   ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/distribute/macosx/*

cp -R ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/Launcher/Launcher.app ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/distribute/macosx/

echo Running macdeployqt
~/QtNew/5.5/clang_64/bin/macdeployqt ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/distribute/macosx/Launcher.app -dmg

echo cat qt.conf
cat ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/distribute/macosx/Launcher.app/Contents/resources/qt.conf

. ./rename_dmg.sh
