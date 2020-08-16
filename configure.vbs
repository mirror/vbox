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
' Copyright (C) 2006-2020 Oracle Corporation
'
' This file is part of VirtualBox Open Source Edition (OSE), as
' available from http://www.virtualbox.org. This file is free software;
' you can redistribute it and/or modify it under the terms of the GNU
' General Public License (GPL) as published by the Free Software
' Foundation, in version 2 as it comes in the "COPYING" file of the
' VirtualBox OSE distribution. VirtualBox OSE is distributed in the
' hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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

' kBuild stuff.
dim g_strPathkBuild, g_strPathkBuildBin, g_strPathDev
g_strPathkBuild = ""
g_strPathkBuildBin = ""
g_strPathDev = ""

dim g_strTargetArch, g_StrTargetArchWin
g_strTargetArch = ""
g_StrTargetArchWin = ""

dim g_strHostArch, g_strHostArchWin
g_strHostArch = ""
g_strHostArchWin = ""

' Visual C++ info.
dim g_strPathVCC, g_strVCCVersion
g_strPathVCC = ""
g_strVCCVersion = ""

' SDK and DDK.
dim g_strPathSDK10, g_strPathPSDK, g_strVerPSDK, g_strPathDDK
g_strPathSDK10 = ""
g_strPathPSDK = ""
g_strVerPSDK = ""
g_strPathDDK = ""

' COM disabling.
dim g_blnDisableCOM, g_strDisableCOM
g_blnDisableCOM = False
g_strDisableCOM = ""

' Whether to ignore (continue) on errors.
dim g_blnContinueOnError, g_rcExit
g_blnContinueOnError = False

' The script's exit code (for ignored errors).
dim g_rcScript
g_rcScript = 0

' Whether to try the internal stuff first or last.
dim g_blnInternalFirst
g_blnInternalFirst = True

' List of program files locations.
dim g_arrProgramFiles
if EnvGet("ProgramFiles(x86)") <> "" then
   g_arrProgramFiles = Array(EnvGet("ProgramFiles"), EnvGet("ProgramFiles(x86)"))
else
   g_arrProgramFiles = Array(EnvGet("ProgramFiles"))
end if

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
' Returns a reverse sorted array (strings).
function ArraySortStrings(arrStrings)
   for i = LBound(arrStrings) to UBound(arrStrings)
      str1 = arrStrings(i)
      for j = i + 1 to UBound(arrStrings)
         str2 = arrStrings(j)
         if StrComp(str2, str1) < 0 then
            arrStrings(j) = str1
            str1 = str2
         end if
      next
      arrStrings(i) = str1
   next
   ArraySortStrings = arrStrings
end function


''
' Prints a string array.
sub ArrayPrintStrings(arrStrings, strPrefix)
   for i = LBound(arrStrings) to UBound(arrStrings)
      Print strPrefix & "arrStrings(" & i & ") = '" & arrStrings(i) & "'"
   next
end sub


''
' Returns a reverse sorted array (strings).
function ArrayRSortStrings(arrStrings)
   ' Sort it.
   arrStrings = ArraySortStrings(arrStrings)

   ' Reverse the array.
   cnt = UBound(arrStrings) - LBound(arrStrings) + 1
   if cnt > 0 then
      j   = UBound(arrStrings)
      iHalf = Fix(LBound(arrStrings) + cnt / 2)
      for i = LBound(arrStrings) to iHalf - 1
         strTmp = arrStrings(i)
         arrStrings(i) = arrStrings(j)
         arrStrings(j) = strTmp
         j = j - 1
      next
   end if
   ArrayRSortStrings = arrStrings
end function


''
' Returns the input array with the string appended.
' Note! There must be some better way of doing this...
function ArrayAppend(arr, str)
   dim i, cnt
   cnt = UBound(arr) - LBound(arr) + 1
   redim arrRet(cnt)
   for i = LBound(arr) to UBound(arr)
      arrRet(i) = arr(i)
   next
   arrRet(UBound(arr) + 1) = str
   ArrayAppend = arrRet
end function


''
' Gets the SID of the current user.
function GetSid()
   dim objNet, strUser, strDomain, offSlash, objWmiUser
   GetSid = ""

   ' Figure the user + domain
   set objNet = CreateObject("WScript.Network")
   strUser   = objNet.UserName
   strDomain = objNet.UserDomain
   offSlash  = InStr(1, strUser, "\")
   if offSlash > 0 then
      strDomain = Left(strUser, offSlash - 1)
      strUser   = Right(strUser, Len(strUser) - offSlash)
   end if

   ' Lookup the user.
   on error resume next
   set objWmiUser = GetObject("winmgmts:{impersonationlevel=impersonate}!/root/cimv2:Win32_UserAccount." _
                              & "Domain='" & strDomain &"',Name='" & strUser & "'")
   if err.number = 0 then
      GetSid = objWmiUser.SID
   end if
end function


''
' Translates a register root name to a value
' This will translate HKCU path to HKEY_USERS and fixing
function RegTransRoot(strRoot, ByRef sSubKeyName)
   const HKEY_LOCAL_MACHINE = &H80000002
   const HKEY_CURRENT_USER  = &H80000001
   const HKEY_USERS         = &H80000003

   select case strRoot
      case "HKLM"
         RegTransRoot = HKEY_LOCAL_MACHINE
      case "HKUS"
         RegTransRoot = HKEY_USERS
      case "HKCU"
         dim strCurrentSid
         strCurrentSid = GetSid()
         if strCurrentSid <> "" then
            sSubKeyName  = strCurrentSid & "\" & sSubKeyName
            RegTransRoot = HKEY_USERS
            'LogPrint "RegTransRoot: HKCU -> HKEY_USERS + " & sSubKeyName
         else
            RegTransRoot = HKEY_CURRENT_USER
            LogPrint "RegTransRoot: Warning! HKCU -> HKEY_USERS failed!"
         end if
      case else
         MsgFatal "RegTransRoot: Unknown root: '" & strRoot & "'"
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
      InParms.hDefKey     = RegTransRoot(strRoot, strKey)
      InParms.sSubKeyName = strKey
      InParms.sValueName  = strValue
      On Error Resume Next
      set OutParms = g_objReg.ExecMethod_("GetStringValue", InParms, , g_objRegCtx)
      if OutParms.ReturnValue = 0 then
         if OutParms.sValue <> Null then
            RegGetString = OutParms.sValue
         end if
      end if
   else
      ' fallback mode
      On Error Resume Next
      RegGetString = g_objShell.RegRead(strName)
   end if
end function


''
' Gets a multi string value from the registry. Returns array of strings if found, otherwise empty array().
function RegGetMultiString(strName)
   RegGetMultiString = Array()
   if RegInit() then
      dim strRoot, strKey, strValue
      dim iRoot

      ' split up into root, key and value parts.
      strRoot = left(strName, instr(strName, "\") - 1)
      strKey = mid(strName, instr(strName, "\") + 1, instrrev(strName, "\") - instr(strName, "\"))
      strValue = mid(strName, instrrev(strName, "\") + 1)

      ' Must use ExecMethod to call the GetStringValue method because of the context.
      Set InParms = g_objReg.Methods_("GetMultiStringValue").Inparameters
      InParms.hDefKey     = RegTransRoot(strRoot, strKey)
      InParms.sSubKeyName = strKey
      InParms.sValueName  = strValue
      On Error Resume Next
      set OutParms = g_objReg.ExecMethod_("GetMultiStringValue", InParms, , g_objRegCtx)
      if OutParms.ReturnValue = 0 then
         if OutParms.sValue <> Null then
            RegGetMultiString = OutParms.sValue
         end if
      end if
   else
      ' fallback mode
      On Error Resume Next
      RegGetMultiString = g_objShell.RegRead(strName)
   end if
end function


''
' Returns an array of subkey strings.
function RegEnumSubKeys(strRoot, ByVal strKeyPath)
   RegEnumSubKeys = Array()
   if RegInit() then
      ' Must use ExecMethod to call the EnumKey method because of the context.
      Set InParms = g_objReg.Methods_("EnumKey").Inparameters
      InParms.hDefKey     = RegTransRoot(strRoot, strKeyPath)
      InParms.sSubKeyName = strKeyPath
      On Error Resume Next
      set OutParms = g_objReg.ExecMethod_("EnumKey", InParms, , g_objRegCtx)
      'LogPrint "RegEnumSubKeys(" & Hex(InParms.hDefKey) & "," & InParms.sSubKeyName &") -> " & OutParms.GetText_(1)
      if OutParms.ReturnValue = 0 then
         if OutParms.sNames <> Null then
            RegEnumSubKeys = OutParms.sNames
         end if
      end if
   else
      ' fallback mode
      dim objReg, rc, arrSubKeys
      set objReg = GetObject("winmgmts:{impersonationLevel=impersonate}!\\.\root\default:StdRegProv")
      On Error Resume Next
      rc = objReg.EnumKey(RegTransRoot(strRoot, strKeyPath), strKeyPath, arrSubKeys)
      if rc = 0 then
         RegEnumSubKeys = arrSubKeys
      end if
   end if
end function


''
' Returns an array of full path subkey strings.
function RegEnumSubKeysFull(strRoot, strKeyPath)
   dim arrTmp
   arrTmp = RegEnumSubKeys(strRoot, strKeyPath)
   for i = LBound(arrTmp) to UBound(arrTmp)
      arrTmp(i) = strKeyPath & "\" & arrTmp(i)
   next
   RegEnumSubKeysFull = arrTmp
end function


''
' Returns an rsorted array of subkey strings.
function RegEnumSubKeysRSort(strRoot, strKeyPath)
   RegEnumSubKeysRSort = ArrayRSortStrings(RegEnumSubKeys(strRoot, strKeyPath))
end function


''
' Returns an rsorted array of subkey strings.
function RegEnumSubKeysFullRSort(strRoot, strKeyPath)
   RegEnumSubKeysFullRSort = ArrayRSortStrings(RegEnumSubKeysFull(strRoot, strKeyPath))
end function


''
' Returns an array of value name strings.
function RegEnumValueNames(strRoot, ByVal strKeyPath)
   RegEnumValueNames = Array()
   if RegInit() then
      ' Must use ExecMethod to call the EnumKey method because of the context.
      Set InParms = g_objReg.Methods_("EnumValues").Inparameters
      InParms.hDefKey     = RegTransRoot(strRoot, strKeyPath)
      InParms.sSubKeyName = strKeyPath
      On Error Resume Next
      set OutParms = g_objReg.ExecMethod_("EnumValues", InParms, , g_objRegCtx)
      'LogPrint "RegEnumValueNames(" & Hex(InParms.hDefKey) & "," & InParms.sSubKeyName &") -> " & OutParms.GetText_(1)
      if OutParms.ReturnValue = 0 then
         if OutParms.sNames <> Null then
            RegEnumValueNames = OutParms.sNames
         end if
      end if
   else
      ' fallback mode
      dim objReg, rc, arrSubKeys
      set objReg = GetObject("winmgmts:{impersonationLevel=impersonate}!\\.\root\default:StdRegProv")
      On Error Resume Next
      rc = objReg.EnumValues(RegTransRoot(strRoot, strKeyPath), strKeyPath, arrSubKeys)
      if rc = 0 then
         RegEnumValueNames = arrSubKeys
      end if
   end if
end function


''
' Returns an array of full path value name strings.
function RegEnumValueNamesFull(strRoot, strKeyPath)
   dim arrTmp
   arrTmp = RegEnumValueNames(strRoot, strKeyPath)
   for i = LBound(arrTmp) to UBound(arrTmp)
      arrTmp(i) = strKeyPath & "\" & arrTmp(i)
   next
   RegEnumValueNamesFull = arrTmp
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
' Gets the first non-empty environment variable of the given two.
function EnvGetFirst(strName1, strName2)
   EnvGetFirst = g_objShell.Environment("PROCESS")(strName1)
   if EnvGetFirst = "" then
      EnvGetFirst = g_objShell.Environment("PROCESS")(strName2)
   end if
end function


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
   strAbs    = g_objFileSys.GetAbsolutePathName(DosSlashes(str))
   strParent = g_objFileSys.GetParentFolderName(strAbs)
   if strParent = "" then
      PathAbs = strAbs
   else
      strParent = PathAbs(strParent)  ' Recurse to resolve parent paths.
      PathAbs   = g_objFileSys.BuildPath(strParent, g_objFileSys.GetFileName(strAbs))

      dim obj
      set obj = Nothing
      if FileExists(PathAbs) then
         set obj = g_objFileSys.GetFile(PathAbs)
      elseif DirExists(PathAbs) then
         set obj = g_objFileSys.GetFolder(PathAbs)
      end if

      if not (obj is nothing) then
         for each objSub in obj.ParentFolder.SubFolders
            if obj.Name = objSub.Name  or  obj.ShortName = objSub.ShortName then
               if  InStr(1, objSub.Name, " ") > 0 _
                Or InStr(1, objSub.Name, "&") > 0 _
                Or InStr(1, objSub.Name, "$") > 0 _
                  then
                  PathAbs = g_objFileSys.BuildPath(strParent, objSub.ShortName)
                  if  InStr(1, PathAbs, " ") > 0 _
                   Or InStr(1, PathAbs, "&") > 0 _
                   Or InStr(1, PathAbs, "$") > 0 _
                     then
                     MsgFatal "PathAbs(" & str & ") attempted to return filename with problematic " _
                      & "characters in it (" & PathAbs & "). The tool/sdk referenced will probably " _
                      & "need to be copied or reinstalled to a location without 'spaces', '$', ';' " _
                      & "or '&' in the path name. (Unless it's a problem with this script of course...)"
                  end if
               else
                  PathAbs = g_objFileSys.BuildPath(strParent, objSub.Name)
               end if
               exit for
            end if
         next
      end if
   end if
end function


''
' Get the abs path, use the long version.
function PathAbsLong(str)
   strAbs    = g_objFileSys.GetAbsolutePathName(DosSlashes(str))
   strParent = g_objFileSys.GetParentFolderName(strAbs)
   if strParent = "" then
      PathAbsLong = strAbs
   else
      strParent = PathAbsLong(strParent)  ' Recurse to resolve parent paths.
      PathAbsLong = g_objFileSys.BuildPath(strParent, g_objFileSys.GetFileName(strAbs))

      dim obj
      set obj = Nothing
      if FileExists(PathAbsLong) then
         set obj = g_objFileSys.GetFile(PathAbsLong)
      elseif DirExists(PathAbsLong) then
         set obj = g_objFileSys.GetFolder(PathAbsLong)
      end if

      if not (obj is nothing) then
         for each objSub in obj.ParentFolder.SubFolders
            if obj.Name = objSub.Name  or  obj.ShortName = objSub.ShortName then
               PathAbsLong = g_objFileSys.BuildPath(strParent, objSub.Name)
               exit for
            end if
         next
      end if
   end if
end function


''
' Returns true if there are subfolders starting with the given string.
function HasSubdirsStartingWith(strFolder, strStartingWith)
   HasSubdirsStartingWith = False
   if DirExists(strFolder) then
      dim obj
      set obj = g_objFileSys.GetFolder(strFolder)
      for each objSub in obj.SubFolders
         if StrComp(Left(objSub.Name, Len(strStartingWith)), strStartingWith) = 0 then
            HasSubdirsStartingWith = True
            LogPrint "# HasSubdirsStartingWith(" & strFolder & "," & strStartingWith & ") found " & objSub.Name
            exit for
         end if
      next
   end if
end function


''
' Returns a sorted array of subfolder names that starts with the given string.
function GetSubdirsStartingWith(strFolder, strStartingWith)
   if DirExists(strFolder) then
      dim obj, i
      set obj = g_objFileSys.GetFolder(strFolder)
      i = 0
      for each objSub in obj.SubFolders
         if StrComp(Left(objSub.Name, Len(strStartingWith)), strStartingWith) = 0 then
            i = i + 1
         end if
      next
      if i > 0 then
         redim arrResult(i - 1)
         i = 0
         for each objSub in obj.SubFolders
            if StrComp(Left(objSub.Name, Len(strStartingWith)), strStartingWith) = 0 then
               arrResult(i) = objSub.Name
               i = i + 1
            end if
         next
         GetSubdirsStartingWith = arrResult
      else
         GetSubdirsStartingWith = Array()
      end if
   else
      GetSubdirsStartingWith = Array()
   end if
end function


''
' Returns a sorted array of subfolder names that starts with the given string.
function GetSubdirsStartingWithSorted(strFolder, strStartingWith)
   GetSubdirsStartingWithSorted = ArraySortStrings(GetSubdirsStartingWith(strFolder, strStartingWith))
end function


''
' Returns a reverse sorted array of subfolder names that starts with the given string.
function GetSubdirsStartingWithRSorted(strFolder, strStartingWith)
   GetSubdirsStartingWithRSorted = ArrayRSortStrings(GetSubdirsStartingWith(strFolder, strStartingWith))
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
' Try find the specified file in the specified path variable.
function WhichEx(strEnvVar, strFile)
   dim strPath, iStart, iEnd, str

   ' the path
   strPath = EnvGet(strEnvVar)
   iStart = 1
   do while iStart <= Len(strPath)
      iEnd = InStr(iStart, strPath, ";")
      if iEnd <= 0 then iEnd = Len(strPath) + 1
      if iEnd > iStart then
         str = Mid(strPath, iStart, iEnd - iStart) & "/" & strFile
         if FileExists(str) then
            WhichEx = str
            exit function
         end if
      end if
      iStart = iEnd + 1
   loop

   ' registry or somewhere?

   WhichEx = ""
end function


''
' Try find the specified file in the path.
function Which(strFile)
   Which = WhichEx("Path", strFile)
end function


''
' Right pads a string with spaces to the given length
function RightPad(str, cch)
   if Len(str) < cch then
      RightPad = str & String(cch - Len(str), " ")
   else
      RightPad = str
   end if
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
sub PrintResultMsg(strTest, strResult)
   dim cchPad
   LogPrint "** " & strTest & ": " & strResult
   Wscript.Echo " Found " & RightPad(strTest & ": ", 22) & strPad & strResult
end sub


''
' Prints a successfully detected path
sub PrintResult(strTest, strPath)
   strLongPath = PathAbsLong(strPath)
   if PathAbs(strPath) <> strLongPath then
      LogPrint         "** " & strTest & ": " & strPath & " (" & UnixSlashes(strLongPath) & ")"
      Wscript.Echo " Found " & RightPad(strTest & ": ", 22) & strPath & " (" & UnixSlashes(strLongPath) & ")"
   else
      LogPrint         "** " & strTest & ": " & strPath
      Wscript.Echo " Found " & RightPad(strTest & ": ", 22) & strPath
   end if
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
   Wscript.Quit(1)
end sub


''
' Error message, fatal unless flag to ignore errors is given.
sub MsgError(strMsg)
   Print "error: " & strMsg
   if g_blnContinueOnError = False then
      Wscript.Quit(1)
   end if
   g_rcScript = 1
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
' Checks if the directory exists and logs failures.
function LogDirExists(strPath)
   LogDirExists = DirExists(strPath)
   if LogDirExists = False then
      LogPrint "Testing '" & strPath & "': not found (or not dir)"
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
   CfgPrint "VBOX_OSE := 1"
   CfgPrint "VBOX_VCC_WERR = $(NO_SUCH_VARIABLE)"
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
' Helper for EnvPrintPrepend and EnvPrintAppend.
sub EnvPrintCleanup(strEnv, strValue, strSep)
   dim cchValueAndSep
   FileAppendLine g_strEnvFile, "set " & strEnv & "=%" & strEnv & ":" & strSep & strValue & strSep & "=" & strSep & "%"
   cchValueAndSep = Len(strValue) + Len(strSep)
   FileAppendLine g_strEnvFile, "if ""%" & strEnv & "%""==""" & strValue & """ set " & strEnv & "="
   FileAppendLine g_strEnvFile, "if ""%" & strEnv & ":~0," & cchValueAndSep & "%""==""" & strValue & strSep & """ set " & strEnv & "=%" & strEnv & ":~" & cchValueAndSep & "%"
   FileAppendLine g_strEnvFile, "if ""%" & strEnv & ":~-"  & cchValueAndSep & "%""==""" & strSep & strValue & """  set " & strEnv & "=%" & strEnv & ":~0,-" & cchValueAndSep & "%"
end sub

'' Use by EnvPrintPrepend to skip ';' stripping.
dim g_strPrependCleanEnvVars

''
' Print a statement prepending strValue to strEnv, removing duplicate values.
sub EnvPrintPrepend(strEnv, strValue, strSep)
   ' Remove old values and any leading separators.
   EnvPrintCleanup strEnv, strValue, strSep
   if InStr(1, g_strPrependCleanEnvVars, "|" & strEnv & "|") = 0 then
      FileAppendLine g_strEnvFile, "if ""%" & strEnv & ":~0,1%""==""" & strSep & """ set " & strEnv & "=%" & strEnv & ":~1%"
      FileAppendLine g_strEnvFile, "if ""%" & strEnv & ":~0,1%""==""" & strSep & """ set " & strEnv & "=%" & strEnv & ":~1%"
      g_strPrependCleanEnvVars = g_strPrependCleanEnvVars & "|" & strEnv & "|"
   end if
   ' Do the setting
   FileAppendLine g_strEnvFile, "set " & strEnv & "=" & strValue & strSep & "%" & strEnv & "%"
end sub


'' Use by EnvPrintPrepend to skip ';' stripping.
dim g_strAppendCleanEnvVars

''
' Print a statement appending strValue to strEnv, removing duplicate values.
sub EnvPrintAppend(strEnv, strValue, strSep)
   ' Remove old values and any trailing separators.
   EnvPrintCleanup strEnv, strValue, strSep
   if InStr(1, g_strAppendCleanEnvVars, "|" & strEnv & "|") = 0 then
      FileAppendLine g_strEnvFile, "if ""%" & strEnv & ":~-1%""==""" & strSep & """ set " & strEnv & "=%" & strEnv & ":~0,-1%"
      FileAppendLine g_strEnvFile, "if ""%" & strEnv & ":~-1%""==""" & strSep & """ set " & strEnv & "=%" & strEnv & ":~0,-1%"
      g_strAppendCleanEnvVars = g_strAppendCleanEnvVars & "|" & strEnv & "|"
   end if
   ' Do the setting.
   FileAppendLine g_strEnvFile, "set " & strEnv & "=%" & strEnv & "%" & strSep & strValue
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
   end if
end sub


''
' No UDPTunnel
sub DisableUDPTunnel(strReason)
   if g_blnDisableUDPTunnel = False then
      LogPrint "Disabled UDPTunnel network transport: " & strReason
      g_blnDisableUDPTunnel = True
      g_strDisableUDPTunnel = strReason
      CfgPrint "VBOX_WITH_UDPTUNNEL="
   end if
end sub


''
' No SDL
sub DisableSDL(strReason)
   if g_blnDisableSDL = False then
      LogPrint "Disabled SDL frontend: " & strReason
      g_blnDisableSDL = True
      g_strDisableSDL = strReason
      CfgPrint "VBOX_WITH_VBOXSDL="
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
   ' any KBUILD_*PATH stuff around.
   '
   blnNeedEnvVars = True
   g_strPathkBuild = strOptkBuild
   g_strPathkBuildBin = ""
   if   (g_strPathkBuild = "") _
    And (EnvGetFirst("KBUILD_PATH", "PATH_KBUILD") = "") _
    And (EnvGetFirst("KBUILD_BIN_PATH", "PATH_KBUILD_BIN") = "") _
    And (Shell("kmk.exe --version", True) = 0) _
    And (InStr(1,g_strShellOutput, "kBuild Make 0.1") > 0) _
    And (InStr(1,g_strShellOutput, "KBUILD_PATH") > 0) _
    And (InStr(1,g_strShellOutput, "KBUILD_BIN_PATH") > 0) then
      '' @todo Need to parse out the KBUILD_PATH and KBUILD_BIN_PATH values to complete the other tests.
      'blnNeedEnvVars = False
      MsgWarning "You've installed kBuild it seems. configure.vbs hasn't been updated to " _
         & "deal with that yet and will use the one it ships with. Sorry."
   end if

   '
   ' Check for the KBUILD_PATH env.var. and fall back on root/kBuild otherwise.
   '
   if g_strPathkBuild = "" then
      g_strPathkBuild = EnvGetFirst("KBUILD_PATH", "PATH_KBUILD")
      if (g_strPathkBuild <> "") and (FileExists(g_strPathkBuild & "/footer.kmk") = False) then
         MsgWarning "Ignoring incorrect kBuild path (KBUILD_PATH=" & g_strPathkBuild & ")"
         g_strPathkBuild = ""
      end if

      if g_strPathkBuild = "" then
         g_strPathkBuild = g_strPath & "/kBuild"
      end if
   end if

   g_strPathkBuild = UnixSlashes(PathAbs(g_strPathkBuild))

   '
   ' Check for env.vars that kBuild uses (do this early to set g_strTargetArch).
   '
   str = EnvGetFirst("KBUILD_TYPE", "BUILD_TYPE")
   if   (str <> "") _
    And (InStr(1, "|release|debug|profile|kprofile", str) <= 0) then
      EnvPrint "set KBUILD_TYPE=release"
      EnvSet "KBUILD_TYPE", "release"
      MsgWarning "Found unknown KBUILD_TYPE value '" & str &"' in your environment. Setting it to 'release'."
   end if

   str = EnvGetFirst("KBUILD_TARGET", "BUILD_TARGET")
   if   (str <> "") _
    And (InStr(1, "win|win32|win64", str) <= 0) then '' @todo later only 'win' will be valid. remember to fix this check!
      EnvPrint "set KBUILD_TARGET=win"
      EnvSet "KBUILD_TARGET", "win"
      MsgWarning "Found unknown KBUILD_TARGET value '" & str &"' in your environment. Setting it to 'win32'."
   end if

   str = EnvGetFirst("KBUILD_TARGET_ARCH", "BUILD_TARGET_ARCH")
   if   (str <> "") _
    And (InStr(1, "x86|amd64", str) <= 0) then
      EnvPrint "set KBUILD_TARGET_ARCH=x86"
      EnvSet "KBUILD_TARGET_ARCH", "x86"
      MsgWarning "Found unknown KBUILD_TARGET_ARCH value '" & str &"' in your environment. Setting it to 'x86'."
      str = "x86"
   end if
   if g_strTargetArch = "" then '' command line parameter --target-arch=x86|amd64 has priority
      if str <> "" then
         g_strTargetArch = str
      elseif (EnvGet("PROCESSOR_ARCHITEW6432") = "AMD64" ) _
          Or (EnvGet("PROCESSOR_ARCHITECTURE") = "AMD64" ) then
         g_strTargetArch = "amd64"
      else
         g_strTargetArch = "x86"
      end if
   else
      if InStr(1, "x86|amd64", g_strTargetArch) <= 0 then
         EnvPrint "set KBUILD_TARGET_ARCH=x86"
         EnvSet "KBUILD_TARGET_ARCH", "x86"
         MsgWarning "Unknown --target-arch=" & str &". Setting it to 'x86'."
      end if
   end if
   LogPrint " Target architecture: " & g_strTargetArch & "."
   Wscript.Echo " Target architecture: " & g_strTargetArch & "."
   EnvPrint "set KBUILD_TARGET_ARCH=" & g_strTargetArch

   ' Windows variant of the arch name.
   g_strTargetArchWin = g_strTargetArch
   if g_strTargetArchWin = "amd64" then g_strTargetArchWin = "x64"

   str = EnvGetFirst("KBUILD_TARGET_CPU", "BUILD_TARGET_CPU")
    ' perhaps a bit pedantic this since this isn't clearly define nor used much...
   if   (str <> "") _
    And (InStr(1, "i386|i486|i686|i786|i868|k5|k6|k7|k8", str) <= 0) then
      EnvPrint "set BUILD_TARGET_CPU=i386"
      EnvSet "KBUILD_TARGET_CPU", "i386"
      MsgWarning "Found unknown KBUILD_TARGET_CPU value '" & str &"' in your environment. Setting it to 'i386'."
   end if

   str = EnvGetFirst("KBUILD_HOST", "BUILD_PLATFORM")
   if   (str <> "") _
    And (InStr(1, "win|win32|win64", str) <= 0) then '' @todo later only 'win' will be valid. remember to fix this check!
      EnvPrint "set KBUILD_HOST=win"
      EnvSet "KBUILD_HOST", "win"
      MsgWarning "Found unknown KBUILD_HOST value '" & str &"' in your environment. Setting it to 'win32'."
   end if

   str = EnvGetFirst("KBUILD_HOST_ARCH", "BUILD_PLATFORM_ARCH")
   if str <> "" then
      if InStr(1, "x86|amd64", str) <= 0 then
         str = "x86"
         MsgWarning "Found unknown KBUILD_HOST_ARCH value '" & str &"' in your environment. Setting it to 'x86'."
      end if
   elseif (EnvGet("PROCESSOR_ARCHITEW6432") = "AMD64" ) _
       Or (EnvGet("PROCESSOR_ARCHITECTURE") = "AMD64" ) then
      str = "amd64"
   else
      str = "x86"
   end if
   LogPrint " Host architecture: " & str & "."
   Wscript.Echo " Host architecture: " & str & "."
   EnvPrint "set KBUILD_HOST_ARCH=" & str
   g_strHostArch = str

   ' Windows variant of the arch name.
   g_strHostArchWin = g_strHostArch
   if g_strHostArchWin = "amd64" then g_strHostArchWin = "x64"


   str = EnvGetFirst("KBUILD_HOST_CPU", "BUILD_PLATFORM_CPU")
    ' perhaps a bit pedantic this since this isn't clearly define nor used much...
   if   (str <> "") _
    And (InStr(1, "i386|i486|i686|i786|i868|k5|k6|k7|k8", str) <= 0) then
      EnvPrint "set KBUILD_HOST_CPU=i386"
      EnvSet "KBUILD_HOST_CPU", "i386"
      MsgWarning "Found unknown KBUILD_HOST_CPU value '" & str &"' in your environment. Setting it to 'i386'."
   end if

   '
   ' Determin the location of the kBuild binaries.
   '
   if g_strPathkBuildBin = "" then
      g_strPathkBuildBin = g_strPathkBuild & "/bin/win." & g_strHostArch
      if FileExists(g_strPathkBuildBin & "/kmk.exe") = False then
         g_strPathkBuildBin = g_strPathkBuild & "/bin/win.x86"
      end if
   end if
   g_strPathkBuildBin = UnixSlashes(PathAbs(g_strPathkBuildBin))

   '
   ' Perform basic validations of the kBuild installation.
   '
   if  (FileExists(g_strPathkBuild & "/footer.kmk") = False) _
    Or (FileExists(g_strPathkBuild & "/header.kmk") = False) _
    Or (FileExists(g_strPathkBuild & "/rules.kmk") = False) then
      MsgFatal "Can't find valid kBuild at '" & g_strPathkBuild & "'. Either there is an " _
         & "incorrect KBUILD_PATH in the environment or the checkout didn't succeed."
      exit sub
   end if
   if  (FileExists(g_strPathkBuildBin & "/kmk.exe") = False) _
    Or (FileExists(g_strPathkBuildBin & "/kmk_ash.exe") = False) then
      MsgFatal "Can't find valid kBuild binaries at '" & g_strPathkBuildBin & "'. Either there is an " _
         & "incorrect KBUILD_PATH in the environment or the checkout didn't succeed."
      exit sub
   end if

   if (Shell(DosSlashes(g_strPathkBuildBin & "/kmk.exe") & " --version", True) <> 0) then
      MsgFatal "Can't execute '" & g_strPathkBuildBin & "/kmk.exe --version'. check configure.log for the out."
      exit sub
   end if

   '
   ' If KBUILD_DEVTOOLS is set, check that it's pointing to something useful.
   '
   str = UnixSlashes(EnvGet("KBUILD_DEVTOOLS"))
   g_strPathDev = str
   if   str <> "" _
    and LogDirExists(str & "/bin") _
    and LogDirExists(str & "/win.amd64/bin") _
    and LogDirExists(str & "/win.x86/bin") _
   then
      LogPrint "Found KBUILD_DEVTOOLS='" & str & "'."
   elseif str <> "" then
      MsgWarning "Ignoring bogus KBUILD_DEVTOOLS='" & str &"' in your environment!"
      g_strPathDev = strNew
   end if
   if g_strPathDev = "" then
      g_strPathDev = UnixSlashes(g_strPath & "/tools")
      LogPrint "Using KBUILD_DEVTOOLS='" & g_strPathDev & "'."
      if str <> "" then
         EnvPrint "set KBUILD_DEVTOOLS=" & g_strPathDev
         EnvSet "KBUILD_DEVTOOLS", g_strPathDev
      end if
   end if

   '
   ' Write KBUILD_PATH and updated PATH to the environment script if necessary.
   '
   if blnNeedEnvVars = True then
      EnvPrint "set KBUILD_PATH=" & g_strPathkBuild
      EnvSet "KBUILD_PATH", g_strPathkBuild

      if Right(g_strPathkBuildBin, 7) = "win.x86" then
         EnvPrintCleanup "PATH", DosSlashes(Left(g_strPathkBuildBin, Len(g_strPathkBuildBin) - 7) & "win.amd64"), ";"
      end if
      if Right(g_strPathkBuildBin, 9) = "win.amd64" then
         EnvPrintCleanup "PATH", DosSlashes(Left(g_strPathkBuildBin, Len(g_strPathkBuildBin) - 9) & "win.x86"), ";"
      end if
      EnvPrintPrepend "PATH", DosSlashes(g_strPathkBuildBin), ";"
      EnvPrepend "PATH", g_strPathkBuildBin & ";"
   end if

   PrintResult "kBuild", g_strPathkBuild
   PrintResult "kBuild binaries", g_strPathkBuildBin
end sub

''
' Class we use for detecting VisualC++
class VisualCPPState
   public m_blnFound
   public m_strPathVC
   public m_strPathVCCommon
   public m_strVersion
   public m_strClVersion
   public m_blnNewLayout

   private sub Class_Initialize
      m_blnFound        = False
      m_strPathVC       = ""
      m_strPathVCCommon = ""
      m_strVersion      = ""
      m_strClVersion    = ""
      m_blnNewLayout    = false
   end sub

   public function checkClExe(strClExe)
      ' We'll have to make sure mspdbXX.dll is somewhere in the PATH.
      dim strSavedPath, rcExit

      strSavedPath = EnvGet("PATH")
      if (m_strPathVCCommon <> "") then
         EnvAppend "PATH", ";" & m_strPathVCCommon & "/IDE"
      end if
      rcExit = Shell(DosSlashes(strClExe), True)
      EnvSet "PATH", strSavedPath

      checkClExe = False
      if rcExit = 0 then
         ' Extract the ' Version xx.yy.build.zz for arch' bit.
         dim offVer, offEol, strVer
         strVer = ""
         offVer = InStr(1, g_strShellOutput, " Version ")
         if offVer > 0 then
            offVer = offVer + Len(" Version ")
            offEol = InStr(offVer, g_strShellOutput, Chr(13))
            if offEol > 0 then
               strVer = Trim(Mid(g_strShellOutput, offVer, offEol - offVer))
            end if
         end if

         ' Check that it's a supported version
         checkClExe = True
         if InStr(1, strVer, "16.") = 1 then
            m_strVersion = "VCC110"
         elseif InStr(1, strVer, "17.") = 1 then
            m_strVersion = "VCC111"
            LogPrint "The Visual C++ compiler ('" & strClExe & "') version isn't really supported, but may work: " & strVer
         elseif InStr(1, strVer, "18.") = 1 then
            m_strVersion = "VCC112"
            LogPrint "The Visual C++ compiler ('" & strClExe & "') version isn't really supported, but may work: " & strVer
         elseif InStr(1, strVer, "19.0") = 1 then
            m_strVersion = "VCC140"
            LogPrint "The Visual C++ compiler ('" & strClExe & "') version isn't really supported, but may work: " & strVer
         elseif InStr(1, strVer, "19.1") = 1 then
            m_strVersion = "VCC141"
            LogPrint "The Visual C++ compiler ('" & strClExe & "') version isn't really supported, but may work: " & strVer
         elseif InStr(1, strVer, "19.2") = 1 then
            m_strVersion = "VCC142"
         else
            LogPrint "The Visual C++ compiler we found ('" & strClExe & "') isn't in the 10.0-19.2x range (" & strVer & ")."
            LogPrint "Check the build requirements and select the appropriate compiler version."
            checkClExe = False
            exit function
         end if
         LogPrint "'" & strClExe & "' = " & m_strVersion & " (" & strVer & ")"
      else
         LogPrint "Executing '" & strClExe & "' (which we believe to be the Visual C++ compiler driver) failed (rcExit=" & rcExit & ")."
      end if
   end function

   public function checkInner(strPathVC) ' For the new layout only
      if m_blnFound = False then
         if   LogDirExists(strPathVC & "/bin") _
          and LogDirExists(strPathVC & "/lib") _
          and LogDirExists(strPathVC & "/include") _
          and LogFileExists(strPathVC, "include/stdarg.h") _
          and LogFileExists(strPathVC, "lib/x64/libcpmt.lib") _
          and LogFileExists(strPathVC, "lib/x86/libcpmt.lib") _
         then
            LogPrint " => seems okay. new layout."
            m_blnFound = checkClExe(strPathVC & "/bin/Host" & g_strHostArchWin & "/bin/" & g_strHostArchWin & "/cl.exe")
            if m_blnFound then
               m_strPathVC = strPathVC
            end if
         end if
      end if
      checkInner = m_blnFound
   end function

   public function check(strPathVC, strPathVCommon)
      if (m_blnFound = False) and (strPathVC <> "") then
         m_strPathVC       = UnixSlashes(PathAbs(strPathVC))
         m_strPathVCCommon = strPathVCCommon
         m_strVersion      = ""

         LogPrint "Trying: strPathVC=" & m_strPathVC & " strPathVCCommon=" & m_strPathVCCommon
         if LogDirExists(m_strPathVC) then
            ' 15.0+ layout?  This is fun because of the multiple CL versions (/tools/msvc/xx.yy.bbbbb/).
            ' OTOH, the user may have pointed us directly to one of them.
            if LogDirExists(m_strPathVC & "/Tools/MSVC") then
               m_blnNewLayout = True
               LogPrint " => seems okay. new layout."
               dim arrFolders, i
               arrFolders = GetSubdirsStartingWithSorted(m_strPathVC & "/Tools/MSVC", "14.2")
               if UBound(arrFolders) < 0 then arrFolders = GetSubdirsStartingWithSorted(m_strPathVC & "/Tools/MSVC", "14.1")
               if UBound(arrFolders) < 0 then arrFolders = GetSubdirsStartingWithSorted(m_strPathVC & "/Tools/MSVC", "1")
               for i = UBound(arrFolders) to LBound(arrFolders) step -1
                  if checkInner(m_strPathVC & "/Tools/MSVC/" & arrFolders(i)) then exit for ' modifies m_strPathVC on success
               next
            elseif LogDirExists(m_strPathVC & "/bin/HostX64") _
                or LogDirExists(m_strPathVC & "/bin/HostX86") then
               checkInner(m_strPathVC)
            ' 14.0 and older layout?
            elseif LogFileExists(m_strPathVC, "/bin/cl.exe") then
               m_blnNewLayout = False
               if   LogFileExists(m_strPathVC, "bin/link.exe") _
                and LogFileExists(m_strPathVC, "include/string.h") _
                and LogFileExists(m_strPathVC, "lib/libcmt.lib") _
                and LogFileExists(m_strPathVC, "lib/msvcrt.lib") _
               then
                  LogPrint " => seems okay. old layout."
                  m_blnFound = checkClExe(m_strPathVC & "/bin/cl.exe")
               end if
            end if
         end if
      end if
      check = m_bldFound
   end function

   public function checkProg(strProg)
      if m_blnFound = False then
         dim str, i, offNewLayout
         str = Which(strProg)
         if str <> "" then
            LogPrint "checkProg: '" & strProg & "' -> '" & str & "'"
            if FileExists(PathStripFilename(str) & "/build.exe") then
               Warning "Ignoring DDK cl.exe (" & str & ")." ' don't know how to deal with this cl.
            else
               ' Assume we've got cl.exe from somewhere under the 'bin' directory..
               m_strPathVC = PathParent(PathStripFilename(str))
               for i = 1 To 5
                  if LogDirExists(m_strPathVC & "/include") then
                     m_strPathVCCommon = PathParent(m_strPathVC) & "/Common7"
                     if DirExists(m_strPathVCCommon) = False then
                        m_strPathVCCommon = ""
                        ' New layout?
                        offNewLayout = InStr(1, LCase(DosSlashes(m_strPathVC)), "\tools\msvc\")
                        if offNewLayout > 0 then m_strPathVC = Left(m_strPathVC, offNewLayout)
                     end if
                     check m_strPathVC, m_strPathVCCommon
                     exit for
                  end if
                  m_strPathVC = PathParent(m_strPathVC)
               next
            end if
         end if
      end if
      checkProg = m_bldFound
   end function

   public function checkProgFiles(strSubdir)
      if m_blnFound = False then
         dim strProgFiles
         for each strProgFiles in g_arrProgramFiles
            check strProgFiles & "/" & strSubdir, ""
         next
      end if
      checkProgFiles = m_blnFound
   end function

   public function checkRegistry(strValueNm, strVCSubdir, strVCommonSubdir)
      if m_blnFound = False then
         dim str, strPrefix, arrPrefixes
         arrPrefixes = Array("HKLM\SOFTWARE\Wow6432Node\", "HKLM\SOFTWARE\", "HKCU\SOFTWARE\Wow6432Node\", "HKCU\SOFTWARE\")
         for each strPrefix in arrPrefixes
            str = RegGetString(strPrefix & strValueNm)
            if str <> "" then
               LogPrint "checkRegistry: '" & strPrefix & strValueNm & "' -> '" & str & "'"
               if check(str & strVCSubdir, str & strVCommonSubdir) = True then
                  exit for
               end if
            end if
         next
      end if
      checkRegistry = m_bldFound
   end function

   public function checkInternal
      check g_strPathDev & "/win.amd64/vcc/v14.2",  ""
      check g_strPathDev & "/win.amd64/vcc/v14.1",  ""
      check g_strPathDev & "/win.amd64/vcc/v14.0",  ""
      check g_strPathDev & "/win.amd64/vcc/v10sp1", ""
      check g_strPathDev & "/win.x86/vcc/v10sp1",   ""
      checkInternal = m_blnFound
   end function
end class

''
' Checks for Visual C++ version 10 (2010).
sub CheckForVisualCPP(strOptVC, strOptVCCommon)
   PrintHdr "Visual C++"

   '
   ' Try find it...
   '
   dim objState, strProgFiles
   set objState = new VisualCPPState
   objState.check strOptVC, strOptVCCommon
   if g_blnInternalFirst = True then objState.checkInternal
   objState.checkProg "cl.exe"
   objState.checkProgFiles "Microsoft Visual Studio\2019\BuildTools\VC"
   objState.checkProgFiles "Microsoft Visual Studio\2019\Professional\VC"
   objState.checkProgFiles "Microsoft Visual Studio\2019\Community\VC"
   objState.checkRegistry "Microsoft\VisualStudio\SxS\VS7\16.0",             "VC", ""        ' doesn't work.
   objState.checkProgFiles "Microsoft Visual Studio\2017\BuildTools\VC"
   objState.checkProgFiles "Microsoft Visual Studio\2017\Professional\VC"
   objState.checkProgFiles "Microsoft Visual Studio\2017\Community\VC"
   objState.checkProgFiles "Microsoft Visual Studio\2017\Express\VC"
   objState.checkRegistry "Microsoft\VisualStudio\SxS\VS7\15.0",             "VC", ""
   objState.checkRegistry "Microsoft\VisualStudio\SxS\VS7\14.0",             "VC", "Common7"
   objState.checkRegistry "Microsoft\VisualStudio\SxS\VS7\12.0",             "VC", "Common7" '?
   objState.checkRegistry "Microsoft\VisualStudio\SxS\VS7\11.0",             "VC", "Common7" '?
   objState.checkRegistry "Microsoft\VisualStudio\10.0\Setup\VS\ProductDir", "VC", "Common7"
   if g_blnInternalFirst = False then objState.checkInternal

   if objState.m_blnFound = False then
      MsgError "Cannot find cl.exe (Visual C++) anywhere on your system. Check the build requirements."
      exit sub
   end if
   g_strPathVCC = objState.m_strPathVC
   g_strVCCVersion = objState.m_strVersion

   '
   ' Ok, emit build config variables.
   '
   CfgPrint "VBOX_VCC_TOOL_STEM    := " & objState.m_strVersion
   CfgPrint "PATH_TOOL_" & objState.m_strVersion & "      := " & g_strPathVCC
   CfgPrint "PATH_TOOL_" & objState.m_strVersion & "X86   := $(PATH_TOOL_" & objState.m_strVersion & ")"
   CfgPrint "PATH_TOOL_" & objState.m_strVersion & "AMD64 := $(PATH_TOOL_" & objState.m_strVersion & ")"

   if   objState.m_strVersion = "VCC100" _
     or objState.m_strVersion = "VCC110" _
     or objState.m_strVersion = "VCC120" _
     or objState.m_strVersion = "VCC140" _
   then
      CfgPrint "VBOX_WITH_NEW_VCC     :=" '?? for VCC110+
   else
      CfgPrint "VBOX_WITH_NEW_VCC     := 1"
   end if
   PrintResult "Visual C++ " & objState.m_strVersion, g_strPathVCC

   ' And the env.bat path fix.
   if objState.m_strPathVCCommon <> "" then
      EnvPrintAppend "PATH", DosSlashes(objState.m_strPathVCCommon) & "\IDE", ";"
   end if
end sub

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
   if strPathPSDK = "" And g_blnInternalFirst then
      str = g_strPathDev & "/win.x86/sdk/v7.1"
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   if strPathPSDK = "" And g_blnInternalFirst then
      str = g_strPathDev & "/win.x86/sdk/v8.0"
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   ' Look for it in the environment
   str = EnvGet("MSSdk")
   if strPathPSDK = "" And str <> "" then
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   str = EnvGet("Mstools")
   if strPathPSDK = "" And str <> "" then
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   ' Check if there is one installed with the compiler.
   if strPathPSDK = "" And str <> "" then
      str = g_strPathVCC & "/PlatformSDK"
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   ' Check the registry next (ASSUMES sorting).
   arrSubKeys = RegEnumSubKeysRSort("HKLM", "SOFTWARE\Microsoft\Microsoft SDKs\Windows")
   for each strSubKey in arrSubKeys
      str = RegGetString("HKLM\SOFTWARE\Microsoft\Microsoft SDKs\Windows\" & strSubKey & "\InstallationFolder")
      if strPathPSDK = "" And str <> "" then
         if CheckForPlatformSDKSub(str) then strPathPSDK = str
      end if
   Next
   arrSubKeys = RegEnumSubKeysRSort("HKCU", "SOFTWARE\Microsoft\Microsoft SDKs\Windows")
   for each strSubKey in arrSubKeys
      str = RegGetString("HKCU\SOFTWARE\Microsoft\Microsoft SDKs\Windows\" & strSubKey & "\InstallationFolder")
      if strPathPSDK = "" And str <> "" then
         if CheckForPlatformSDKSub(str) then strPathPSDK = str
      end if
   Next

   ' The tools location (post).
   if (strPathPSDK = "") And (g_blnInternalFirst = False) then
      str = g_strPathDev & "/win.x86/sdk/v7.1"
      if CheckForPlatformSDKSub(str) then strPathPSDK = str
   end if

   if (strPathPSDK = "") And (g_blnInternalFirst = False) then
      str = g_strPathDev & "/win.x86/sdk/v8.0"
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
   CfgPrint "PATH_SDK_WINPSDK" & g_strVerPSDK & "    := " & strPathPSDK
   CfgPrint "VBOX_WINPSDK          := WINPSDK" & g_strVerPSDK

   PrintResult "Windows Platform SDK (v" & g_strVerPSDK & ")", strPathPSDK
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
    And  LogFileExists(strPathPSDK, "bin/rc.exe") _
    And  Shell("""" & DosSlashes(strPathPSDK & "/bin/rc.exe") & """" , True) <> 0 _
      then
      if InStr(1, g_strShellOutput, "Resource Compiler Version 6.2.") > 0 then
         g_strVerPSDK = "80"
         CheckForPlatformSDKSub = True
      elseif InStr(1, g_strShellOutput, "Resource Compiler Version 6.1.") > 0 then
         g_strVerPSDK = "71"
         CheckForPlatformSDKSub = True
      end if
   end if
end function


''
' Checks for a platform SDK that works with the compiler
sub CheckForSDK10(strOptSDK10, strOptSDK10Version)
   dim strPathSDK10, strSDK10Version, str
   PrintHdr "Windows 10 SDK/WDK"
   '' @todo implement strOptSDK10Version

   '
   ' Try find the Windows 10 kit.
   '
   strSDK10Version = ""
   strPathSDK10 = CheckForSDK10Sub(strOptSDK10, strSDK10Version)
   if strPathSDK10 = "" and g_blnInternalFirst = True  then strPathSDK10 = CheckForSDK10ToolsSub(strSDK10Version)
   if strPathSDK10 = "" then
      str = RegGetString("HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots\KitsRoot10")
      strPathSDK10 = CheckForSDK10Sub(str, strSDK10Version)
   end if
   if strPathSDK10 = "" then
      for each str in g_arrProgramFiles
         strPathSDK10 = CheckForSDK10Sub(str & "/Windows Kits/10", strSDK10Version)
         if strPathSDK10 <> "" then exit for
      next
   end if
   if strPathSDK10 = "" and g_blnInternalFirst = False then strPathSDK10 = CheckForSDK10ToolsSub()

   if strPathSDK10 = "" then
      MsgError "Cannot find a suitable Windows 10 SDK.  Check configure.log and the build requirements."
      exit sub
   end if

   '
   ' Emit the config.
   '
   strPathSDK10 = UnixSlashes(PathAbs(strPathSDK10))
   CfgPrint "PATH_SDK_WINSDK10     := " & strPathSDK10
   CfgPrint "SDK_WINSDK10_VERSION  := " & strSDK10Version

   PrintResult "Windows 10 SDK (" & strSDK10Version & ")", strPathSDK10
   g_strPathSDK10 = strPathSDK10
end sub

''
' Checks the tools directory.
function CheckForSDK10ToolsSub(ByRef strSDK10Version)
   dim arrToolsDirs, strToolsDir, arrDirs, strDir
   CheckForSDK10ToolSub = ""
   arrToolsDirs = Array(g_strPathDev & "/win." & g_strTargetArch & "/sdk", _
                        g_strPathDev & "/win.x86/sdk", g_strPathDev & "/win.amd64/sdk")
   for each strToolsDir in arrToolsDirs
      arrDirs = GetSubdirsStartingWithRSorted(strToolsDir, "v10.")
      for each strDir in arrDirs
         CheckForSDK10ToolsSub = CheckForSDK10Sub(strToolsDir & "/" & strDir, strSDK10Version)
         if CheckForSDK10ToolsSub <> "" then
            exit function
         end if
      next
   next

end function

''
' Checks if the specified path points to a usable Windows 10 SDK/WDK.
function CheckForSDK10Sub(strPathSDK10, ByRef strSDK10Version)
   CheckForSDK10Sub = ""
   if strPathSDK10 <> "" then
      LogPrint "Trying: strPathSDK10=" & strPathSDK10
      if LogDirExists(strPathSDK10) then
         if   LogDirExists(strPathSDK10 & "/Bin") _
          and LogDirExists(strPathSDK10 & "/Include") _
          and LogDirExists(strPathSDK10 & "/Lib") _
          and LogDirExists(strPathSDK10 & "/Redist") _
         then
            ' Only testing the highest one, for now. '' @todo incorporate strOptSDK10Version
            dim arrVersions
            arrVersions = GetSubdirsStartingWithSorted(strPathSDK10 & "/Include", "10.0.")
            if UBound(arrVersions) >= 0 then
               dim strVersion
               strVersion = arrVersions(UBound(arrVersions))
               LogPrint "Trying version: " & strVersion
               if    LogFileExists(strPathSDK10, "include/" & strVersion & "/um/Windows.h") _
                and  LogFileExists(strPathSDK10, "include/" & strVersion & "/ucrt/malloc.h") _
                and  LogFileExists(strPathSDK10, "include/" & strVersion & "/ucrt/stdio.h") _
                and  LogFileExists(strPathSDK10, "lib/" & strVersion & "/um/"   & g_strTargetArchWin & "/kernel32.lib") _
                and  LogFileExists(strPathSDK10, "lib/" & strVersion & "/um/"   & g_strTargetArchWin & "/user32.lib") _
                and  LogFileExists(strPathSDK10, "lib/" & strVersion & "/ucrt/" & g_strTargetArchWin & "/libucrt.lib") _
                and  LogFileExists(strPathSDK10, "lib/" & strVersion & "/ucrt/" & g_strTargetArchWin & "/ucrt.lib") _
                and  LogFileExists(strPathSDK10, "bin/" & strVersion & "/" & g_strHostArchWin & "/rc.exe") _
                and  LogFileExists(strPathSDK10, "bin/" & strVersion & "/" & g_strHostArchWin & "/midl.exe") _
               then
                  '' @todo check minimum version (for WinHv).
                  strSDK10Version  = strVersion
                  CheckForSDK10Sub = strPathSDK10
               end if
            else
               LogPrint "Found no 10.0.* subdirectories under '" & strPathSDK10 & "/Include'!"
            end if
         end if
      end if
   end if
end function


''
' Checks for a Windows 7 Driver Kit.
sub CheckForWinDDK(strOptDDK)
   dim strPathDDK, str, strSubKeys
   PrintHdr "Windows DDK v7.1"

   '
   ' Find the DDK.
   '
   strPathDDK = ""
   ' The specified path.
   if strPathDDK = "" And strOptDDK <> "" then
      if CheckForWinDDKSub(strOptDDK, True) then strPathDDK = strOptDDK
   end if

   ' The tools location (first).
   if strPathDDK = "" And g_blnInternalFirst then
      str = g_strPathDev & "/win.x86/ddk/7600.16385.1"
      if CheckForWinDDKSub(str, False) then strPathDDK = str
   end if

   ' Check the environment
   str = EnvGet("DDK_INC_PATH")
   if strPathDDK = "" And str <> "" then
      str = PathParent(PathParent(str))
      if CheckForWinDDKSub(str, True) then strPathDDK = str
   end if

   str = EnvGet("BASEDIR")
   if strPathDDK = "" And str <> "" then
      if CheckForWinDDKSub(str, True) then strPathDDK = str
   end if

   ' Some array constants to ease the work.
   arrSoftwareKeys = array("SOFTWARE", "SOFTWARE\Wow6432Node")
   arrRoots        = array("HKLM", "HKCU")

   ' Windows 7 WDK.
   arrLocations = array()
   for each strSoftwareKey in arrSoftwareKeys
      for each strSubKey in RegEnumSubKeysFull("HKLM", strSoftwareKey & "\Microsoft\KitSetup\configured-kits")
         for each strSubKey2 in RegEnumSubKeysFull("HKLM", strSubKey)
            str = RegGetString("HKLM\" & strSubKey2 & "\setup-install-location")
            if str <> "" then
               arrLocations = ArrayAppend(arrLocations, PathAbsLong(str))
            end if
         next
      next
   next
   arrLocations = ArrayRSortStrings(arrLocations)

   ' Check the locations we've gathered.
   for each str in arrLocations
      if strPathDDK = "" then
         if CheckForWinDDKSub(str, True) then strPathDDK = str
      end if
   next

   ' The tools location (post).
   if (strPathDDK = "") And (g_blnInternalFirst = False) then
      str = g_strPathDev & "/win.x86/ddk/7600.16385.1"
      if CheckForWinDDKSub(str, False) then strPathDDK = str
   end if

   ' Give up.
   if strPathDDK = "" then
      MsgError "Cannot find the Windows DDK v7.1. Check configure.log and the build requirements."
      exit sub
   end if

   '
   ' Emit the config.
   '
   strPathDDK = UnixSlashes(PathAbs(strPathDDK))
   CfgPrint "PATH_SDK_WINDDK71     := " & strPathDDK

   PrintResult "Windows DDK v7.1", strPathDDK
   g_strPathDDK = strPathDDK
end sub

'' Quick check if the DDK is in the specified directory or not.
function CheckForWinDDKSub(strPathDDK, blnCheckBuild)
   CheckForWinDDKSub = False
   LogPrint "trying: strPathDDK=" & strPathDDK & " blnCheckBuild=" & blnCheckBuild
   if   LogFileExists(strPathDDK, "inc/api/ntdef.h") _
    And LogFileExists(strPathDDK, "lib/win7/i386/int64.lib") _
    And LogFileExists(strPathDDK, "lib/wlh/i386/int64.lib") _
    And LogFileExists(strPathDDK, "lib/wnet/i386/int64.lib") _
    And LogFileExists(strPathDDK, "lib/wxp/i386/int64.lib") _
    And Not LogFileExists(strPathDDK, "lib/win8/i386/int64.lib") _
    And LogFileExists(strPathDDK, "bin/x86/rc.exe") _
      then
      if Not blnCheckBuild then
         CheckForWinDDKSub = True
      '' @todo Find better build check.
      elseif Shell("""" & DosSlashes(strPathDDK & "/bin/x86/rc.exe") & """" , True) <> 0 _
         And InStr(1, g_strShellOutput, "Resource Compiler Version 6.1.") > 0 then
         CheckForWinDDKSub = True
      end if
   end if
end function


''
' Finds midl.exe
sub CheckForMidl()
   dim strMidl
   PrintHdr "Midl.exe"

   ' Skip if no COM/ATL.
   if g_blnDisableCOM then
      PrintResultMsg "Midl", "Skipped (" & g_strDisableCOM & ")"
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

   CfgPrint "VBOX_MAIN_IDL         := " & strMidl
   PrintResult "Midl.exe", strMidl
end sub


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
   if (strPathlibSDL = "") And (g_blnInternalFirst = True) then
      str = g_strPathDev & "/win." & g_strTargetArch & "/libsdl"
      if HasSubdirsStartingWith(str, "v") then
         PrintResult "libSDL", str & "/v* (auto)"
         exit sub
      end if
   end if

   ' Poke about in the path.
   if strPathlibSDL = "" then
      str = WhichEx("LIB", "SDLmain.lib")
      if str = "" then str = Which("..\lib\SDLmain.lib")
      if str = "" then str = Which("SDLmain.lib")
      if str <> "" then
         str = PathParent(PathStripFilename(str))
         if CheckForlibSDLSub(str) then strPathlibSDL = str
      end if
   end if

   if strPathlibSDL = "" then
      str = Which("SDL.dll")
      if str <> "" then
         str = PathParent(PathStripFilename(str))
         if CheckForlibSDLSub(str) then strPathlibSDL = str
      end if
   end if

   ' The tools location (post).
   if (strPathlibSDL = "") And (g_blnInternalFirst = False) then
      str = g_strPathDev & "/win." & g_strTargetArch & "/libsdl"
      if HasSubdirsStartingWith(str, "v") then
         PrintResult "libSDL", str & "/v* (auto)"
         exit sub
      end if
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

   '
   ' Part of tarball / svn, so we can exit immediately if no path was specified.
   '
   if (strOptXml2 = "") then
      PrintResultMsg "libxml2", "src/lib/libxml2-*"
      exit sub
   end if

   ' Skip if no COM/ATL.
   if g_blnDisableCOM then
      PrintResultMsg "libxml2", "Skipped (" & g_strDisableCOM & ")"
      exit sub
   end if

   '
   ' Try find some xml2 dll/lib.
   '
   strPathXml2 = ""
   if (strPathXml2 = "") And (strOptXml2 <> "") then
      if CheckForXml2Sub(strOptXml2) then strPathXml2 = strOptXml2
   end if

   if strPathXml2 = "" then
      str = Which("libxml2.lib")
      if str <> "" then
         str = PathParent(PathStripFilename(str))
         if CheckForXml2Sub(str) then strPathXml2 = str
      end if
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
   CfgPrint "SDK_VBOX_LIBXML2_DEFS  := _REENTRANT"
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
' Checks for openssl
sub CheckForSsl(strOptSsl, bln32Bit)
   dim strPathSsl, str
   PrintHdr "openssl"

   strOpenssl = "openssl"
   if bln32Bit = True then
       strOpenssl = "openssl32"
   end if

   '
   ' Part of tarball / svn, so we can exit immediately if no path was specified.
   '
   if (strOptSsl = "") then
      PrintResult strOpenssl, "src/libs/openssl-*"
      exit sub
   end if

   '
   ' Try find some openssl dll/lib.
   '
   strPathSsl = ""
   if (strPathSsl = "") And (strOptSsl <> "") then
      if CheckForSslSub(strOptSsl) then strPathSsl = strOptSsl
   end if

   if strPathSsl = "" then
      str = Which("libssl.lib")
      if str <> "" then
         str = PathParent(PathStripFilename(str))
         if CheckForSslSub(str) then strPathSsl = str
      end if
   end if

   ' Success?
   if strPathSsl = "" then
      if strOptSsl = "" then
         MsgError "Can't locate " & strOpenssl & ". " _
                & "Try specify the path with the --with-" & strOpenssl & "=<path> argument. " _
                & "If still no luck, consult the configure.log and the build requirements."
      else
         MsgError "Can't locate " & strOpenssl & ". " _
                & "Please consult the configure.log and the build requirements."
      end if
      exit sub
   end if

   strPathSsl = UnixSlashes(PathAbs(strPathSsl))
   if bln32Bit = True then
      CfgPrint "SDK_VBOX_OPENSSL-x86_INCS := " & strPathSsl & "/include"
      CfgPrint "SDK_VBOX_OPENSSL-x86_LIBS := " & strPathSsl & "/lib/libcrypto.lib" & " " & strPathSsl & "/lib/libssl.lib"
      CfgPrint "SDK_VBOX_BLD_OPENSSL-x86_LIBS := " & strPathSsl & "/lib/libcrypto.lib" & " " & strPathSsl & "/lib/libssl.lib"
   else
      CfgPrint "SDK_VBOX_OPENSSL_INCS := " & strPathSsl & "/include"
      CfgPrint "SDK_VBOX_OPENSSL_LIBS := " & strPathSsl & "/lib/libcrypto.lib" & " " & strPathSsl & "/lib/libssl.lib"
      CfgPrint "SDK_VBOX_BLD_OPENSSL_LIBS := " & strPathSsl & "/lib/libcrypto.lib" & " " & strPathSsl & "/lib/libssl.lib"
   end if

   PrintResult strOpenssl, strPathSsl
end sub

''
' Checks if the specified path points to an usable openssl or not.
function CheckForSslSub(strPathSsl)

   CheckForSslSub = False
   LogPrint "trying: strPathSsl=" & strPathSsl
   if   LogFileExists(strPathSsl, "include/openssl/md5.h") _
    And LogFindFile(strPathSsl, "lib/libssl.lib") <> "" _
      then
         CheckForSslSub = True
      end if
end function


''
' Checks for libcurl
sub CheckForCurl(strOptCurl, bln32Bit)
   dim strPathCurl, str
   PrintHdr "libcurl"

   strCurl = "libcurl"
   if bln32Bit = True then
       strCurl = "libcurl32"
   end if

   '
   ' Part of tarball / svn, so we can exit immediately if no path was specified.
   '
   if (strOptCurl = "") then
      PrintResult strCurl, "src/libs/curl-*"
      exit sub
   end if

   '
   ' Try find some cURL dll/lib.
   '
   strPathCurl = ""
   if (strPathCurl = "") And (strOptCurl <> "") then
      if CheckForCurlSub(strOptCurl) then strPathCurl = strOptCurl
   end if

   if strPathCurl = "" then
      str = Which("libcurl.lib")
      if str <> "" then
         str = PathParent(PathStripFilename(str))
         if CheckForCurlSub(str) then strPathCurl = str
      end if
   end if

   ' Success?
   if strPathCurl = "" then
      if strOptCurl = "" then
         MsgError "Can't locate " & strCurl & ". " _
                & "Try specify the path with the --with-" & strCurl & "=<path> argument. " _
                & "If still no luck, consult the configure.log and the build requirements."
      else
         MsgError "Can't locate " & strCurl & ". " _
                & "Please consult the configure.log and the build requirements."
      end if
      exit sub
   end if

   strPathCurl = UnixSlashes(PathAbs(strPathCurl))
   if bln32Bit = True then
      CfgPrint "SDK_VBOX_LIBCURL-x86_INCS := " & strPathCurl & "/include"
      CfgPrint "SDK_VBOX_LIBCURL-x86_LIBS.x86 := " & strPathCurl & "/libcurl.lib"
   else
      CfgPrint "SDK_VBOX_LIBCURL_INCS := " & strPathCurl & "/include"
      CfgPrint "SDK_VBOX_LIBCURL_LIBS := " & strPathCurl & "/libcurl.lib"
   end if

   PrintResult strCurl, strPathCurl
end sub

''
' Checks if the specified path points to an usable libcurl or not.
function CheckForCurlSub(strPathCurl)

   CheckForCurlSub = False
   LogPrint "trying: strPathCurl=" & strPathCurl
   if   LogFileExists(strPathCurl, "include/curl/curl.h") _
    And LogFindFile(strPathCurl, "libcurl.dll") <> "" _
    And LogFindFile(strPathCurl, "libcurl.lib") <> "" _
      then
         CheckForCurlSub = True
      end if
end function


''
' Checks for any Qt5 binaries.
sub CheckForQt(strOptQt5, strOptInfix)
   dim strPathQt5, strInfixQt5, arrFolders, arrVccInfixes, strVccInfix
   PrintHdr "Qt5"

   '
   ' Try to find the Qt5 installation (user specified path with --with-qt5)
   '
   LogPrint "Checking for user specified path of Qt5 ... "
   strPathQt5 = ""
   if strOptQt5 <> "" then
      strPathQt5 = CheckForQt5Sub(UnixSlashes(strOptQt5), strOptInfix, strInfixQt5)
   end if

   if strPathQt = "" then
      '
      ' Collect links from "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\UFH\SHC"
      '
      ' Typical string list:
      '   C:\Users\someuser\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Qt\5.x.y\MSVC 20zz (64-bit)\Qt 5.x.y (MSVC 20zz 64-bit).lnk
      '   C:\Windows\System32\cmd.exe
      '   /A /Q /K E:\qt\installed\5.x.y\msvc20zz_64\bin\qtenv2.bat
      '
      dim arrValues, strValue, arrStrings, str, arrCandidates, iCandidates, strCandidate, off
      arrValues = RegEnumValueNamesFull("HKCU", "SOFTWARE\Microsoft\Windows\CurrentVersion\UFH\SHC")
      redim arrCandidates(UBound(arrValues) - LBound(arrValues) + 1)
      iCandidates   = 0
      for each strValue in arrValues
         arrStrings = RegGetMultiString("HKCU\" & strValue)
         if UBound(arrStrings) >= 0 and UBound(arrStrings) - LBound(arrStrings) >= 2 then
            str = Trim(arrStrings(UBound(arrStrings)))
            if   LCase(Right(str, Len("\bin\qtenv2.bat"))) = "\bin\qtenv2.bat" _
             and InStr(1, LCase(str), "\msvc20") > 0 _
             and InStr(1, str, ":") > 0 _
            then
               off = InStr(1, str, ":") - 1
               arrCandidates(iCandidates) = Mid(str, off, Len(str) - off - Len("\bin\qtenv2.bat") + 1)
               LogPrint "qt5 candidate #" & iCandidates & "=" & arrCandidates(iCandidates) & " (" & str & ")"
               iCandidates = iCandidates + 1
            end if
         end if
      next
      redim preserve arrCandidates(iCandidates)
      if iCandidates > 0 then arrCandidates = ArrayRSortStrings(arrCandidates)   ' Kind of needs version sorting here...
      LogPrint "Testing qtenv2.bat links (" & iCandidates & ") ..."

      ' VC infixes/subdir names to consider (ASSUMES 64bit)
      if     g_strVCCVersion = "VCC142" or g_strVCCVersion = "" then
         arrVccInfixes = Array("msvc2019_64", "msvc2017_64", "msvc2015_64")
      elseif g_strVCCVersion = "VCC141" then
         arrVccInfixes = Array("msvc2017_64", "msvc2015_64", "msvc2019_64")
      elseif g_strVCCVersion = "VCC140" then
         arrVccInfixes = Array("msvc2015_64", "msvc2017_64", "msvc2019_64")
      elseif g_strVCCVersion = "VCC120" then
         arrVccInfixes = Array("msvc2013_64")
      elseif g_strVCCVersion = "VCC110" then
         arrVccInfixes = Array("msvc2012_64")
      elseif g_strVCCVersion = "VCC100" then
         arrVccInfixes = Array("msvc2010_64")
      else
         MsgFatal "Unexpected VC version: " & g_strVCCVersion
         arrVccInfixes = Array()
      end if
      for each strVccInfix in arrVccInfixes
         for each strCandidate in arrCandidates
            if InStr(1, LCase(strCandidate), strVccInfix) > 0 then
               strPathQt5 = CheckForQt5Sub(strCandidate, strOptInfix, strInfixQt5)
               if strPathQt5 <> "" then exit for
            end if
         next
         if strPathQt5 <> "" then exit for
      next
   end if

   ' Check the dev tools - prefer ones matching the compiler.
   if strPathQt5 = "" then
      LogPrint "Testing tools dir (" & g_strPathDev & "/win." & g_strTargetArch & "/qt/v5*) ..."
      arrFolders = GetSubdirsStartingWithSorted(g_strPathDev & "/win." & g_strTargetArch & "/qt", "v5")
      arrVccInfixes = Array(LCase(g_strVCCVersion), Left(LCase(g_strVCCVersion), Len(g_strVCCVersion) - 1), "")
      for each strVccInfix in arrVccInfixes
         for i = UBound(arrFolders) to LBound(arrFolders) step -1
            if strVccInfix = "" or InStr(1, LCase(arrFolders(i)), strVccInfix) > 0 then
               strPathQt5 = CheckForQt5Sub(g_strPathDev & "/win." & g_strTargetArch & "/qt/" & arrFolders(i), strOptInfix, strInfixQt5)
               if strPathQt5 <> "" then exit for
            end if
         next
         if strPathQt5 <> "" then exit for
      next
   end if

   '
   ' Display the result and output the config.
   '
   if strPathQt5 <> "" then
      PrintResult "Qt5", strPathQt5
      PrintResultMsg "Qt5 infix", strInfixQt5
      CfgPrint "PATH_SDK_QT5          := " & strPathQt5
      CfgPrint "PATH_TOOL_QT5         := $(PATH_SDK_QT5)"
      CfgPrint "VBOX_PATH_QT          := $(PATH_SDK_QT5)"
      CfgPrint "VBOX_QT_INFIX         := " & strInfixQt5
      CfgPrint "VBOX_WITH_QT_PAYLOAD  := 1"
   else
      CfgPrint "VBOX_WITH_QTGUI       :="
      PrintResultMsg "Qt5", "not found"
   end if
end sub

''
' Checks if the specified path points to an usable Qt5 library.
function CheckForQt5Sub(strPathQt5, strOptInfix, ByRef strInfixQt5)
   CheckForQt5Sub = ""
   LogPrint "trying: strPathQt5=" & strPathQt5

   if   LogFileExists(strPathQt5, "bin/moc.exe") _
    and LogFileExists(strPathQt5, "bin/uic.exe") _
    and LogFileExists(strPathQt5, "include/QtWidgets/qwidget.h") _
    and LogFileExists(strPathQt5, "include/QtWidgets/QApplication") _
    and LogFileExists(strPathQt5, "include/QtGui/QImage") _
    and LogFileExists(strPathQt5, "include/QtNetwork/QHostAddress") _
   then
      ' Infix testing.
      if   LogFileExists(strPathQt5, "lib/Qt5Core.lib") _
       and LogFileExists(strPathQt5, "lib/Qt5Network.lib") then
         strInfixQt5 = ""
         CheckForQt5Sub = UnixSlashes(PathAbs(strPathQt5))
      elseif LogFileExists(strPathQt5, "lib/Qt5Core" & strOptInfix & ".lib") _
         and LogFileExists(strPathQt5, "lib/Qt5Network" & strOptInfix & ".lib") then
         strInfixQt5 = strOptInfix
         CheckForQt5Sub = UnixSlashes(PathAbs(strPathQt5))
      elseif LogFileExists(strPathQt5, "lib/Qt5CoreVBox.lib") _
         and LogFileExists(strPathQt5, "lib/Qt5NetworkVBox.lib") then
         strInfixQt5 = "VBox"
         CheckForQt5Sub = UnixSlashes(PathAbs(strPathQt5))
      end if
   end if
end function


''
' Checks for python.
function CheckForPython(strOptPython)
   dim strPathPython, arrVersions, strVer, str
   PrintHdr "Python"
   CheckForPython = False

   '
   ' Locate it.
   '
   strPathPython = CheckForPythonSub(strOptPython)
   if strPathPython = "" then
      arrVersions = Array("3.12", "3.11", "3.10", "3.9", "3.8", "3.7", "3.6", "3.5", "2.7")
      for each strVer in arrVersions
         strPathPython = CheckForPythonSub(RegGetString("HKLM\SOFTWARE\Python\PythonCore\" & strVer & "\InstallPath\"))
         if strPathPython <> "" then exit for
      next
   end if
   if strPathPython = "" then strPathPython = CheckForPythonSub(PathStripFilename(Which("python.exe")))

   '
   ' Output config & result.
   '
   CheckForPython = strPathPython <> ""
   if CheckForPython then
      CfgPrint "VBOX_BLD_PYTHON       := " & strPathPython
      PrintResult "Python", strPathPython
   else
      PrintResultMsg "Python", "not found"
   end if
end function

'' Worker for CheckForPython.
'
function CheckForPythonSub(strPathPython)
   CheckForPythonSub = ""
   if strPathPython <> "" then
      if   LogFileExists(strPathPython, "python.exe") _
       and LogDirExists(strPathPython & "/DLLs") _
      then
         CheckForPythonSub = UnixSlashes(PathAbs(strPathPython & "/python.exe"))
      end if
   end if
end function


''
' Show usage.
sub usage
   Print "Usage: cscript configure.vbs [options]"
   Print ""
   Print "Configuration:"
   Print "  -h, --help              Display this."
   Print "  --target-arch=x86|amd64 The target architecture."
   Print "  --continue-on-error     Do not stop on errors."
   Print "  --internal-last         Check internal tools (tools/win.*) last."
   Print "  --internal-first        Check internal tools (tools/win.*) first (default)."
   Print ""
   Print "Components:"
   Print "  --disable-COM           Disables all frontends and API."
   Print "  --disable-SDL           Disables the SDL frontend."
   Print "  --disable-UDPTunnel"
   Print ""
   Print "Locations:"
   Print "  --with-kBuild=PATH      Where kBuild is to be found."
   Print "  --with-libSDL=PATH      Where libSDL is to be found."
   Print "  --with-Qt5=PATH         Where Qt5 is to be found."
   Print "  --with-DDK=PATH         Where the WDK is to be found."
   Print "  --with-SDK=PATH         Where the Windows SDK is to be found."
   Print "  --with-SDK10=PATH       Where the Windows 10 SDK/WDK is to be found."
   Print "  --with-VC=PATH          Where the Visual C++ compiler is to be found."
   Print "                          (Expecting bin, include and lib subdirs.)"
   Print "  --with-VC-Common=PATH   Maybe needed for 2015 and older to"
   Print "                          locate the Common7 directory."
   Print "  --with-python=PATH      The python to use."
   Print "  --with-libxml2=PATH     To use a libxml2 other than the VBox one."
   Print "  --with-openssl=PATH     To use an openssl other than the VBox one."
   Print "  --with-openssl32=PATH   The 32-bit variant of openssl."
   Print "  --with-libcurl=PATH     To use a cURL other than the VBox one."
   Print "  --with-libcurl32=PATH   The 32-bit variant of cURL."
end sub


''
' The main() like function.
'
function Main
   '
   ' Write the log header and check that we're not using wscript.
   '
   LogInit
   if UCase(Right(Wscript.FullName, 11)) = "WSCRIPT.EXE" then
      Wscript.Echo "This script must be run under CScript."
      Main = 1
      exit function
   end if

   '
   ' Parse arguments.
   '
   strOptDDK = ""
   strOptDXDDK = ""
   strOptkBuild = ""
   strOptlibSDL = ""
   strOptQt5 = ""
   strOptQt5Infix = ""
   strOptSDK = ""
   strOptSDK10 = ""
   strOptSDK10Version = ""
   strOptVC = ""
   strOptVCCommon = ""
   strOptXml2 = ""
   strOptSsl = ""
   strOptSsl32 = ""
   strOptCurl = ""
   strOptCurl32 = ""
   strOptPython = ""
   blnOptDisableCOM = False
   blnOptDisableUDPTunnel = False
   blnOptDisableSDL = False
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
         ' --with-something:
         case "--with-ddk"
            strOptDDK = strPath
         case "--with-dxsdk"
            MsgWarning "Ignoring --with-dxsdk (the DirectX SDK is no longer required)."
         case "--with-kbuild"
            strOptkBuild = strPath
         case "--with-libsdl"
            strOptlibSDL = strPath
         case "--with-mingw32"
            ' ignore
         case "--with-mingw-w64"
            ' ignore
         case "--with-qt5"
            strOptQt5 = strPath
         case "--with-qt5-infix"
            strOptQt5Infix = strPath
         case "--with-sdk"
            strOptSDK = strPath
         case "--with-sdk10"
            strOptSDK10 = strPath
         case "--with-sdk10-version"
            strOptSDK10Version = strPath
         case "--with-vc"
            strOptVC = strPath
         case "--with-vc-common"
            strOptVCCommon = strPath
         case "--with-vc-express-edition"
            ' ignore
         case "--with-w32api"
            ' ignore
         case "--with-libxml2"
            strOptXml2 = strPath
         case "--with-openssl"
            strOptSsl = strPath
         case "--with-openssl32"
            strOptSsl32 = strPath
         case "--with-libcurl"
            strOptCurl = strPath
         case "--with-libcurl32"
            strOptCurl32 = strPath
         case "--with-python"
            strOptPython = strPath

         ' --disable-something/--enable-something
         case "--disable-com"
            blnOptDisableCOM = True
         case "--enable-com"
            blnOptDisableCOM = False
         case "--disable-udptunnel"
            blnOptDisableUDPTunnel = True
         case "--enable-udptunnel"
            blnOptDisableUDPTunnel = False
         case "--disable-sdl"
            blnOptDisableSDL = True
         case "--endable-sdl"
            blnOptDisableSDL = False

         ' Other stuff.
         case "--continue-on-error"
            g_blnContinueOnError = True
         case "--internal-first"
            g_blnInternalFirst = True
         case "--internal-last"
            g_blnInternalFirst = False
         case "--target-arch"
            g_strTargetArch = strPath
         case "-h", "--help", "-?"
            usage
            Main = 0
            exit function
         case else
            Wscript.echo "syntax error: Unknown option '" & str &"'."
            usage
            Main = 2
            exit function
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
      Print "Shell test Test -> '" & g_strShellOutput & "'"
      MsgFatal "shell inheritance or shell execution isn't working right. Make sure you use cmd.exe."
   end if
   g_objShell.Environment("PROCESS")("TESTING_ENVIRONMENT_INHERITANCE") = ""
   Print "Shell inheritance test: OK"

   '
   ' Do the checks.
   '
   if blnOptDisableCOM = True then
      DisableCOM "--disable-com"
   end if
   if blnOptDisableUDPTunnel = True then
      DisableUDPTunnel "--disable-udptunnel"
   end if
   CheckSourcePath
   CheckForkBuild       strOptkBuild
   CheckForWinDDK       strOptDDK
   CheckForVisualCPP    strOptVC, strOptVCCommon
   CheckForPlatformSDK  strOptSDK
   CheckForSDK10        strOptSDK10, strOptSDK10Version
   CheckForMidl
   CfgPrint "VBOX_WITH_OPEN_WATCOM := " '' @todo look for openwatcom 1.9+
   CfgPrint "VBOX_WITH_LIBVPX := " '' @todo look for libvpx 1.1.0+
   CfgPrint "VBOX_WITH_LIBOPUS := " '' @todo look for libopus 1.2.1+

   EnvPrintAppend "PATH", DosSlashes(g_strPath & "\tools\win." & g_strHostArch & "\bin"), ";" '' @todo look for yasm
   if g_strHostArch = "amd64" then
      EnvPrintAppend "PATH", DosSlashes(g_strPath & "\tools\win.x86\bin"), ";"
   else
      EnvPrintCleanup "PATH", DosSlashes(g_strPath & "\tools\win.amd64\bin"), ";"
   end if
   if blnOptDisableSDL = True then
      DisableSDL "--disable-sdl"
   else
      CheckForlibSDL    strOptlibSDL
   end if
   CheckForXml2         strOptXml2
   CheckForSsl          strOptSsl, False
   if g_strTargetArch = "amd64" then
       ' 32-bit openssl required as well
       CheckForSsl      strOptSsl32, True
   end if
   CheckForCurl         strOptCurl, False
   if g_strTargetArch = "amd64" then
       ' 32-bit Curl required as well
       CheckForCurl     strOptCurl32, True
   end if
   CheckForQt           strOptQt5, strOptQt5Infix
   CheckForPython       strOptPython

   Print ""
   Print "Execute env.bat once before you start to build VBox:"
   Print ""
   Print "  env.bat"
   Print "  kmk"
   Print ""
   if g_rcScript <> 0 then
      Print "Warning: ignored errors. See above or in configure.log."
   end if

   Main = g_rcScript
end function

'
' What crt0.o typically does:
'
WScript.Quit(Main())

