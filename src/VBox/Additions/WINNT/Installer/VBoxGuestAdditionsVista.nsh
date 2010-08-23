
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

  ; VBoxNET drivers are not tested yet - commented out until officially supported and released
  ;nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /i "PCI\VEN_1022&DEV_2000&SUBSYS_20001022&REV_40" "$INSTDIR\VBoxNET.inf" "Net"'
  ;Pop $0                      ; Ret value
  ;IntCmp $0 0 +1 error error  ; Check ret value (0=OK, 1=Error)
  ;Goto done

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

!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
  ; Remove network card driver
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /u "PCI\VEN_1022&DEV_2000&SUBSYS_20001022&REV_40"'
  Pop $0    ; Ret value
  ; @todo Add error handling here!
!endif

FunctionEnd
!macroend
!insertmacro Vista_Uninstall ""
!insertmacro Vista_Uninstall "un."
