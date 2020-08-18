' $Id$
'' @file
' Common VBScript helpers used by configure.vbs and later others.
'
' Requires the script including it to define a LogPrint function.
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


''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
'  Global Variables                                                                                                              '
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
dim g_objShell
Set g_objShell   = WScript.CreateObject("WScript.Shell")

dim g_objFileSys
Set g_objFileSys = WScript.CreateObject("Scripting.FileSystemObject")


''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
'  Helpers: Paths                                                                                                                '
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

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


''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
'  Helpers: Files and Dirs                                                                                                       '
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

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
function GetSubdirsStartingWithVerSorted(strFolder, strStartingWith)
   GetSubdirsStartingWithVerSorted = ArrayVerSortStrings(GetSubdirsStartingWith(strFolder, strStartingWith))
end function


''
' Returns a reverse version sorted array of subfolder names that starts with the given string.
function GetSubdirsStartingWithRVerSorted(strFolder, strStartingWith)
   GetSubdirsStartingWithRVerSorted = ArrayRVerSortStrings(GetSubdirsStartingWith(strFolder, strStartingWith))
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


''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
'  Helpers: Processes                                                                                                            '
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

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
' Executes a command in the shell catching output in strOutput
function Shell(strCommand, blnBoth, ByRef strOutput)
   dim strShell, strCmdline, objExec, str

   strShell = g_objShell.ExpandEnvironmentStrings("%ComSpec%")
   if blnBoth = true then
      strCmdline = strShell & " /c " & strCommand & " 2>&1"
   else
      strCmdline = strShell & " /c " & strCommand & " 2>nul"
   end if

   LogPrint "# Shell: " & strCmdline
   Set objExec = g_objShell.Exec(strCmdLine)
   strOutput = objExec.StdOut.ReadAll()
   objExec.StdErr.ReadAll()
   do while objExec.Status = 0
      Wscript.Sleep 20
      strOutput = strOutput & objExec.StdOut.ReadAll()
      objExec.StdErr.ReadAll()
   loop

   LogPrint "# Status: " & objExec.ExitCode
   LogPrint "# Start of Output"
   LogPrint strOutput
   LogPrint "# End of Output"

   Shell = objExec.ExitCode
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


''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
'  Helpers: Environment                                                                                                          '
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

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


''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
'  Helpers: Strings                                                                                                              '
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

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
' Checks if the given character is a decimal digit
function CharIsDigit(ch)
   CharIsDigit = (InStr(1, "0123456789", ch) > 0)
end function

''
' Worker for StrVersionCompare
' The offset is updated to point to the first non-digit character.
function CountDigitsIgnoreLeadingZeros(ByRef str, ByRef off)
   dim cntDigits, blnLeadingZeros, ch, offInt
   cntDigits = 0
   if CharIsDigit(Mid(str, off, 1)) then
      ' Rewind to start of digest sequence.
      do while off > 1
         if not CharIsDigit(Mid(str, off - 1, 1)) then exit do
         off = off - 1
      loop
      ' Count digits, ignoring leading zeros.
      blnLeadingZeros = True
      for off = off to Len(str)
         ch = Mid(str, off, 1)
         if CharIsDigit(ch) then
            if ch <> "0" or blnLeadingZeros = False then
               cntDigits = cntDigits + 1
               blnLeadingZeros = False
            end if
         else
            exit for
         end if
      next
      ' If all zeros, count one of them.
      if cntDigits = 0 then cntDigits = 1
   end if
   CountDigitsIgnoreLeadingZeros = cntDigits
end function

''
' Very simple version string compare function.
' @returns < 0 if str1 is smaller than str2
' @returns 0   if str1 and str2 are equal
' @returns > 1 if str2 is larger than str1
function StrVersionCompare(str1, str2)
   ' Compare the strings.  We can rely on StrComp if equal or one is empty.
   'LogPrint "StrVersionCompare("&str1&","&str2&"):"
   StrVersionCompare = StrComp(str2, str1)
   if StrVersionCompare <> 0 then
      dim cch1, cch2, off1, off2, ch1, ch2, chPrev1, chPrev2, intDiff, cchDigits
      cch1 = Len(str1)
      cch2 = Len(str2)
      if cch1 > 0 and cch2 > 0 then
         ' Compare the common portion
         off1 = 1
         off2 = 1
         chPrev1 = "x"
         chPrev2 = "x"
         do while off1 <= cch1 and off2 <= cch2
            ch1 = Mid(str1, off1, 1)
            ch2 = Mid(str2, off2, 1)
            if ch1 = ch2 then
               off1 = off1 + 1
               off2 = off2 + 1
               chPrev1 = ch1
               chPrev2 = ch2
            else
               ' Is there a digest sequence in play.  This includes the scenario where one of the
               ' string ran out of digests.
               dim blnDigest1 : blnDigest1 = CharIsDigit(ch1)
               dim blnDigest2 : blnDigest2 = CharIsDigit(ch2)
               if    (blnDigest1 = True or blnDigest2 = True) _
                 and (blnDigest1 = True or CharIsDigit(chPrev1) = True) _
                 and (blnDigest2 = True or CharIsDigit(chPrev2) = True) _
               then
                  'LogPrint "StrVersionCompare: off1="&off1&" off2="&off2&" ch1="&ch1&" chPrev1="&chPrev1&" ch2="&ch2&" chPrev2="&chPrev2
                  if blnDigest1 = False then off1 = off1 - 1
                  if blnDigest2 = False then off2 = off2 - 1
                  ' The one with the fewer digits comes first.
                  ' Note! off1 and off2 are adjusted to next non-digit character in the strings.
                  cchDigits = CountDigitsIgnoreLeadingZeros(str1, off1)
                  intDiff = cchDigits - CountDigitsIgnoreLeadingZeros(str2, off2)
                  'LogPrint "StrVersionCompare: off1="&off1&" off2="&off2&" cchDigits="&cchDigits
                  if intDiff <> 0 then
                     StrVersionCompare = intDiff
                     'LogPrint "StrVersionCompare: --> "&intDiff&" #1"
                     exit function
                  end if

                  ' If the same number of digits, the smaller digit wins. However, because of
                  ' potential leading zeros, we must redo the compare. Assume ASCII-like stuff
                  ' and we can use StrComp for this.
                  intDiff = StrComp(Mid(str1, off1 - cchDigits, cchDigits), Mid(str2, off2 - cchDigits, cchDigits))
                  if intDiff <> 0 then
                     StrVersionCompare = intDiff
                     'LogPrint "StrVersionCompare: --> "&intDiff&" #2"
                     exit function
                  end if
                  chPrev1 = "x"
                  chPrev2 = "x"
               else
                  if blnDigest1 then
                     StrVersionCompare = -1   ' Digits before characters
                     'LogPrint "StrVersionCompare: --> -1 (#3)"
                  elseif blnDigest2 then
                     StrVersionCompare = 1       ' Digits before characters
                     'LogPrint "StrVersionCompare: --> 1 (#4)"
                  else
                     StrVersionCompare = StrComp(ch1, ch2)
                     'LogPrint "StrVersionCompare: --> "&StrVersionCompare&" (#5)"
                  end if
                  exit function
               end if
            end if
         loop

         ' The common part matches up, so the shorter string 'wins'.
         StrVersionCompare = (cch1 - off1) - (cch2 - off2)
      end if
   end if
   'LogPrint "StrVersionCompare: --> "&StrVersionCompare&" (#6)"
end function


''
' Returns the first list of the given string.
function StrGetFirstLine(str)
   dim off
   off = InStr(1, str, Chr(10))
   if off <= 0 then off = InStr(1, str, Chr(13))
   if off > 0 then
      StrGetFirstLine = Mid(str, 1, off)
   else
      StrGetFirstLine = str
   end if
end function


''
' Returns the first word in the given string.
'
' Only recognizes space, tab, newline and carriage return as word separators.
'
function StrGetFirstWord(str)
   dim strSep, offWord, offEnd, offEnd2, strSeparators
   strSeparators = " " & Chr(9) & Chr(10) & Chr(13)

   ' Skip leading separators.
   for offWord = 1 to Len(str)
      if InStr(1, strSeparators, Mid(str, offWord, 1)) < 1 then exit for
   next

   ' Find the end.
   offEnd = Len(str) + 1
   for offSep = 1 to Len(strSeparators)
      offEnd2 = InStr(offWord, str, Mid(strSeparators, offSep, 1))
      if offEnd2 > 0 and offEnd2 < offEnd then offEnd = offEnd2
   next

   StrGetFirstWord = Mid(str, offWord, offEnd - offWord)
end function


''
' Checks if the string starts with the given prefix (case sensitive).
function StrStartsWith(str, strPrefix)
   if len(str) >= Len(strPrefix) then
      StrStartsWith = (StrComp(Left(str, Len(strPrefix)), strPrefix, vbBinaryCompare) = 0)
   else
      StrStartsWith = false
   end if
end function


''
' Checks if the string starts with the given prefix, case insenstive edition.
function StrStartsWithI(str, strPrefix)
   if len(str) >= Len(strPrefix) then
      StrStartsWithI = (StrComp(Left(str, Len(strPrefix)), strPrefix, vbTextCompare) = 0)
   else
      StrStartsWithI = false
   end if
end function


''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
'  Helpers: Arrays                                                                                                               '
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

''
' Returns a reverse array (copy).
function ArrayReverse(arr)
   dim cnt, i, j, iHalf, objTmp
   cnt = UBound(arr) - LBound(arr) + 1
   if cnt > 0 then
      j     = UBound(arr)
      iHalf = Fix(LBound(arr) + cnt / 2)
      for i = LBound(arr) to iHalf - 1
         objTmp = arr(i)
         arr(i) = arr(j)
         arr(j) = objTmp
         j = j - 1
      next
   end if
   ArrayReverse = arr
end function


''
' Returns a reverse sorted array (strings).
function ArraySortStringsEx(arrStrings, ByRef fnCompare)
   dim str1, str2, i, j
   for i = LBound(arrStrings) to UBound(arrStrings)
      str1 = arrStrings(i)
      for j = i + 1 to UBound(arrStrings)
         str2 = arrStrings(j)
         if fnCompare(str2, str1) < 0 then
            arrStrings(j) = str1
            str1 = str2
         end if
      next
      arrStrings(i) = str1
   next
   ArraySortStringsEx = arrStrings
end function


''
' Returns a reverse sorted array (strings).
function ArraySortStrings(arrStrings)
   ArraySortStrings = ArraySortStringsEx(arrStrings, GetRef("StrComp"))
end function


''
' Returns a reverse sorted array (strings).
function ArrayVerSortStrings(arrStrings)
   ArrayVerSortStrings = ArraySortStringsEx(arrStrings, GetRef("StrVersionCompare"))
end function


''
' Returns a reverse sorted array (strings).
function ArrayRSortStrings(arrStrings)
   ArrayRSortStrings = ArrayReverse(ArraySortStringsEx(arrStrings, GetRef("StrComp")))
end function


''
' Returns a reverse version sorted array (strings).
function ArrayRVerSortStrings(arrStrings)
   ArrayRVerSortStrings = ArrayReverse(ArraySortStringsEx(arrStrings, GetRef("StrVersionCompare")))
end function


''
' Prints a string array.
sub ArrayPrintStrings(arrStrings, strPrefix)
   for i = LBound(arrStrings) to UBound(arrStrings)
      Print strPrefix & "arrStrings(" & i & ") = '" & arrStrings(i) & "'"
   next
end sub


''
' Returns the input array with the string appended.
' Note! There must be some better way of doing this...
' @todo Lots of copying here...
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
' Checks if the array contains the given string (case sensitive).
function ArrayContainsString(ByRef arr, str)
   dim strCur
   ArrayContainsString = False
   for each strCur in arr
      if StrComp(strCur, str) = 0 then
         ArrayContainsString = True
         exit function
      end if
   next
end function


''
' Checks if the array contains the given string, using case insensitive compare.
function ArrayContainsStringI(ByRef arr, str)
   dim strCur
   ArrayContainsStringI = False
   for each strCur in arr
      if StrComp(strCur, str, vbTextCompare) = 0 then
         ArrayContainsStringI = True
         exit function
      end if
   next
end function

''
' Returns the number of entries in an array.
function ArraySize(ByRef arr)
   if (UBound(arr) >= 0) then
      ArraySize = UBound(arr) - LBound(arr) + 1
   else
      ArraySize = 0
   end if
end function


''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
'  Helpers: Registry                                                                                                             '
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

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
         LogPrint "RegInit: WoW64"
      end if
      set objLocator = CreateObject("Wbemscripting.SWbemLocator")
      set objServices = objLocator.ConnectServer("", "root\default", "", "", , , , g_objRegCtx)
      set g_objReg = objServices.Get("StdRegProv")
      g_blnRegistry = true
   end if
   RegInit = true
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


''
' Gets a value from the registry. Returns "" if string wasn't found / valid.
function RegGetString(strName)
   RegGetString = ""
   if RegInit() then
      dim strRoot, strKey, strValue

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
         if not IsNull(OutParms.sValue) then
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
function RegEnumSubKeysRVerSorted(strRoot, strKeyPath)
   RegEnumSubKeysRVerSorted = ArrayRVerSortStrings(RegEnumSubKeys(strRoot, strKeyPath))
end function


''
' Returns an rsorted array of subkey strings.
function RegEnumSubKeysFullRVerSorted(strRoot, strKeyPath)
   RegEnumSubKeysFullRVerSorted = ArrayRVerSortStrings(RegEnumSubKeysFull(strRoot, strKeyPath))
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
' Extract relevant paths from program links using a callback function.
'
' Enumerates start menu program links from "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\UFH\SHC"
' and similar, using the given callback to examine each and return a path if relevant.  The relevant
' paths are returned in reverse sorted order.
'
' The callback prototype is as follows fnCallback(ByRef arrStrings, cStrings, ByRef objUser).
' Any non-empty return strings are collected, reverse sorted uniquely and returned.
'
function CollectFromProgramItemLinks(ByRef fnCallback, ByRef objUser)
   dim arrValues, strValue, arrStrings, str, arrCandidates, iCandidates, cStrings
   CollectFromProgramItemLinks = Array()

   arrValues = RegEnumValueNamesFull("HKCU", "SOFTWARE\Microsoft\Windows\CurrentVersion\UFH\SHC")
   redim arrCandidates(UBound(arrValues) - LBound(arrValues) + 1)
   iCandidates = 0
   for each strValue in arrValues
      arrStrings = RegGetMultiString("HKCU\" & strValue)
      if UBound(arrStrings) >= 0 then
         cStrings = UBound(arrStrings) + 1 - LBound(arrStrings)
         str = fnCallback(arrStrings, cStrings, objUser)
         if str <> "" then
            if not ArrayContainsStringI(arrCandidates, str) then
               arrCandidates(iCandidates) = str
               iCandidates = iCandidates + 1
            end if
         end if
      end if
   next
   if iCandidates > 0 then
      redim preserve arrCandidates(iCandidates - 1)
      arrCandidates = ArrayRVerSortStrings(arrCandidates)
      for iCandidates = LBound(arrCandidates) to UBound(arrCandidates)
         LogPrint "CollectFromProgramItemLinks: #" & iCandidates & ": " & arrCandidates(iCandidates)
      next
      CollectFromProgramItemLinks = arrCandidates
   end if
end function


''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
'  Helpers: Messaging and Output                                                                                                 '
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

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


''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
'  Testcases                                                                                                                     '
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

''
' Self test for some of the above routines.
'
sub SelfTest
   dim i, str
   str = "0123456789"
   for i = 1 to Len(str)
      if CharIsDigit(Mid(str, i, 1)) <> True then MsgFatal "SelfTest failed: CharIsDigit("&Mid(str, i, 1)&")"
   next
   str = "abcdefghijklmnopqrstuvwxyz~`!@#$%^&*()_+-=ABCDEFGHIJKLMNOPQRSTUVWXYZ/\[]{}"
   for i = 1 to Len(str)
      if CharIsDigit(Mid(str, i, 1)) <> False then MsgFatal "SelfTest failed: CharIsDigit("&Mid(str, i, 1)&")"
   next

   if StrVersionCompare("1234", "1234") <> 0 then MsgFatal "SelfTest failed: StrVersionCompare #1"
   if StrVersionCompare("1", "1") <> 0 then MsgFatal "SelfTest failed: StrVersionCompare #2"
   if StrVersionCompare("2", "1") <= 0 then MsgFatal "SelfTest failed: StrVersionCompare #3"
   if StrVersionCompare("1", "2") >= 0 then MsgFatal "SelfTest failed: StrVersionCompare #4"
   if StrVersionCompare("01", "1") <> 0 then MsgFatal "SelfTest failed: StrVersionCompare #5"
   if StrVersionCompare("01", "001") <> 0 then MsgFatal "SelfTest failed: StrVersionCompare #6"
   if StrVersionCompare("12", "123") >= 0 then MsgFatal "SelfTest failed: StrVersionCompare #7"
   if StrVersionCompare("v123", "123") <= 0 then MsgFatal "SelfTest failed: StrVersionCompare #8"
   if StrVersionCompare("v1.2.3", "v1.3.4") >= 0 then MsgFatal "SelfTest failed: StrVersionCompare #9"
   if StrVersionCompare("v1.02.3", "v1.3.4") >= 0 then MsgFatal "SelfTest failed: StrVersionCompare #10"
   if StrVersionCompare("v1.2.3", "v1.03.4") >= 0 then MsgFatal "SelfTest failed: StrVersionCompare #11"
   if StrVersionCompare("v1.2.4", "v1.23.4") >= 0 then MsgFatal "SelfTest failed: StrVersionCompare #12"
   if StrVersionCompare("v10.0.17163", "v10.00.18363") >= 0 then MsgFatal "SelfTest failed: StrVersionCompare #13"
   if StrVersionCompare("n 2.15.0", "2.12.0") <= 0 then MsgFatal "SelfTest failed: StrVersionCompare #14"

   if StrGetFirstWord("1") <> "1" then MsgFatal "SelfTest: StrGetFirstWord #1"
   if StrGetFirstWord(" 1 ") <> "1" then MsgFatal "SelfTest: StrGetFirstWord #2"
   if StrGetFirstWord(" 1  2 ") <> "1" then MsgFatal "SelfTest: StrGetFirstWord #3"
   if StrGetFirstWord("1 2") <> "1" then MsgFatal "SelfTest: StrGetFirstWord #4"
   if StrGetFirstWord("1234 5") <> "1234" then MsgFatal "SelfTest: StrGetFirstWord #5"
   if StrGetFirstWord("  ") <> "" then MsgFatal "SelfTest: StrGetFirstWord #6"
end sub

