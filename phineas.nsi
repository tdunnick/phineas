; phineas.nsi
;--------------------------------
!include nsDialogs.nsh
!include LogicLib.nsh
!include FileFunc.nsh

; The name of the installer
Name "Phineas"

; Our own configuration variables

; version normally passed on command line, but...
!ifndef VERSION
!define VERSION 0.0
!endif

!ifndef PVERSION
!define PVERSION "0.0.0.1"
!endif

; if we want to generate certificates...
; !define CERTS

var partyWin
var orgWin
var partyId
var org

!ifdef CERTS
var unitWin
var emailWin
var stateWin
var localWin
var countryWin
var commonWin
var unit
var email
var state
var local
var country
var common
!endif

; Version information...

VIProductVersion ${PVERSION}
VIAddVersionKey "ProductName" "Phineas"
VIAddVersionKey "Comments" "Extensible PHINMS Compatible Transport"
VIAddVersionKey "CompanyName" ""
VIAddVersionKey "LegalTrademarks" ""
VIAddVersionKey "LegalCopyright" "© 2011-2012 Thomas Dunnick"
VIAddVersionKey "FileDescription" "Phineas Installer"
VIAddVersionKey "FileVersion" ${VERSION}

; The file to write
OutFile "C:\usr\www\html\mywebspace\phineas\phineas${VERSION}.exe"

; Make sure nobody fiddled with this...
CRCCheck force

; The default installation directory
; Note we will tack on "\Phineas" to the install directory for all Files
InstallDir "C:\Program Files"

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
; InstallDirRegKey HKLM "Software\NSIS_Example2" "Install_Dir"

; Request application privileges for Windows Vista
RequestExecutionLevel admin

; Our license
LicenseData License.txt
; And Icons
Icon images\install.ico
UninstallIcon images\uninstall.ico

;--------------------------------

; Pages

Page license
Page components
Page custom setupPage setupPageLeave
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------
; The stuff to install

; basic files...
Section "Phineas Core (required)"

  SectionIn RO
  
  IfFileExists "$INSTDIR\Phineas\uninstall.exe" 0 +4
  MessageBox MB_YESNO "Phineas is already installed in this folder$\nReplace Installation" IDYES +2 IDNO 0
  Quit
  Call Cleanup
  SetOutPath "$INSTDIR\Phineas"
  File "README.TXT"
  File "License.txt"  
  File "OpenSSL.txt"  
  SetOutPath "$INSTDIR\Phineas\bin"
  File "bin\psetup.exe"
  SearchPath $0 "ssleay32.dll"
  IfErrors +3
  ${GetFileVersion} $0 $1
  StrCmp $1 "0.9.8.7" +2
  File "bin\ssleay32.dll"
  SearchPath $0 "libeay32.dll"
  IfErrors +3
  ${GetFileVersion} $0 $1
  StrCmp $1 "0.9.8.7" +2
  File "bin\libeay32.dll"
  SetOutPath "$INSTDIR\Phineas\console"
  File "console\"
  SetOutPath "$INSTDIR\Phineas\console\images"
  File "console\images\"
  SetOutPath "$INSTDIR\Phineas\cpa"
  File "cpa\loopback.xml"
  SetOutPath "$INSTDIR\Phineas\security"
  File "security\sslcert.pfx"
  File "security\sslcert.ca"
  File "security\phineas.pfx"
  File "security\phineas.pem"
  SetOutPath "$INSTDIR\Phineas\templates"
  File "templates\"
  SetOutPath "$INSTDIR\Phineas\logs"
  SetOutPath "$INSTDIR\Phineas\tmp"
  SetOutPath "$INSTDIR\Phineas\queues"
  SetOutPath "$INSTDIR\Phineas\data\ebxml\ack"
  SetOutPath "$INSTDIR\Phineas\data\ebxml\outgoing"
  SetOutPath "$INSTDIR\Phineas\data\ebxml\incoming"
  SetOutPath "$INSTDIR\Phineas\data\ebxml\processed"

  ; create an uninstaller and link
  WriteUninstaller "Phineas\uninstall.exe"
  CreateShortCut "$INSTDIR\Phineas\bin\Uninstall.lnk" "$INSTDIR\Phineas\uninstall.exe" "" "$INSTDIR\Phineas\uninstall.exe" 0
  
SectionEnd

; services

SectionGroup "Servers"

Section "Phineas GUI"
  SetOutPath "$INSTDIR\Phineas\bin"
  File "bin\Phineas.exe"
  CreateShortCut "Phineas.lnk" "$INSTDIR\Phineas\bin\Phineas.exe" "" "$INSTDIR\Phineas\bin\Phineas.exe" 0 "" "" "Phineas Transceiver"
SectionEnd

Section /o "Phineas Service"
  SetOutPath "$INSTDIR\Phineas\bin"
  File "bin\Phineasd.exe"
  File "bin\addservice.bat"
  File "bin\delservice.bat"
SectionEnd

Section /o "Phineas Command Line"
  SetOutPath "$INSTDIR\Phineas\bin"
  File "bin\Phineasc.exe"
  CreateShortCut "Phineas Command Line.lnk" "$INSTDIR\Phineas\bin\Phineasc.exe" "" "$INSTDIR\Phineas\bin\Phineasc.exe" 0 "" "" "Phineas Command Line"
  IfFileExists "Phineas.lnk" +2
  Rename "Phineas Command Line.lnk" "Phineas.lnk"

SectionEnd

SectionGroupEnd

; utilities

SectionGroup "Utilities"

Section /o "Help Generator"
  SetOutPath "$INSTDIR\Phineas\bin"
  File "bin\chelp.exe"
SectionEnd

Section /o "CPA Generator"
  SetOutPath "$INSTDIR\Phineas\bin"
  File "bin\cpa.exe"
SectionEnd

Section /o "XML Beautifier"
  SetOutPath "$INSTDIR\Phineas\bin"
  File "bin\xmlb.exe"
SectionEnd

Section /o "Access dB generator"
  SetOutPath "$INSTDIR\Phineas\bin"
  File "bin\accessdb.vbs"
SectionEnd

Section /o "XML envelope encryptor"
  SetOutPath "$INSTDIR\Phineas\bin"
  File "bin\xcrypt.exe"
SectionEnd

Section /o "Phineas configuration tool"
  SetOutPath "$INSTDIR\Phineas\bin"
  File "bin\psetup.exe"
SectionEnd

SectionGroupEnd

; data base options

Section /o "Access Queues"
  AddSize 81
  IfFileExists "$INSTDIR\Phineas\bin\accessdb.vbs" +3
  SetOutPath "$INSTDIR\Phineas\bin"
  File "bin\accessdb.vbs"
  SetOutPath "$INSTDIR\Phineas"
  ExecWait 'cmd /c "$INSTDIR\Phineas\bin\accessdb.vbs"'
  IfErrors 0 +2
  Abort "Couldn't create ACCESS db"
SectionEnd

; various short cuts...

SectionGroup "Shortcuts"

Section "Start Programs Menu"
  CreateDirectory "$SMPROGRAMS\Phineas"
  CopyFiles "$INSTDIR\Phineas\bin\*.lnk" "$SMPROGRAMS\Phineas"
SectionEnd

Section /o "Start Menu"
  CopyFiles "$INSTDIR\Phineas\bin\Phineas.lnk" "$STARTMENU"
SectionEnd

Section /o "Desk Top"
  CopyFiles "$INSTDIR\Phineas\bin\Phineas*.lnk" "$DESKTOP"
SectionEnd

Section /o "Quick Start"
  CopyFiles "$INSTDIR\Phineas\bin\Phineas.lnk" "$QUICKLAUNCH"
SectionEnd

Section /o "Auto Start"
  CopyFiles "$INSTDIR\Phineas\bin\Phineas.lnk" "$SMSTARTUP"
SectionEnd

SectionGroupEnd

; stuff to do after everything else has been copied...

Section "- complete setup"


  ; make sure we have everything needed to run...
  IfFileExists "$INSTDIR\Phineas\bin\phineas*.exe" 0 noserver
  ; create our running configuration...
  ExecWait '"$INSTDIR\Phineas\bin\psetup.exe" -o "$org" -p "$partyId"'
  IfErrors setupfail 0
  ; ExecShell "open" "$INSTDIR\Phineas\logs\psetup.log"
  ; ExecShell "open" "$INSTDIR\Phineas\README.TXT"
  Return

noserver:
  MessageBox MB_OK "No servers selected! Installation aborted."
  Goto clean

setupfail:
  MessageBox MB_OK "setup failed! Installation aborted."
  ExecShell "open" "$INSTDIR\Phineas\logs\psetup.log"
  Goto clean

clean:
  Call Cleanup

SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
  
  ; Note since we tack on "Phineas" above, but run this from "inside",
  ; $INSTDIR is actually where the un-installer lives.
  ; we'll check that we are in the right place before proceeding
  IfFileExists $INSTDIR\templates\Phineas.xml 0 notfound
  ; remove any desktop shortcuts
  Delete "$DESKTOP\Phineas*.lnk"
  Delete "$QUICKLAUNCH\Phineas*.lnk"
  Delete "$STARTMENU\Phineas*.lnk"
  Delete "$SMSTARTUP\Phineas*.lnk"
  RMDir /r "$SMPROGRAMS\Phineas"
  RMDir /r "$INSTDIR"
  Return

notfound:
  Abort "Uninstaller may not be in proper location - Aborting"

SectionEnd

; ---------------------

; functions...


; custom setup for Phineas and certs

Function setupPage
  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${Endif}

!ifdef CERTS
  ${If} $country == ""
    StrCpy $country "USA"
  ${Endif}
!endif 

  ${NSD_CreateLabel} 0 0u 30% 12u "Party ID *"
  Pop $0
  ${NSD_CreateText} 30% 0u 100% 12u $partyId
  Pop $partyWin
  ${NSD_CreateLabel} 0 15u 30% 12u "Organization *"
  Pop $0
  ${NSD_CreateText} 30% 15u 100% 12u $org
  Pop $orgWin
!ifdef CERTS
  ${NSD_CreateLabel} 0 30u 30% 12u "Organizational Unit"
  Pop $0
  ${NSD_CreateText} 30% 30u 100% 12u $unit
  Pop $unitWin
  ${NSD_CreateLabel} 0 45u 30% 12u "email"
  Pop $0
  ${NSD_CreateText} 30% 45u 100% 12u $email
  Pop $emailWin
  ${NSD_CreateLabel} 0 60u 30% 12u "Common Name"
  Pop $0
  ${NSD_CreateText} 30% 60u 100% 12u $common
  Pop $commonWin
  ${NSD_CreateLabel} 0 75u 30% 12u "City"
  Pop $0
  ${NSD_CreateText} 30% 75u 100% 12u $local
  Pop $localWin
  ${NSD_CreateLabel} 0 90u 30% 12u "State"
  Pop $0
  ${NSD_CreateText} 30% 90u 100% 12u $state
  Pop $stateWin
  ${NSD_CreateLabel} 0 105u 30% 12u "Country"
  Pop $0
  ${NSD_CreateText} 30% 105u 100% 12u $country
  Pop $countryWin
!endif
  ${NSD_CreateLabel} 0 120u 30% 12u "(* required)"
  Pop $0
  nsDialogs::Show
FunctionEnd

; when we leave the custom page, copy out the values
; checking for required ones

Function setupPageLeave
  ${NSD_GetText} $partyWin $partyId
  ${If} $partyId == ""
    MessageBox MB_OK "You must complete the Party ID"
    Abort
  ${Endif}
  ${NSD_GetText} $orgWin $org
  ${If} $org == ""
    MessageBox MB_OK "You must complete the Organization"
    Abort
  ${Endif}
!ifdef CERTS
  ${NSD_GetText} $emailWin $email
  ${NSD_GetText} $commonWin $common
  ${NSD_GetText} $stateWin $state
  ${NSD_GetText} $localWin $local
  ${NSD_GetText} $countryWin $country
  ${NSD_GetText} $commonWin $common
  ${NSD_GetText} $unitWin $unit
!endif
FunctionEnd

; after failures or aborts clean up artifacts of partial install

Function .onInstFailed
  Call Cleanup
FunctionEnd

Function .onUserAbort
  Call Cleanup
FunctionEnd

Function Cleanup
  SetOutPath $INSTDIR
  Delete "$DESKTOP\Phineas*.lnk"
  Delete "$QUICKLAUNCH\Phineas*.lnk"
  Delete "$STARTMENU\Phineas*.lnk"
  Delete "$SMSTARTUP\Phineas*.lnk"
  RMDir /r "$SMPROGRAMS\Phineas"
  RMDir /r "$INSTDIR\Phineas"
FunctionEnd

