
Function W2K_SetVideoResolution

  ; NSIS only supports global vars, even in functions -- great
  Var /GLOBAL i
  Var /GLOBAL tmp
  Var /GLOBAL tmppath
  Var /GLOBAL dev_id
  Var /GLOBAL dev_desc

  ; Check for all required parameters
  StrCmp $g_iScreenX "0" exit
  StrCmp $g_iScreenY "0" exit
  StrCmp $g_iScreenBpp "0" exit

  DetailPrint "Setting display parameters ($g_iScreenXx$g_iScreenY, $g_iScreenBpp BPP) ..."

  ; Enumerate all video devices (up to 32 at the moment, use key "MaxObjectNumber" key later)
  ${For} $i 0 32

    ReadRegStr $tmp HKLM "HARDWARE\DEVICEMAP\VIDEO" "\Device\Video$i"
    StrCmp $tmp "" dev_not_found

    ; Extract path to video settings
    ; Ex: \Registry\Machine\System\CurrentControlSet\Control\Video\{28B74D2B-F0A9-48E0-8028-D76F6BB1AE65}\0000
    ; Or: \Registry\Machine\System\CurrentControlSet\Control\Video\vboxvideo\Device0
    ; Result: Machine\System\CurrentControlSet\Control\Video\{28B74D2B-F0A9-48E0-8028-D76F6BB1AE65}\0000
    Push "$tmp"     ; String
    Push "\"        ; SubString
    Push ">"        ; SearchDirection
    Push ">"        ; StrInclusionDirection
    Push "0"        ; IncludeSubString
    Push "2"        ; Loops
    Push "0"        ; CaseSensitive
    Call StrStrAdv
    Pop $tmppath    ; $1 only contains the full path
    StrCmp $tmppath "" dev_not_found

    ; Get device description
    ReadRegStr $dev_desc HKLM "$tmppath" "Device Description"
!ifdef _DEBUG
    DetailPrint "Registry path: $tmppath"
    DetailPrint "Registry path to device name: $temp"
!endif
    DetailPrint "Detected video device: $dev_desc"

    ${If} $dev_desc == "VirtualBox Graphics Adapter"
      DetailPrint "VirtualBox video device found!"
      Goto dev_found
    ${EndIf}
  ${Next}
  Goto dev_not_found

dev_found:

  ; If we're on Windows 2000, skip the ID detection ...
  ${If} $g_strWinVersion == "2000"
    Goto change_res
  ${EndIf}
  Goto dev_found_detect_id

dev_found_detect_id:

  StrCpy $i 0 ; Start at index 0
  DetailPrint "Detecting device ID ..."

dev_found_detect_id_loop:

  ; Resolve real path to hardware instance "{GUID}"
  EnumRegKey $dev_id HKLM "SYSTEM\CurrentControlSet\Control\Video" $i
  StrCmp $dev_id "" dev_not_found ; No more entries? Jump out
!ifdef _DEBUG
  DetailPrint "Got device ID: $dev_id"
!endif
  ReadRegStr $dev_desc HKLM "SYSTEM\CurrentControlSet\Control\Video\$dev_id\0000" "Device Description" ; Try to read device name
  ${If} $dev_desc == "VirtualBox Graphics Adapter"
    DetailPrint "Device ID of $dev_desc: $dev_id"
    Goto change_res
  ${EndIf}

  IntOp $i $i + 1 ; Increment index
  goto dev_found_detect_id_loop

dev_not_found:

  DetailPrint "No VirtualBox video device (yet) detected! No custom mode set."
  Goto exit

change_res:

!ifdef _DEBUG
  DetailPrint "Device description: $dev_desc"
  DetailPrint "Device ID: $dev_id"
!endif

  Var /GLOBAL reg_path_device
  Var /GLOBAL reg_path_monitor

  DetailPrint "Custom mode set: Platform is Windows $g_strWinVersion"
  ${If} $g_strWinVersion == "2000"
  ${OrIf} $g_strWinVersion == "Vista"
    StrCpy $reg_path_device "SYSTEM\CurrentControlSet\SERVICES\VBoxVideo\Device0"
    StrCpy $reg_path_monitor "SYSTEM\CurrentControlSet\SERVICES\VBoxVideo\Device0\Mon00000001"
  ${ElseIf} $g_strWinVersion == "XP"
  ${OrIf} $g_strWinVersion == "7"
    StrCpy $reg_path_device "SYSTEM\CurrentControlSet\Control\Video\$dev_id\0000"
    StrCpy $reg_path_monitor "SYSTEM\CurrentControlSet\Control\VIDEO\$dev_id\0000\Mon00000001"
  ${Else}
    DetailPrint "Custom mode set: Windows $g_strWinVersion not supported yet"
    Goto exit
  ${EndIf}

  ; Write the new value in the adapter config (VBoxVideo.sys) using hex values in binary format
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /registry write HKLM $reg_path_device CustomXRes REG_BIN $g_iScreenX DWORD'
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /registry write HKLM $reg_path_device CustomYRes REG_BIN $g_iScreenY DWORD'
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /registry write HKLM $reg_path_device CustomBPP REG_BIN $g_iScreenBpp DWORD'

  ; ... and tell Windows to use that mode on next start!
  WriteRegDWORD HKCC $reg_path_device "DefaultSettings.XResolution" "$g_iScreenX"
  WriteRegDWORD HKCC $reg_path_device "DefaultSettings.YResolution" "$g_iScreenY"
  WriteRegDWORD HKCC $reg_path_device "DefaultSettings.BitsPerPixel" "$g_iScreenBpp"

  WriteRegDWORD HKCC $reg_path_monitor "DefaultSettings.XResolution" "$g_iScreenX"
  WriteRegDWORD HKCC $reg_path_monitor "DefaultSettings.YResolution" "$g_iScreenY"
  WriteRegDWORD HKCC $reg_path_monitor "DefaultSettings.BitsPerPixel" "$g_iScreenBpp"

  DetailPrint "Custom mode set to $g_iScreenXx$g_iScreenY, $g_iScreenBpp BPP on next restart."

exit:

FunctionEnd

Function W2K_Prepare

  ; Stop / kill VBoxService
  Call StopVBoxService

  ; Stop / kill VBoxTray
  Call StopVBoxTray

  ; Delete VBoxService from registry
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "VBoxService"

  ; Delete old VBoxService.exe from install directory (replaced by VBoxTray.exe)
  Delete /REBOOTOK "$INSTDIR\VBoxService.exe"

FunctionEnd

Function W2K_CopyFiles

  SetOutPath "$INSTDIR"

  ; Video driver
  FILE "$%PATH_OUT%\bin\additions\VBoxVideo.sys"
  FILE "$%PATH_OUT%\bin\additions\VBoxDisp.dll"

  ; Mouse driver
  FILE "$%PATH_OUT%\bin\additions\VBoxMouse.sys"
  FILE "$%PATH_OUT%\bin\additions\VBoxMouse.inf"
!ifdef VBOX_SIGN_ADDITIONS
  FILE "$%PATH_OUT%\bin\additions\VBoxMouse.cat"
!endif

  ; Guest driver
  FILE "$%PATH_OUT%\bin\additions\VBoxGuest.sys"
  FILE "$%PATH_OUT%\bin\additions\VBoxGuest.inf"
!ifdef VBOX_SIGN_ADDITIONS
  FILE "$%PATH_OUT%\bin\additions\VBoxGuest.cat"
!endif

  ; Guest driver files
  FILE "$%PATH_OUT%\bin\additions\VBCoInst.dll"
  FILE "$%PATH_OUT%\bin\additions\VBoxTray.exe"
  FILE "$%PATH_OUT%\bin\additions\VBoxControl.exe"	; Not used by W2K and up, but required by the .INF file

  ; WHQL fake
!ifdef WHQL_FAKE
  FILE "$%PATH_OUT%\bin\additions\VBoxWHQLFake.exe"
!endif

  SetOutPath $g_strSystemDir

  ; VBoxService
  FILE "$%PATH_OUT%\bin\additions\VBoxService.exe" ; Only used by W2K and up (for Shared Folders at the moment)

!if $%VBOX_WITH_CROGL% == "1"
  !if $%VBOX_WITH_WDDM% == "1"
    ${If} $g_bWithWDDM == "true"
      SetOutPath "$INSTDIR"
      ; WDDM Video driver
      FILE "$%PATH_OUT%\bin\additions\VBoxVideoWddm.sys"
      FILE "$%PATH_OUT%\bin\additions\VBoxDispD3D.dll"
      FILE "$%PATH_OUT%\bin\additions\VBoxVideoWddm.inf"
      FILE "$%PATH_OUT%\bin\additions\VBoxOGLarrayspu.dll"
      FILE "$%PATH_OUT%\bin\additions\VBoxOGLcrutil.dll"
      FILE "$%PATH_OUT%\bin\additions\VBoxOGLerrorspu.dll"
      FILE "$%PATH_OUT%\bin\additions\VBoxOGLpackspu.dll"
      FILE "$%PATH_OUT%\bin\additions\VBoxOGLpassthroughspu.dll"
      FILE "$%PATH_OUT%\bin\additions\VBoxOGLfeedbackspu.dll"
      FILE "$%PATH_OUT%\bin\additions\VBoxOGL.dll"
      FILE "$%PATH_OUT%\bin\additions\libWine.dll"
      FILE "$%PATH_OUT%\bin\additions\VBoxD3D9wddm.dll"
      FILE "$%PATH_OUT%\bin\additions\wined3dwddm.dll"
      SetOutPath $g_strSystemDir
      Goto doneCr
    ${EndIf}
  !endif
  ; crOpenGL
  SetOutPath $g_strSystemDir
  FILE "$%PATH_OUT%\bin\additions\VBoxOGLarrayspu.dll"
  FILE "$%PATH_OUT%\bin\additions\VBoxOGLcrutil.dll"
  FILE "$%PATH_OUT%\bin\additions\VBoxOGLerrorspu.dll"
  FILE "$%PATH_OUT%\bin\additions\VBoxOGLpackspu.dll"
  FILE "$%PATH_OUT%\bin\additions\VBoxOGLpassthroughspu.dll"
  FILE "$%PATH_OUT%\bin\additions\VBoxOGLfeedbackspu.dll"
  FILE "$%PATH_OUT%\bin\additions\VBoxOGL.dll"

  !if $%BUILD_TARGET_ARCH% == "amd64"
    ; Only 64-bit installer: Also copy 32-bit DLLs on 64-bit target arch in
    ; Wow64 node (32-bit sub system).
    ${EnableX64FSRedirection}
    SetOutPath $SYSDIR
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\VBoxOGLarrayspu.dll"
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\VBoxOGLcrutil.dll"
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\VBoxOGLerrorspu.dll"
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\VBoxOGLpackspu.dll"
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\VBoxOGLpassthroughspu.dll"
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\VBoxOGLfeedbackspu.dll"
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\VBoxOGL.dll"
    ${DisableX64FSRedirection}
  !endif

doneCr:

!endif ; VBOX_WITH_CROGL

FunctionEnd

!ifdef WHQL_FAKE

Function W2K_WHQLFakeOn

  StrCmp $g_bFakeWHQL "true" do
  Goto exit

do:

  DetailPrint "Turning off WHQL protection..."
  nsExec::ExecToLog '"$INSTDIR\VBoxWHQLFake.exe" "ignore"'

exit:

FunctionEnd

Function W2K_WHQLFakeOff

  StrCmp $g_bFakeWHQL "true" do
  Goto exit

do:

  DetailPrint "Turning back on WHQL protection..."
  nsExec::ExecToLog '"$INSTDIR\VBoxWHQLFake.exe" "warn"'

exit:

FunctionEnd

!endif

Function W2K_InstallFiles

  ; The Shared Folder IFS goes to the system directory
  FILE /oname=$g_strSystemDir\drivers\VBoxSF.sys "$%PATH_OUT%\bin\additions\VBoxSF.sys"
  !insertmacro ReplaceDLL "$%PATH_OUT%\bin\additions\VBoxMRXNP.dll" "$g_strSystemDir\VBoxMRXNP.dll" "$INSTDIR"
  AccessControl::GrantOnFile "$g_strSystemDir\VBoxMRXNP.dll" "(BU)" "GenericRead"

  ; The VBoxTray hook DLL also goes to the system directory; it might be locked
  !insertmacro ReplaceDLL "$%PATH_OUT%\bin\additions\VBoxHook.dll" "$g_strSystemDir\VBoxHook.dll" "$INSTDIR"
  AccessControl::GrantOnFile "$g_strSystemDir\VBoxHook.dll" "(BU)" "GenericRead"

  DetailPrint "Installing Drivers..."

drv_video:

  StrCmp $g_bNoVideoDrv "true" drv_guest
  SetOutPath "$INSTDIR"
  ${If} $g_bWithWDDM == "true"
    DetailPrint "Installing WDDM video driver ..."
    nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /i "PCI\VEN_80EE&DEV_BEEF&SUBSYS_00000000&REV_00" "$INSTDIR\VBoxVideoWddm.inf" "Display"'
  ${Else}
    DetailPrint "Installing video driver ..."
    nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /i "PCI\VEN_80EE&DEV_BEEF&SUBSYS_00000000&REV_00" "$INSTDIR\VBoxVideo.inf" "Display"'
  ${EndIf}
  Pop $0                      ; Ret value
  LogText "Video driver result: $0"
  IntCmp $0 0 +1 error error  ; Check ret value (0=OK, 1=Error)

drv_guest:

  StrCmp $g_bNoGuestDrv "true" drv_mouse
  DetailPrint "Installing guest driver ..."
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /i "PCI\VEN_80EE&DEV_CAFE&SUBSYS_00000000&REV_00" "$INSTDIR\VBoxGuest.inf" "Media"'
  Pop $0                      ; Ret value
  LogText "Guest driver result: $0"
  IntCmp $0 0 +1 error error  ; Check ret value (0=OK, 1=Error)

drv_mouse:

  StrCmp $g_bNoMouseDrv "true" vbox_service
  DetailPrint "Installing mouse filter ..."
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /inf "$INSTDIR\VBoxMouse.inf"'
  Pop $0                      ; Ret value
  LogText "Mouse driver returned: $0"
  IntCmp $0 0 +1 error error  ; Check ret value (0=OK, 1=Error)

vbox_service:

  DetailPrint "Installing VirtualBox service ..."

  ; Create the VBoxService service
  ; No need to stop/remove the service here! Do this only on uninstallation!
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /createsvc "VBoxService" "VirtualBox Guest Additions Service" 16 2 "system32\VBoxService.exe" "Base"'
  Pop $0                      ; Ret value
  LogText "VBoxService returned: $0"

  ; Set service description
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\VBoxService" "Description" "Manages VM runtime information, time synchronization, remote sysprep execution and miscellaneous utilities for guest operating systems."

sf:

  DetailPrint "Installing Shared Folders service ..."

  ; Create the Shared Folders service ...
  ; No need to stop/remove the service here! Do this only on uninstallation!
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /createsvc "VBoxSF" "VirtualBox Shared Folders" 2 1 "system32\drivers\VBoxSF.sys" "NetworkProvider"'

  ; ... and the link to the network provider
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\VBoxSF\NetworkProvider" "DeviceName" "\Device\VBoxMiniRdr"
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\VBoxSF\NetworkProvider" "Name" "VirtualBox Shared Folders"
  WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\VBoxSF\NetworkProvider" "ProviderPath" "$SYSDIR\VBoxMRXNP.dll"

  ; Add default network providers (if not present or corrupted)
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /addnetprovider WebClient'
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /addnetprovider LanmanWorkstation'
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /addnetprovider RDPNP'

  ; Add the shared folders network provider
  DetailPrint "Adding network provider (Order = $g_iSfOrder) ..."
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /addnetprovider VBoxSF $g_iSfOrder'
  Pop $0                      ; Ret value
  IntCmp $0 0 +1 error error  ; Check ret value (0=OK, 1=Error)

!if $%VBOX_WITH_CROGL% == "1"
cropengl:
  ${If} $g_bWithWDDM == "true"
    ; Nothing to do here
  ${Else}
    DetailPrint "Installing 3D OpenGL support ..."
    WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers\VBoxOGL" "Version" 2
    WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers\VBoxOGL" "DriverVersion" 1
    WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers\VBoxOGL" "Flags" 1
    WriteRegStr   HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers\VBoxOGL" "Dll" "VBoxOGL.dll"
!if $%BUILD_TARGET_ARCH% == "amd64"
    ; Write additional keys required for Windows XP, Vista and 7 64-bit (but for 32-bit stuff)
    ${If} $g_strWinVersion   == '7'
    ${OrIf} $g_strWinVersion == 'Vista'
    ${OrIf} $g_strWinVersion == 'XP'
      WriteRegDWORD HKLM "SOFTWARE\Wow6432Node\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers\VBoxOGL" "Version" 2
      WriteRegDWORD HKLM "SOFTWARE\Wow6432Node\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers\VBoxOGL" "DriverVersion" 1
      WriteRegDWORD HKLM "SOFTWARE\Wow6432Node\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers\VBoxOGL" "Flags" 1
      WriteRegStr   HKLM "SOFTWARE\Wow6432Node\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers\VBoxOGL" "Dll" "VBoxOGL.dll"
    ${EndIf}
!endif
  ${Endif}
!endif

  Goto done

error:

  Abort "ERROR: Could not install files for Windows 2000 / XP / Vista! Installation aborted."

done:

FunctionEnd

Function W2K_Main

  SetOutPath "$INSTDIR"
  SetOverwrite on

  Call W2K_Prepare
  Call W2K_CopyFiles

!ifdef WHQL_FAKE
  Call W2K_WHQLFakeOn
!endif

  Call W2K_InstallFiles

!ifdef WHQL_FAKE
  Call W2K_WHQLFakeOff
!endif

  Call W2K_SetVideoResolution

FunctionEnd

!macro W2K_UninstallInstDir un
Function ${un}W2K_UninstallInstDir

  Delete /REBOOTOK "$INSTDIR\VBoxVideo.sys"
  Delete /REBOOTOK "$INSTDIR\VBoxVideo.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxVideo.cat"
  Delete /REBOOTOK "$INSTDIR\VBoxDisp.dll"

  Delete /REBOOTOK "$INSTDIR\VBoxMouse.sys"
  Delete /REBOOTOK "$INSTDIR\VBoxMouse.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxMouse.cat"

  Delete /REBOOTOK "$INSTDIR\VBoxTray.exe"

  Delete /REBOOTOK "$INSTDIR\VBoxGuest.sys"
  Delete /REBOOTOK "$INSTDIR\VBoxGuest.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxGuest.cat"

  Delete /REBOOTOK "$INSTDIR\VBCoInst.dll"
  Delete /REBOOTOK "$INSTDIR\VBoxControl.exe"
  Delete /REBOOTOK "$INSTDIR\VBoxService.exe" ; File from an older installation maybe, not present here anymore

  ; WHQL fake
!ifdef WHQL_FAKE
  Delete /REBOOTOK "$INSTDIR\VBoxWHQLFake.exe"
!endif

  ; Log file
  Delete /REBOOTOK "$INSTDIR\install.log"
  Delete /REBOOTOK "$INSTDIR\install_ui.log"

FunctionEnd
!macroend
!insertmacro W2K_UninstallInstDir ""
!insertmacro W2K_UninstallInstDir "un."

!macro W2K_Uninstall un
Function ${un}W2K_Uninstall

  Push $0
!if $%VBOX_WITH_WDDM% == "1"
  ; First check whether WDDM driver is installed
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /matchdrv "PCI\VEN_80EE&DEV_BEEF&SUBSYS_00000000&REV_00" "WDDM"'
  Pop $0    ; Ret value
  ${If} $0 == "0"
    DetailPrint "WDDM display driver is installed"
    StrCpy $g_bWithWDDM "true"
  ${ElseIf} $0 == "4"
    DetailPrint "Non-WDDM display driver is installed"
  ${Else}
    DetailPrint "Error occured"
    ; @todo Add error handling here!
  ${Endif}
!endif

  ; Remove VirtualBox graphics adapter & PCI base drivers
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /u "PCI\VEN_80EE&DEV_BEEF&SUBSYS_00000000&REV_00"'
  Pop $0    ; Ret value
  ; @todo Add error handling here!

  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /u "PCI\VEN_80EE&DEV_CAFE&SUBSYS_00000000&REV_00"'
  Pop $0    ; Ret value
  ; @todo Add error handling here!

  ; @todo restore old drivers

  ; Remove video driver
  ${If} $g_bWithWDDM == "true"
    nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /delsvc VBoxVideoWddm'
    Delete /REBOOTOK "$g_strSystemDir\drivers\VBoxVideoWddm.sys"
    Delete /REBOOTOK "$g_strSystemDir\VBoxDispD3D.dll"
  ${Else}
    nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /delsvc VBoxVideo'
    Delete /REBOOTOK "$g_strSystemDir\drivers\VBoxVideo.sys"
    Delete /REBOOTOK "$g_strSystemDir\VBoxDisp.dll"
  ${Endif}

  ; Remove mouse driver
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /delsvc VBoxMouse'
  Pop $0    ; Ret value
  Delete /REBOOTOK "$g_strSystemDir\drivers\VBoxMouse.sys"
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /reg_delmultisz "SYSTEM\CurrentControlSet\Control\Class\{4D36E96F-E325-11CE-BFC1-08002BE10318}" "UpperFilters" "VBoxMouse"'
  Pop $0    ; Ret value
  ; @todo Add error handling here!

  ; Delete the VBoxService service
  Call ${un}StopVBoxService
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /delsvc VBoxService'
  Pop $0    ; Ret value
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "VBoxService"
  Delete /REBOOTOK "$g_strSystemDir\VBoxService.exe"

  ; GINA
  Delete /REBOOTOK "$g_strSystemDir\VBoxGINA.dll"
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinLogon" "GinaDLL"
  ${If} $0 == "VBoxGINA.dll"
    DeleteRegValue HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinLogon" "GinaDLL"
  ${EndIf}

  ; Delete VBoxTray
  Call ${un}StopVBoxTray
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "VBoxTray"

  ; Remove guest driver
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /delsvc VBoxGuest'
  Pop $0    ; Ret value
  Delete /REBOOTOK "$g_strSystemDir\drivers\VBoxGuest.sys"
  Delete /REBOOTOK "$g_strSystemDir\vbcoinst.dll"
  Delete /REBOOTOK "$g_strSystemDir\VBoxTray.exe"
  Delete /REBOOTOK "$g_strSystemDir\VBoxHook.dll"
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "VBoxTray"      ; Remove VBoxTray autorun
  Delete /REBOOTOK "$g_strSystemDir\VBoxControl.exe"

  ; Remove shared folders driver
  call ${un}RemoveProvider                        ; Remove Shared Folders network provider from registry
                                                  ; @todo Add a /delnetprovider to VBoxDrvInst for doing this job!
  nsExec::ExecToLog '"$INSTDIR\VBoxDrvInst.exe" /delsvc VBoxSF'
  Delete /REBOOTOK "$g_strSystemDir\VBoxMRXNP.dll"        ; The network provider DLL will be locked
  Delete /REBOOTOK "$g_strSystemDir\drivers\VBoxSF.sys"

!if $%VBOX_WITH_CROGL% == "1"

  DetailPrint "Removing 3D graphics support ..."
  !if $%BUILD_TARGET_ARCH% == "x86"
    Delete /REBOOTOK "$g_strSystemDir\VBoxOGLarrayspu.dll"
    Delete /REBOOTOK "$g_strSystemDir\VBoxOGLcrutil.dll"
    Delete /REBOOTOK "$g_strSystemDir\VBoxOGLerrorspu.dll"
    Delete /REBOOTOK "$g_strSystemDir\VBoxOGLpackspu.dll"
    Delete /REBOOTOK "$g_strSystemDir\VBoxOGLpassthroughspu.dll"
    Delete /REBOOTOK "$g_strSystemDir\VBoxOGLfeedbackspu.dll"
    Delete /REBOOTOK "$g_strSystemDir\VBoxOGL.dll"

    ; Remove D3D stuff
    ; @todo add a feature flag to only remove if installed explicitly
    Delete /REBOOTOK "$g_strSystemDir\libWine.dll"
    Delete /REBOOTOK "$g_strSystemDir\VBoxD3D8.dll"
    Delete /REBOOTOK "$g_strSystemDir\VBoxD3D9.dll"
    Delete /REBOOTOK "$g_strSystemDir\wined3d.dll"
    ; Update DLL cache
    IfFileExists "$g_strSystemDir\dllcache\msd3d8.dll" 0 +2
      Delete /REBOOTOK "$g_strSystemDir\dllcache\d3d8.dll"
      Rename /REBOOTOK "$g_strSystemDir\dllcache\msd3d8.dll" "$g_strSystemDir\dllcache\d3d8.dll"
    IfFileExists g_strSystemDir\dllcache\msd3d9.dll" 0 +2
      Delete /REBOOTOK "$g_strSystemDir\dllcache\d3d9.dll"
      Rename /REBOOTOK "$g_strSystemDir\dllcache\msd3d9.dll" "$g_strSystemDir\dllcache\d3d9.dll"
    ; Restore original DX DLLs
    IfFileExists "$g_strSystemDir\msd3d8.dll" 0 +2
      Delete /REBOOTOK "$g_strSystemDir\d3d8.dll"
      Rename /REBOOTOK "$g_strSystemDir\msd3d8.dll" "$g_strSystemDir\d3d8.dll"
    IfFileExists "$g_strSystemDir\msd3d9.dll" 0 +2
      Delete /REBOOTOK "$g_strSystemDir\d3d9.dll"
      Rename /REBOOTOK "$g_strSystemDir\msd3d9.dll" "$g_strSystemDir\d3d9.dll"

  !else ; amd64

    ; Only 64-bit installer: Also remove 32-bit DLLs on 64-bit target arch in Wow64 node
    ${EnableX64FSRedirection}
    Delete /REBOOTOK "$SYSDIR\VBoxOGLarrayspu.dll"
    Delete /REBOOTOK "$SYSDIR\VBoxOGLcrutil.dll"
    Delete /REBOOTOK "$SYSDIR\VBoxOGLerrorspu.dll"
    Delete /REBOOTOK "$SYSDIR\VBoxOGLpackspu.dll"
    Delete /REBOOTOK "$SYSDIR\VBoxOGLpassthroughspu.dll"
    Delete /REBOOTOK "$SYSDIR\VBoxOGLfeedbackspu.dll"
    Delete /REBOOTOK "$SYSDIR\VBoxOGL.dll"

    ; Remove D3D stuff
    ; @todo add a feature flag to only remove if installed explicitly
    Delete /REBOOTOK "$SYSDIR\libWine.dll"
    Delete /REBOOTOK "$SYSDIR\VBoxD3D8.dll"
    Delete /REBOOTOK "$SYSDIR\VBoxD3D9.dll"
    Delete /REBOOTOK "$SYSDIR\wined3d.dll"
    ; Update DLL cache
    IfFileExists "$SYSDIR\dllcache\msd3d8.dll" 0 +2
      Delete /REBOOTOK "$SYSDIR\dllcache\d3d8.dll"
      Rename /REBOOTOK "$SYSDIR\dllcache\msd3d8.dll" "$SYSDIR\dllcache\d3d8.dll"
    IfFileExists "$SYSDIR\dllcache\msd3d9.dll" 0 +2
      Delete /REBOOTOK "$SYSDIR\dllcache\d3d9.dll"
      Rename /REBOOTOK "$SYSDIR\dllcache\msd3d9.dll" "$SYSDIR\dllcache\d3d9.dll"
    ; Restore original DX DLLs
    IfFileExists "$SYSDIR\msd3d8.dll" 0 +2
      Delete /REBOOTOK "$SYSDIR\d3d8.dll"
      Rename /REBOOTOK "$SYSDIR\msd3d8.dll" "$SYSDIR\d3d8.dll"
    IfFileExists "$SYSDIR\msd3d9.dll" 0 +2
      Delete /REBOOTOK "$SYSDIR\d3d9.dll"
      Rename /REBOOTOK "$SYSDIR\msd3d9.dll" "$SYSDIR\d3d9.dll"
    DeleteRegKey HKLM "SOFTWARE\Wow6432Node\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers\VBoxOGL"
    ${DisableX64FSRedirection}
  !endif ; amd64

  DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers\VBoxOGL"

!endif ; VBOX_WITH_CROGL

  Pop $0

FunctionEnd
!macroend
!insertmacro W2K_Uninstall ""
!insertmacro W2K_Uninstall "un."

