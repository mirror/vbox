
Function Vista_CopyFiles

  SetOutPath "$INSTDIR"
  SetOverwrite on

  ; The files are for Vista only, they go into the application directory

  ; VBoxNET drivers are not tested yet - commented out until officially supported and released
  ;FILE "$%PATH_OUT%\bin\additions\VBoxNET.inf"
  ;FILE "$%PATH_OUT%\bin\additions\VBoxNET.sys"


FunctionEnd

Function Vista_InstallFiles

  DetailPrint "Installing Drivers for Vista / Windows 7 ..."

  SetOutPath "$INSTDIR"
  ; Nothing here yet
  Goto done

error:
  Abort "ERROR: Could not install files for Vista / Windows 7! Installation aborted."

done:

FunctionEnd

Function Vista_Main

  Call Vista_CopyFiles
  Call Vista_InstallFiles

FunctionEnd

!macro Vista_UninstallInstDir un
Function ${un}Vista_UninstallInstDir

!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
  Delete /REBOOTOK "$INSTDIR\netamd.inf"
  Delete /REBOOTOK "$INSTDIR\pcntpci5.cat"
  Delete /REBOOTOK "$INSTDIR\PCNTPCI5.sys"
!endif

FunctionEnd
!macroend
!insertmacro Vista_UninstallInstDir ""
!insertmacro Vista_UninstallInstDir "un."

!macro Vista_Uninstall un
Function ${un}Vista_Uninstall

   ; Remove credential provider
   DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}"
   DeleteRegKey HKCR "CLSID\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}"
   Delete /REBOOTOK "$g_strSystemDir\VBoxCredProv.dll"

FunctionEnd
!macroend
!insertmacro Vista_Uninstall ""
!insertmacro Vista_Uninstall "un."
