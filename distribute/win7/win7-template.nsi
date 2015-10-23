Name "Launcher"  

# General Symbol Definitions  
!define REGKEY "SOFTWARE\$(^Name)"  
!define VERSION 0.1  
!define COMPANY CalcUA
!define URL https://www.uantwerpen.be/calcua
!define DISTRIBUTION_DIR  C:\Users\etijskens\qws\build-Launcher-Desktop_Qt_5_5_0_MinGW_32bit-Release\distribute\win7
  
# MUI Symbol Definitions  
;!define MUI_ICON "resources\Common files\Small Instrument Software 1.0.ico"  
!define MUI_FINISHPAGE_NOAUTOCLOSE  
!define MUI_FINISHPAGE_RUN "$INSTDIR\Launcher.exe"  
;!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"  
!define MUI_UNFINISHPAGE_NOAUTOCLOSE  
  
# Included files  
!include Sections.nsh  
!include MUI.nsh  
  
# Variables  
Var StartMenuGroup  
  
# Installer pages  
!insertmacro MUI_PAGE_WELCOME  
!insertmacro MUI_PAGE_INSTFILES  
!insertmacro MUI_PAGE_FINISH  
!insertmacro MUI_UNPAGE_CONFIRM  
!insertmacro MUI_UNPAGE_INSTFILES  
  
# Installer languages  
!insertmacro MUI_LANGUAGE English  
  
# Installer attributes  
OutFile $$_OUT_FILE_$$  
InstallDir "$PROGRAMFILES\Launcher"  
CRCCheck on  
XPStyle on  
ShowInstDetails show  
VIProductVersion 0.1.0.0
VIAddVersionKey ProductName "Launcher"  
VIAddVersionKey ProductVersion "${VERSION}"  
VIAddVersionKey CompanyName "${COMPANY}"  
VIAddVersionKey CompanyWebsite "${URL}"  
VIAddVersionKey FileVersion "${VERSION}"  
VIAddVersionKey FileDescription ""  
VIAddVersionKey LegalCopyright ""  
InstallDirRegKey HKLM "${REGKEY}" Path  
ShowUninstDetails show  
  
# Installer sections  
$$_MAIN_INSTALL_FOLDER_$$
  
;Section -windowsFiles SEC0001  
;    SetOutPath $SYSDIR  
;    SetOverwrite try  
;    File "resources\Common files\msvcr71.dll"  
;    File "resources\Common files\msvcr70.dll"  
;    File "resources\Common files\msvcp71.dll"  
;    File "resources\Common files\MSVCI70.dll"  
;    File "resources\Common files\MFC71.dll"  
;    File "resources\Common files\mfc70.dll"  
;    WriteRegStr HKLM "${REGKEY}\Components" windowsFiles 1  
;SectionEnd  

;Section -folders SEC0003  
;    SetOutPath $PROFILE  
;    SetOverwrite on  
;    CreateDirectory "Launcher"  
;    CreateDirectory "Output Files"  
;    WriteRegStr HKLM "${REGKEY}\Components" folders 1  
;SectionEnd  
  
;Section -shortcuts SEC0004  
;    SetShellVarContext all  
;    SetOutPath "$SMPROGRAMS\$StartMenuGroup"  
;    CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Command Centre™ for Sprint.lnk" "$INSTDIR\Command Centre™ for Sprint v1.2.exe" "" "$INSTDIR\Sprint.ico"  
;    CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Help.lnk" "$INSTDIR\Command Centre™ for Sprint v1.0 Help.chm"  
;    CreateShortcut "$SMPROGRAMS\$StartMenuGroup\User Manual.lnk" "$INSTDIR\Command Centre™ for Sprint v1.0 User Manual.pdf"  
;    CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Uninstall.lnk" "$INSTDIR\Uninstall.exe"      
;    SetOutPath $DESKTOP  
;    CreateShortcut "$DESKTOP\Command Centre™ for Sprint.lnk" "$INSTDIR\Command Centre™ for Sprint v1.2.exe" "" "$INSTDIR\Sprint.ico"  
;    WriteRegStr HKLM "${REGKEY}\Components" shortcuts 1  
;SectionEnd  
  
;Section -post SEC0005  
;    WriteRegStr HKLM "${REGKEY}" Path $INSTDIR  
;    SetOutPath $INSTDIR  
;    WriteUninstaller $INSTDIR\uninstall.exe  
;    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" DisplayName "$(^Name)"  
;    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" DisplayVersion "${VERSION}"  
;    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" Publisher "${COMPANY}"  
;    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" URLInfoAbout "${URL}"  
;    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" DisplayIcon $INSTDIR\uninstall.exe  
;    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" UninstallString $INSTDIR\uninstall.exe  
;    WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" NoModify 1  
;    WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" NoRepair 1  
;SectionEnd  
  
# Macro for selecting uninstaller sections  
;!macro SELECT_UNSECTION SECTION_NAME UNSECTION_ID  
;    Push $R0  
;    ReadRegStr $R0 HKLM "${REGKEY}\Components" "${SECTION_NAME}"  
;    StrCmp $R0 1 0 next${UNSECTION_ID}  
;    !insertmacro SelectSection "${UNSECTION_ID}"  
;    GoTo done${UNSECTION_ID}  
;next${UNSECTION_ID}:  
;    !insertmacro UnselectSection "${UNSECTION_ID}"  
;done${UNSECTION_ID}:  
;    Pop $R0  
;!macroend  
  
# Uninstaller sections  
  
  
;Section /o -un.folders UNSEC0004  
;    RmDir "$INSTDIR\LogFiles"  
;    RmDir "$INSTDIR\Output Files"  
;    RmDir  $SMPROGRAMS\$StartMenuGroup  
;    Delete /REBOOTOK $INSTDIR\*  
;    RmDir /REBOOTOK $INSTDIR  
;    Delete /REBOOTOK $INSTDIR\*  
;    RmDir /REBOOTOK $INSTDIR  
;    Delete $PROGRAMFILES\$INSTDIR\*  
;    RmDir $PROGRAMFILES\$INSTDIR  
;    RmDir $PROGRAMFILES\Arrayjet  
;    DeleteRegValue HKLM "${REGKEY}\Components" folders  
;SectionEnd  
  
;Section /o -un.fonts UNSEC0003  
;    Delete /REBOOTOK $FONTS\lte50331.ttf  
;    DeleteRegValue HKLM "${REGKEY}\Components" fonts  
;SectionEnd  
  
;Section /o -un.windowsFiles UNSEC0002  
;    Delete /REBOOTOK $SYSDIR\mfc70.dll  
;    Delete /REBOOTOK $SYSDIR\MFC71.dll  
 ;   Delete /REBOOTOK $SYSDIR\MSVCI70.dll  
;    Delete /REBOOTOK $SYSDIR\msvcp71.dll  
;    Delete /REBOOTOK $SYSDIR\msvcr70.dll  
;    Delete /REBOOTOK $SYSDIR\msvcr71.dll  
;    DeleteRegValue HKLM "${REGKEY}\Components" windowsFiles  
;SectionEnd  
  
;Section /o -un.MainInstallFolder UNSEC0001  
;    Delete $INSTDIR\runtime.aj  
;    Delete $INSTDIR\inst_param.aj  
;    Delete "$INSTDIR\Command Centre™ Release Notes v1.0.chm"  
;    Delete /REBOOTOK "$INSTDIR\Command Centre™ for Sprint v1.2.exe"  
;    Delete /REBOOTOK "$INSTDIR\Command Centre™ for Sprint v1.0 User Manual.pdf"  
;    Delete /REBOOTOK "$INSTDIR\Command Centre™ for Sprint v1.0 Help.chm"  
;    Delete /REBOOTOK "$INSTDIR\Sprint.ico"  
;    DeleteRegValue HKLM "${REGKEY}\Components" MainInstallFolder  
;SectionEnd  
  
;Section /o -un.shortcuts UNSEC0000  
;    SetShellVarContext all  
;    Delete "$SMPROGRAMS\$StartMenuGroup\Command Centre™ for Sprint.lnk"  
;    Delete "$SMPROGRAMS\$StartMenuGroup\Help.lnk"  
;    Delete "$SMPROGRAMS\$StartMenuGroup\User Manual.lnk"  
;    Delete "$SMPROGRAMS\$StartMenuGroup\Uninstall.lnk"     
;    Delete "$SMPROGRAMS\$StartMenuGroup\*.*"  
;    RMDir  "$SMPROGRAMS\$StartMenuGroup"  
;    Delete "$DESKTOP\Command Centre™ for Sprint.lnk"   
;    DeleteRegValue HKLM "${REGKEY}\Components" shortcuts  
;SectionEnd  
  
;Section -un.post UNSEC0005  
;    DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)"  
;    Delete /REBOOTOK $INSTDIR\uninstall.exe  
;    DeleteRegValue HKLM "${REGKEY}" Path  
;    DeleteRegKey /IfEmpty HKLM "${REGKEY}\Components"  
;    DeleteRegKey /IfEmpty HKLM "${REGKEY}"  
;    RmDir /REBOOTOK $INSTDIR  
;SectionEnd  
  
# Installer functions  
;Function .onInit  
;    InitPluginsDir  
;    StrCpy $StartMenuGroup "Arrayjet"  
;FunctionEnd  
  
# Uninstaller functions  
;Function un.onInit  
;    ReadRegStr $INSTDIR HKLM "${REGKEY}" Path  
;    StrCpy $StartMenuGroup "Arrayjet"  
;    !insertmacro SELECT_UNSECTION shortcuts ${UNSEC0000}  
;    !insertmacro SELECT_UNSECTION MainInstallFolder ${UNSEC0001}  
;    !insertmacro SELECT_UNSECTION windowsFiles ${UNSEC0002}  
;    !insertmacro SELECT_UNSECTION fonts ${UNSEC0003}  
;    !insertmacro SELECT_UNSECTION folders ${UNSEC0004}  
;FunctionEnd
