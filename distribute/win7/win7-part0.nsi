Name "install-Launcher"  

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
OutFile "Install-Launcher-win7.exe"  
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
; followed by win7-part1.nsi
