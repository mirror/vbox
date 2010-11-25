
!if $%BUILD_TYPE% == "debug"
  !define _DEBUG     ; Turn this on to get extra output
!endif

; Defines for special functions
!define WHQL_FAKE    ; Turns on the faking of non WHQL signed / approved drivers
                     ; Needs the VBoxWHQLFake.exe in the additions output directory!

!define VENDOR_ROOT_KEY             "SOFTWARE\$%VBOX_VENDOR_SHORT%"

!define PRODUCT_NAME                "$%VBOX_PRODUCT% Guest Additions"
!define PRODUCT_DESC                "$%VBOX_PRODUCT% Guest Additions"
!define PRODUCT_VERSION             "$%VBOX_VERSION_MAJOR%.$%VBOX_VERSION_MINOR%.$%VBOX_VERSION_BUILD%.0"
!define PRODUCT_PUBLISHER           "$%VBOX_VENDOR%"
!define PRODUCT_COPYRIGHT           "(C) $%VBOX_C_YEAR% $%VBOX_VENDOR%"
!define PRODUCT_OUTPUT              "VBoxWindowsAdditions-$%BUILD_TARGET_ARCH%.exe"
!define PRODUCT_WEB_SITE            "http://www.virtualbox.org"
!define PRODUCT_INSTALL_KEY         "${VENDOR_ROOT_KEY}\VirtualBox Guest Additions"
!define PRODUCT_UNINST_KEY          "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY     "HKLM"

; Needed for InstallLib macro: Install libraries in every case
!define LIBRARY_IGNORE_VERSION

VIProductVersion "${PRODUCT_VERSION}"
VIAddVersionKey "FileVersion"       "$%VBOX_VERSION_STRING%"
VIAddVersionKey "ProductName"       "${PRODUCT_NAME}"
VIAddVersionKey "ProductVersion"    "${PRODUCT_VERSION}"
VIAddVersionKey "CompanyName"       "${PRODUCT_PUBLISHER}"
VIAddVersionKey "FileDescription"   "${PRODUCT_DESC}"
VIAddVersionKey "LegalCopyright"    "${PRODUCT_COPYRIGHT}"
VIAddVersionKey "InternalName"      "${PRODUCT_OUTPUT}"

; This registry key will hold the mouse driver path before install (NT4 only)
!define ORG_MOUSE_PATH "MousePath"

; If we have our guest install helper DLL, add the
; plugin path so that NSIS can find it when compiling the installer
; Note: NSIS plugins *always* have to be compiled in 32-bit!
!if $%VBOX_WITH_GUEST_INSTALL_HELPER% == "1"
  !addplugindir "$%PATH_TARGET_X86%\VBoxGuestInstallHelper"
!endif

!include "LogicLib.nsh"
!include "FileFunc.nsh"
  !insertmacro GetParameters
  !insertmacro GetOptions
!include "WordFunc.nsh"
  !insertmacro WordFind
  !insertmacro StrFilter

!include "nsProcess.nsh"
!include "Library.nsh"
!include "strstr.nsh"         ; Function "strstr"
!include "servicepack.nsh"    ; Function "GetServicePack"
!include "winver.nsh"         ; Function for determining Windows version
!define REPLACEDLL_NOREGISTER ; Replace in use DLL function
!include "ReplaceDLL.nsh"
!include "dumplog.nsh"        ; Dump log to file function

!if $%BUILD_TARGET_ARCH% == "amd64"
  !include "x64.nsh"
!endif

; Set Modern UI (MUI) as default
!define USE_MUI

!ifdef USE_MUI
  ; Use modern UI, version 2
  !include "MUI2.nsh"

  ; MUI Settings
  !define MUI_WELCOMEFINISHPAGE_BITMAP "$%VBOX_BRAND_WIN_ADD_INST_DLGBMP%"
  !define MUI_ABORTWARNING
  !define MUI_WELCOMEPAGE_TITLE_3LINES "Welcome to the ${PRODUCT_NAME} Additions Setup"

  ; API defines
  !define SM_CLEANBOOT 67

  ; Icons
  !if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
    !define MUI_ICON "$%VBOX_NSIS_ICON_FILE%"
    !define MUI_UNICON "$%VBOX_NSIS_ICON_FILE%"
  !else   ; 64-bit
    !define MUI_ICON "$%VBOX_WINDOWS_ADDITIONS_ICON_FILE%"
    !define MUI_UNICON "$%VBOX_WINDOWS_ADDITIONS_ICON_FILE%"
  !endif

  ; Welcome page
  !insertmacro MUI_PAGE_WELCOME
  ; License page
  !insertmacro MUI_PAGE_LICENSE "$(VBOX_LICENSE)"
  !define MUI_LICENSEPAGE_RADIOBUTTONS
  ; Directory page
  !insertmacro MUI_PAGE_DIRECTORY
  ; Components Page
  !insertmacro MUI_PAGE_COMPONENTS
  ; Instfiles page
  !insertmacro MUI_PAGE_INSTFILES

  !ifndef _DEBUG
    !define MUI_FINISHPAGE_TITLE_3LINES   ; Have a bit more vertical space for text
    !insertmacro MUI_PAGE_FINISH          ; Only show in release mode - useful information for debugging!
  !endif

  ; Uninstaller pages
  !insertmacro MUI_UNPAGE_INSTFILES

  ; Define languages we will use
  !insertmacro MUI_LANGUAGE "English"
  !insertmacro MUI_LANGUAGE "French"
  !insertmacro MUI_LANGUAGE "German"

  ; Set branding text which appears on the horizontal line at the bottom
!ifdef _DEBUG
  BrandingText "VirtualBox Windows Additions $%VBOX_VERSION_STRING% ($%VBOX_SVN_REV%) - Debug Build"
!else
  BrandingText "VirtualBox Windows Additions $%VBOX_VERSION_STRING%"
!endif

  ; Set license language
  LicenseLangString VBOX_LICENSE ${LANG_ENGLISH} "$%VBOX_BRAND_LICENSE_RTF%"

  ; If license files not available (OSE / PUEL) build, then use the English one as default
  !ifdef VBOX_BRAND_fr_FR_LICENSE_RTF
    LicenseLangString VBOX_LICENSE ${LANG_FRENCH} "$%VBOX_BRAND_fr_FR_LICENSE_RTF%"
  !else
    LicenseLangString VBOX_LICENSE ${LANG_FRENCH} "$%VBOX_BRAND_LICENSE_RTF%"
  !endif
  !ifdef VBOX_BRAND_de_DE_LICENSE_RTF
    LicenseLangString VBOX_LICENSE ${LANG_GERMAN} "$%VBOX_BRAND_de_DE_LICENSE_RTF%"
  !else
    LicenseLangString VBOX_LICENSE ${LANG_GERMAN} "$%VBOX_BRAND_LICENSE_RTF%"
  !endif

  !insertmacro MUI_RESERVEFILE_LANGDLL
!else ; !USE_MUI
    XPStyle on
    Page license
    Page components
    Page directory
    Page instfiles
!endif

; Language files
!include "Languages\English.nsh"
!include "Languages\French.nsh"
!include "Languages\German.nsh"

; Variables and output files
Name "${PRODUCT_NAME} $%VBOX_VERSION_STRING%"
!ifdef UNINSTALLER_ONLY
  !echo "Uninstaller only!"
  OutFile "$%PATH_TARGET%\VBoxWindowsAdditions-$%BUILD_TARGET_ARCH%-uninst.exe"
!else
  OutFile "VBoxWindowsAdditions-$%BUILD_TARGET_ARCH%.exe"
!endif ; UNINSTALLER_ONLY

; Define default installation directory
!if $%BUILD_TARGET_ARCH% == "x86" ; 32-bit
  InstallDir  "$PROGRAMFILES32\$%VBOX_VENDOR_SHORT%\VirtualBox Guest Additions"
!else       ; 64-bit
  InstallDir  "$PROGRAMFILES64\$%VBOX_VENDOR_SHORT%\VirtualBox Guest Additions"
!endif

InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show
RequestExecutionLevel highest

; Internal parameters
Var g_iSystemMode                       ; Current system mode (0 = Normal boot, 1 = Fail-safe boot, 2 = Fail-safe with network boot)
Var g_strSystemDir                      ; Windows system directory
Var g_strCurUser                        ; Current user using the system
Var g_strAddVerMaj                      ; Installed Guest Additions: Major version
Var g_strAddVerMin                      ; Installed Guest Additions: Minor version
Var g_strAddVerBuild                    ; Installed Guest Additions: Build number
Var g_strAddVerRev                      ; Installed Guest Additions: SVN revision
Var g_strWinVersion                     ; Current Windows version we're running on
Var g_bLogEnable                        ; Do logging when installing? "true" or "false"
Var g_bWithWDDM                         ; Install the WDDM driver instead of the normal one
Var g_bCapWDDM                          ; Capability: Is the guest able to handle/use our WDDM driver?

; Command line parameters - these can be set/modified
; on the command line
Var g_bFakeWHQL                         ; Cmd line: Fake Windows to install non WHQL certificated drivers (only for W2K and XP currently!!) ("/unsig_drv")
Var g_bForceInstall                     ; Cmd line: Force installation on unknown Windows OS version
Var g_bUninstall                        ; Cmd line: Just uninstall any previous Guest Additions and exit
Var g_bRebootOnExit                     ; Cmd line: Auto-Reboot on successful installation. Good for unattended installations ("/reboot")
Var g_iScreenBpp                        ; Cmd line: Screen depth ("/depth=X")
Var g_iScreenX                          ; Cmd line: Screen resolution X ("/resx=X")
Var g_iScreenY                          ; Cmd line: Screen resolution Y ("/resy=Y")
Var g_iSfOrder                          ; Cmd line: Order of Shared Folders network provider (0=first, 1=second, ...)
Var g_bIgnoreUnknownOpts                ; Cmd line: Ignore unknown options (don't display the help)
Var g_bNoVBoxServiceExit                ; Cmd line: Do not quit VBoxService before updating - install on next reboot
Var g_bNoVBoxTrayExit                   ; Cmd line: Do not quit VBoxTray before updating - install on next reboot
Var g_bNoVideoDrv                       ; Cmd line: Do not install the VBoxVideo driver
Var g_bNoGuestDrv                       ; Cmd line: Do not install the VBoxGuest driver
Var g_bNoMouseDrv                       ; Cmd line: Do not install the VBoxMouse driver
Var g_bWithAutoLogon                    ; Cmd line: Install VBoxGINA / VBoxCredProv for auto logon support
Var g_bWithD3D                          ; Cmd line: Install Direct3D support
Var g_bOnlyExtract                      ; Cmd line: Only extract all files, do *not* install them. Only valid with param "/D" (target directory)
Var g_bPostInstallStatus                ; Cmd line: Post the overall installation status to some external program (VBoxTray)

; Platform parts of this installer
!include "VBoxGuestAdditionsCommon.nsh"
!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit only
!include "VBoxGuestAdditionsNT4.nsh"
!endif
!include "VBoxGuestAdditionsW2KXP.nsh"
!include "VBoxGuestAdditionsVista.nsh"
!include "VBoxGuestAdditionsUninstall.nsh"    ; Product uninstallation
!include "VBoxGuestAdditionsUninstallOld.nsh" ; Uninstallation of deprecated versions which must be removed first

Function HandleCommandLine

  Push $0                                     ; Command line (without process name)
  Push $1                                     ; Number of parameters
  Push $2                                     ; Current parameter index
  Push $3                                     ; Current parameter pair (name=value)
  Push $4                                     ; Current parameter name
  Push $5                                     ; Current parameter value (if present)

  StrCpy $1 "0"                               ; Init param counter
  StrCpy $2 "1"                               ; Init current param counter

  ${GetParameters} $0                         ; Extract command line
  ${If} $0 == ""                              ; If no parameters at all exit
    Goto exit
  ${EndIf}

  ; Enable for debugging
  ;MessageBox MB_OK "CmdLine: $0"

  ${WordFind} $0 " " "#" $1                   ; Get number of parameters in cmd line
  ${If} $0 == $1                              ; If result matches the input then
    StrCpy $1 "1"                             ; no delimiter was found. Correct to 1 word total
  ${EndIf}

  ${While} $2 <= $1                           ; Loop through all params

    ${WordFind} $0 " " "+$2" $3               ; Get current name=value pair
    ${WordFind} $3 "=" "+1" $4                ; Get current param name
    ${WordFind} $3 "=" "+2" $5                ; Get current param value

    ${StrFilter} $4 "-" "" "" $4              ; Transfer param name to lowercase

    ; Enable for debugging
    ;MessageBox MB_OK "#$2 of #$1, param='$3', name=$4, val=$5"

    ${Switch} $4

      ${Case} '/d' ; NSIS: /D=<instdir> switch, skip
        ${Break}

      ${Case} '/depth'
      ${Case} 'depth'
        StrCpy $g_iScreenBpp $5
        ${Break}

      ${Case} '/extract'
        StrCpy $g_bOnlyExtract "true"
        ${Break}

      ${Case} '/force'
        StrCpy $g_bForceInstall "true"
        ${Break}

      ${Case} '/help'
      ${Case} '/H'
      ${Case} '/h'
      ${Case} '/?'
        Goto usage
        ${Break}

      ${Case} '/ignore_unknownopts' ; Not officially documented
        StrCpy $g_bIgnoreUnknownOpts "true"
        ${Break}

      ${Case} '/l'
      ${Case} '/log'
      ${Case} '/logging'
        StrCpy $g_bLogEnable "true"
        ${Break}

      ${Case} '/ncrc' ; NSIS: /NCRC switch, skip
        ${Break}

      ${Case} '/no_vboxservice_exit' ; Not officially documented
        StrCpy $g_bNoVBoxServiceExit "true"
        ${Break}

      ${Case} '/no_vboxtray_exit' ; Not officially documented
        StrCpy $g_bNoVBoxTrayExit "true"
        ${Break}

      ${Case} '/no_videodrv' ; Not officially documented
        StrCpy $g_bNoVideoDrv "true"
        ${Break}

      ${Case} '/no_guestdrv' ; Not officially documented
        StrCpy $g_bNoGuestDrv "true"
        ${Break}

      ${Case} '/no_mousedrv' ; Not officially documented
        StrCpy $g_bNoMouseDrv "true"
        ${Break}

!if $%VBOX_WITH_GUEST_INSTALL_HELPER% == "1"
      ; This switch tells our installer that it
      ; - should not quit VBoxTray during the update, because ...
      ; - ... it should show the overall installation status
      ;   using VBoxTray's balloon message feature (since VBox 4.0)
      ${Case} '/post_installstatus' ; Not officially documented
        StrCpy $g_bNoVBoxTrayExit "true"
        StrCpy $g_bPostInstallStatus "true"
        ${Break}
!endif

      ${Case} '/reboot'
        StrCpy $g_bRebootOnExit "true"
        ${Break}

      ${Case} '/s' ; NSIS: /S switch, skip
        ${Break}

      ${Case} '/sforder'
      ${Case} 'sforder'
        StrCpy $g_iSfOrder $5
        ${Break}

    !ifdef WHQL_FAKE
      ${Case} '/unsig_drv'
        StrCpy $g_bFakeWHQL "true"
      ${Break}
    !endif

      ${Case} '/uninstall'
        StrCpy $g_bUninstall "true"
        ${Break}

      ${Case} '/with_autologon'
        StrCpy $g_bWithAutoLogon "true"
        ${Break}

    !if $%VBOX_WITH_CROGL% == "1"
      ${Case} '/with_d3d'
      ${Case} '/with_direct3d'
        StrCpy $g_bWithD3D "true"
        ${Break}
    !endif

      ${Case} '/xres'
      ${Case} 'xres'
        StrCpy $g_iScreenX $5
        ${Break}

      ${Case} '/yres'
      ${Case} 'yres'
        StrCpy $g_iScreenY $5
        ${Break}

      ${Default} ; Unknown parameter, print usage message
        ; Prevent popping up usage message on (yet) unknown parameters
        ; in silent mode, just skip
        IfSilent 0 +2
          ${Break}
        goto usage
        ${Break}

    ${EndSwitch}

next_param:

    IntOp $2 $2 + 1

  ${EndWhile}
  Goto exit

usage:

  ; If we were told to ignore unknown (invalid) options, just return to
  ; the parsing loop ...
  ${If} $g_bIgnoreUnknownOpts == "true"
    Goto next_param
  ${EndIf}
  MessageBox MB_OK "${PRODUCT_NAME} Installer$\r$\n$\r$\n \
                    Usage: VBoxWindowsAdditions-$%BUILD_TARGET_ARCH% [OPTIONS] [/l] [/S] [/D=<PATH>]$\r$\n$\r$\n \
                    Options:$\r$\n \
                    /depth=BPP$\tSets the guest's display color depth (bits per pixel)$\r$\n \
                    /extract$\t$\tOnly extract installation files$\r$\n \
                    /force$\t$\tForce installation on unknown/undetected Windows versions$\r$\n \
                    /uninstall$\t$\tJust uninstalls the Guest Additions and exits$\r$\n \
                    /with_autologon$\tInstalls auto-logon support$\r$\n \
                    /with_d3d$\tInstalls D3D support$\r$\n \
                    /xres=X$\t$\tSets the guest's display resolution (width in pixels)$\r$\n \
                    /yres=Y$\t$\tSets the guest's display resolution (height in pixels)$\r$\n \
                    $\r$\n \
                    Installer parameters:$\r$\n \
                    /l$\t$\tEnables logging$\r$\n \
                    /S$\t$\tSilent install$\r$\n \
                    /D=<PATH>$\tSets the default install path$\r$\n \
                    $\r$\n \
                    Note: Order of options and installer parameters are mandatory." /SD IDOK

  ; No stack restore needed, we're about to quit
  Quit

done:

  IfSilent 0 +2
    LogText "Installer is in silent mode!"

  LogText "Property: XRes: $g_iScreenX"
  LogText "Property: YRes: $g_iScreenY"
  LogText "Property: BPP: $g_iScreenBpp"
  LogText "Property: Logging enabled: $g_bLogEnable"

exit:

  Pop $5
  Pop $4
  Pop $3
  Pop $2
  Pop $1
  Pop $0

FunctionEnd

Function CheckForOldGuestAdditions

  Push $0
  Push $1
  Push $2

begin:

sun_check:

  ; Check for old "Sun VirtualBox Guest Additions"
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Sun VirtualBox Guest Additions" "UninstallString"
  StrCmp $0 "" sun_xvm_check     ; If string is empty, Sun additions are probably not installed (anymore)

  MessageBox MB_YESNO $(VBOX_SUN_FOUND) /SD IDYES IDYES sun_uninstall
    Pop $2
    Pop $1
    Pop $0
    MessageBox MB_ICONSTOP $(VBOX_SUN_ABORTED) /SD IDOK
    Quit

sun_uninstall:

  Call Uninstall_Sun
  Goto success

sun_xvm_check:

  ; Check for old "innotek" Guest Additions" before rebranding to "Sun"
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Sun xVM VirtualBox Guest Additions" "UninstallString"
  StrCmp $0 "" innotek_check     ; If string is empty, Sun xVM additions are probably not installed (anymore)

  MessageBox MB_YESNO $(VBOX_SUN_FOUND) /SD IDYES IDYES sun_xvm_uninstall
    Pop $2
    Pop $1
    Pop $0
    MessageBox MB_ICONSTOP $(VBOX_SUN_ABORTED) /SD IDOK
    Quit

sun_xvm_uninstall:

  Call Uninstall_SunXVM
  Goto success

innotek_check:

  ; Check for old "innotek" Guest Additions" before rebranding to "Sun"
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\innotek VirtualBox Guest Additions" "UninstallString"
  StrCmp $0 "" exit     ; If string is empty, Guest Additions are probably not installed (anymore)

  MessageBox MB_YESNO $(VBOX_INNOTEK_FOUND) /SD IDYES IDYES innotek_uninstall
    Pop $2
    Pop $1
    Pop $0
    MessageBox MB_ICONSTOP $(VBOX_INNOTEK_ABORTED) /SD IDOK
    Quit

innotek_uninstall:

  Call Uninstall_Innotek
  Goto success

success:

  ; Nothing to do here yet

exit:

  Pop $2
  Pop $1
  Pop $0

FunctionEnd

Function CheckArchitecture

  System::Call "kernel32::GetCurrentProcess() i .s"
  System::Call "kernel32::IsWow64Process(i s, *i .r0)"
  DetailPrint "Running on 64bit: $0"

!if $%BUILD_TARGET_ARCH% == "amd64"   ; 64-bit
  IntCmp $0 0 not_32bit_platform
!else   ; 32-bit
  IntCmp $0 1 not_64bit_platform
!endif

  Goto exit

not_32bit_platform:

  MessageBox MB_ICONSTOP $(VBOX_NOTICE_ARCH_AMD64) /SD IDOK
  Quit

not_64bit_platform:

  MessageBox MB_ICONSTOP $(VBOX_NOTICE_ARCH_X86) /SD IDOK
  Quit

exit:

FunctionEnd

Function PrepareForUpdate

  StrCmp $g_strAddVerMaj "1" v1     ; Handle major version "v1.x"
  StrCmp $g_strAddVerMaj "2" v2     ; Handle major version "v2.x"
  StrCmp $g_strAddVerMaj "3" v3     ; Handle major version "v3.x"
  Goto exit

v3:

  Goto exit

v2:

  Goto exit

v1:

  StrCmp $g_strAddVerMin "5" v1_5   ; Handle minor version "v1.5.x"
  StrCmp $g_strAddVerMin "6" v1_6   ; Handle minor version "v1.6.x"

v1_5:

  Goto exit

v1_6:

  Goto exit

exit:

FunctionEnd

Function Common_CopyFiles

  SetOutPath "$INSTDIR"
  SetOverwrite on

  FILE "$%PATH_OUT%\bin\additions\VBoxDrvInst.exe"

  FILE "$%PATH_OUT%\bin\additions\VBoxVideo.inf"
!ifdef VBOX_SIGN_ADDITIONS
  FILE "$%PATH_OUT%\bin\additions\VBoxVideo.cat"
!endif

  FILE "iexplore.ico"

FunctionEnd

; Main Files
Section $(VBOX_COMPONENT_MAIN) SEC01

  SectionIn RO ; Section cannot be unselected (read-only)

  SetOutPath "$INSTDIR"
  SetOverwrite on

  ; Because this NSIS installer is always built in 32-bit mode, we have to
  ; do some tricks for the Windows paths
!if $%BUILD_TARGET_ARCH% == "amd64"
  ; Because the next two lines will crash at the license page (??) we have to re-enable that here again
  ${DisableX64FSRedirection}
  SetRegView 64
!endif

  StrCpy $g_strSystemDir "$SYSDIR"

  Call EnableLog
  Call PrepareForUpdate

  DetailPrint "Version: $%VBOX_VERSION_STRING% (Rev $%VBOX_SVN_REV%)"
  ${If} $g_strAddVerMaj != ""
    DetailPrint "Previous version: $g_strAddVerMaj.$g_strAddVerMin.$g_strAddVerBuild (Rev $g_strAddVerRev)"
  ${Else}
    DetailPrint "No previous version of ${PRODUCT_NAME} detected."
  ${EndIf}
  DetailPrint "Detected OS: Windows $g_strWinVersion"
  DetailPrint "System Directory: $g_strSystemDir"

!ifdef _DEBUG
  DetailPrint "Debug!"
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
nt4:      ; Windows NT4

  Call GetServicePack
  Pop $R0   ; Major version
  Pop $R1   ; Minor version

  ; At least Service Pack 6 installed?
  StrCmp $R0 "6" +3
    MessageBox MB_YESNO $(VBOX_NT4_NO_SP6) /SD IDYES IDYES +2
    Quit

  ; Copy some common files ...
  Call Common_CopyFiles

  Call NT_Main
  goto success
!endif

vista: ; Windows Vista / Windows 7

  ; Copy some common files ...
  Call Common_CopyFiles

  Call W2K_Main     ; First install stuff from Windows 2000 / XP
  Call Vista_Main   ; ... and some specific stuff for Vista / Windows 7
  goto success

w2k: ; Windows 2000 and XP ...

  ; Copy some common files ...
  Call Common_CopyFiles

  Call W2K_Main
  goto success

notsupported:

  MessageBox MB_ICONSTOP $(VBOX_PLATFORM_UNSUPPORTED) /SD IDOK
  goto exit

success:

  ; Write a registry key with version and installation path for later lookup
  WriteRegStr HKLM "${PRODUCT_INSTALL_KEY}" "Version" "$%VBOX_VERSION_STRING%"
  WriteRegStr HKLM "${PRODUCT_INSTALL_KEY}" "Revision" "$%VBOX_SVN_REV%"
  WriteRegStr HKLM "${PRODUCT_INSTALL_KEY}" "InstallDir" "$INSTDIR"

!ifndef _DEBUG
  SetRebootFlag true  ; This will show a reboot page at end of installation
!endif

exit:

  Call WriteLogUI

SectionEnd

; Auto-logon support (section is hidden at the moment -- only can be enabled via command line switch)
Section /o -$(VBOX_COMPONENT_AUTOLOGON) SEC02

  ; Because this NSIS installer is always built in 32-bit mode, we have to
  ; do some tricks for the Windows paths
!if $%BUILD_TARGET_ARCH% == "amd64"
  ; Because the next two lines will crash at the license page (??) we have to re-enable that here again
  ${DisableX64FSRedirection}
  SetRegView 64
!endif

  Call GetWindowsVersion
  Pop $R0 ; Windows Version

  DetailPrint "Installing auto-logon support ..."

  ; Another GINA already is installed? Check if this is ours, otherwise let the user decide (unless it's a silent setup)
  ; whether to replace it with the VirtualBox one or not
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinLogon" "GinaDLL"
  ${If} $0 != ""
    ${If} $0 != "VBoxGINA.dll"
      MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON1 $(VBOX_COMPONENT_AUTOLOGON_WARN_3RDPARTY) /SD IDYES IDYES install
      goto exit
    ${EndIf}
  ${EndIf}

install:

  ; Do we need VBoxCredProv or VBoxGINA?
  ${If}   $R0 == 'Vista'
  ${OrIf} $R0 == '7'
    !insertmacro ReplaceDLL "$%PATH_OUT%\bin\additions\VBoxCredProv.dll" "$g_strSystemDir\VBoxCredProv.dll" "$INSTDIR"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}" "" "VBoxCredProv" ; adding to (default) key
    WriteRegStr HKCR "CLSID\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}" "" "VBoxCredProv"                       ; adding to (Default) key
    WriteRegStr HKCR "CLSID\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}\InprocServer32" "" "VBoxCredProv.dll"    ; adding to (Default) key
    WriteRegStr HKCR "CLSID\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}\InprocServer32" "ThreadingModel" "Apartment"
  ${Else}
    !insertmacro ReplaceDLL "$%PATH_OUT%\bin\additions\VBoxGINA.dll" "$g_strSystemDir\VBoxGINA.dll" "$INSTDIR"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinLogon" "GinaDLL" "VBoxGINA.dll"
  ${EndIf}

exit:

SectionEnd

; Prepares the access rights for replacing a WRP protected file
Function PrepareWRPFile

  Pop $0
  IfFileExists "$g_strSystemDir\takeown.exe" 0 +2
    nsExec::ExecToLog '"$g_strSystemDir\takeown.exe" /F "$0"'
  AccessControl::SetFileOwner "$0" "(S-1-5-32-545)"
  Pop $1
  DetailPrint "Setting file owner for '$0': $1"
  AccessControl::GrantOnFile "$0" "(S-1-5-32-545)" "FullAccess"
  Pop $1
  DetailPrint "Setting access rights for '$0': $1"

FunctionEnd

; Direct3D support
!if $%VBOX_WITH_CROGL% == "1"
Section /o $(VBOX_COMPONENT_D3D) SEC03

!if $%VBOX_WITH_WDDM% == "1"
  ${If} $g_bWithWDDM == "true"
    ; All D3D components are installed with WDDM driver package, nothing to be done here
    Return
  ${EndIf}
!endif

  SetOverwrite on

  ${If} $g_strSystemDir == ''
    StrCpy $g_strSystemDir "$SYSDIR"
  ${EndIf}

  ; crOpenGL: Do *not* install 64-bit files - they don't work yet (use !define LIBRARY_X64 later)
  ;           Only 32-bit apps on 64-bit work (see next block)
  !if $%BUILD_TARGET_ARCH% == "x86"
    SetOutPath $g_strSystemDir
    DetailPrint "Installing Direct3D support ..."
    FILE "$%PATH_OUT%\bin\additions\libWine.dll"
    FILE "$%PATH_OUT%\bin\additions\VBoxD3D8.dll"
    FILE "$%PATH_OUT%\bin\additions\VBoxD3D9.dll"
    FILE "$%PATH_OUT%\bin\additions\wined3d.dll"

    ; Update DLL cache
    SetOutPath "$g_strSystemDir\dllcache"
    IfFileExists "$g_strSystemDir\dllcache\msd3d8.dll" +1
      CopyFiles /SILENT "$g_strSystemDir\dllcache\d3d8.dll" "$g_strSystemDir\dllcache\msd3d8.dll"
    IfFileExists "$g_strSystemDir\dllcache\msd3d9.dll" +1
      CopyFiles /SILENT "$g_strSystemDir\dllcache\d3d9.dll" "$g_strSystemDir\dllcache\msd3d9.dll"

    Push "$g_strSystemDir\dllcache\d3d8.dll"
    Call PrepareWRPFile

    Push "$g_strSystemDir\dllcache\d3d9.dll"
    Call PrepareWRPFile

    ; Exchange DLLs
    !insertmacro InstallLib DLL NOTSHARED NOREBOOT_NOTPROTECTED "$%PATH_OUT%\bin\additions\d3d8.dll" "$g_strSystemDir\dllcache\d3d8.dll" "$TEMP"
    !insertmacro InstallLib DLL NOTSHARED NOREBOOT_NOTPROTECTED "$%PATH_OUT%\bin\additions\d3d9.dll" "$g_strSystemDir\dllcache\d3d9.dll" "$TEMP"

    ; If exchange above failed, do it on reboot
    !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED "$%PATH_OUT%\bin\additions\d3d8.dll" "$g_strSystemDir\dllcache\d3d8.dll" "$TEMP"
    !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED "$%PATH_OUT%\bin\additions\d3d9.dll" "$g_strSystemDir\dllcache\d3d9.dll" "$TEMP"

    ; Save original DLLs ...
    SetOutPath $g_strSystemDir
    IfFileExists "$g_strSystemDir\msd3d8.dll" +1
      CopyFiles /SILENT "$g_strSystemDir\d3d8.dll" "$g_strSystemDir\msd3d8.dll"
    IfFileExists "$g_strSystemDir\msd3d8.dll" +1
      CopyFiles /SILENT "$g_strSystemDir\d3d9.dll" "$g_strSystemDir\msd3d9.dll"

    Push "$g_strSystemDir\d3d8.dll"
    Call PrepareWRPFile

    Push "$g_strSystemDir\d3d9.dll"
    Call PrepareWRPFile

    ; Exchange DLLs
    !insertmacro InstallLib DLL NOTSHARED NOREBOOT_NOTPROTECTED "$%PATH_OUT%\bin\additions\d3d8.dll" "$g_strSystemDir\d3d8.dll" "$TEMP"
    !insertmacro InstallLib DLL NOTSHARED NOREBOOT_NOTPROTECTED "$%PATH_OUT%\bin\additions\d3d9.dll" "$g_strSystemDir\d3d9.dll" "$TEMP"

    ; If exchange above failed, do it on reboot
    !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED "$%PATH_OUT%\bin\additions\d3d8.dll" "$g_strSystemDir\d3d8.dll" "$TEMP"
    !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED "$%PATH_OUT%\bin\additions\d3d9.dll" "$g_strSystemDir\d3d9.dll" "$TEMP"
  !endif

  !if $%BUILD_TARGET_ARCH% == "amd64"
    ; Only 64-bit installer: Also copy 32-bit DLLs on 64-bit target arch in
    ; Wow64 node (32-bit sub system)
    ${EnableX64FSRedirection}
    SetOutPath $SYSDIR
    DetailPrint "Installing Direct3D support (Wow64) ..."
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\libWine.dll"
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\VBoxD3D8.dll"
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\VBoxD3D9.dll"
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\wined3d.dll"

    ; Update DLL cache
    SetOutPath "$SYSDIR\dllcache"
    IfFileExists "$SYSDIR\dllcache\msd3d8.dll" +1
      CopyFiles /SILENT "$SYSDIR\dllcache\d3d8.dll" "$SYSDIR\dllcache\msd3d8.dll"
    IfFileExists "$SYSDIR\dllcache\msd3d9.dll" +1
      CopyFiles /SILENT "$SYSDIR\dllcache\d3d9.dll" "$SYSDIR\dllcache\msd3d9.dll"

    Push "$SYSDIR\dllcache\d3d8.dll"
    Call PrepareWRPFile

    Push "$SYSDIR\dllcache\d3d9.dll"
    Call PrepareWRPFile

    ; Exchange DLLs
    !insertmacro InstallLib DLL NOTSHARED NOREBOOT_NOTPROTECTED "$%VBOX_PATH_ADDITIONS_WIN_X86%\d3d8.dll" "$SYSDIR\dllcache\d3d8.dll" "$TEMP"
    !insertmacro InstallLib DLL NOTSHARED NOREBOOT_NOTPROTECTED "$%VBOX_PATH_ADDITIONS_WIN_X86%\d3d9.dll" "$SYSDIR\dllcache\d3d9.dll" "$TEMP"

    ; If exchange above failed, do it on reboot
    !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED "$%VBOX_PATH_ADDITIONS_WIN_X86%\d3d8.dll" "$SYSDIR\dllcache\d3d8.dll" "$TEMP"
    !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED "$%VBOX_PATH_ADDITIONS_WIN_X86%\d3d9.dll" "$SYSDIR\dllcache\d3d9.dll" "$TEMP"

    ; Save original DLLs ...
    SetOutPath $SYSDIR
    IfFileExists "$SYSDIR\dllcache\msd3d8.dll" +1
      CopyFiles /SILENT "$SYSDIR\d3d8.dll" "$SYSDIR\msd3d8.dll"
    IfFileExists "$SYSDIR\dllcache\msd3d9.dll" +1
      CopyFiles /SILENT "$SYSDIR\d3d9.dll" "$SYSDIR\msd3d9.dll"

    Push "$SYSDIR\d3d8.dll"
    Call PrepareWRPFile

    Push "$SYSDIR\d3d9.dll"
    Call PrepareWRPFile

    ; Exchange DLLs
    !insertmacro InstallLib DLL NOTSHARED NOREBOOT_NOTPROTECTED "$%VBOX_PATH_ADDITIONS_WIN_X86%\d3d8.dll" "$SYSDIR\d3d8.dll" "$TEMP"
    !insertmacro InstallLib DLL NOTSHARED NOREBOOT_NOTPROTECTED "$%VBOX_PATH_ADDITIONS_WIN_X86%\d3d9.dll" "$SYSDIR\d3d9.dll" "$TEMP"

    ; If exchange above failed, do it on reboot
    !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED "$%VBOX_PATH_ADDITIONS_WIN_X86%\d3d8.dll" "$SYSDIR\d3d8.dll" "$TEMP"
    !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED "$%VBOX_PATH_ADDITIONS_WIN_X86%\d3d9.dll" "$SYSDIR\d3d9.dll" "$TEMP"

    ${DisableX64FSRedirection}
  !endif ; amd64
  Goto done

error:
  ; @todo
  Goto exit

done:
  MessageBox MB_ICONINFORMATION|MB_OK $(VBOX_WFP_WARN_REPLACE) /SD IDOK
  Goto exit

exit:

SectionEnd
!endif ; VBOX_WITH_CROGL

!ifdef USE_MUI
  ;Assign language strings to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC01} $(VBOX_COMPONENT_MAIN_DESC)
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC02} $(VBOX_COMPONENT_AUTOLOGON_DESC)
  !if $%VBOX_WITH_CROGL% == "1"
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC03} $(VBOX_COMPONENT_D3D_DESC)
  !endif
  !insertmacro MUI_FUNCTION_DESCRIPTION_END
!endif ; USE_MUI

Section -Content

  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"

SectionEnd

Section -StartMenu

  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url" "" "$INSTDIR\iexplore.ico"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" "$INSTDIR\uninst.exe"

SectionEnd

; This section is called after all the files are in place
Section -Post

!ifdef _DEBUG
  DetailPrint "Doing post install ..."
!endif

!ifdef EXTERNAL_UNINSTALLER
  SetOutPath "$INSTDIR"
  FILE "$%PATH_TARGET%\uninst.exe"
!else
  WriteUninstaller "$INSTDIR\uninst.exe"
!endif

  ; Write uninstaller in "Add / Remove programs"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"

  ; Tune TcpWindowSize for a better network throughput
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\Tcpip\Parameters" "TcpWindowSize" 64240

  DetailPrint "Installation completed."

SectionEnd

; !!! NOTE: This function *has* to be right under the last section; otherwise it does
;           *not* get called! Don't ask me why ... !!!
Function .onSelChange

  Push $0

  ; Handle selection of D3D component
  SectionGetFlags ${SEC03} $0
  ${If} $0 == ${SF_SELECTED}

!if $%VBOX_WITH_WDDM% == "1"
    ; If we're able to use the WDDM driver just use it instead of the replaced
    ; D3D components below
    ${If} $g_bCapWDDM == "true"
      StrCpy $g_bWithWDDM "true"
      Goto exit
    ${EndIf}
!endif

    ${If} $g_bForceInstall != "true"
      ; Do not install on < XP
      ${If}   $g_strWinVersion == "NT4"
      ${OrIf} $g_strWinVersion == "2000"
      ${OrIf} $g_strWinVersion == ""
        MessageBox MB_ICONINFORMATION|MB_OK $(VBOX_COMPONENT_D3D_NOT_SUPPORTED) /SD IDOK
        Goto d3d_disable
      ${EndIf}
    ${EndIf}

    ; If we're not in safe mode, print a warning and don't install D3D support
    ${If} $g_iSystemMode == '0'
      MessageBox MB_ICONINFORMATION|MB_OK $(VBOX_COMPONENT_D3D_NO_SM) /SD IDOK
      Goto d3d_disable
    ${EndIf}
  ${Else} ; D3D unselected again
    StrCpy $g_bWithWDDM "false"
  ${EndIf}
  Goto exit

d3d_disable:

  IntOp $0 $0 & ${SECTION_OFF} ; Unselect section again
  SectionSetFlags ${SEC03} $0
  Goto exit

exit:

  Pop $0

FunctionEnd

; This function is called when a critical error occurred
Function .onInstFailed

  MessageBox MB_ICONSTOP $(VBOX_ERROR_INST_FAILED) /SD IDOK

  Push "Error while installing ${PRODUCT_NAME}!"
  Push 2 ; Message type = error
  Call WriteLogVBoxTray

  StrCpy $g_bLogEnable "true"
  Call WriteLogUI
  SetErrorLevel 1

FunctionEnd

; This function is called when installation was successful!
Function .onInstSuccess

  Push "${PRODUCT_NAME} successfully updated!"
  Push 0 ; Message type = info
  Call WriteLogVBoxTray

FunctionEnd

; This function is called at the very beginning of installer execution
Function .onInit

  ; Init values
  StrCpy $g_iSystemMode "0"
  StrCpy $g_strAddVerMaj "0"
  StrCpy $g_strAddVerMin "0"
  StrCpy $g_strAddVerBuild "0"
  StrCpy $g_strAddVerRev "0"

  StrCpy $g_bIgnoreUnknownOpts "false"
  StrCpy $g_bLogEnable "false"
  StrCpy $g_bFakeWHQL "false"
  StrCpy $g_bForceInstall "false"
  StrCpy $g_bUninstall "false"
  StrCpy $g_bRebootOnExit "false"
  StrCpy $g_iScreenX "0"
  StrCpy $g_iScreenY "0"
  StrCpy $g_iScreenBpp "0"
  StrCpy $g_iSfOrder "0"
  StrCpy $g_bNoVBoxServiceExit "false"
  StrCpy $g_bNoVBoxTrayExit "false"
  StrCpy $g_bNoVideoDrv "false"
  StrCpy $g_bNoGuestDrv "false"
  StrCpy $g_bNoMouseDrv "false"
  StrCpy $g_bWithAutoLogon "false"
  StrCpy $g_bWithD3D "false"
  StrCpy $g_bOnlyExtract "false"
  StrCpy $g_bWithWDDM "false"
  StrCpy $g_bCapWDDM "false"
  StrCpy $g_bPostInstallStatus "false"

  SetErrorLevel 0
  ClearErrors

!ifndef UNINSTALLER_ONLY

  ; Handle command line
  Call HandleCommandLine

  Push "${PRODUCT_NAME} update started, please wait ..."
  Push 0 ; Message type = info
  Call WriteLogVBoxTray

  ; Retrieve Windows version and store result in $g_strWinVersion
  Call GetWindowsVer

  ; Retrieve capabilities
  Call CheckForCapabilities

  ; Retrieve system mode and store result in
  System::Call 'user32::GetSystemMetrics(i ${SM_CLEANBOOT}) i .r0'
  StrCpy $g_iSystemMode $0
  DetailPrint "System mode: $g_iSystemMode"

  ; Get user Name
  AccessControl::GetCurrentUserName
  Pop $g_strCurUser
  DetailPrint "Current user: $g_strCurUser"

  ; Only uninstall?
  StrCmp $g_bUninstall "true" uninstall

  ; Only extract files?
  StrCmp $g_bOnlyExtract "true" extract_files

  ; Set section bits
  ${If} $g_bWithAutoLogon == "true" ; Auto-logon support
    SectionSetFlags ${SEC02} ${SF_SELECTED}
  ${EndIf}
!if $%VBOX_WITH_CROGL% == "1"
  ${If} $g_bWithD3D == "true" ; D3D support
    SectionSetFlags ${SEC03} ${SF_SELECTED}
  ${EndIf}
!endif

!ifdef USE_MUI
  ; Display language selection dialog (will be hidden in silent mode!)
  !ifdef VBOX_INSTALLER_ADD_LANGUAGES
    !insertmacro MUI_LANGDLL_DISPLAY
  !endif
!endif

  ; Do some checks before we actually start ...
  Call IsUserAdmin

  ; Check if there's already another instance of the installer is running -
  ; important for preventing NT4 to spawn the installer twice
  System::Call 'kernel32::CreateMutexA(i 0, i 0, t "VBoxGuestInstaller") ?e'
  Pop $R0
  StrCmp $R0 0 +1 exit

  ; Check for correct architecture
  Call CheckArchitecture

  ; Because this NSIS installer is always built in 32-bit mode, we have to
  ; do some tricks for the Windows paths for checking for old additions
  ; in block below
!if $%BUILD_TARGET_ARCH% == "amd64"
  ${DisableX64FSRedirection}
  SetRegView 64
!endif

  ; Check for old additions
  Call CheckForOldGuestAdditions
  Call GetAdditionsVersion

  ; Due to some bug in NSIS the license page won't be displayed if we're in
  ; 64-bit registry view, so as a workaround switch back to 32-bit (Wow6432Node)
  ; mode for now
!if $%BUILD_TARGET_ARCH% == "amd64"
  ${EnableX64FSRedirection}
  SetRegView 32
!endif

  Goto done

uninstall:

  Call Uninstall_Innotek
  Call Uninstall
  MessageBox MB_ICONINFORMATION|MB_OK $(VBOX_UNINST_SUCCESS) /SD IDOK
  Goto exit

extract_files:

  Call ExtractFiles
  MessageBox MB_OK|MB_ICONINFORMATION $(VBOX_EXTRACTION_COMPLETE) /SD IDOK
  Goto exit

!else ; UNINSTALLER_ONLY

  ;
  ; If UNINSTALLER_ONLY is defined, we're only interested in uninst.exe
  ; so we can sign it
  ;
  ; Note that the Quit causes the exit status to be 2 instead of 0
  ;
  WriteUninstaller "$%PATH_TARGET%\uninst.exe"
  Goto exit

!endif ; UNINSTALLER_ONLY

exit:    ; Abort installer for some reason

  Quit

done:

FunctionEnd

;
; The uninstaller is built separately when doing code signing
; For some reason NSIS still finds the Uninstall section even
; when EXTERNAL_UNINSTALLER is defined. This causes a silly warning
;
!ifndef EXTERNAL_UNINSTALLER

Function un.onUninstSuccess

  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK $(VBOX_UNINST_SUCCESS) /SD IDOK

FunctionEnd

Function un.onInit

  Call un.IsUserAdmin

  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 $(VBOX_UNINST_CONFIRM) /SD IDYES IDYES proceed
  Quit

proceed:

  ; Because this NSIS installer is always built in 32-bit mode, we have to
  ; do some tricks for the Windows paths
!if $%BUILD_TARGET_ARCH% == "amd64"
  ${DisableX64FSRedirection}
  SetRegView 64
!endif

  ; Set system directory
  StrCpy $g_strSystemDir "$SYSDIR"

  ; Retrieve Windows version we're running on and store it in $g_strWinVersion
  Call un.GetWindowsVer

  ; Retrieve capabilities
  Call un.CheckForCapabilities

FunctionEnd

Section Uninstall

!ifdef _DEBUG
  ; Enable logging
  Call un.EnableLog
!endif

  ; Because this NSIS installer is always built in 32-bit mode, we have to
  ; do some tricks for the Windows paths
!if $%BUILD_TARGET_ARCH% == "amd64"
  ; Do *not* add this line in .onInit - it will crash at the license page (??) because of a weird NSIS bug
  ${DisableX64FSRedirection}
  SetRegView 64
!endif

  ; Call the uninstall main function
  Call un.Uninstall

  ; ... and remove the local install directory
  Call un.UninstallInstDir

!ifndef _DEBUG
  SetAutoClose true
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 $(VBOX_REBOOT_REQUIRED) /SD IDNO IDYES restart
  StrCmp $g_bRebootOnExit "true" restart
!endif

  Goto exit

restart:

  DetailPrint "Rebooting ..."
  Reboot

exit:

SectionEnd

; !EXTERNAL_UNINSTALLER
!endif

;Direct the output to our bin dir
!cd "$%PATH_OUT%\bin\additions"
