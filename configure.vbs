' $Id$
'' @file
' The purpose of this script is to check for all external tools, headers, and
' libraries VBox OSE depends on.
'
' The script generates the build configuration file 'AutoConfig.kmk' and the
' environment setup script 'env.bat'. A log of what has been done is
' written to 'configure.log'.
'

'
' Copyright (C) 2006-2007 Sun Microsystems, Inc.
'
' This file is part of VirtualBox Open Source Edition (OSE), as
' available from http://www.virtualbox.org. This file is free software;
' you can redistribute it and/or modify it under the terms of the GNU
' General Public License (GPL) as published by the Free Software
' Foundation, in version 2 as it comes in the "COPYING" file of the
' VirtualBox OSE distribution. VirtualBox OSE is distributed in the
' hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
'
' Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
' Clara, CA 95054 USA or visit http://www.sun.com if you need
' additional information or have any questions.
'


'*****************************************************************************
'* Global Variables                                                          *
'*****************************************************************************
dim g_strPath, g_strEnvFile, g_strLogFile, g_strCfgFile, g_strShellOutput
g_strPath = Left(Wscript.ScriptFullName, Len(Wscript.ScriptFullName) - Len("\configure.vbs"))
g_strEnvFile = g_strPath & "\env.bat"
g_strCfgFile = g_strPath & "\AutoConfig.kmk"
g_strLogFile = g_strPath & "\configure.log"
'g_strTmpFile = g_strPath & "\configure.tmp"

dim g_objShell, g_objFileSys
Set g_objShell = WScript.CreateObject("WScript.Shell")
Set g_objFileSys = WScript.CreateObject("Scripting.FileSystemObject")

dim g_strPathkBuild, g_strPathkBuildBin, g_strPathDev, g_strPathVCC, g_strPathPSDK, g_strPathDDK, g_strSubOutput
g_strPathkBuild = ""
g_strPathDev = ""
g_strPathVCC = ""
g_strPathPSDK = ""
g_strPathDDK = ""

dim g_blnDisableCOM, g_strDisableCOM
g_blnDisableCOM = False
g_strDisableCOM = ""

' The internal mode is primarily for skipping some large libraries.
dim g_blnInternalMode
g_blnInternalMode = False

' Whether to try the internal stuff first or last.
dim g_blnInternalFirst
g_blnInternalFirst = True



''
' Converts to unix slashes
function UnixSlashes(str)
   UnixSlashes = replace(str, "\", "/")
end function


''
' Converts to dos slashes
function DosSlashes(str)
   DosSlashes = replace(str, "/", "\")
end function


''
' Read a file (typically the tmp file) into a string.
function FileToString(strFilename)
   const ForReading = 1, TristateFalse = 0
   dim objLogFile, str

   set objFile = g_objFileSys.OpenTextFile(DosSlashes(strFilename), ForReading, False, TristateFalse)
   str = objFile.ReadAll()
   objFile.Close()

   FileToString = str
end function


''
' Deletes a file
sub FileDelete(strFilename)
   if g_objFileSys.FileExists(DosSlashes(strFilename)) then
      g_objFileSys.DeleteFile(DosSlashes(strFilename))
   end if
end sub


''
' Appends a line to an ascii file.
sub FileAppendLine(strFilename, str)
   const ForAppending = 8, TristateFalse = 0
   dim objFile

   set objFile = g_objFileSys.OpenTextFile(DosSlashes(strFilename), ForAppending, True, TristateFalse)
   objFile.WriteLine(str)
   objFile.Close()
end sub


''
' Checks if the file exists.
function FileExists(strFilename)
   FileExists = g_objFileSys.FileExists(DosSlashes(strFilename))
end function


''
' Checks if the directory exists.
function DirExists(strDirectory)
   DirExists = g_objFileSys.FolderExists(DosSlashes(strDirectory))
end function


''
' Checks if this is a WOW64 process.
function IsWow64()
   if g_objShell.Environment("PROCESS")("PROCESSOR_ARCHITEW6432") <> "" then
      IsWow64 = 1
   else
      IsWow64 = 0
   end if
end function


''
' Translates a register root name to a value
function RegTransRoot(strRoot)
   const HKEY_LOCAL_MACHINE = &H80000002
   const HKEY_CURRENT_USER  = &H80000001
   select case strRoot
      case "HKLM"
         RegTransRoot = HKEY_LOCAL_MACHINE
      case "HKCU"
         RegTransRoot = HKEY_CURRENT_USER
      case else
         MsgFatal "RegEnumSubKeys: Unknown root: " & strRoot
         RegTransRoot = 0
   end select
end function


'' The registry globals
dim g_objReg, g_objRegCtx
dim g_blnRegistry
g_blnRegistry = false


''
' Init the register provider globals.
function RegInit()
   RegInit = false
   On Error Resume Next
   if g_blnRegistry = false then
      set g_objRegCtx = CreateObject("WbemScripting.SWbemNamedValueSet")
      ' Comment out the following for lines if the cause trouble on your windows version.
      if IsWow64() then
         g_objRegCtx.Add "__ProviderArchitecture", 64
         g_objRegCtx.Add "__RequiredArchitecture", true
      end if
      set objLocator = CreateObject("Wbemscripting.SWbemLocator")
      set objServices = objLocator.ConnectServer("", "root\default", "", "", , , , g_objRegCtx)
      set g_objReg = objServices.Get("StdRegProv")
      g_blnRegistry = true
   end if
   RegInit = true
end function


''
' Gets a value from the registry. Returns "" if string wasn't found / valid.
function RegGetString(strName)
   RegGetString = ""
   if RegInit() then
      dim strRoot, strKey, strValue
      dim iRoot

      ' split up into root, key and value parts.
      strRoot = left(strName, instr(strName, "\") - 1)
      strKey = mid(strName, instr(strName, "\") + 1, instrrev(strName, "\") - instr(strName, "\"))
      strValue = mid(strName, instrrev(strName, "\") + 1)

      ' Must use ExecMethod to call the GetStringValue method because of the context.
      Set InParms = g_objReg.Methods_("GetStringValue").Inparameters
      InParms.hDefKey     = RegTransRoot(strRoot)
      InParms.sSubKeyName = strKey
      InParms.sValueName  = strValue
      On Error Resume Next
      set OutParms = g_objReg.ExecMethod_("GetStringValue", InParms, , g_objRegCtx)
      if OutParms.ReturnValue = 0 then
         RegGetString = OutParms.sValue
      end if
   else
      ' fallback mode
      On Error Resume Next
      RegGetString = g_objShell.RegRead(strName)
   end if
end function


''
' Returns an array of subkey strings.
function RegEnumSubKeys(strRoot, strKeyPath)
   dim iRoot
   iRoot = RegTransRoot(strRoot)
   RegEnumSubKeys = Array()

   if RegInit() then
      ' Must use ExecMethod to call the EnumKey method because of the context.
      Set InParms = g_objReg.Methods_("EnumKey").Inparameters
      InParms.hDefKey     = RegTransRoot(strRoot)
      InParms.sSubKeyName = strKeyPath
      On Error Resume Next
      set OutParms = g_objReg.ExecMethod_("EnumKey", InParms, , g_objRegCtx)
      if OutParms.ReturnValue = 0 then
         RegEnumSubKeys = OutParms.sNames
      end if
   else
      ' fallback mode
      dim objReg, rc, arrSubKeys
      set objReg = GetObject("winmgmts:{impersonationLevel=impersonate}!\\.\root\default:StdRegProv")
      On Error Resume Next
      rc = objReg.EnumKey(iRoot, strKeyPath, arrSubKeys)
      if rc = 0 then
         RegEnumSubKeys = arrSubKeys
      end if
   end if
end function


''
' Gets the commandline used to invoke the script.
function GetCommandline()
   dim str, i

   '' @todo find an api for querying it instead of reconstructing it like this...
   GetCommandline = "cscript configure.vbs"
   for i = 1 to WScript.Arguments.Count
      str = WScript.Arguments.Item(i - 1)
      if str = "" then
         str = """"""
      elseif (InStr(1, str, " ")) then
         str = """" & str & """"
      end if
      GetCommandline = GetCommandline & " " & str
   next
end function


''
' Gets an environment variable.
function EnvGet(strName)
   EnvGet = g_objShell.Environment("PROCESS")(strName)
end function


''
' Sets an environment variable.
sub EnvSet(strName, strValue)
   g_objShell.Environment("PROCESS")(strName) = strValue
   LogPrint "EnvSet: " & strName & "=" & strValue
end sub


''
' Appends a string to an environment variable
sub EnvAppend(strName, strValue)
   dim str
   str = g_objShell.Environment("PROCESS")(strName)
   g_objShell.Environment("PROCESS")(strName) =  str & strValue
   LogPrint "EnvAppend: " & strName & "=" & str & strValue
end sub


''
' Prepends a string to an environment variable
sub EnvPrepend(strName, strValue)
   dim str
   str = g_objShell.Environment("PROCESS")(strName)
   g_objShell.Environment("PROCESS")(strName) =  strValue & str
   LogPrint "EnvPrepend: " & strName & "=" & strValue & str
end sub


''
' Get the path of the parent directory. Returns root if root was specified.
' Expects abs path.
function PathParent(str)
   PathParent = g_objFileSys.GetParentFolderName(DosSlashes(str))
end function


''
' Strips the filename from at path.
function PathStripFilename(str)
   PathStripFilename = g_objFileSys.GetParentFolderName(DosSlashes(str))
end function


''
' Get the abs path, use the short version if necessary.
function PathAbs(str)
   PathAbs = g_objFileSys.GetAbsolutePathName(DosSlashes(str))
   if  (InStr(1, PathAbs, " ") > 0) _
    Or (InStr(1, PathAbs, "&") > 0) _
    Or (InStr(1, PathAbs, "$") > 0) _
      then
         if FileExists(PathAbs) then
            dim objFile
            set objFile = g_objFileSys.GetFile(PathAbs)
            PathAbs = objFile.ShortPath
         elseif DirExists(PathAbs) then
            dim objFolder
            set objFolder = g_objFileSys.GetFolder(PathAbs)
            PathAbs = objFolder.ShortPath
         else
            ' ignore non-existing paths.
         end if
   end if


   if (FileExists(PathAbs) Or DirExists(PathAbs)) _
    And (   (InStr(1, PathAbs, " ") > 0) _
         Or (InStr(1, PathAbs, "&") > 0) _
         Or (InStr(1, PathAbs, "$") > 0)) _
      then
      MsgFatal "PathAbs(" & str & ") attempted to return filename with problematic " _
             & "characters in it (" & PathAbs & "). The tool/sdk referenced will probably " _
             & "need to be copied or reinstalled to a location without 'spaces', '$', ';' " _
             & "or '&' in the path name. (Unless it's a problem with this script of course...)"
   end if
end function


''
' Executes a command in the shell catching output in g_strShellOutput
function Shell(strCommand, blnBoth)
   dim strShell, strCmdline, objExec, str

   strShell = g_objShell.ExpandEnvironmentStrings("%ComSpec%")
   if blnBoth = true then
      strCmdline = strShell & " /c " & strCommand & " 2>&1"
   else
      strCmdline = strShell & " /c " & strCommand & " 2>nul"
   end if

   LogPrint "# Shell: " & strCmdline
   Set objExec = g_objShell.Exec(strCmdLine)
   g_strShellOutput = objExec.StdOut.ReadAll()
   objExec.StdErr.ReadAll()
   do while objExec.Status = 0
      Wscript.Sleep 20
      g_strShellOutput = g_strShellOutput & objExec.StdOut.ReadAll()
      objExec.StdErr.ReadAll()
   loop

   LogPrint "# Status: " & objExec.ExitCode
   LogPrint "# Start of Output"
   LogPrint g_strShellOutput
   LogPrint "# End of Output"

   Shell = objExec.ExitCode
end function


''
' Try find the specified file in the path.
function Which(strFile)
   dim strPath, iStart, iEnd, str

   ' the path
   strPath = EnvGet("Path")
   iStart = 1
   do while iStart <= Len(strPath)
      iEnd = InStr(iStart, strPath, ";")
      if iEnd <= 0 then iEnd = Len(strPath) + 1
      if iEnd > iStart then
         str = Mid(strPath, iStart, iEnd - iStart) & "/" & strFile
         if FileExists(str) then
            Which = str
            exit function
         end if
      end if
      iStart = iEnd + 1
   loop

   ' registry or somewhere?

   Which = ""
end function


''
' Append text to the log file and echo it to stdout
sub Print(str)
   LogPrint str
   Wscript.Echo str
end sub


''
' Prints a test header
sub PrintHdr(strTest)
   LogPrint "***** Checking for " & strTest & " *****"
   Wscript.Echo "Checking for " & StrTest & "..."
end sub


''
' Prints a success message
sub PrintResult(strTest, strResult)
   LogPrint "** " & strTest & ": " & strResult
   Wscript.Echo " Found "& strTest & ": " & strResult
end sub


''
' Warning message.
sub MsgWarning(strMsg)
   Print "warning: " & strMsg
end sub


''
' Fatal error.
sub MsgFatal(strMsg)
   Print "fatal error: " & strMsg
   Wscript.Quit
end sub


''
' Error message, fatal unless flag to ignore errors is given.
sub MsgError(strMsg)
   Print "error: " & strMsg
   if g_blnInternalMode = False then
      Wscript.Quit
   end if
end sub


''
' Write a log header with some basic info.
sub LogInit
   FileDelete g_strLogFile
   LogPrint "# Log file generated by " & Wscript.ScriptFullName
   for i = 1 to WScript.Arguments.Count
      LogPrint "# Arg #" & i & ": " & WScript.Arguments.Item(i - 1)
   next
   if Wscript.Arguments.Count = 0 then
      LogPrint "# No arguments given"
   end if
   LogPrint "# Reconstructed command line: " & GetCommandline()

   ' some Wscript stuff
   LogPrint "# Wscript properties:"
   LogPrint "#   ScriptName: " & Wscript.ScriptName
   LogPrint "#   Version:    " & Wscript.Version
   LogPrint "#   Build:      " & Wscript.BuildVersion
   LogPrint "#   Name:       " & Wscript.Name
   LogPrint "#   Full Name:  " & Wscript.FullName
   LogPrint "#   Path:       " & Wscript.Path
   LogPrint "#"


   ' the environment
   LogPrint "# Environment:"
   dim objEnv
   for each strVar in g_objShell.Environment("PROCESS")
      LogPrint "#   " & strVar
   next
   LogPrint "#"
end sub


''
' Append text to the log file.
sub LogPrint(str)
   FileAppendLine g_strLogFile, str
   'Wscript.Echo "dbg: " & str
end sub


''
' Checks if the file exists and logs failures.
function LogFileExists(strPath, strFilename)
   LogFileExists = FileExists(strPath & "/" & strFilename)
   if LogFileExists = False then
      LogPrint "Testing '" & strPath & "': " & strFilename & " not found"
   end if

end function


''
' Finds the first file matching the pattern.
' If no file is found, log the failure.
function LogFindFile(strPath, strPattern)
   dim str

   '
   ' Yes, there are some facy database kinda interface to the filesystem
   ' however, breaking down the path and constructing a usable query is
   ' too much hassle. So, we'll do it the unix way...
   '
   if Shell("dir /B """ & DosSlashes(strPath) & "\" & DosSlashes(strPattern) & """", True) = 0 _
    And InStr(1, g_strShellOutput, Chr(13)) > 1 _
      then
      ' return the first word.
      LogFindFile = Left(g_strShellOutput, InStr(1, g_strShellOutput, Chr(13)) - 1)
   else
      LogPrint "Testing '" & strPath & "': " & strPattern & " not found"
      LogFindFile = ""
   end if
end function


''
' Finds the first directory matching the pattern.
' If no directory is found, log the failure,
' else return the complete path to the found directory.
function LogFindDir(strPath, strPattern)
   dim str

   '
   ' Yes, there are some facy database kinda interface to the filesystem
   ' however, breaking down the path and constructing a usable query is
   ' too much hassle. So, we'll do it the unix way...
   '

   ' List the alphabetically last names as first entries (with /O-N).
   if Shell("dir /B /AD /O-N """ & DosSlashes(strPath) & "\" & DosSlashes(strPattern) & """", True) = 0 _
    And InStr(1, g_strShellOutput, Chr(13)) > 1 _
      then
      ' return the first word.
      LogFindDir = strPath & "/" & Left(g_strShellOutput, InStr(1, g_strShellOutput, Chr(13)) - 1)
   else
      LogPrint "Testing '" & strPath & "': " & strPattern & " not found"
      LogFindDir = ""
   end if
end function


''
' Initializes the config file.
sub CfgInit
   FileDelete g_strCfgFile
   CfgPrint "# -*- Makefile -*-"
   CfgPrint "#"
   CfgPrint "# Build configuration generated by " & GetCommandline()
   CfgPrint "#"
   if g_blnInternalMode = False then
      CfgPrint "VBOX_OSE := 1"
   end if
end sub


''
' Prints a string to the config file.
sub CfgPrint(str)
   FileAppendLine g_strCfgFile, str
end sub


''
' Initializes the environment batch script.
sub EnvInit
   FileDelete g_strEnvFile
   EnvPrint "@echo off"
   EnvPrint "rem"
   EnvPrint "rem Environment setup script generated by " & GetCommandline()
   EnvPrint "rem"
end sub


''
' Prints a string to the environment batch script.
sub EnvPrint(str)
   FileAppendLine g_strEnvFile, str
end sub


''
' No COM
sub DisableCOM(strReason)
   if g_blnDisableCOM = False then
      LogPrint "Disabled COM components: " & strReason
      g_blnDisableCOM = True
      g_strDisableCOM = strReason
      CfgPrint "VBOX_WITH_MAIN="
      CfgPrint "VBOX_WITH_QTGUI="
      CfgPrint "VBOX_WITH_VBOXSDL="
      CfgPrint "VBOX_WITH_DEBUGGER_GUI="
      CfgPrint "VBOX_WITHOUT_COM=1"
   end if
end sub


''
' Checks the the path doesn't contain characters the tools cannot deal with.
sub CheckSourcePath
   dim sPwd

   sPwd = PathAbs(g_strPath)
   if InStr(1, sPwd, " ") > 0 then
      MsgError "Source path contains spaces! Please move it. (" & sPwd & ")"
   end if
   if InStr(1, sPwd, "$") > 0 then
      MsgError "Source path contains the '$' char! Please move it. (" & sPwd & ")"
   end if
   if InStr(1, sPwd, "%") > 0 then
      MsgError "Source path contains the '%' char! Please move it. (" & sPwd & ")"
   end if
   if  InStr(1, sPwd, Chr(10)) > 0 _
    Or InStr(1, sPwd, Chr(13)) > 0 _
    Or InStr(1, sPwd, Chr(9)) > 0 _
    then
      MsgError "Source path contains control characters! Please move it. (" & sPwd & ")"
   end if
   Print "Source path: OK"
end sub


''
' Checks for kBuild - very simple :)
sub CheckForkBuild(strOptkBuild)
   PrintHdr "kBuild"

   '
   ' Check if there is a 'kmk' in the path somewhere without
   ' any PATH_KBUILD* stuff around.
   '
   blnNeedEnvVars = True
   g_strPathkBuild = strOptkBuild
   g_strPathkBuildBin = ""
   if   (g_strPathkBuild = "") _
    And (EnvGet("PATH_KBUILD") = "") _
    And (EnvGet("PATH_KBUILD_BIN") = "") _
    And (Shell("kmk.exe --version", True) = 0) _
    And (InStr(1,g_strShellOutput, "kBuild Make 0.1") > 0) _
    And (InStr(1,g_strShellOutput, "PATH_KBUILD") > 0) _
    And (InStr(1,g_strShellOutput, "PATH_KBUILD_BIN") > 0) then
      '' @todo Need to parse out the PATH_KBUILD and PATH_KBUILD_BIN values to complete the other tests.
      'blnNeedEnvVars = False
      MsgWarning "You've installed kBuild it seems. configure.vbs hasn't been updated to " _
         & "deal with that yet and will use the one it ships with. Sorry."
   end if

   '
   ' Check for the PATH_KBUILD env.var. and fall back on root/kBuild otherwise.
   '
   if g_strPathkBuild = "" then
      g_strPathkBuild = EnvGet("PATH_KBUILD")
      if (g_strPathkBuild <> "") and (FileExists(g_strPathkBuild & "/footer.kmk") = False) then
         MsgWarning "Ignoring incorrect kBuild path (PATH_KBUILD=" & g_strPathkBuild & ")"
         g_strPathkBuild = ""
      end if

      if g_strPathkBuild = "" then
         g_strPathkBuild = g_strPath & "/kBuild"
      end if
   end if

   g_strPathkBuild = UnixSlashes(PathAbs(g_strPathkBuild))

   '
   ' Determin the location of the kBuild binaries.
   '
   if g_strPathkBuildBin = "" then
      dim str2
      if EnvGet("PROCESSOR_ARCHITECTURE") = "x86" then
         g_strPathkBuildBin = g_strPathkBuild & "/bin/win.x86"
      else ' boldly assumes there is only x86 and amd64.
         g_strPathkBuildBin = g_strPathkBuild & "/bin/win.amd64"
         if FileExists(g_strPathkBuild & "/kmk.exe") = False then
            g_strPathkBuildBin = g_strPathkBuild & "/bin/win.x86"
         end if
      end if
      if FileExists(g_strPathkBuild & "/kmk.exe") = False then
         g_strPathkBuildBin = g_strPathkBuild & "/bin/win.x86"
      end if
   end if

   '
   ' Perform basic validations of the kBuild installation.
   '
   if  (FileExists(g_strPathkBuild & "/footer.kmk") = False) _
    Or (FileExists(g_strPathkBuild & "/header.kmk") = False) _
    Or (FileExists(g_strPathkBuild & "/rules.kmk") = False) then
      MsgFatal "Can't find valid kBuild at '" & g_strPathkBuild & "'. Either there is an " _
         & "incorrect PATH_KBUILD in the environment or the checkout didn't succeed."
      exit sub
   end if
   if  (FileExists(g_strPathkBuildBin & "/kmk.exe") = False) _
    Or (FileExists(g_strPathkBuildBin & "/kmk_ash.exe") = False) then
      MsgFatal "Can't find valid kBuild binaries at '" & g_strPathkBuildBin & "'. Either there is an " _
         & "incorrect PATH_KBUILD in the environment or the checkout didn't succeed."
      exit sub
   end if

   if (Shell(DosSlashes(g_strPathkBuildBin & "/kmk.exe") & " --version", True) <> 0) Then
      MsgFatal "Can't execute '" & g_strPathkBuildBin & "/kmk.exe --version'. check configure.log for the out."
      exit sub
   end if

   '
   ' Check for env.vars that kBuild uses.
   '
   str = EnvGet("BUILD_TYPE")
   if   (str <> "") _
    And (InStr(1, "|release|debug|profile|kprofile", str) <= 0) then
      EnvPrint "set BUILD_TYPE=release"
      EnvSet "BUILD_TYPE", "release"
      MsgWarning "Found unknown BUILD_TYPE value '" & str &"' in your environment. Setting it to 'release'."
   end if

   str = EnvGet("BUILD_TARGET")
   if   (str <> "") _
    And (InStr(1, "win|win32|win64", str) <= 0) then '' @todo later only 'win' will be valid. remember to fix this check!
      EnvPrint "set BUILD_TARGET=win"
      EnvSet "BUILD_TARGET", "win"
      MsgWarning "Found unknown BUILD_TARGET value '" & str &"' in your environment. Setting it to 'win32'."
   end if

   str = EnvGet("BUILD_TARGET_ARCH")
   if   (str <> "") _
    And (InStr(1, "x86|amd64", str) <= 0) then
      EnvPrint "set BUILD_TARGET_ARCH=x86"
      EnvSet "BUILD_TARGET_ARCH", "x86"
      MsgWarning "Found unknown BUILD_TARGET_ARCH value '" & str &"' in your environment. Setting it to 'x86'."
   end if

   str = EnvGet("BUILD_TARGET_CPU")
    ' perhaps a bit pedantic this since this isn't clearly define nor used much...
   if   (str <> "") _
    And (InStr(1, "i386|i486|i686|i786|i868|k5|k6|k7|k8", str) <= 0) then
      EnvPrint "set BUILD_TARGET_CPU=i386"
      EnvSet "BUILD_TARGET_CPU", "i386"
      MsgWarning "Found unknown BUILD_TARGET_CPU value '" & str &"' in your environment. Setting it to 'i386'."
   end if

   str = EnvGet("BUILD_PLATFORM")
   if   (str <> "") _
    And (InStr(1, "win|win32|win64", str) <= 0) then '' @todo later only 'win' will be valid. remember to fix this check!
      EnvPrint "set BUILD_PLATFORM=win"
      EnvSet "BUILD_PLATFORM", "win"
      MsgWarning "Found unknown BUILD_PLATFORM value '" & str &"' in your environment. Setting it to 'win32'."
   end if

   str = EnvGet("BUILD_PLATFORM_ARCH")
   if   (str <> "") _
    And (InStr(1, "x86|amd64", str) <= 0) then
      EnvPrint "set BUILD_PLATFORM_ARCH=x86"
      EnvSet "BUILD_PLATFORM_ARCH", "x86"
      MsgWarning "Found unknown BUILD_PLATFORM_ARCH value '" & str &"' in your environment. Setting it to 'x86'."
   end if

   str = EnvGet("BUILD_PLATFORM_CPU")
    ' perhaps a bit pedantic this since this isn't clearly define nor used much...
   if   (str <> "") _
    And (InStr(1, "i386|i486|i686|i786|i868|k5|k6|k7|k8", str) <= 0) then
      EnvPrint "set BUILD_PLATFORM_CPU=i386"
      EnvSet "BUILD_PLATFORM_CPU", "i386"
      MsgWarning "Found unknown BUILD_PLATFORM_CPU value '" & str &"' in your environment. Setting it to 'i386'."
   end if

   '
   ' If PATH_DEV is set, check that it's pointing to something useful.
   '
   str = EnvGet("PATH_DEV")
   g_strPathDev = str
   if (str <> "") _
    And False then '' @todo add some proper tests here.
      strNew = UnixSlashes(g_strPath & "/tools")
      EnvPrint "set PATH_DEV=" & strNew
      EnvSet "PATH_DEV", strNew
      MsgWarning "Found PATH_DEV='" & str &"' in your environment. Setting it to '" & strNew & "'."
      g_strPathDev = strNew
   end if
   if g_strPathDev = "" then g_strPathDev = UnixSlashes(g_strPath & "/tools")

   '
   ' Write PATH_KBUILD to the environment script if necessary.
   '
   if blnNeedEnvVars = True then
      EnvPrint "set PATH_KBUILD=" & g_strPathkBuild
      EnvSet "PATH_KBUILD", g_strPathkBuild
      EnvPrint "set PATH=" & g_strPathkBuildBin & ";%PATH%"
      EnvPrepend "PATH", g_strPathkBuildBin & ";"
   end if

   PrintResult "kBuild", g_strPathkBuild
   PrintResult "kBuild binaries", g_strPathkBuildBin
end sub


''
' Checks for Visual C++ version 7 or 8.
sub CheckForVisualCPP(strOptVC, strOptVCCommon, blnOptVCExpressEdition)
   dim strPathVC, strPathVCCommon, str, str2, blnNeedMsPDB
   PrintHdr "Visual C++"

   '
   ' Try find it...
   '
   strPathVC = ""
   strPathVCCommon = ""
   if (strPathVC = "") And (strOptVC <> "") then
      if CheckForVisualCPPSub(strOptVC, strOptVCCommon, blnOptVCExpressEdition) then
         strPathVC = strOptVC
         strPathVCCommon = strOptVCCommon
      end if
   end if

   if (strPathVC = "") And (g_blnInternalFirst = True) Then
      strPathVC = g_strPathDev & "/win.x86/vcc/v8"
      if CheckForVisualCPPSub(strPathVC, "", blnOptVCExpressEdition) = False then
         strPathVC = g_strPathDev & "/win.x86/vcc/v7"
         if CheckForVisualCPPSub(strPathVC, "", blnOptVCExpressEdition) = False then
            strPathVC = ""
         end if
      end if
   end if

   if   (strPathVC = "") _
    And (Shell("cl.exe", True) = 0) then
      str = Which("cl.exe")
      if FileExists(PathStripFilename(strClExe) & "/build.exe") then
         ' don't know how to deal with this cl.
         Warning "Ignoring DDK cl.exe (" & str & ")."
      else
         strPathVC = PathParent(PathStripFilename(str))
         strPathVCCommon = PathParent(strPathVC) & "/Common7"
      end if
   end if

   if strPathVC = "" then
      str = RegGetString("HKLM\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\8.0\Setup\VS\ProductDir")
      str2 = RegGetString("HKLM\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\8.0\Setup\VS\EnvironmentDirectory")
      if str <> "" And str2 <> "" Then
         str = str & "VC"
         str2 = PathParent(str2)
         if CheckForVisualCPPSub(str, str2, blnOptVCExpressEdition) then
            strPathVC = str
            strPathVCCommon = str2
         end if
      end if
   end if

   if strPathVC = "" then
      str = RegGetString("HKLM\SOFTWARE\Microsoft\VisualStudio\8.0\Setup\VS\ProductDir")
      str2 = RegGetString("HKLM\SOFTWARE\Microsoft\VisualStudio\8.0\Setup\VS\EnvironmentDirectory")
      if str <> "" And str2 <> "" Then
         str = str & "VC"
         str2 = PathParent(str2)
         if CheckForVisualCPPSub(str, str2, blnOptVCExpressEdition) then
            strPathVC = str
            strPathVCCommon = str2
         end if
      end if
   end if

   if strPathVC = "" then
      '' @todo check what this really looks like on 7.1
      str = RegGetString("HKLM\SOFTWARE\Microsoft\VisualStudio\7.1\Setup\VS\ProductDir")
      str2 = RegGetString("HKLM\SOFTWARE\Microsoft\VisualStudio\7.1\Setup\VS\EnvironmentDirectory")
      if str <> "" And str2 <> "" Then
         str = str & "VC7"
         str2 = PathParent(str2)
         if CheckForVisualCPPSub(str, str2, blnOptVCExpressEdition) then
            strPathVC = str
            strPathVCCommon = str2
         end if
      end if
   end if

   if strPathVC = "" then
      str = RegGetString("HKLM\SOFTWARE\Microsoft\VisualStudio\7.0\Setup\VS\ProductDir")
      str2 = RegGetString("HKLM\SOFTWARE\Microsoft\VisualStudio\7.0\Setup\VS\EnvironmentDirectory")
      if str <> "" And str2 <> "" Then
         str = str & "VC7"
         str2 = PathParent(str2)
         if CheckForVisualCPPSub(str, str2, blnOptVCExpressEdition) then
            strPathVC = str
            strPathVCCommon = str2
         end if
      end if
   end if

   if strPathVC = "" then
      str = RegGetString("HKLM\SOFTWARE\Microsoft\Wow6432Node\VisualStudio\SxS\VC7\8.0")
      if str <> "" then
         str2 = PathParent(str) & "/Common7"
         if CheckForVisualCPPSub(str, str2, blnOptVCExpressEdition) then
            strPathVC = str
            strPathVCCommon = str2
         end if
      end if
   end if

   if strPathVC = "" then
      str = RegGetString("HKLM\SOFTWARE\Microsoft\VisualStudio\SxS\VC7\8.0")
      if str <> "" then
         str2 = PathParent(str) & "/Common7"
         if CheckForVisualCPPSub(str, str2, blnOptVCExpressEdition) then
            strPathVC = str
            strPathVCCommon = str2
         end if
      end if
   end if

   ' finally check for the express edition.
   if strPathVC = "" then
      str = RegGetString("HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Microsoft Visual C++ 2005 Express Edition - ENU\InstallLocation")
      if str <> "" then
         str2 = str & "Common7"
         str = str & "VC/"
         if CheckForVisualCPPSub(str, str2, blnOptVCExpressEdition) then
            strPathVC = str
            strPathVCCommon = str2
         end if
      end if
   end if

   if (strPathVC = "") And (g_blnInternalFirst = False) Then
      strPathVC = g_strPathDev & "/win.x86/vcc/v8"
      if CheckForVisualCPPSub(strPathVC, "", blnOptVCExpressEdition) = False then
         strPathVC = g_strPathDev & "/win.x86/vcc/v7"
         if CheckForVisualCPPSub(strPathVC, "", blnOptVCExpressEdition) = False then
            strPathVC = ""
         end if
      end if
   end if

   if strPathVC = "" then
      MsgError "Cannot find cl.exe (Visual C++) anywhere on your system. Check the build requirements."
      exit sub
   end if

   '
   ' Clean up the path and determin the VC directory.
   '
   strPathVC = UnixSlashes(PathAbs(strPathVC))
   g_strPathVCC = strPathVC

   '
   ' Check the version.
   ' We'll have to make sure mspdbXX.dll is somewhere in the PATH.
   '
   if (strPathVCCommon <> "") Then
      EnvAppend "PATH", ";" & strPathVCCommon & "/IDE"
   end if
   if Shell(DosSlashes(strPathVC & "/bin/cl.exe"), True) <> 0 then
      MsgError "Executing '" & strClExe & "' (which we believe to be the Visual C++ compiler driver) failed."
      exit sub
   end if

   if   (InStr(1, g_strShellOutput, "Version 13.10") <= 0) _
    And (InStr(1, g_strShellOutput, "Version 14.") <= 0) then
      MsgError "The Visual C++ compiler we found ('" & strPathVC & "') isn't 7.1 or 8.0. Check the build requirements."
      exit sub
   end if

   '
   ' Ok, emit build config variables.
   '
   if InStr(1, g_strShellOutput, "Version 14.") > 0 then
      CfgPrint "VBOX_USE_VCC80        := 1"
      CfgPrint "PATH_TOOL_VCC80       := " & g_strPathVCC
      CfgPrint "PATH_TOOL_VCC80X86     = $(PATH_TOOL_VCC80)"
      CfgPrint "PATH_TOOL_VCC80AMD64   = $(PATH_TOOL_VCC80)"
      if   blnOptVCExpressEdition _
       And LogFileExists(strPathVC, "atlmfc/include/atlbase.h") = False _
         then
         CfgPrint "TOOL_VCC80X86_MT = $(PATH_SDK_WINPSDK)/Bin/mt.exe"
         CfgPrint "TOOL_VCC80AMD64_MT = $(TOOL_VCC80X86_MT)"
         CfgPrint "VBOX_WITHOUT_COMPILER_REDIST=1"
         DisableCOM "No ATL"
         PrintResult "Visual C++ v8 (or later) without ATL", g_strPathVCC
      else
         PrintResult "Visual C++ v8 (or later)", g_strPathVCC
      end if
   else
      CfgPrint "PATH_TOOL_VCC70 := " & g_strPathVCC
      if   blnOptVCExpressEdition _
       And LogFileExists(strPathVC, "atlmfc/include/atlbase.h") = False _
         then
         CfgPrint "VBOX_WITHOUT_COMPILER_REDIST=1"
         DisableCOM "No ATL"
         PrintResult "Visual C++ v7.1 without ATL", g_strPathVCC
      else
         PrintResult "Visual C++ v7.1", g_strPathVCC
      end if
   end if

   ' and the env.bat path fix.
   if strPathVCCommon <> "" then
      EnvPrint "set PATH=%PATH%;" & strPathVCCommon & "/IDE;"
   end if
end sub

''
' Checks if the specified path points to a usable PSDK.
function CheckForVisualCPPSub(strPathVC, strPathVCCommon, blnOptVCExpressEdition)
   strPathVC = UnixSlashes(PathAbs(strPathVC))
   CheckForVisualCPPSub = False
   LogPrint "trying: strPathVC=" & strPathVC & " strPathVCCommon=" & strPathVCCommon & " blnOptVCExpressEdition=" & blnOptVCExpressEdition
   if   LogFileExists(strPathVC, "bin/cl.exe") _
    And LogFileExists(strPathVC, "bin/link.exe") _
    And LogFileExists(strPathVC, "include/string.h") _
    And LogFileExists(strPathVC, "lib/libcmt.lib") _
    And LogFileExists(strPathVC, "lib/msvcrt.lib") _
      then
      if blnOptVCExpressEdition _
       Or (    LogFileExists(strPathVC, "atlmfc/include/atlbase.h") _
           And LogFileExists(strPathVC, "atlmfc/lib/atls.lib")) _
         Then
         '' @todo figure out a way we can verify the version/build!
         CheckForVisualCPPSub = True
      end if
   end if
end function


''
' Checks for a platform SDK that works with the compiler
sub CheckForPlatformSDK(strOptSDK)
   dim strPathPSDK, str
   PrintHdr "Windows Platform SDK (recent)"

   strPathPSDK = ""

   ' Check the supplied argument first.
   str = strOptSDK
   if str <> "" then
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   ' The tools location (first).
   if (strPathPSDK = "") And (g_blnInternalFirst = True) then
      str = g_strPathDev & "/win.x86/sdk/200604"
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   if (strPathPSDK = "") And (g_blnInternalFirst = True) then
      str = g_strPathDev & "/win.x86/sdk/200504"
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   if (strPathPSDK = "") And (g_blnInternalFirst = True) then
      str = g_strPathDev & "/win.x86/sdk/200209"
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   ' Look for it in the environment
   str = EnvGet("MSSdk")
   if (strPathPSDK = "") And (str <> "") then
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   str = EnvGet("Mstools")
   if (strPathPSDK = "") And (str <> "") then
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   ' Check if there is one installed with the compiler.
   if (strPathPSDK = "") And (str <> "") then
      str = g_strPathVCC & "/PlatformSDK"
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   ' Check the registry next. (first pair is vista, second is pre-vista)
   arrSubKeys = RegEnumSubKeys("HKLM", "SOFTWARE\Microsoft\Microsoft SDKs\Windows")
   for Each strSubKey In arrSubKeys
      str = RegGetString("HKLM\SOFTWARE\Microsoft\Microsoft SDKs\Windows\" & strSubKey & "\InstallationFolder")
      if (strPathPSDK = "") And (str <> "") then
         if CheckForPlatformSDKSub(str) then strPathPSDK = str
      end if
   Next
   arrSubKeys = RegEnumSubKeys("HKCU", "SOFTWARE\Microsoft\Microsoft SDKs\Windows")
   for Each strSubKey In arrSubKeys
      str = RegGetString("HKCU\SOFTWARE\Microsoft\Microsoft SDKs\Windows\" & strSubKey & "\InstallationFolder")
      if (strPathPSDK = "") And (str <> "") then
         if CheckForPlatformSDKSub(str) then strPathPSDK = str
      end if
   Next

   arrSubKeys = RegEnumSubKeys("HKLM", "SOFTWARE\Microsoft\MicrosoftSDK\InstalledSDKs")
   for Each strSubKey In arrSubKeys
      str = RegGetString("HKLM\SOFTWARE\Microsoft\MicrosoftSDK\InstalledSDKs\" & strSubKey & "\Install Dir")
      if (strPathPSDK = "") And (str <> "") then
         if CheckForPlatformSDKSub(str) then strPathPSDK = str
      end if
   Next
   arrSubKeys = RegEnumSubKeys("HKCU", "SOFTWARE\Microsoft\MicrosoftSDK\InstalledSDKs")
   for Each strSubKey In arrSubKeys
      str = RegGetString("HKCU\SOFTWARE\Microsoft\MicrosoftSDK\InstalledSDKs\" & strSubKey & "\Install Dir")
      if (strPathPSDK = "") And (str <> "") then
         if CheckForPlatformSDKSub(str) then strPathPSDK = str
      end if
   Next

   ' The tools location (post).
   if (strPathPSDK = "") And (g_blnInternalFirst = False) then
      str = g_strPathDev & "/win.x86/sdk/200604"
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   if (strPathPSDK = "") And (g_blnInternalFirst = False) then
      str = g_strPathDev & "/win.x86/sdk/200504"
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   if (strPathPSDK = "") And (g_blnInternalFirst = False) then
      str = g_strPathDev & "/win.x86/sdk/200209"
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   ' Give up.
   if strPathPSDK = "" then
      MsgError "Cannot find a suitable Platform SDK. Check configure.log and the build requirements."
      exit sub
   end if

   '
   ' Emit the config.
   '
   strPathPSDK = UnixSlashes(PathAbs(strPathPSDK))
   CfgPrint "PATH_SDK_WINPSDK      := " & strPathPSDK
   CfgPrint "PATH_SDK_WINPSDKINCS   = $(PATH_SDK_WINPSDK)"
   CfgPrint "PATH_SDK_WIN32SDK      = $(PATH_SDK_WINPSDK)"
   CfgPrint "PATH_SDK_WIN64SDK      = $(PATH_SDK_WINPSDK)"

   PrintResult "Windows Platform SDK", strPathPSDK
   g_strPathPSDK = strPathPSDK
end sub

''
' Checks if the specified path points to a usable PSDK.
function CheckForPlatformSDKSub(strPathPSDK)
   CheckForPlatformSDKSub = False
   LogPrint "trying: strPathPSDK=" & strPathPSDK
   if    LogFileExists(strPathPSDK, "include/Windows.h") _
    And  LogFileExists(strPathPSDK, "lib/Kernel32.Lib") _
    And  LogFileExists(strPathPSDK, "lib/User32.Lib") _
      then
      CheckForPlatformSDKSub = True
   end if
end function


''
' Checks for a Windows 2003 DDK that works with the compiler intrinsics.
sub CheckForWin2k3DDK(strOptDDK)
   dim strPathDDK, str, strSubKeys
   PrintHdr "Windows 2003 DDK, build 3790 or later"

   '
   ' Find the DDK.
   '
   strPathDDK = ""
   ' The specified path.
   if (strPathDDK = "") And (strOptDDK <> "") then
      if CheckForWin2k3DDKSub(strOptDDK, True) then strPathDDK = strOptDDK
   end if

   ' The tools location (first).
   if (strPathDDK = "") And (g_blnInternalFirst = True) then
      str = g_strPathDev & "/win.x86/ddkwin2k3/200503"
      if CheckForWin2k3DDKSub(str, False) then strPathDDK = str
   end if

   if (strPathDDK = "") And (g_blnInternalFirst = True) then
      str = g_strPathDev & "/win.x86/ddkwin2k3/2004"
      if CheckForWin2k3DDKSub(str, False) then strPathDDK = str
   end if

   ' Check the environment
   str = EnvGet("DDK_INC_PATH")
   if (strPathDDK = "") And (str <> "") then
      str = PathParent(PathParent(str))
      if CheckForWin2k3DDKSub(str, True) then strPathDDK = str
   end if

   str = EnvGet("BASEDIR")
   if (strPathDDK = "") And (str <> "") then
      if CheckForWin2k3DDKSub(str, True) then strPathDDK = str
   end if

   ' Check the registry next. (the first pair is for vista (WDK), the second for pre-vista (DDK))
   arrSubKeys = RegEnumSubKeys("HKLM", "SOFTWARE\Microsoft\WINDDK") '' @todo Need some sorting stuff here.
   for Each strSubKey In arrSubKeys
      str = RegGetString("HKLM\SOFTWARE\Microsoft\WINDDK\" & strSubKey & "\Setup\BUILD")
      if (strPathDDK = "") And (str <> "") then
         if CheckForWin2k3DDKSub(str, False) then strPathDDK = str
      end if
   Next
   arrSubKeys = RegEnumSubKeys("HKCU", "SOFTWARE\Microsoft\WINDDK") '' @todo Need some sorting stuff here.
   for Each strSubKey In arrSubKeys
      str = RegGetString("HKCU\SOFTWARE\Microsoft\WINDDK\" & strSubKey & "\Setup\BUILD")
      if (strPathDDK = "") And (str <> "") then
         if CheckForWin2k3DDKSub(str, False) then strPathDDK = str
      end if
   Next

   arrSubKeys = RegEnumSubKeys("HKLM", "SOFTWARE\Microsoft\WINDDK") '' @todo Need some sorting stuff here.
   for Each strSubKey In arrSubKeys
      str = RegGetString("HKLM\SOFTWARE\Microsoft\WINDDK\" & strSubKey & "\SFNDirectory")
      if (strPathDDK = "") And (str <> "") then
         if CheckForWin2k3DDKSub(str, False) then strPathDDK = str
      end if
   Next
   arrSubKeys = RegEnumSubKeys("HKCU", "SOFTWARE\Microsoft\WINDDK") '' @todo Need some sorting stuff here.
   for Each strSubKey In arrSubKeys
      str = RegGetString("HKCU\SOFTWARE\Microsoft\WINDDK\" & strSubKey & "\SFNDirectory")
      if (strPathDDK = "") And (str <> "") then
         if CheckForWin2k3DDKSub(str, False) then strPathDDK = str
      end if
   Next

   ' The tools location (post).
   if (strPathDDK = "") And (g_blnInternalFirst = False) then
      str = g_strPathDev & "/win.x86/ddkwin2k3/200503"
      if CheckForWin2k3DDKSub(str, False) then strPathDDK = str
   end if

   if (strPathDDK = "") And (g_blnInternalFirst = False) then
      str = g_strPathDev & "/win.x86/ddkwin2k3/2004"
      if CheckForWin2k3DDKSub(str, False) then strPathDDK = str
   end if

   ' Give up.
   if strPathDDK = "" then
      MsgError "Cannot find a suitable Windows 2003 DDK. Check configure.log and the build requirements."
      exit sub
   end if

   '
   ' Emit the config.
   '
   strPathDDK = UnixSlashes(PathAbs(strPathDDK))
   CfgPrint "PATH_SDK_W2K3DDK      := " & strPathDDK
   CfgPrint "PATH_SDK_W2K3DDKX86    = $(PATH_SDK_W2K3DDK)"
   CfgPrint "PATH_SDK_W2K3DDKAMD64  = $(PATH_SDK_W2K3DDK)"

   PrintResult "Windows 2003 DDK", strPathDDK
   g_strPathDDK = strPathDDK
end sub

'' Quick check if the DDK is in the specified directory or not.
function CheckForWin2k3DDKSub(strPathDDK, blnCheckBuild)
   CheckForWin2k3DDKSub = False
   LogPrint "trying: strPathDDK=" & strPathDDK & " blnCheckBuild=" & blnCheckBuild
   '' @todo vista: if   (   LogFileExists(strPathDDK, "inc/ddk/wnet/ntdef.h") _
   '      Or LogFileExists(strPathDDK, "inc/api/ntdef.h")) _
   if   LogFileExists(strPathDDK, "inc/ddk/wnet/ntdef.h") _
    And LogFileExists(strPathDDK, "lib/wnet/i386/int64.lib") _
      then
      '' @todo figure out a way we can verify the version/build!
      CheckForWin2k3DDKSub = True
   end if
end function


''
' Finds midl.exe
sub CheckForMidl()
   dim strMidl
   PrintHdr "Midl.exe"

   ' Skip if no COM/ATL.
   if g_blnDisableCOM then
      PrintResult "Midl", "Skipped (" & g_strDisableCOM & ")"
      exit sub
   end if

   if LogFileExists(g_strPathPSDK, "bin/Midl.exe") then
      strMidl = g_strPathPSDK & "/bin/Midl.exe"
   elseif LogFileExists(g_strPathVCC, "Common7/Tools/Bin/Midl.exe") then
      strMidl = g_strPathVCC & "/Common7/Tools/Bin/Midl.exe"
   elseif LogFileExists(g_strPathDDK, "bin/x86/Midl.exe") then
      strMidl = g_strPathDDK & "/bin/x86/Midl.exe"
   elseif LogFileExists(g_strPathDDK, "bin/Midl.exe") then
      strMidl = g_strPathDDK & "/bin/Midl.exe"
   elseif LogFileExists(g_strPathDev, "win.x86/bin/Midl.exe") then
      strMidl = g_strPathDev & "/win.x86/bin/Midl.exe"
   else
      MsgWarning "Midl.exe not found!"
      exit sub
   end if

   CfgPrint "MAIN_IDL = " & strMidl
   PrintResult "Midl.exe", strMidl
end sub


''
' Checks for a recent DirectX SDK.
sub CheckForDirectXSDK(strOptDXSDK)
   dim strPathDXSDK, str, arrSubKeys, arrSubKeys2, strKey, strKey2
   PrintHdr "Direct X SDK"

   '
   ' Find the DX SDK.
   '
   strPathDXSDK = ""
   ' The specified path.
   if (strPathDXSDK = "") And (strOptDXSDK <> "") then
      if CheckForDirectXSDKSub(strOptDXSDK) then strPathDXSDK = strOptDXSDK
   end if

   ' The tools location (first).
   if (strPathDXSDK = "") And (g_blnInternalFirst = True) then
      str = g_strPathDev & "/win.x86/dxsdk/200610"
      if CheckForDirectXSDKSub(str) then strPathDXSDK = str
   end if

   ' Check the installer registry (sucks a bit).
   arrSubKeys = RegEnumSubKeys("HKLM", "SOFTWARE\Microsoft\Windows\CurrentVersion\Installer\UserData")
   for Each strSubKey In arrSubKeys
      strKey = "SOFTWARE\Microsoft\Windows\CurrentVersion\Installer\UserData\" & strSubKey & "\Products"
      arrSubKeys2 = RegEnumSubKeys("HKLM", strKey)
      for Each strSubKey2 In arrSubKeys2
         strKey2 = "HKLM\" & strKey & "\" & strSubKey2 & "\InstallProperties"
         str = RegGetString(strKey2 & "\DisplayName")
         if InStr(1, str, "Microsoft DirectX SDK") > 0 then
            str = RegGetString(strKey2 & "\InstallLocation")
            if (str <> "") And (strPathDXSDK = "") then
               if CheckForDirectXSDKSub(str) then
                  strPathDXSDK = str
                  Exit For
               end if
            end if
         end if
      Next
   Next

   ' The tools location (post).
   if (strPathDXSDK = "") And (g_blnInternalFirst = False) then
      str = g_strPathDev & "/win.x86/dxsdk/200610"
      if CheckForDirectXSDKSub(str) then strPathDXSDK = str
   end if

   ' Give up.
   if strPathDXSDK = "" then
      MsgError "Cannot find a suitable Direct X SDK. Check configure.log and the build requirements."
      exit sub
   end if

   '
   ' Emit the config.
   '
   strPathDXSDK = UnixSlashes(PathAbs(strPathDXSDK))
   CfgPrint "PATH_SDK_DXSDK        := " & strPathDXSDK
   CfgPrint "PATH_SDK_DXSDKX86      = $(PATH_SDK_DXSDK)"
   CfgPrint "PATH_SDK_DXSDKAMD64    = $(PATH_SDK_DXSDK)"

   PrintResult "Direct X SDK", strPathDXSDK
end sub

'' Quick check if the DXSDK is in the specified directory or not.
function CheckForDirectXSDKSub(strPathDXSDK)
   CheckForDirectXSDKSub = False
   LogPrint "trying: strPathDXSDK=" & strPathDXSDK
   if LogFileExists(strPathDXSDK, "Lib/x86/dxguid.lib") _
      then
      '' @todo figure out a way we can verify the version/build!
      CheckForDirectXSDKSub = True
   end if
end function


''
' Checks for a MingW32 suitable for building the recompiler.
'
' strOptW32API is currently ignored.
'
sub CheckForMingW(strOptMingw, strOptW32API)
   dim strPathMingW, strPathW32API, str
   PrintHdr "MinGW GCC v3.3.x + Binutils + Runtime + W32API"

   '
   ' Find the MinGW and W32API tools.
   '
   strPathMingW = ""
   strPathW32API = ""

   ' The specified path.
   if (strPathMingW = "") And (strOptMingW <> "") then
      if CheckForMingWSub(strOptMingW, strOptW32API) then
         strPathMingW = strOptMingW
         strPathW32API = strOptW32API
      end if
   end if

   ' The tools location (first).
   if (strPathMingW = "") And (g_blnInternalFirst = True) then
      str = g_strPathDev & "/win.x86/mingw32/v3.3.3"
      str2 = g_strPathDev & "/win.x86/w32api/v2.5"
      if CheckForMingWSub(str, str2) then
         strPathMingW = str
         strPathW32API = str2
      end if
   end if

   ' See if there is any gcc around.
   if strPathMingW = "" then
      str = Which("mingw32-gcc.exe")
      if (str <> "") then
         str = PathParent(PathStripFilename(str))
         if CheckForMingWSub(str, str) then strPathMingW = str
      end if
   end if

   if strPathMingW = "" then
      str = Which("gcc.exe")
      if (str <> "") then
         str = PathParent(PathStripFilename(str))
         if CheckForMingWSub(str, str) then strPathMingW = str
      end if
   end if

   ' The tools location (post).
   if (strPathMingW = "") And (g_blnInternalFirst = False) then
      str = g_strPathDev & "/win.x86/mingw32/v3.3.3"
      str2 = g_strPathDev & "/win.x86/w32api/v2.5"
      if CheckForMingWSub(str, str2) then
         strPathMingW = str
         strPathW32API = str2
      end if
   end if

   ' Success?
   if strPathMingW = "" then
      if strOptMingw = "" then
         MsgError "Can't locate a suitable MinGW installation. Try specify the path with " _
            & "the --with-MinGW=<path> argument. If still no luck, consult the configure.log and the build requirements."
      else
         MsgError "Can't locate a suitable MinGW installation. Please consult the configure.log and the build requirements."
      end if
      exit sub
   end if

   '
   ' Emit the config.
   '
   strPathMingW = UnixSlashes(PathAbs(strPathMingW))
   CfgPrint "PATH_TOOL_MINGW32     := " & strPathMingW
   PrintResult "MinGW (GCC v" & g_strSubOutput & ")", strPathMingW
   if (strPathMingW = strPathW32API) Or strPathW32API = "" then
      CfgPrint "PATH_SDK_W32API        = $(PATH_TOOL_MINGW32)"
   else
      CfgPrint "PATH_SDK_W32API        = " & strPathW32API
      PrintResult "W32API", strPathW32API
   end if
end sub

''
' Checks if the specified path points to an usable MinGW or not.
function CheckForMingWSub(strPathMingW, strPathW32API)
   g_strSubOutput = ""
   if strPathW32API = "" then strPathW32API = strPathMingW
   LogPrint "trying: strPathMingW="  &strPathMingW & " strPathW32API=" & strPathW32API

   if   LogFileExists(strPathMingW, "bin/mingw32-gcc.exe") _
    And LogFileExists(strPathMingW, "bin/ld.exe") _
    And LogFileExists(strPathMingW, "bin/objdump.exe") _
    And LogFileExists(strPathMingW, "bin/dllwrap.exe") _
    And LogFileExists(strPathMingW, "bin/as.exe") _
    And LogFileExists(strPathMingW, "include/string.h") _
    And LogFileExists(strPathMingW, "include/_mingw.h") _
    And LogFileExists(strPathMingW, "lib/dllcrt1.o") _
    And LogFileExists(strPathMingW, "lib/dllcrt2.o") _
    And LogFileExists(strPathMingW, "lib/libmsvcrt.a") _
    _
    And LogFileExists(strPathW32API, "lib/libkernel32.a") _
    And LogFileExists(strPathW32API, "include/windows.h") _
      then
      if Shell(DosSlashes(strPathMingW & "/bin/gcc.exe") & " --version", True) = 0 then
         dim offVer, iMajor, iMinor, iPatch, strVer

         ' extract the version.
         strVer = ""
         offVer = InStr(1, g_strShellOutput, "(GCC) ")
         if offVer > 0 then
            strVer = LTrim(Mid(g_strShellOutput, offVer + Len("(GCC) ")))
            strVer = RTrim(Left(strVer, InStr(1, strVer, " ")))
            if   (Mid(strVer, 2, 1) = ".") _
             And (Mid(strVer, 4, 1) = ".") then
               iMajor = Int(Left(strVer, 1)) ' Is Int() the right thing here? I want atoi()!!!
               iMinor = Int(Mid(strVer, 3, 1))
               iPatch = Int(Mid(strVer, 5))
            else
               LogPrint "Malformed version: '" & strVer & "'"
               strVer = ""
            end if
         end if
         if strVer <> "" then
            if (iMajor = 3) And (iMinor = 3) then
               CheckForMingWSub = True
               g_strSubOutput = strVer
            else
               LogPrint "MinGW version '" & iMajor & "." & iMinor & "." & iPatch & "' is not supported (or configure.vbs failed to parse it correctly)."
            end if
         else
            LogPrint "Couldn't locate the GCC version in the output!"
         end if

      else
         LogPrint "Failed to run gcc.exe!"
      end if
   end if
end function


''
' Checks for any libSDL binaries.
sub CheckForlibSDL(strOptlibSDL)
   dim strPathlibSDL, str
   PrintHdr "libSDL"

   '
   ' Try find some SDL library.
   '

   ' First, the specific location.
   strPathlibSDL = ""
   if (strPathlibSDL = "") And (strOptlibSDL <> "") then
      if CheckForlibSDLSub(strOptlibSDL) then strPathlibSDL = strOptlibSDL
   end if

   ' The tools location (first).
   if (strPathlibSDL = "") And (g_blnInternalFirst = True) Then
      str = g_strPathDev & "/win.x86/libsdl/v1.2.11"
      if CheckForlibSDLSub(str) then strPathlibSDL = str
   end if

   if (strPathlibSDL = "") And (g_blnInternalFirst = True) Then
      str = g_strPathDev & "/win.x86/libsdl/v1.2.7-InnoTek"
      if CheckForlibSDLSub(str) then strPathlibSDL = str
   end if

   ' Poke about in the path.
   str = Which("SDLmain.lib")
   if (strPathlibSDL = "") And (str <> "") Then
      str = PathParent(PathStripFilename(str))
      if CheckForlibSDLSub(str) then strPathlibSDL = str
   end if

   str = Which("SDL.dll")
   if (strPathlibSDL = "") And (str <> "") Then
      str = PathParent(PathStripFilename(str))
      if CheckForlibSDLSub(str) then strPathlibSDL = str
   end if

   ' The tools location (post).
   if (strPathlibSDL = "") And (g_blnInternalFirst = False) Then
      str = g_strPathDev & "/win.x86/libsdl/v1.2.11"
      if CheckForlibSDLSub(str) then strPathlibSDL = str
   end if

   if (strPathlibSDL = "") And (g_blnInternalFirst = False) Then
      str = g_strPathDev & "/win.x86/libsdl/v1.2.7-InnoTek"
      if CheckForlibSDLSub(str) then strPathlibSDL = str
   end if

   ' Success?
   if strPathlibSDL = "" then
      if strOptlibSDL = "" then
         MsgError "Can't locate libSDL. Try specify the path with the --with-libSDL=<path> argument. " _
                & "If still no luck, consult the configure.log and the build requirements."
      else
         MsgError "Can't locate libSDL. Please consult the configure.log and the build requirements."
      end if
      exit sub
   end if

   strPathLibSDL = UnixSlashes(PathAbs(strPathLibSDL))
   CfgPrint "PATH_SDK_LIBSDL       := " & strPathlibSDL

   PrintResult "libSDL", strPathlibSDL
end sub

''
' Checks if the specified path points to an usable libSDL or not.
function CheckForlibSDLSub(strPathlibSDL)
   CheckForlibSDLSub = False
   LogPrint "trying: strPathlibSDL=" & strPathlibSDL
   if   LogFileExists(strPathlibSDL, "lib/SDL.lib") _
    And LogFileExists(strPathlibSDL, "lib/SDLmain.lib") _
    And LogFileExists(strPathlibSDL, "lib/SDL.dll") _
    And LogFileExists(strPathlibSDL, "include/SDL.h") _
    And LogFileExists(strPathlibSDL, "include/SDL_syswm.h") _
    And LogFileExists(strPathlibSDL, "include/SDL_version.h") _
      then
      CheckForlibSDLSub = True
   end if
end function


''
' Checks for libxml2.
sub CheckForXml2(strOptXml2)
   dim strPathXml2, str
   PrintHdr "libxml2"

   ' Skip if no COM/ATL.
   if g_blnDisableCOM then
      PrintResult "libxml2", "Skipped (" & g_strDisableCOM & ")"
      exit sub
   end if

   '
   ' Try find some xml2 dll/lib.
   '
   strPathXml2 = ""
   if (strPathXml2 = "") And (strOptXml2 <> "") then
      if CheckForXml2Sub(strOptXml2) then strPathXml2 = strOptXml2
   end if

   if strPathXml2 = "" Then
      str = Which("libxml2.lib")
      if str <> "" Then
         str = PathParent(PathStripFilename(str))
         if CheckForXml2Sub(str) then strPathXml2 = str
      end if
   end if

   ' Ignore failure if we're in 'internal' mode.
   if (strPathXml2 = "") and g_blnInternalMode then
      PrintResult "libxml2", "ignored (internal mode)"
      exit sub
   end if

   ' Success?
   if strPathXml2 = "" then
      if strOptXml2 = "" then
         MsgError "Can't locate libxml2. Try specify the path with the --with-libxml2=<path> argument. " _
                & "If still no luck, consult the configure.log and the build requirements."
      else
         MsgError "Can't locate libxml2. Please consult the configure.log and the build requirements."
      end if
      exit sub
   end if

   strPathXml2 = UnixSlashes(PathAbs(strPathXml2))
   CfgPrint "SDK_VBOX_LIBXML2_INCS  := " & strPathXml2 & "/include"
   CfgPrint "SDK_VBOX_LIBXML2_LIBS  := " & strPathXml2 & "/lib/libxml2.lib"

   PrintResult "libxml2", strPathXml2
end sub

''
' Checks if the specified path points to an usable libxml2 or not.
function CheckForXml2Sub(strPathXml2)
   dim str

   CheckForXml2Sub = False
   LogPrint "trying: strPathXml2=" & strPathXml2
   if   LogFileExists(strPathXml2, "include/libxml/xmlexports.h") _
    And LogFileExists(strPathXml2, "include/libxml/xmlreader.h") _
      then
      str = LogFindFile(strPathXml2, "bin/libxml2.dll")
      if str <> "" then
         if LogFindFile(strPathXml2, "lib/libxml2.lib") <> "" then
            CheckForXml2Sub = True
         end if
      end if
   end if
end function


''
' Checks for libxslt.
sub CheckForXslt(strOptXslt)
   dim strPathXslt, str
   PrintHdr "libxslt"

   ' Skip if no COM/ATL.
   if g_blnDisableCOM then
      PrintResult "libxslt", "Skipped (" & g_strDisableCOM & ")"
      exit sub
   end if

   '
   ' Try find some libxslt dll/lib.
   '
   strPathXslt = ""
   if (strPathXslt = "") And (strOptXslt <> "") then
      if CheckForXsltSub(strOptXslt) then strPathXslt = strOptXslt
   end if

   if strPathXslt = "" Then
      str = Which("libxslt.lib")
      if str <> "" Then
         str = PathParent(PathStripFilename(str))
         if CheckForXsltSub(str) then strPathXslt = str
      end if
   end if

   if strPathXslt = "" Then
      str = Which("libxslt.dll")
      if str <> "" Then
         str = PathParent(PathStripFilename(str))
         if CheckForXsltSub(str) then strPathXslt = str
      end if
   end if

   ' Ignore failure if we're in 'internal' mode.
   if (strPathXslt = "") and g_blnInternalMode then
      PrintResult "libxslt", "ignored (internal mode)"
      exit sub
   end if

   ' Success?
   if strPathXslt = "" then
      if strOptXslt = "" then
         MsgError "Can't locate libxslt. Try specify the path with the --with-libxslt=<path> argument. " _
                & "If still no luck, consult the configure.log and the build requirements."
      else
         MsgError "Can't locate libxslt. Please consult the configure.log and the build requirements."
      end if
      exit sub
   end if

   strPathXslt = UnixSlashes(PathAbs(strPathXslt))
   CfgPrint "SDK_VBOX_LIBXSLT_INCS   := " & strPathXslt & "/include"
   CfgPrint "SDK_VBOX_LIBXSLT_LIBS   := " & strPathXslt & "/lib/libxslt.lib"

   PrintResult "libxslt", strPathXslt
end sub


''
' Checks if the specified path points to an usable libxslt or not.
function CheckForXsltSub(strPathXslt)
   dim str

   CheckForXsltSub = False
   LogPrint "trying: strPathXslt=" & strPathXslt

   if   LogFileExists(strPathXslt, "include/libxslt/namespaces.h") _
    And LogFileExists(strPathXslt, "include/libxslt/xsltutils.h") _
      then
      str = LogFindFile(strPathXslt, "lib/libxslt.dll")
      if str <> "" then
         if   LogFileExists(strPathXslt, "lib/libxslt.lib") _
            then
            CheckForXsltSub = True
         end if
      end if
   end if
end function


''
''
' Checks for any Qt4 binaries.
sub CheckForQt4(strOptQt4)
   dim strPathQt4 

   PrintHdr "Qt4"

   '
   ' Try to find the Qt4 installation (user specified path with --with-qt4)
   '
   strPathQt4 = ""

   LogPrint "Checking for user specified path of Qt4 ... "
   if (strPathQt4 = "") And (strOptQt4 <> "") then
      strOptQt4 = UnixSlashes(strOptQt4)
      if CheckForQt4Sub(strOptQt4) then strPathQt4 = strOptQt4
   end if

   if strPathQt4 = "" then
      CfgPrint "VBOX_WITH_QT4GUI="
      PrintResult "Qt4", "not found"
   else
      CfgPrint "PATH_SDK_QT4          := " & strPathQt4
      CfgPrint "PATH_TOOL_QT4          = $(PATH_SDK_QT4)"
      CfgPrint "VBOX_PATH_QT4          = $(PATH_SDK_QT4)"
      PrintResult "Qt4 ", strPathQt4
   end if
end sub


'
'
function CheckForQt4Sub(strPathQt4)

   CheckForQt4Sub = False
   LogPrint "trying: strPathQt4=" & strPathQt4

   if   LogFileExists(strPathQt4, "bin/moc.exe") _
    And LogFileExists(strPathQt4, "bin/uic.exe") _
    And LogFileExists(strPathQt4, "include/Qt/qwidget.h") _
    And LogFileExists(strPathQt4, "include/QtGui/QApplication") _
    And LogFileExists(strPathQt4, "include/QtNetwork/QHostAddress") _
    And (   LogFileExists(strPathQt4, "lib/QtCore4.lib") _
         Or LogFileExists(strPathQt4, "lib/VBoxQtCore4.lib")) _
    And (   LogFileExists(strPathQt4, "lib/QtNetwork4.lib") _
         Or LogFileExists(strPathQt4, "lib/VBoxQtNetwork4.lib")) _
      then
         CheckForQt4Sub = True
   end if

end function


''
' Show usage.
sub usage
   Print "Usage: cscript configure.vbs [options]"
   Print ""
   Print "Configuration:"
   Print "  -h, --help"
   Print "  --internal"
   Print "  --internal-last"
   Print ""
   Print "Components:"
   Print "  --disable-COM"
   Print ""
   Print "Locations:"
   Print "  --with-DDK=PATH       "
   Print "  --with-DXSDK=PATH     "
   Print "  --with-kBuild=PATH    "
   Print "  --with-libSDL=PATH    "
   Print "  --with-MinGW=PATH     "
   Print "  --with-Qt4=PATH       "
   Print "  --with-SDK=PATH       "
   Print "  --with-VC=PATH        "
   Print "  --with-VC-Common=PATH "
   Print "  --with-VC-Express-Edition"
   Print "  --with-W32API=PATH    "
   Print "  --with-libxml2=PATH   "
   Print "  --with-libxslt=PATH   "
end sub


''
' The main() like function.
'
Sub Main
   '
   ' Write the log header and check that we're not using wscript.
   '
   LogInit
   If UCase(Right(Wscript.FullName, 11)) = "WSCRIPT.EXE" Then
      Wscript.Echo "This script must be run under CScript."
      Wscript.Quit(1)
   End If

   '
   ' Parse arguments.
   '
   strOptDDK = ""
   strOptDXDDK = ""
   strOptkBuild = ""
   strOptlibSDL = ""
   strOptMingW = ""
   strOptQt4 = ""
   strOptSDK = ""
   strOptVC = ""
   strOptVCCommon = ""
   blnOptVCExpressEdition = False
   strOptW32API = ""
   strOptXml2 = ""
   strOptXslt = ""
   blnOptDisableCOM = False
   for i = 1 to Wscript.Arguments.Count
      dim str, strArg, strPath

      ' Separate argument and path value
      str = Wscript.Arguments.item(i - 1)
      if InStr(1, str, "=") > 0 then
         strArg = Mid(str, 1, InStr(1, str, "=") - 1)
         strPath = Mid(str, InStr(1, str, "=") + 1)
         if strPath = "" then MsgFatal "Syntax error! Argument #" & i & " is missing the path."
      else
         strArg = str
         strPath = ""
      end if

      ' Process the argument
      select case LCase(strArg)
         case "--with-ddk"
            strOptDDK = strPath
         case "--with-dxsdk"
            strOptDXSDK = strPath
         case "--with-kbuild"
            strOptkBuild = strPath
         case "--with-libsdl"
            strOptlibSDL = strPath
         case "--with-mingw"
            strOptMingW = strPath
         case "--with-qt4"
            strOptQt4 = strPath
         case "--with-sdk"
            strOptSDK = strPath
         case "--with-vc"
            strOptVC = strPath
         case "--with-vc-common"
            strOptVCCommon = strPath
         case "--with-vc-express-edition"
            blnOptVCExpressEdition = True
         case "--with-w32api"
            strOptW32API = strPath
         case "--with-libxml2"
            strOptXml2 = strPath
         case "--with-libxslt"
            strOptXslt = strPath
         case "--disable-com"
            blnOptDisableCOM = True
         case "--enable-com"
            blnOptDisableCOM = False
         case "--internal"
            g_blnInternalMode = True
         case "--internal-last"
            g_blnInternalFirst = False
         case "-h", "--help", "-?"
            usage
            Wscript.Quit(0)
         case else
            Wscript.echo "syntax error: Unknown option '" & str &"'."
            usage
            Wscript.Quit(1)
      end select
   next

   '
   ' Initialize output files.
   '
   CfgInit
   EnvInit

   '
   ' Check that the Shell function is sane.
   '
   g_objShell.Environment("PROCESS")("TESTING_ENVIRONMENT_INHERITANCE") = "This works"
   if Shell("set TESTING_ENVIRONMENT_INHERITANC", False) <> 0 then ' The 'E' is missing on purpose (4nt).
      MsgFatal "shell execution test failed!"
   end if
   if g_strShellOutput <> "TESTING_ENVIRONMENT_INHERITANCE=This works" & CHR(13) & CHR(10)  then
      MsgFatal "shell inheritance or shell execution isn't working right."
   end if
   g_objShell.Environment("PROCESS")("TESTING_ENVIRONMENT_INHERITANCE") = ""
   Print "Shell inheritance test: OK"

   '
   ' Do the checks.
   '
   if blnOptDisableCOM = True then
      DisableCOM "--disable-com"
   end if
   CheckSourcePath
   CheckForkBuild strOptkBuild
   CheckForVisualCPP strOptVC, strOptVCCommon, blnOptVCExpressEdition
   CheckForPlatformSDK strOptSDK
   CheckForWin2k3DDK strOptDDK
   CheckForMidl
   CheckForDirectXSDK strOptDXSDK
   CheckForMingW strOptMingw, strOptW32API
   CheckForlibSDL strOptlibSDL
   ' Don't check for these libraries by default as they are part of OSE
   ' Using external libs can add a dependency to iconv
   if (strOptXml2 <> "") then
      CheckForXml2 strOptXml2
   end if
   if (strOptXslt <> "") then
      CheckForXslt strOptXslt
   end if
   CheckForQt4 strOptQt4
   if g_blnInternalMode then
      EnvPrint "call " & g_strPathDev & "/env.cmd %1 %2 %3 %4 %5 %6 %7 %8 %9"
   end if

End Sub


Main

