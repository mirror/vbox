# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=C0302

"""
Common Utility Functions.
"""

__copyright__ = \
"""
Copyright (C) 2012-2015 Oracle Corporation

This file is part of VirtualBox Open Source Edition (OSE), as
available from http://www.virtualbox.org. This file is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License (GPL) as published by the Free Software
Foundation, in version 2 as it comes in the "COPYING" file of the
VirtualBox OSE distribution. VirtualBox OSE is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL) only, as it comes in the "COPYING.CDDL" file of the
VirtualBox OSE distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.
"""
__version__ = "$Revision$"


# Standard Python imports.
import datetime;
import os;
import platform;
import re;
import stat;
import subprocess;
import sys;
import tarfile;
import time;
import traceback;
import unittest;
import zipfile

if sys.platform == 'win32':
    import win32api;            # pylint: disable=F0401
    import win32con;            # pylint: disable=F0401
    import win32console;        # pylint: disable=F0401
    import win32process;        # pylint: disable=F0401
else:
    import signal;

# Python 3 hacks:
if sys.version_info[0] >= 3:
    long = int;     # pylint: disable=W0622,C0103


#
# Host OS and CPU.
#

def getHostOs():
    """
    Gets the host OS name (short).

    See the KBUILD_OSES variable in kBuild/header.kmk for possible return values.
    """
    sPlatform = platform.system();
    if sPlatform in ('Linux', 'Darwin', 'Solaris', 'FreeBSD', 'NetBSD', 'OpenBSD'):
        sPlatform = sPlatform.lower();
    elif sPlatform == 'Windows':
        sPlatform = 'win';
    elif sPlatform == 'SunOS':
        sPlatform = 'solaris';
    else:
        raise Exception('Unsupported platform "%s"' % (sPlatform,));
    return sPlatform;

g_sHostArch = None;

def getHostArch():
    """
    Gets the host CPU architecture.

    See the KBUILD_ARCHES variable in kBuild/header.kmk for possible return values.
    """
    global g_sHostArch;
    if g_sHostArch is None:
        sArch = platform.machine();
        if sArch in ('i386', 'i486', 'i586', 'i686', 'i786', 'i886', 'x86'):
            sArch = 'x86';
        elif sArch in ('AMD64', 'amd64', 'x86_64'):
            sArch = 'amd64';
        elif sArch == 'i86pc': # SunOS
            if platform.architecture()[0] == '64bit':
                sArch = 'amd64';
            else:
                try:
                    sArch = processOutputChecked(['/usr/bin/isainfo', '-n',]);
                except:
                    pass;
                sArch = sArch.strip();
                if sArch != 'amd64':
                    sArch = 'x86';
        else:
            raise Exception('Unsupported architecture/machine "%s"' % (sArch,));
        g_sHostArch = sArch;
    return g_sHostArch;


def getHostOsDotArch():
    """
    Gets the 'os.arch' for the host.
    """
    return '%s.%s' % (getHostOs(), getHostArch());


def isValidOs(sOs):
    """
    Validates the OS name.
    """
    if sOs in ('darwin', 'dos', 'dragonfly', 'freebsd', 'haiku', 'l4', 'linux', 'netbsd', 'nt', 'openbsd', \
               'os2', 'solaris', 'win', 'os-agnostic'):
        return True;
    return False;


def isValidArch(sArch):
    """
    Validates the CPU architecture name.
    """
    if sArch in ('x86', 'amd64', 'sparc32', 'sparc64', 's390', 's390x', 'ppc32', 'ppc64', \
               'mips32', 'mips64', 'ia64', 'hppa32', 'hppa64', 'arm', 'alpha'):
        return True;
    return False;

def isValidOsDotArch(sOsDotArch):
    """
    Validates the 'os.arch' string.
    """

    asParts = sOsDotArch.split('.');
    if asParts.length() != 2:
        return False;
    return isValidOs(asParts[0]) \
       and isValidArch(asParts[1]);

def getHostOsVersion():
    """
    Returns the host OS version.  This is platform.release with additional
    distro indicator on linux.
    """
    sVersion = platform.release();
    sOs = getHostOs();
    if sOs == 'linux':
        sDist = '';
        try:
            # try /etc/lsb-release first to distinguish between Debian and Ubuntu
            oFile = open('/etc/lsb-release');
            for sLine in oFile:
                oMatch = re.search(r'(?:DISTRIB_DESCRIPTION\s*=)\s*"*(.*)"', sLine);
                if oMatch is not None:
                    sDist = oMatch.group(1).strip();
        except:
            pass;
        if sDist:
            sVersion += ' / ' + sDist;
        else:
            asFiles = \
            [
                [ '/etc/debian_version', 'Debian v'],
                [ '/etc/gentoo-release', '' ],
                [ '/etc/oracle-release', '' ],
                [ '/etc/redhat-release', '' ],
                [ '/etc/SuSE-release', '' ],
            ];
            for sFile, sPrefix in asFiles:
                if os.path.isfile(sFile):
                    try:
                        oFile = open(sFile);
                        sLine = oFile.readline();
                        oFile.close();
                    except:
                        continue;
                    sLine = sLine.strip()
                    if len(sLine) > 0:
                        sVersion += ' / ' + sPrefix + sLine;
                    break;

    elif sOs == 'solaris':
        sVersion = platform.version();
        if os.path.isfile('/etc/release'):
            try:
                oFile = open('/etc/release');
                sLast = oFile.readlines()[-1];
                oFile.close();
                sLast = sLast.strip();
                if len(sLast) > 0:
                    sVersion += ' (' + sLast + ')';
            except:
                pass;

    elif sOs == 'darwin':
        sOsxVersion = platform.mac_ver()[0];
        codenames = {"4": "Tiger",
                     "5": "Leopard",
                     "6": "Snow Leopard",
                     "7": "Lion",
                     "8": "Mountain Lion",
                     "9": "Mavericks",
                     "10": "Yosemite",
                     "11": "El Capitan",
                     "12": "Fuji" }
        sVersion += ' / OS X ' + sOsxVersion + ' (' + codenames[sOsxVersion.split('.')[1]] + ')'

    return sVersion;

#
# File system.
#

def openNoInherit(sFile, sMode = 'r'):
    """
    Wrapper around open() that tries it's best to make sure the file isn't
    inherited by child processes.

    This is a best effort thing at the moment as it doesn't synchronizes with
    child process spawning in any way.  Thus it can be subject to races in
    multithreaded programs.
    """

    try:
        from fcntl import FD_CLOEXEC, F_GETFD, F_SETFD, fcntl; # pylint: disable=F0401
    except:
        return open(sFile, sMode);

    oFile = open(sFile, sMode)
    #try:
    fcntl(oFile, F_SETFD, fcntl(oFile, F_GETFD) | FD_CLOEXEC);
    #except:
    #    pass;
    return oFile;

def noxcptReadLink(sPath, sXcptRet):
    """
    No exceptions os.readlink wrapper.
    """
    try:
        sRet = os.readlink(sPath); # pylint: disable=E1101
    except:
        sRet = sXcptRet;
    return sRet;

def readFile(sFile, sMode = 'rb'):
    """
    Reads the entire file.
    """
    oFile = open(sFile, sMode);
    sRet = oFile.read();
    oFile.close();
    return sRet;

def noxcptReadFile(sFile, sXcptRet, sMode = 'rb'):
    """
    No exceptions common.readFile wrapper.
    """
    try:
        sRet = readFile(sFile, sMode);
    except:
        sRet = sXcptRet;
    return sRet;

def noxcptRmDir(sDir, oXcptRet = False):
    """
    No exceptions os.rmdir wrapper.
    """
    oRet = True;
    try:
        os.rmdir(sDir);
    except:
        oRet = oXcptRet;
    return oRet;

def noxcptDeleteFile(sFile, oXcptRet = False):
    """
    No exceptions os.remove wrapper.
    """
    oRet = True;
    try:
        os.remove(sFile);
    except:
        oRet = oXcptRet;
    return oRet;


#
# SubProcess.
#

def _processFixPythonInterpreter(aPositionalArgs, dKeywordArgs):
    """
    If the "executable" is a python script, insert the python interpreter at
    the head of the argument list so that it will work on systems which doesn't
    support hash-bang scripts.
    """

    asArgs = dKeywordArgs.get('args');
    if asArgs is None:
        asArgs = aPositionalArgs[0];

    if asArgs[0].endswith('.py'):
        if sys.executable is not None  and  len(sys.executable) > 0:
            asArgs.insert(0, sys.executable);
        else:
            asArgs.insert(0, 'python');

        # paranoia...
        if dKeywordArgs.get('args') is not None:
            dKeywordArgs['args'] = asArgs;
        else:
            aPositionalArgs = (asArgs,) + aPositionalArgs[1:];
    return None;

def processCall(*aPositionalArgs, **dKeywordArgs):
    """
    Wrapper around subprocess.call to deal with its absense in older
    python versions.
    Returns process exit code (see subprocess.poll).
    """
    assert dKeywordArgs.get('stdout') == None;
    assert dKeywordArgs.get('stderr') == None;
    _processFixPythonInterpreter(aPositionalArgs, dKeywordArgs);
    oProcess = subprocess.Popen(*aPositionalArgs, **dKeywordArgs);
    return oProcess.wait();

def processOutputChecked(*aPositionalArgs, **dKeywordArgs):
    """
    Wrapper around subprocess.check_output to deal with its absense in older
    python versions.
    """
    _processFixPythonInterpreter(aPositionalArgs, dKeywordArgs);
    oProcess = subprocess.Popen(stdout=subprocess.PIPE, *aPositionalArgs, **dKeywordArgs);

    sOutput, _ = oProcess.communicate();
    iExitCode  = oProcess.poll();

    if iExitCode is not 0:
        asArgs = dKeywordArgs.get('args');
        if asArgs is None:
            asArgs = aPositionalArgs[0];
        print(sOutput);
        raise subprocess.CalledProcessError(iExitCode, asArgs);

    return str(sOutput); # str() make pylint happy.

g_fOldSudo = None;
def _sudoFixArguments(aPositionalArgs, dKeywordArgs, fInitialEnv = True):
    """
    Adds 'sudo' (or similar) to the args parameter, whereever it is.
    """

    # Are we root?
    fIsRoot = True;
    try:
        fIsRoot = os.getuid() == 0; # pylint: disable=E1101
    except:
        pass;

    # If not, prepend sudo (non-interactive, simulate initial login).
    if fIsRoot is not True:
        asArgs = dKeywordArgs.get('args');
        if asArgs is None:
            asArgs = aPositionalArgs[0];

        # Detect old sudo.
        global g_fOldSudo;
        if g_fOldSudo is None:
            try:
                sVersion = processOutputChecked(['sudo', '-V']);
            except:
                sVersion = '1.7.0';
            sVersion = sVersion.strip().split('\n')[0];
            sVersion = sVersion.replace('Sudo version', '').strip();
            g_fOldSudo = len(sVersion) >= 4 \
                     and sVersion[0] == '1' \
                     and sVersion[1] == '.' \
                     and sVersion[2] <= '6' \
                     and sVersion[3] == '.';

        asArgs.insert(0, 'sudo');
        if not g_fOldSudo:
            asArgs.insert(1, '-n');
        if fInitialEnv and not g_fOldSudo:
            asArgs.insert(1, '-i');

        # paranoia...
        if dKeywordArgs.get('args') is not None:
            dKeywordArgs['args'] = asArgs;
        else:
            aPositionalArgs = (asArgs,) + aPositionalArgs[1:];
    return None;


def sudoProcessCall(*aPositionalArgs, **dKeywordArgs):
    """
    sudo (or similar) + subprocess.call
    """
    _processFixPythonInterpreter(aPositionalArgs, dKeywordArgs);
    _sudoFixArguments(aPositionalArgs, dKeywordArgs);
    return processCall(*aPositionalArgs, **dKeywordArgs);

def sudoProcessOutputChecked(*aPositionalArgs, **dKeywordArgs):
    """
    sudo (or similar) + subprocess.check_output.
    """
    _processFixPythonInterpreter(aPositionalArgs, dKeywordArgs);
    _sudoFixArguments(aPositionalArgs, dKeywordArgs);
    return processOutputChecked(*aPositionalArgs, **dKeywordArgs);

def sudoProcessOutputCheckedNoI(*aPositionalArgs, **dKeywordArgs):
    """
    sudo (or similar) + subprocess.check_output, except '-i' isn't used.
    """
    _processFixPythonInterpreter(aPositionalArgs, dKeywordArgs);
    _sudoFixArguments(aPositionalArgs, dKeywordArgs, False);
    return processOutputChecked(*aPositionalArgs, **dKeywordArgs);

def sudoProcessPopen(*aPositionalArgs, **dKeywordArgs):
    """
    sudo (or similar) + subprocess.Popen.
    """
    _processFixPythonInterpreter(aPositionalArgs, dKeywordArgs);
    _sudoFixArguments(aPositionalArgs, dKeywordArgs);
    return subprocess.Popen(*aPositionalArgs, **dKeywordArgs);


#
# Generic process stuff.
#

def processInterrupt(uPid):
    """
    Sends a SIGINT or equivalent to interrupt the specified process.
    Returns True on success, False on failure.

    On Windows hosts this may not work unless the process happens to be a
    process group leader.
    """
    if sys.platform == 'win32':
        try:
            win32console.GenerateConsoleCtrlEvent(win32con.CTRL_BREAK_EVENT, uPid); # pylint
            fRc = True;
        except:
            fRc = False;
    else:
        try:
            os.kill(uPid, signal.SIGINT);
            fRc = True;
        except:
            fRc = False;
    return fRc;

def sendUserSignal1(uPid):
    """
    Sends a SIGUSR1 or equivalent to nudge the process into shutting down
    (VBoxSVC) or something.
    Returns True on success, False on failure or if not supported (win).

    On Windows hosts this may not work unless the process happens to be a
    process group leader.
    """
    if sys.platform == 'win32':
        fRc = False;
    else:
        try:
            os.kill(uPid, signal.SIGUSR1); # pylint: disable=E1101
            fRc = True;
        except:
            fRc = False;
    return fRc;

def processTerminate(uPid):
    """
    Terminates the process in a nice manner (SIGTERM or equivalent).
    Returns True on success, False on failure.
    """
    fRc = False;
    if sys.platform == 'win32':
        try:
            hProcess = win32api.OpenProcess(win32con.PROCESS_TERMINATE, False, uPid);
        except:
            pass;
        else:
            try:
                win32process.TerminateProcess(hProcess, 0x40010004); # DBG_TERMINATE_PROCESS
                fRc = True;
            except:
                pass;
            win32api.CloseHandle(hProcess)
    else:
        try:
            os.kill(uPid, signal.SIGTERM);
            fRc = True;
        except:
            pass;
    return fRc;

def processKill(uPid):
    """
    Terminates the process with extreme prejudice (SIGKILL).
    Returns True on success, False on failure.
    """
    if sys.platform == 'win32':
        fRc = processTerminate(uPid);
    else:
        try:
            os.kill(uPid, signal.SIGKILL); # pylint: disable=E1101
            fRc = True;
        except:
            fRc = False;
    return fRc;

def processKillWithNameCheck(uPid, sName):
    """
    Like processKill(), but checks if the process name matches before killing
    it.  This is intended for killing using potentially stale pid values.

    Returns True on success, False on failure.
    """

    if processCheckPidAndName(uPid, sName) is not True:
        return False;
    return processKill(uPid);


def processExists(uPid):
    """
    Checks if the specified process exits.
    This will only work if we can signal/open the process.

    Returns True if it positively exists, False otherwise.
    """
    if sys.platform == 'win32':
        fRc = False;
        try:
            hProcess = win32api.OpenProcess(win32con.PROCESS_QUERY_INFORMATION, False, uPid);
        except:
            pass;
        else:
            win32api.CloseHandle(hProcess)
            fRc = True;
    else:
        try:
            os.kill(uPid, 0);
            fRc = True;
        except:
            fRc = False;
    return fRc;

def processCheckPidAndName(uPid, sName):
    """
    Checks if a process PID and NAME matches.
    """
    fRc = processExists(uPid);
    if fRc is not True:
        return False;

    if sys.platform == 'win32':
        try:
            from win32com.client import GetObject; # pylint: disable=F0401
            oWmi = GetObject('winmgmts:');
            aoProcesses = oWmi.InstancesOf('Win32_Process');
            for oProcess in aoProcesses:
                if long(oProcess.Properties_("ProcessId").Value) == uPid:
                    sCurName = oProcess.Properties_("Name").Value;
                    #reporter.log2('uPid=%s sName=%s sCurName=%s' % (uPid, sName, sCurName));
                    sName    = sName.lower();
                    sCurName = sCurName.lower();
                    if os.path.basename(sName) == sName:
                        sCurName = os.path.basename(sCurName);

                    if   sCurName == sName \
                      or sCurName + '.exe' == sName \
                      or sCurName == sName  + '.exe':
                        fRc = True;
                    break;
        except:
            #reporter.logXcpt('uPid=%s sName=%s' % (uPid, sName));
            pass;
    else:
        if sys.platform in ('linux2', ):
            asPsCmd = ['/bin/ps',     '-p', '%u' % (uPid,), '-o', 'fname='];
        elif sys.platform in ('sunos5',):
            asPsCmd = ['/usr/bin/ps', '-p', '%u' % (uPid,), '-o', 'fname='];
        elif sys.platform in ('darwin',):
            asPsCmd = ['/bin/ps',     '-p', '%u' % (uPid,), '-o', 'ucomm='];
        else:
            asPsCmd = None;

        if asPsCmd is not None:
            try:
                oPs = subprocess.Popen(asPsCmd, stdout=subprocess.PIPE);
                sCurName = oPs.communicate()[0];
                iExitCode = oPs.wait();
            except:
                #reporter.logXcpt();
                return False;

            # ps fails with non-zero exit code if the pid wasn't found.
            if iExitCode is not 0:
                return False;
            if sCurName is None:
                return False;
            sCurName = sCurName.strip();
            if sCurName is '':
                return False;

            if os.path.basename(sName) == sName:
                sCurName = os.path.basename(sCurName);
            elif os.path.basename(sCurName) == sCurName:
                sName = os.path.basename(sName);

            if sCurName != sName:
                return False;

            fRc = True;
    return fRc;


class ProcessInfo(object):
    """Process info."""
    def __init__(self, iPid):
        self.iPid       = iPid;
        self.iParentPid = None;
        self.sImage     = None;
        self.sName      = None;
        self.asArgs     = None;
        self.sCwd       = None;
        self.iGid       = None;
        self.iUid       = None;
        self.iProcGroup = None;
        self.iSessionId = None;

    def loadAll(self):
        """Load all the info."""
        sOs = getHostOs();
        if sOs == 'linux':
            sProc = '/proc/%s/' % (self.iPid,);
            if self.sImage   is None: self.sImage = noxcptReadLink(sProc + 'exe', None);
            if self.sCwd     is None: self.sCwd   = noxcptReadLink(sProc + 'cwd', None);
            if self.asArgs   is None: self.asArgs = noxcptReadFile(sProc + 'cmdline', '').split('\x00');
        elif sOs == 'solaris':
            sProc = '/proc/%s/' % (self.iPid,);
            if self.sImage   is None: self.sImage = noxcptReadLink(sProc + 'path/a.out', None);
            if self.sCwd     is None: self.sCwd   = noxcptReadLink(sProc + 'path/cwd', None);
        else:
            pass;
        if self.sName is None and self.sImage is not None:
            self.sName = self.sImage;

    def windowsGrabProcessInfo(self, oProcess):
        """Windows specific loadAll."""
        try:    self.sName      = oProcess.Properties_("Name").Value;
        except: pass;
        try:    self.sImage     = oProcess.Properties_("ExecutablePath").Value;
        except: pass;
        try:    self.asArgs     = oProcess.Properties_("CommandLine").Value; ## @todo split it.
        except: pass;
        try:    self.iParentPid = oProcess.Properties_("ParentProcessId").Value;
        except: pass;
        try:    self.iSessionId = oProcess.Properties_("SessionId").Value;
        except: pass;
        if self.sName is None and self.sImage is not None:
            self.sName = self.sImage;

    def getBaseImageName(self):
        """
        Gets the base image name if available, use the process name if not available.
        Returns image/process base name or None.
        """
        sRet = self.sImage if self.sName is None else self.sName;
        if sRet is None:
            self.loadAll();
            sRet = self.sImage if self.sName is None else self.sName;
            if sRet is None:
                if self.asArgs is None or len(self.asArgs) == 0:
                    return None;
                sRet = self.asArgs[0];
                if len(sRet) == 0:
                    return None;
        return os.path.basename(sRet);

    def getBaseImageNameNoExeSuff(self):
        """
        Same as getBaseImageName, except any '.exe' or similar suffix is stripped.
        """
        sRet = self.getBaseImageName();
        if sRet is not None and len(sRet) > 4 and sRet[-4] == '.':
            if (sRet[-4:]).lower() in [ '.exe', '.com', '.msc', '.vbs', '.cmd', '.bat' ]:
                sRet = sRet[:-4];
        return sRet;


def processListAll(): # pylint: disable=R0914
    """
    Return a list of ProcessInfo objects for all the processes in the system
    that the current user can see.
    """
    asProcesses = [];

    sOs = getHostOs();
    if sOs == 'win':
        from win32com.client import GetObject; # pylint: disable=F0401
        oWmi = GetObject('winmgmts:');
        aoProcesses = oWmi.InstancesOf('Win32_Process');
        for oProcess in aoProcesses:
            try:
                iPid = int(oProcess.Properties_("ProcessId").Value);
            except:
                continue;
            oMyInfo = ProcessInfo(iPid);
            oMyInfo.windowsGrabProcessInfo(oProcess);
            asProcesses.append(oMyInfo);

    elif sOs in [ 'linux', 'solaris' ]:
        try:
            asDirs = os.listdir('/proc');
        except:
            asDirs = [];
        for sDir in asDirs:
            if sDir.isdigit():
                asProcesses.append(ProcessInfo(int(sDir),));

    elif sOs == 'darwin':
        # Try our best to parse ps output. (Not perfect but does the job most of the time.)
        try:
            sRaw = processOutputChecked([ '/bin/ps', '-A',
                                          '-o', 'pid=',
                                          '-o', 'ppid=',
                                          '-o', 'pgid=',
                                          '-o', 'sess=',
                                          '-o', 'uid=',
                                          '-o', 'gid=',
                                          '-o', 'comm=' ]);
        except:
            return asProcesses;

        for sLine in sRaw.split('\n'):
            sLine = sLine.lstrip();
            if len(sLine) < 7 or not sLine[0].isdigit():
                continue;

            iField   = 0;
            off      = 0;
            aoFields = [None, None, None, None, None, None, None];
            while iField < 7:
                # Eat whitespace.
                while off < len(sLine) and (sLine[off] == ' ' or sLine[off] == '\t'):
                    off += 1;

                # Final field / EOL.
                if iField == 6:
                    aoFields[6] = sLine[off:];
                    break;
                if off >= len(sLine):
                    break;

                # Generic field parsing.
                offStart = off;
                off += 1;
                while off < len(sLine) and sLine[off] != ' ' and sLine[off] != '\t':
                    off += 1;
                try:
                    if iField != 3:
                        aoFields[iField] = int(sLine[offStart:off]);
                    else:
                        aoFields[iField] = long(sLine[offStart:off], 16); # sess is a hex address.
                except:
                    pass;
                iField += 1;

            if aoFields[0] is not None:
                oMyInfo = ProcessInfo(aoFields[0]);
                oMyInfo.iParentPid = aoFields[1];
                oMyInfo.iProcGroup = aoFields[2];
                oMyInfo.iSessionId = aoFields[3];
                oMyInfo.iUid       = aoFields[4];
                oMyInfo.iGid       = aoFields[5];
                oMyInfo.sName      = aoFields[6];
                asProcesses.append(oMyInfo);

    return asProcesses;


def processCollectCrashInfo(uPid, fnLog, fnCrashFile):
    """
    Looks for information regarding the demise of the given process.
    """
    sOs = getHostOs();
    if sOs == 'darwin':
        #
        # On darwin we look for crash and diagnostic reports.
        #
        asLogDirs = [
            u'/Library/Logs/DiagnosticReports/',
            u'/Library/Logs/CrashReporter/',
            u'~/Library/Logs/DiagnosticReports/',
            u'~/Library/Logs/CrashReporter/',
        ];
        for sDir in asLogDirs:
            sDir = os.path.expanduser(sDir);
            if not os.path.isdir(sDir):
                continue;
            try:
                asDirEntries = os.listdir(sDir);
            except:
                continue;
            for sEntry in asDirEntries:
                # Only interested in .crash files.
                _, sSuff = os.path.splitext(sEntry);
                if sSuff != '.crash':
                    continue;

                # The pid can be found at the end of the first line.
                sFull = os.path.join(sDir, sEntry);
                try:
                    oFile = open(sFull, 'r');
                    sFirstLine = oFile.readline();
                    oFile.close();
                except:
                    continue;
                if len(sFirstLine) <= 4 or sFirstLine[-2] != ']':
                    continue;
                offPid = len(sFirstLine) - 3;
                while offPid > 1 and sFirstLine[offPid - 1].isdigit():
                    offPid -= 1;
                try:    uReportPid = int(sFirstLine[offPid:-2]);
                except: continue;

                # Does the pid we found match?
                if uReportPid == uPid:
                    fnLog('Found crash report for %u: %s' % (uPid, sFull,));
                    fnCrashFile(sFull, False);
    elif sOs == 'win':
        #
        # Getting WER reports would be great, however we have trouble match the
        # PID to those as they seems not to mention it in the brief reports.
        # Instead we'll just look for crash dumps in C:\CrashDumps (our custom
        # location - see the windows readme for the testbox script) and what
        # the MSDN article lists for now.
        #
        # It's been observed on Windows server 2012 that the dump files takes
        # the form: <processimage>.<decimal-pid>.dmp
        #
        asDmpDirs = [
            u'%SystemDrive%/CrashDumps/',                   # Testboxes.
            u'%LOCALAPPDATA%/CrashDumps/',                  # MSDN example.
            u'%WINDIR%/ServiceProfiles/LocalServices/',     # Local and network service.
            u'%WINDIR%/ServiceProfiles/NetworkSerices/',
            u'%WINDIR%/ServiceProfiles/',
            u'%WINDIR%/System32/Config/SystemProfile/',     # System services.
        ];
        sMatchSuffix = '.%u.dmp' % (uPid,);

        for sDir in asDmpDirs:
            sDir = os.path.expandvars(sDir);
            if not os.path.isdir(sDir):
                continue;
            try:
                asDirEntries = os.listdir(sDir);
            except:
                continue;
            for sEntry in asDirEntries:
                if sEntry.endswith(sMatchSuffix):
                    sFull = os.path.join(sDir, sEntry);
                    fnLog('Found crash dump for %u: %s' % (uPid, sFull,));
                    fnCrashFile(sFull, True);

    else:
        pass; ## TODO
    return None;


#
# Time.
#

def timestampNano():
    """
    Gets a nanosecond timestamp.
    """
    if sys.platform == 'win32':
        return long(time.clock() * 1000000000);
    return long(time.time() * 1000000000);

def timestampMilli():
    """
    Gets a millisecond timestamp.
    """
    if sys.platform == 'win32':
        return long(time.clock() * 1000);
    return long(time.time() * 1000);

def timestampSecond():
    """
    Gets a second timestamp.
    """
    if sys.platform == 'win32':
        return long(time.clock());
    return long(time.time());

def getTimePrefix():
    """
    Returns a timestamp prefix, typically used for logging. UTC.
    """
    try:
        oNow = datetime.datetime.utcnow();
        sTs = '%02u:%02u:%02u.%06u' % (oNow.hour, oNow.minute, oNow.second, oNow.microsecond);
    except:
        sTs = 'getTimePrefix-exception';
    return sTs;

def getTimePrefixAndIsoTimestamp():
    """
    Returns current UTC as log prefix and iso timestamp.
    """
    try:
        oNow = datetime.datetime.utcnow();
        sTsPrf = '%02u:%02u:%02u.%06u' % (oNow.hour, oNow.minute, oNow.second, oNow.microsecond);
        sTsIso = formatIsoTimestamp(oNow);
    except:
        sTsPrf = sTsIso = 'getTimePrefix-exception';
    return (sTsPrf, sTsIso);

def formatIsoTimestamp(oNow):
    """Formats the datetime object as an ISO timestamp."""
    assert oNow.tzinfo is None;
    sTs = '%s.%09uZ' % (oNow.strftime('%Y-%m-%dT%H:%M:%S'), oNow.microsecond * 1000);
    return sTs;

def getIsoTimestamp():
    """Returns the current UTC timestamp as a string."""
    return formatIsoTimestamp(datetime.datetime.utcnow());


def getLocalHourOfWeek():
    """ Local hour of week (0 based). """
    oNow = datetime.datetime.now();
    return (oNow.isoweekday() - 1) * 24 + oNow.hour;


def formatIntervalSeconds(cSeconds):
    """ Format a seconds interval into a nice 01h 00m 22s string  """
    # Two simple special cases.
    if cSeconds < 60:
        return '%ss' % (cSeconds,);
    if cSeconds < 3600:
        cMins = cSeconds / 60;
        cSecs = cSeconds % 60;
        if cSecs == 0:
            return '%sm' % (cMins,);
        return '%sm %ss' % (cMins, cSecs,);

    # Generic and a bit slower.
    cDays     = cSeconds / 86400;
    cSeconds %= 86400;
    cHours    = cSeconds / 3600;
    cSeconds %= 3600;
    cMins     = cSeconds / 60;
    cSecs     = cSeconds % 60;
    sRet = '';
    if cDays > 0:
        sRet = '%sd ' % (cDays,);
    if cHours > 0:
        sRet += '%sh ' % (cHours,);
    if cMins > 0:
        sRet += '%sm ' % (cMins,);
    if cSecs > 0:
        sRet += '%ss ' % (cSecs,);
    assert len(sRet) > 0; assert sRet[-1] == ' ';
    return sRet[:-1];

def formatIntervalSeconds2(oSeconds):
    """
    Flexible input version of formatIntervalSeconds for use in WUI forms where
    data is usually already string form.
    """
    if isinstance(oSeconds, int) or isinstance(oSeconds, long):
        return formatIntervalSeconds(oSeconds);
    if not isString(oSeconds):
        try:
            lSeconds = long(oSeconds);
        except:
            pass;
        else:
            if lSeconds >= 0:
                return formatIntervalSeconds2(lSeconds);
    return oSeconds;

def parseIntervalSeconds(sString):
    """
    Reverse of formatIntervalSeconds.

    Returns (cSeconds, sError), where sError is None on success.
    """

    # We might given non-strings, just return them without any fuss.
    if not isString(sString):
        if isinstance(sString, int) or isinstance(sString, long) or  sString is None:
            return (sString, None);
        ## @todo time/date objects?
        return (int(sString), None);

    # Strip it and make sure it's not empty.
    sString = sString.strip();
    if len(sString) == 0:
        return (0, 'Empty interval string.');

    #
    # Split up the input into a list of 'valueN, unitN, ...'.
    #
    # Don't want to spend too much time trying to make re.split do exactly what
    # I need here, so please forgive the extra pass I'm making here.
    #
    asRawParts = re.split(r'\s*([0-9]+)\s*([^0-9,;]*)[\s,;]*', sString);
    asParts    = [];
    for sPart in asRawParts:
        sPart = sPart.strip();
        if len(sPart) > 0:
            asParts.append(sPart);
    if len(asParts) == 0:
        return (0, 'Empty interval string or something?');

    #
    # Process them one or two at the time.
    #
    cSeconds   = 0;
    asErrors   = [];
    i          = 0;
    while i < len(asParts):
        sNumber = asParts[i];
        i += 1;
        if sNumber.isdigit():
            iNumber = int(sNumber);

            sUnit = 's';
            if i < len(asParts) and not asParts[i].isdigit():
                sUnit = asParts[i];
                i += 1;

            sUnitLower = sUnit.lower();
            if sUnitLower in [ 's', 'se', 'sec', 'second', 'seconds' ]:
                pass;
            elif sUnitLower in [ 'm', 'mi', 'min', 'minute', 'minutes' ]:
                iNumber *= 60;
            elif sUnitLower in [ 'h', 'ho', 'hou', 'hour', 'hours' ]:
                iNumber *= 3600;
            elif sUnitLower in [ 'd', 'da', 'day', 'days' ]:
                iNumber *= 86400;
            elif sUnitLower in [ 'w', 'week', 'weeks' ]:
                iNumber *= 7 * 86400;
            else:
                asErrors.append('Unknown unit "%s".' % (sUnit,));
            cSeconds += iNumber;
        else:
            asErrors.append('Bad number "%s".' % (sNumber,));
    return (cSeconds, None if len(asErrors) == 0 else ' '.join(asErrors));

def formatIntervalHours(cHours):
    """ Format a hours interval into a nice 1w 2d 1h string. """
    # Simple special cases.
    if cHours < 24:
        return '%sh' % (cHours,);

    # Generic and a bit slower.
    cWeeks    = cHours / (7 * 24);
    cHours   %= 7 * 24;
    cDays     = cHours / 24;
    cHours   %= 24;
    sRet = '';
    if cWeeks > 0:
        sRet = '%sw ' % (cWeeks,);
    if cDays > 0:
        sRet = '%sd ' % (cDays,);
    if cHours > 0:
        sRet += '%sh ' % (cHours,);
    assert len(sRet) > 0; assert sRet[-1] == ' ';
    return sRet[:-1];

def parseIntervalHours(sString):
    """
    Reverse of formatIntervalHours.

    Returns (cHours, sError), where sError is None on success.
    """

    # We might given non-strings, just return them without any fuss.
    if not isString(sString):
        if isinstance(sString, int) or isinstance(sString, long) or  sString is None:
            return (sString, None);
        ## @todo time/date objects?
        return (int(sString), None);

    # Strip it and make sure it's not empty.
    sString = sString.strip();
    if len(sString) == 0:
        return (0, 'Empty interval string.');

    #
    # Split up the input into a list of 'valueN, unitN, ...'.
    #
    # Don't want to spend too much time trying to make re.split do exactly what
    # I need here, so please forgive the extra pass I'm making here.
    #
    asRawParts = re.split(r'\s*([0-9]+)\s*([^0-9,;]*)[\s,;]*', sString);
    asParts    = [];
    for sPart in asRawParts:
        sPart = sPart.strip();
        if len(sPart) > 0:
            asParts.append(sPart);
    if len(asParts) == 0:
        return (0, 'Empty interval string or something?');

    #
    # Process them one or two at the time.
    #
    cHours     = 0;
    asErrors   = [];
    i          = 0;
    while i < len(asParts):
        sNumber = asParts[i];
        i += 1;
        if sNumber.isdigit():
            iNumber = int(sNumber);

            sUnit = 'h';
            if i < len(asParts) and not asParts[i].isdigit():
                sUnit = asParts[i];
                i += 1;

            sUnitLower = sUnit.lower();
            if sUnitLower in [ 'h', 'ho', 'hou', 'hour', 'hours' ]:
                pass;
            elif sUnitLower in [ 'd', 'da', 'day', 'days' ]:
                iNumber *= 24;
            elif sUnitLower in [ 'w', 'week', 'weeks' ]:
                iNumber *= 7 * 24;
            else:
                asErrors.append('Unknown unit "%s".' % (sUnit,));
            cHours += iNumber;
        else:
            asErrors.append('Bad number "%s".' % (sNumber,));
    return (cHours, None if len(asErrors) == 0 else ' '.join(asErrors));


#
# Introspection.
#

def getCallerName(oFrame=None, iFrame=2):
    """
    Returns the name of the caller's caller.
    """
    if oFrame is None:
        try:
            raise Exception();
        except:
            oFrame = sys.exc_info()[2].tb_frame.f_back;
        while iFrame > 1:
            if oFrame is not None:
                oFrame = oFrame.f_back;
            iFrame = iFrame - 1;
    if oFrame is not None:
        sName = '%s:%u' % (oFrame.f_code.co_name, oFrame.f_lineno);
        return sName;
    return "unknown";


def getXcptInfo(cFrames = 1):
    """
    Gets text detailing the exception. (Good for logging.)
    Returns list of info strings.
    """

    #
    # Try get exception info.
    #
    try:
        oType, oValue, oTraceback = sys.exc_info();
    except:
        oType = oValue = oTraceback = None;
    if oType is not None:

        #
        # Try format the info
        #
        asRet = [];
        try:
            try:
                asRet = asRet + traceback.format_exception_only(oType, oValue);
                asTraceBack = traceback.format_tb(oTraceback);
                if cFrames is not None and cFrames <= 1:
                    asRet.append(asTraceBack[-1]);
                else:
                    asRet.append('Traceback:')
                    for iFrame in range(min(cFrames, len(asTraceBack))):
                        asRet.append(asTraceBack[-iFrame - 1]);
                    asRet.append('Stack:')
                    asRet = asRet + traceback.format_stack(oTraceback.tb_frame.f_back, cFrames);
            except:
                asRet.append('internal-error: Hit exception #2! %s' % (traceback.format_exc(),));

            if len(asRet) == 0:
                asRet.append('No exception info...');
        except:
            asRet.append('internal-error: Hit exception! %s' % (traceback.format_exc(),));
    else:
        asRet = ['Couldn\'t find exception traceback.'];
    return asRet;


#
# TestSuite stuff.
#

def isRunningFromCheckout(cScriptDepth = 1):
    """
    Checks if we're running from the SVN checkout or not.
    """

    try:
        sFile = __file__;
        cScriptDepth = 1;
    except:
        sFile = sys.argv[0];

    sDir = os.path.abspath(sFile);
    while cScriptDepth >= 0:
        sDir = os.path.dirname(sDir);
        if    os.path.exists(os.path.join(sDir, 'Makefile.kmk')) \
           or os.path.exists(os.path.join(sDir, 'Makefile.kup')):
            return True;
        cScriptDepth -= 1;

    return False;


#
# Bourne shell argument fun.
#


def argsSplit(sCmdLine):
    """
    Given a bourne shell command line invocation, split it up into arguments
    assuming IFS is space.
    Returns None on syntax error.
    """
    ## @todo bourne shell argument parsing!
    return sCmdLine.split(' ');

def argsGetFirst(sCmdLine):
    """
    Given a bourne shell command line invocation, get return the first argument
    assuming IFS is space.
    Returns None on invalid syntax, otherwise the parsed and unescaped argv[0] string.
    """
    asArgs = argsSplit(sCmdLine);
    if asArgs is None  or  len(asArgs) == 0:
        return None;

    return asArgs[0];

#
# String helpers.
#

def stricmp(sFirst, sSecond):
    """
    Compares to strings in an case insensitive fashion.

    Python doesn't seem to have any way of doing the correctly, so this is just
    an approximation using lower.
    """
    if sFirst == sSecond:
        return 0;
    sLower1 = sFirst.lower();
    sLower2 = sSecond.lower();
    if sLower1 == sLower2:
        return 0;
    if sLower1 < sLower2:
        return -1;
    return 1;


#
# Misc.
#

def versionCompare(sVer1, sVer2):
    """
    Compares to version strings in a fashion similar to RTStrVersionCompare.
    """

    ## @todo implement me!!

    if sVer1 == sVer2:
        return 0;
    if sVer1 < sVer2:
        return -1;
    return 1;


def formatNumber(lNum, sThousandSep = ' '):
    """
    Formats a decimal number with pretty separators.
    """
    sNum = str(lNum);
    sRet = sNum[-3:];
    off  = len(sNum) - 3;
    while off > 0:
        off -= 3;
        sRet = sNum[(off if off >= 0 else 0):(off + 3)] + sThousandSep + sRet;
    return sRet;


def formatNumberNbsp(lNum):
    """
    Formats a decimal number with pretty separators.
    """
    sRet = formatNumber(lNum);
    return unicode(sRet).replace(' ', u'\u00a0');


def isString(oString):
    """
    Checks if the object is a string object, hiding difference between python 2 and 3.

    Returns True if it's a string of some kind.
    Returns False if not.
    """
    if sys.version_info[0] >= 3:
        return isinstance(oString, str);
    return isinstance(oString, basestring);


def hasNonAsciiCharacters(sText):
    """
    Returns True is specified string has non-ASCII characters.
    """
    sTmp = unicode(sText, errors='ignore') if isinstance(sText, str) else sText
    return not all(ord(cChar) < 128 for cChar in sTmp)


def chmodPlusX(sFile):
    """
    Makes the specified file or directory executable.
    Returns success indicator, no exceptions.

    Note! Symbolic links are followed and the target will be changed.
    """
    try:
        oStat = os.stat(sFile);
    except:
        return False;
    try:
        os.chmod(sFile, oStat.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH);
    except:
        return False;
    return True;


def unpackFile(sArchive, sDstDir, fnLog, fnError = None):
    """
    Unpacks the given file if it has a know archive extension, otherwise do
    nothing.

    Returns list of the extracted files (full path) on success.
    Returns empty list if not a supported archive format.
    Returns None on failure.  Raises no exceptions.
    """
    if fnError is None:
        fnError = fnLog;

    asMembers = [];

    sBaseNameLower = os.path.basename(sArchive).lower();
    if sBaseNameLower.endswith('.zip'):
        fnLog('Unzipping "%s" to "%s"...' % (sArchive, sDstDir));
        try:
            oZipFile = zipfile.ZipFile(sArchive, 'r')
            asMembers = oZipFile.namelist();
            for sMember in asMembers:
                if sMember.endswith('/'):
                    os.makedirs(os.path.join(sDstDir, sMember.replace('/', os.path.sep)), 0775);
                else:
                    oZipFile.extract(sMember, sDstDir);
            oZipFile.close();
        except Exception, oXcpt:
            fnError('Error unpacking "%s" into "%s": %s' % (sArchive, sDstDir, oXcpt));
            return None;

    elif sBaseNameLower.endswith('.tar') \
      or sBaseNameLower.endswith('.tar.gz') \
      or sBaseNameLower.endswith('.tgz') \
      or sBaseNameLower.endswith('.tar.bz2'):
        fnLog('Untarring "%s" to "%s"...' % (sArchive, sDstDir));
        try:
            oTarFile = tarfile.open(sArchive, 'r:*');
            asMembers = [oTarInfo.name for oTarInfo in oTarFile.getmembers()];
            oTarFile.extractall(sDstDir);
            oTarFile.close();
        except Exception, oXcpt:
            fnError('Error unpacking "%s" into "%s": %s' % (sArchive, sDstDir, oXcpt));
            return None;

    else:
        fnLog('Not unpacking "%s".' % (sArchive,));
        return [];

    #
    # Change asMembers to local slashes and prefix with path.
    #
    asMembersRet = [];
    for sMember in asMembers:
        asMembersRet.append(os.path.join(sDstDir, sMember.replace('/', os.path.sep)));

    return asMembersRet;


def getDiskUsage(sPath):
    """
    Get free space of a partition that corresponds to specified sPath in MB.

    Returns partition free space value in MB.
    """
    if platform.system() == 'Windows':
        import ctypes
        oCTypeFreeSpace = ctypes.c_ulonglong(0);
        ctypes.windll.kernel32.GetDiskFreeSpaceExW(ctypes.c_wchar_p(sPath), None, None,
                                                   ctypes.pointer(oCTypeFreeSpace));
        cbFreeSpace = oCTypeFreeSpace.value;
    else:
        oStats = os.statvfs(sPath); # pylint: disable=E1101
        cbFreeSpace = long(oStats.f_frsize) * oStats.f_bfree;

    # Convert to MB
    cMbFreeSpace = long(cbFreeSpace) / (1024 * 1024);

    return cMbFreeSpace;


#
# Unit testing.
#

# pylint: disable=C0111
class BuildCategoryDataTestCase(unittest.TestCase):
    def testIntervalSeconds(self):
        self.assertEqual(parseIntervalSeconds(formatIntervalSeconds(3600)), (3600, None));
        self.assertEqual(parseIntervalSeconds(formatIntervalSeconds(1209438593)), (1209438593, None));
        self.assertEqual(parseIntervalSeconds('123'), (123, None));
        self.assertEqual(parseIntervalSeconds(123), (123, None));
        self.assertEqual(parseIntervalSeconds(99999999999), (99999999999, None));
        self.assertEqual(parseIntervalSeconds(''), (0, 'Empty interval string.'));
        self.assertEqual(parseIntervalSeconds('1X2'), (3, 'Unknown unit "X".'));
        self.assertEqual(parseIntervalSeconds('1 Y3'), (4, 'Unknown unit "Y".'));
        self.assertEqual(parseIntervalSeconds('1 Z 4'), (5, 'Unknown unit "Z".'));
        self.assertEqual(parseIntervalSeconds('1 hour 2m 5second'), (3725, None));
        self.assertEqual(parseIntervalSeconds('1 hour,2m ; 5second'), (3725, None));

if __name__ == '__main__':
    unittest.main();
    # not reached.

