
; @todo Replace this crappy stuff with a "VBoxDrvInst /delnetprovider".
!macro RemoveFromProvider un
Function ${un}RemoveFromProvider
  Exch $0
  Push $1
  Push $2
  Push $3
  Push $4
  Push $5
  Push $6

  ReadRegStr $1 HKLM "$R0" "ProviderOrder"
  StrCpy $5 $1 1 -1 # copy last char
  StrCmp $5 "," +2 # if last char != ,
  StrCpy $1 "$1," # append ,
  Push $1
  Push "$0,"
  Call ${un}StrStr ; Find `$0,` in $1
  Pop $2 ; pos of our dir
  StrCmp $2 "" unRemoveFromPath_done
  ; else, it is in path
  # $0 - path to add
  # $1 - path var
  StrLen $3 "$0,"
  StrLen $4 $2
  StrCpy $5 $1 -$4 # $5 is now the part before the path to remove
  StrCpy $6 $2 "" $3 # $6 is now the part after the path to remove
  StrCpy $3 $5$6

  StrCpy $5 $3 1 -1 # copy last char
  StrCmp $5 "," 0 +2 # if last char == ,
  StrCpy $3 $3 -1 # remove last char

  WriteRegStr HKLM "$R0" "ProviderOrder" $3

unRemoveFromPath_done:
  Pop $6
  Pop $5
  Pop $4
  Pop $3
  Pop $2
  Pop $1
  Pop $0
FunctionEnd
!macroend
!insertmacro RemoveFromProvider ""
!insertmacro RemoveFromProvider "un."

!macro RemoveProvider un
Function ${un}RemoveProvider
  Push $R0
  StrCpy $R0 "VBoxSF"
  Push $R0
  StrCpy $R0 "SYSTEM\CurrentControlSet\Control\NetworkProvider\HWOrder"
  Call ${un}RemoveFromProvider
  StrCpy $R0 "VBoxSF"
  Push $R0
  StrCpy $R0 "SYSTEM\CurrentControlSet\Control\NetworkProvider\Order"
  Call ${un}RemoveFromProvider
  Pop $R0
FunctionEnd
!macroend
!insertmacro RemoveProvider ""
!insertmacro RemoveProvider "un."

!macro UninstallCommon un
Function ${un}UninstallCommon

  Delete /REBOOTOK "$INSTDIR\install.log"
  Delete /REBOOTOK "$INSTDIR\uninst.exe"
  Delete /REBOOTOK "$INSTDIR\${PRODUCT_NAME}.url"

  ; Remove common files
  Delete /REBOOTOK "$INSTDIR\VBoxDrvInst.exe"

  Delete /REBOOTOK "$INSTDIR\VBoxVideo.inf"
!ifdef VBOX_SIGN_ADDITIONS
  Delete /REBOOTOK "$INSTDIR\VBoxVideo.cat"
!endif

  Delete /REBOOTOK "$INSTDIR\VBoxGINA.dll"
  Delete /REBOOTOK "$INSTDIR\iexplore.ico"

  ; Delete registry keys
  DeleteRegKey /ifempty HKLM "${PRODUCT_INSTALL_KEY}"
  DeleteRegKey /ifempty HKLM "${VENDOR_ROOT_KEY}"

  ; Delete desktop & start menu entries
  Delete "$DESKTOP\${PRODUCT_NAME} Guest Additions.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME} Guest Additions\Uninstall.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME} Guest Additions\Website.lnk"
  RMDIR "$SMPROGRAMS\${PRODUCT_NAME} Guest Additions"

  ; Delete Guest Additions directory (only if completely empty)
  RMDir /REBOOTOK "$INSTDIR"

  ; Delete vendor installation directory (only if completely empty)
!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
  RMDir /REBOOTOK "$PROGRAMFILES32\$%VBOX_VENDOR_SHORT%"
!else   ; 64-bit
  RMDir /REBOOTOK "$PROGRAMFILES64\$%VBOX_VENDOR_SHORT%"
!endif

  ; Remove registry entries
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"

FunctionEnd
!macroend
!insertmacro UninstallCommon ""
!insertmacro UninstallCommon "un."

!macro Uninstall un
Function ${un}Uninstall

  DetailPrint "Uninstalling system files ..."
!ifdef _DEBUG
  DetailPrint "Detected OS version: Windows $g_strWinVersion"
  DetailPrint "System Directory: $g_strSystemDir"
!endif

  ; Which OS are we using?
!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
  StrCmp $g_strWinVersion "NT4" nt4     ; Windows NT 4.0
!endif
  StrCmp $g_strWinVersion "2000" w2k    ; Windows 2000
  StrCmp $g_strWinVersion "XP" w2k      ; Windows XP
  StrCmp $g_strWinVersion "2003" w2k    ; Windows 2003 Server
  StrCmp $g_strWinVersion "Vista" vista ; Windows Vista
  StrCmp $g_strWinVersion "7" vista     ; Windows 7

  ${If} $g_bForceInstall == "true"
    Goto vista ; Assume newer OS than we know of ...
  ${EndIf}

  Goto notsupported

!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
nt4:

  Call ${un}NT_Uninstall
  goto common
!endif

w2k:

  Call ${un}W2K_Uninstall
  goto common

vista:

  Call ${un}W2K_Uninstall
  Call ${un}Vista_Uninstall
  goto common

notsupported:

  MessageBox MB_ICONSTOP $(VBOX_PLATFORM_UNSUPPORTED) /SD IDOK
  Goto exit

common:

exit:

FunctionEnd
!macroend
!insertmacro Uninstall ""
!insertmacro Uninstall "un."

!macro UninstallInstDir un
Function ${un}UninstallInstDir

  DetailPrint "Uninstalling directory ..."
!ifdef _DEBUG
  DetailPrint "Detected OS version: Windows $g_strWinVersion"
  DetailPrint "System Directory: $g_strSystemDir"
!endif

  ; Which OS are we using?
!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
  StrCmp $g_strWinVersion "NT4" nt4     ; Windows NT 4.0
!endif
  StrCmp $g_strWinVersion "2000" w2k    ; Windows 2000
  StrCmp $g_strWinVersion "XP" w2k      ; Windows XP
  StrCmp $g_strWinVersion "2003" w2k    ; Windows 2003 Server
  StrCmp $g_strWinVersion "Vista" vista ; Windows Vista
  StrCmp $g_strWinVersion "7" vista     ; Windows 7

  ${If} $g_bForceInstall == "true"
    Goto vista ; Assume newer OS than we know of ...
  ${EndIf}

  Goto notsupported

!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
nt4:

  Call ${un}NT_UninstallInstDir
  goto common
!endif

w2k:

  Call ${un}W2K_UninstallInstDir
  goto common

vista:

  Call ${un}W2K_UninstallInstDir
  Call ${un}Vista_UninstallInstDir
  goto common

notsupported:

  MessageBox MB_ICONSTOP $(VBOX_PLATFORM_UNSUPPORTED) /SD IDOK
  Goto exit

common:

  Call ${un}UninstallCommon

exit:

FunctionEnd
!macroend
!insertmacro UninstallInstDir ""
!insertmacro UninstallInstDir "un."
