; $Id: $
;; @file
; VBoxGuestAdditionsVista.nsh - Guest Additions installation for Windows Vista/7.
;

;
; Copyright (C) 2006-2011 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

Function Vista_CheckForRequirements

  Push $0

  DetailPrint "Checking for installation requirements for Vista / Windows 7 / Windows 8 ..."

  ${If} $g_bForceInstall == "true"
    DetailPrint "Forcing installatoin, checking requirements skipped"
    goto success
  ${EndIf}

  ${If} $g_bWithD3D == "true"
    ; Validate D3D files
    Call ValidateD3DFiles
    Pop $0
    ${If} $0 == "1" ; D3D files are invalid
      ${If} $g_bWithWDDM == "true"
        MessageBox MB_ICONSTOP|MB_OK $(VBOX_COMPONENT_D3D_INVALID_WDDM) /SD IDOK
        goto failure
      ${Else}
        MessageBox MB_ICONSTOP|MB_OKCANCEL $(VBOX_COMPONENT_D3D_INVALID_XPDM) /SD IDCANCEL IDCANCEL failure
      ${EndIf}
    ${EndIf}
  ${EndIf}

  Goto success

failure:

  Abort "ERROR: Requirements not met! Installation aborted."
  goto exit

success:

  ; Nothing to do here right now
  Goto exit

exit:

  Pop $0

FunctionEnd

Function Vista_CopyFiles

  SetOutPath "$INSTDIR"
  SetOverwrite on

  ; The files are for Vista only, they go into the application directory

  ; VBoxNET drivers are not tested yet - commented out until officially supported and released
  ;FILE "$%PATH_OUT%\bin\additions\VBoxNET.inf"
  ;FILE "$%PATH_OUT%\bin\additions\VBoxNET.sys"


FunctionEnd

Function Vista_InstallFiles

  DetailPrint "Installing drivers for Vista / Windows 7 / Windows 8 ..."

  SetOutPath "$INSTDIR"
  ; Nothing here yet
  Goto done

error:

  Abort "ERROR: Could not install files! Installation aborted."

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
   DetailPrint "Removing auto-logon support ..."
   DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}"
   DeleteRegKey HKCR "CLSID\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}"
   Delete /REBOOTOK "$g_strSystemDir\VBoxCredProv.dll"

FunctionEnd
!macroend
!insertmacro Vista_Uninstall ""
!insertmacro Vista_Uninstall "un."
