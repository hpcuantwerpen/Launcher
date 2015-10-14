#!/bin/bash
DMG_HOME=~/qws/build-Launcher-Desktop_Qt_5_5_0_clang_64bit-Release/distribute/macosx/
cd $DMG_HOME
rm ttt.dmg
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
diskutil renameVolume /Volumes/:\Users* Launcher
rm -rf /Volumes/Launcher/.Trashes /Volumes/Launcher/.fseventsd
diskutil eject disk$i
hdiutil convert ttt.dmg -format UDRO -ov -o Launcher.dmg

ls
