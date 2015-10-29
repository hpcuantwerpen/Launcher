#!/bin/bash
#set -e

cd ~/qws/Launcher
################################################################################
# process command line arguments
################################################################################
#branch to checkout
if [ -n "$1" ];
then
     git checkout $1
fi
################################################################################
# retrieve git revision info
################################################################################
./git_revision.sh

################################################################################
# build release version:
################################################################################
#cd into build directory
cd ~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release
#run qmake, make
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

# rename_dmg
DMG_HOME=~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/distribute/macosx/
cd $DMG_HOME
rm -f ttt.dmg
hdiutil convert Launcher.dmg -format UDRW -o ttt.dmg
mv Launcher.dmg Launcher.0.dmg
open ttt.dmg
sleep 20
#diskutil list (kijk na onder welke /dev/diskX de image gemount is, bvb. disk3)
diskutil list
for i in `seq 1 10`;
do
        echo $i
        output=$(diskutil list disk$i | grep /Users/etijskens)
        if [ -n "$output" ]
        then
#            echo ${output}
#            echo disk$i
            break
        fi
done
#echo disk${i}

git_revision=$(cat ~/qws/Launcher/revision.txt)
echo ${git_revision}

diskutil renameVolume /Volumes/:\Users* Launcher-${git_revision}
sudo rm -rf /Volumes/Launcher/.Trashes /Volumes/Launcher/.fseventsd
diskutil eject disk$i
name=Launcher_${git_revision}
hdiutil convert ttt.dmg -format UDRO -ov -o ${name}.dmg

ls -l

cp ${DMG_HOME}/${name} ~/Dropbox
