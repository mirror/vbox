#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
AUtostart testcase using.
"""

__copyright__ = \
"""
Copyright (C) 2013-2020 Oracle Corporation

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
__version__ = "$Id$"


# Standard Python imports.
import array;
import os;
import sys;
import re;
import ssl;

# Python 3 hacks:
if sys.version_info[0] < 3:
    import urllib2 as urllib; # pylint: disable=import-error,no-name-in-module
    from urllib2        import ProxyHandler as urllib_ProxyHandler; # pylint: disable=import-error,no-name-in-module
    from urllib2        import build_opener as urllib_build_opener; # pylint: disable=import-error,no-name-in-module
else:
    import urllib; # pylint: disable=import-error,no-name-in-module
    from urllib.request import ProxyHandler as urllib_ProxyHandler; # pylint: disable=import-error,no-name-in-module
    from urllib.request import build_opener as urllib_build_opener; # pylint: disable=import-error,no-name-in-module


# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0];
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksValidationKitDir);

# Validation Kit imports.
from testdriver import reporter;
from testdriver import base;
from testdriver import vbox;
from testdriver import vboxcon;
from testdriver import vboxwrappers;
from common     import utils;

# Python 3 hacks:
if sys.version_info[0] >= 3:
    long = int      # pylint: disable=redefined-builtin,invalid-name
    xrange = range; # pylint: disable=redefined-builtin,invalid-name


class VBoxManageStdOutWrapper(object):
    """ Parser for VBoxManage list runningvms """
    def __init__(self):
        self.sVmRunning = '';

    def __del__(self):
        self.close();

    def close(self):
        """file.close"""
        return;

    def read(self, cb):
        """file.read"""
        _ = cb;
        return "";

    def write(self, sText):
        """VBoxManage stdout write"""
        if sText is None:
            return None;

        try:    sText = str(sText); # pylint: disable=redefined-variable-type
        except: pass;

        asLines = sText.splitlines();
        for sLine in asLines:
            sLine = sLine.strip();

            reporter.log('Logging: ' + sLine);

                # Extract the value
            idxVmNameStart = sLine.find('"');
            if idxVmNameStart == -1:
                raise Exception('VBoxManageStdOutWrapper: Invalid output');

            idxVmNameStart += 1;
            idxVmNameEnd = idxVmNameStart;
            while sLine[idxVmNameEnd] != '"':
                idxVmNameEnd += 1;

            self.sVmRunning = sLine[idxVmNameStart:idxVmNameEnd];
            reporter.log('Logging: ' + self.sVmRunning);

        return None;


def downloadFile(sUrlFile, sDstFile, sLocalPrefix, fnLog, fnError = None, fNoProxies=True):
    """
    Downloads the given file if an URL is given, otherwise assume it's
    something on the build share and copy it from there.

    Raises no exceptions, returns log + success indicator instead.

    Note! This method may use proxies configured on the system and the
          http_proxy, ftp_proxy, no_proxy environment variables.

    """
    if fnError is None:
        fnError = fnLog;

    if  sUrlFile.startswith('http://') \
     or sUrlFile.startswith('https://') \
     or sUrlFile.startswith('ftp://'):
        # Download the file.
        fnLog('Downloading "%s" to "%s"...' % (sUrlFile, sDstFile));
        try:
            # Disable SSL certificate verification for our servers
            ssl_ctx = ssl.create_default_context();
            ssl_ctx.check_hostname = False;
            ssl_ctx.verify_mode = ssl.CERT_NONE;

            ## @todo We get 404.html content instead of exceptions here, which is confusing and should be addressed.
            if not fNoProxies:
                oOpener = urllib_build_opener(urllib.HTTPSHandler(context = ssl_ctx));
            else:
                oOpener = urllib_build_opener(urllib.HTTPSHandler(context = ssl_ctx), urllib_ProxyHandler(proxies = dict()));
            oSrc = oOpener.open(sUrlFile);
            oDst = utils.openNoInherit(sDstFile, 'wb');
            oDst.write(oSrc.read());
            oDst.close();
            oSrc.close();
        except Exception as oXcpt:
            fnError('Error downloading "%s" to "%s": %s' % (sUrlFile, sDstFile, oXcpt));
            return False;
    else:
        # Assumes file from the build share.
        sSrcPath = os.path.join(sLocalPrefix, sUrlFile);
        fnLog('Copying "%s" to "%s"...' % (sSrcPath, sDstFile));
        try:
            utils.copyFileSimple(sSrcPath, sDstFile);
        except Exception as oXcpt:
            fnError('Error copying "%s" to "%s": %s' % (sSrcPath, sDstFile, oXcpt));
            return False;

    return True;


class tdAutostartOs(object):
    """
    Base autostart helper class to provide common methods.
    """

    def __init__(self, oTestDriver, fpApiVer, sGuestAdditionsIso):
        self.oTestDriver = oTestDriver;
        self.fpApiVer = fpApiVer;
        self.sGuestAdditionsIso = sGuestAdditionsIso;

    def _findFile(self, sRegExp, sTestBuildDir):
        """
        Returns a filepath based on the given regex and path to look into
        or None if no matching file is found.
        """

        oRegExp = re.compile(sRegExp);

        #return most recent file if there are several ones matching the pattern
        asFiles = [s for s in os.listdir(sTestBuildDir)
                   if os.path.isfile(os.path.join(sTestBuildDir, s))];
        asFiles = (s for s in asFiles
                   if oRegExp.match(os.path.basename(s))
                   and os.path.exists(sTestBuildDir + '/' + s));
        asFiles = sorted(asFiles, reverse = True, key = lambda s: os.path.getmtime(os.path.join(sTestBuildDir, s)));
        if asFiles:
            return sTestBuildDir + '/' + asFiles[0];

        reporter.error('Failed to find a file matching "%s" in %s.' % (sRegExp, sTestBuildDir));
        return None;

    def _createAutostartCfg(self, sDefaultPolicy = 'allow', asUserAllow = (), asUserDeny = ()):
        """
        Creates a autostart config for VirtualBox
        """

        sVBoxCfg = 'default_policy=' + sDefaultPolicy + '\n';

        for sUserAllow in asUserAllow:
            sVBoxCfg = sVBoxCfg + sUserAllow + ' = {\n allow = true\n }\n';

        for sUserDeny in asUserDeny:
            sVBoxCfg = sVBoxCfg + sUserDeny + ' = {\n allow = false\n }\n';

        return sVBoxCfg;

    def createSession(self, oSession, sName, sUser, sPassword, cMsTimeout = 10 * 1000, fIsError = True):
        """
        Creates (opens) a guest session.
        Returns (True, IGuestSession) on success or (False, None) on failure.
        """
        oGuest = oSession.o.console.guest;
        if sName is None:
            sName = "<untitled>";

        reporter.log('Creating session "%s" ...' % (sName,));
        try:
            oGuestSession = oGuest.createSession(sUser, sPassword, '', sName);
        except:
            # Just log, don't assume an error here (will be done in the main loop then).
            reporter.maybeErrXcpt(fIsError, 'Creating a guest session "%s" failed; sUser="%s", pw="%s"'
                                  % (sName, sUser, sPassword));
            return (False, None);

        reporter.log('Waiting for session "%s" to start within %dms...' % (sName, cMsTimeout));
        aeWaitFor = [ vboxcon.GuestSessionWaitForFlag_Start, ];
        try:
            waitResult = oGuestSession.waitForArray(aeWaitFor, cMsTimeout);

            #
            # Be nice to Guest Additions < 4.3: They don't support session handling and
            # therefore return WaitFlagNotSupported.
            #
            if waitResult not in (vboxcon.GuestSessionWaitResult_Start, vboxcon.GuestSessionWaitResult_WaitFlagNotSupported):
                # Just log, don't assume an error here (will be done in the main loop then).
                reporter.maybeErr(fIsError, 'Session did not start successfully, returned wait result: %d' % (waitResult,));
                return (False, None);
            reporter.log('Session "%s" successfully started' % (sName,));
        except:
            # Just log, don't assume an error here (will be done in the main loop then).
            reporter.maybeErrXcpt(fIsError, 'Waiting for guest session "%s" (usr=%s;pw=%s) to start failed:'
                                  % (sName, sUser, sPassword,));
            return (False, None);
        return (True, oGuestSession);

    def closeSession(self, oGuestSession, fIsError = True):
        """
        Closes the guest session.
        """
        if oGuestSession is not None:
            try:
                sName = oGuestSession.name;
            except:
                return reporter.errorXcpt();

            reporter.log('Closing session "%s" ...' % (sName,));
            try:
                oGuestSession.close();
                oGuestSession = None;
            except:
                # Just log, don't assume an error here (will be done in the main loop then).
                reporter.maybeErrXcpt(fIsError, 'Closing guest session "%s" failed:' % (sName,));
                return False;
        return True;

    def guestProcessExecute(self, oGuestSession, sTestName, cMsTimeout, sExecName, asArgs = (),
                            fGetStdOut = True, fIsError = True):
        """
        Helper function to execute a program on a guest, specified in the current test.
        Returns (True, ProcessStatus, ProcessExitCode, ProcessStdOutBuffer) on success or (False, 0, 0, None) on failure.
        """
        fRc = True; # Be optimistic.

        reporter.testStart(sTestName);

        reporter.log2('Using session user=%s, name=%s, timeout=%d'
                      % (oGuestSession.user, oGuestSession.name, oGuestSession.timeout,));

        #
        # Start the process:
        #
        reporter.log2('Executing sCmd=%s, timeoutMS=%d, asArgs=%s'
                      % (sExecName, cMsTimeout, asArgs, ));
        fTaskFlags = [];
        if fGetStdOut:
            fTaskFlags = [vboxcon.ProcessCreateFlag_WaitForStdOut,
                          vboxcon.ProcessCreateFlag_WaitForStdErr];
        try:
            oProcess = oGuestSession.processCreate(sExecName,
                                                   asArgs if self.fpApiVer >= 5.0 else asArgs[1:],
                                                   [], fTaskFlags, cMsTimeout);
        except:
            reporter.maybeErrXcpt(fIsError, 'asArgs=%s' % (asArgs,));
            return (False, 0, 0, None);
        if oProcess is None:
            return (reporter.error('oProcess is None! (%s)' % (asArgs,)), 0, 0, None);

        #time.sleep(5); # try this if you want to see races here.

        # Wait for the process to start properly:
        reporter.log2('Process start requested, waiting for start (%dms) ...' % (cMsTimeout,));
        iPid = -1;
        aeWaitFor = [ vboxcon.ProcessWaitForFlag_Start, ];
        aBuf = None;
        try:
            eWaitResult = oProcess.waitForArray(aeWaitFor, cMsTimeout);
        except:
            reporter.maybeErrXcpt(fIsError, 'waitforArray failed for asArgs=%s' % (asArgs,));
            fRc = False;
        else:
            try:
                eStatus = oProcess.status;
                iPid    = oProcess.PID;
            except:
                fRc = reporter.errorXcpt('asArgs=%s' % (asArgs,));
            else:
                reporter.log2('Wait result returned: %d, current process status is: %d' % (eWaitResult, eStatus,));

                #
                # Wait for the process to run to completion if necessary.
                #
                # Note! The above eWaitResult return value can be ignored as it will
                #       (mostly) reflect the process status anyway.
                #
                if eStatus == vboxcon.ProcessStatus_Started:

                    # What to wait for:
                    aeWaitFor = [ vboxcon.ProcessWaitForFlag_Terminate,
                                  vboxcon.ProcessWaitForFlag_StdOut,
                                  vboxcon.ProcessWaitForFlag_StdErr];

                    reporter.log2('Process (PID %d) started, waiting for termination (%dms), aeWaitFor=%s ...'
                                  % (iPid, cMsTimeout, aeWaitFor));
                    acbFdOut = [0,0,0];
                    while True:
                        try:
                            eWaitResult = oProcess.waitForArray(aeWaitFor, cMsTimeout);
                        except KeyboardInterrupt: # Not sure how helpful this is, but whatever.
                            reporter.error('Process (PID %d) execution interrupted' % (iPid,));
                            try: oProcess.close();
                            except: pass;
                            break;
                        except:
                            fRc = reporter.errorXcpt('asArgs=%s' % (asArgs,));
                            break;
                        reporter.log2('Wait returned: %d' % (eWaitResult,));

                        # Process output:
                        for eFdResult, iFd, sFdNm in [ (vboxcon.ProcessWaitResult_StdOut, 1, 'stdout'),
                                                       (vboxcon.ProcessWaitResult_StdErr, 2, 'stderr'), ]:
                            if eWaitResult in (eFdResult, vboxcon.ProcessWaitResult_WaitFlagNotSupported):
                                reporter.log2('Reading %s ...' % (sFdNm,));
                                try:
                                    abBuf = oProcess.Read(1, 64 * 1024, cMsTimeout);
                                except KeyboardInterrupt: # Not sure how helpful this is, but whatever.
                                    reporter.error('Process (PID %d) execution interrupted' % (iPid,));
                                    try: oProcess.close();
                                    except: pass;
                                except:
                                    pass; ## @todo test for timeouts and fail on anything else!
                                else:
                                    if abBuf:
                                        reporter.log2('Process (PID %d) got %d bytes of %s data' % (iPid, len(abBuf), sFdNm,));
                                        acbFdOut[iFd] += len(abBuf);
                                        ## @todo Figure out how to uniform + append!
                                        if aBuf:
                                            aBuf += str(abBuf);
                                        else:
                                            aBuf = str(abBuf);

                        ## Process input (todo):
                        #if eWaitResult in (vboxcon.ProcessWaitResult_StdIn, vboxcon.ProcessWaitResult_WaitFlagNotSupported):
                        #    reporter.log2('Process (PID %d) needs stdin data' % (iPid,));

                        # Termination or error?
                        if eWaitResult in (vboxcon.ProcessWaitResult_Terminate,
                                           vboxcon.ProcessWaitResult_Error,
                                           vboxcon.ProcessWaitResult_Timeout,):
                            try:    eStatus = oProcess.status;
                            except: fRc = reporter.errorXcpt('asArgs=%s' % (asArgs,));
                            reporter.log2('Process (PID %d) reported terminate/error/timeout: %d, status: %d'
                                          % (iPid, eWaitResult, eStatus,));
                            break;

                    # End of the wait loop.
                    _, cbStdOut, cbStdErr = acbFdOut;

                    try:    eStatus = oProcess.status;
                    except: fRc = reporter.errorXcpt('asArgs=%s' % (asArgs,));
                    reporter.log2('Final process status (PID %d) is: %d' % (iPid, eStatus));
                    reporter.log2('Process (PID %d) %d stdout, %d stderr' % (iPid, cbStdOut, cbStdErr));

        #
        # Get the final status and exit code of the process.
        #
        try:
            uExitStatus = oProcess.status;
            iExitCode   = oProcess.exitCode;
        except:
            fRc = reporter.errorXcpt('asArgs=%s' % (asArgs,));
        reporter.log2('Process (PID %d) has exit code: %d; status: %d ' % (iPid, iExitCode, uExitStatus));
        reporter.testDone();

        return (fRc, uExitStatus, iExitCode, aBuf);

    def uploadString(self, oSession, sSrcString, sDst):
        """
        Upload the string into guest.
        """

        fRc, oGuestSession = self.createSession(oSession, 'vbox session for installing guest additions',
                                                'vbox', 'password', cMsTimeout = 10 * 1000, fIsError = True);
        if fRc is not True:
            return reporter.errorXcpt('Upload string failed: Could not create session for vbox');

        try:
            oFile = oGuestSession.fileOpenEx(sDst, vboxcon.FileAccessMode_ReadWrite, vboxcon.FileOpenAction_CreateOrReplace,
                                             vboxcon.FileSharingMode_All, 0, []);
        except:
            fRc = reporter.errorXcpt('Upload string failed. Could not create and open the file %s' % sDst);
        else:
            try:
                oFile.write(bytearray(sSrcString), 60*1000);
            except:
                fRc = reporter.errorXcpt('Upload string failed. Could not write the string into the file %s' % sDst);
        try:
            oFile.close();
        except:
            fRc = reporter.errorXcpt('Upload string failed. Could not close the file %s' % sDst);

        self.closeSession(oGuestSession);
        return fRc;

    def uploadFile(self, oSession, sSrc, sDst):
        """
        Upload the string into guest.
        """

        fRc, oGuestSession = self.createSession(oSession, 'vbox session for upload the file',
                                                'vbox', 'password', cMsTimeout = 10 * 1000, fIsError = True);
        if fRc is not True:
            return reporter.errorXcpt('Upload file failed: Could not create session for vbox');

        try:
            if self.fpApiVer >= 5.0:
                oCurProgress = oGuestSession.fileCopyToGuest(sSrc, sDst, [0]);
            else:
                oCurProgress = oGuestSession.copyTo(sSrc, sDst, [0]);
        except:
            reporter.maybeErrXcpt(True, 'Upload file exception for sSrc="%s":'
                                  % (self.sGuestAdditionsIso,));
            fRc = False;
        else:
            if oCurProgress is not None:
                oWrapperProgress = vboxwrappers.ProgressWrapper(oCurProgress, self.oTestDriver.oVBoxMgr,
                                                                self.oTestDriver, "uploadFile");
                oWrapperProgress.wait();
                if not oWrapperProgress.isSuccess():
                    oWrapperProgress.logResult(fIgnoreErrors = False);
                    fRc = False;
            else:
                fRc = reporter.error('No progress object returned');

        self.closeSession(oGuestSession);
        return fRc;

    def downloadFile(self, oSession, sSrc, sDst, fIgnoreErrors = False):
        """
        Upload the string into guest.
        """

        fRc, oGuestSession = self.createSession(oSession, 'vbox session for download the file',
                                                'vbox', 'password', cMsTimeout = 10 * 1000, fIsError = True);
        if fRc is not True:
            if not fIgnoreErrors:
                return reporter.errorXcpt('Download file failed: Could not create session for vbox');
            reporter.log('warning: Download file failed: Could not create session for vbox');
            return False;

        try:
            if self.fpApiVer >= 5.0:
                oCurProgress = oGuestSession.fileCopyFromGuest(sSrc, sDst, [0]);
            else:
                oCurProgress = oGuestSession.copyFrom(sSrc, sDst, [0]);
        except:
            if not fIgnoreErrors:
                reporter.errorXcpt('Download file exception for sSrc="%s":' % (self.sGuestAdditionsIso,));
            else:
                reporter.log('warning: Download file exception for sSrc="%s":' % (self.sGuestAdditionsIso,));
            fRc = False;
        else:
            if oCurProgress is not None:
                oWrapperProgress = vboxwrappers.ProgressWrapper(oCurProgress, self.oTestDriver.oVBoxMgr,
                                                                self.oTestDriver, "downloadFile");
                oWrapperProgress.wait();
                if not oWrapperProgress.isSuccess():
                    oWrapperProgress.logResult(fIgnoreErrors);
                    fRc = False;
            else:
                if not fIgnoreErrors:
                    reporter.error('No progress object returned');
                else:
                    reporter.log('warning: No progress object returned');
                fRc = False;

        self.closeSession(oGuestSession);
        return fRc;

    def downloadFiles(self, oSession, asFiles, fIgnoreErrors = False):
        """
        Convenience function to get files from the guest and stores it
        into the scratch directory for later (manual) review.

        Returns True on success.

        Returns False on failure, logged.
        """
        fRc = True;
        for sGstFile in asFiles:
            sTmpFile = os.path.join(self.oTestDriver.sScratchPath, 'tmp-' + os.path.basename(sGstFile));
            reporter.log2('Downloading file "%s" to "%s" ...' % (sGstFile, sTmpFile));
            # First try to remove (unlink) an existing temporary file, as we don't truncate the file.
            try:    os.unlink(sTmpFile);
            except: pass;
            ## @todo Check for already existing files on the host and create a new
            #        name for the current file to download.
            fRc = self.downloadFile(oSession, sGstFile, sTmpFile, fIgnoreErrors);
            if fRc:
                reporter.addLogFile(sTmpFile, 'misc/other', 'guest - ' + sGstFile);
            else:
                if fIgnoreErrors is not True:
                    reporter.error('error downloading file "%s" to "%s"' % (sGstFile, sTmpFile));
                    return fRc;
                reporter.log('warning: file "%s" was not downloaded, ignoring.' % (sGstFile,));
        return True;


class tdAutostartOsLinux(tdAutostartOs):
    """
    Autostart support methods for Linux guests.
    """

    def __init__(self, oTestDriver, sTestBuildDir, fpApiVer, sGuestAdditionsIso):
        tdAutostartOs.__init__(self, oTestDriver, fpApiVer, sGuestAdditionsIso);
        self.sTestBuild = self._findFile('^VirtualBox-.*\\.run$', sTestBuildDir);
        if not self.sTestBuild:
            raise base.GenError("VirtualBox install package not found");

    def waitVMisReady(self, oSession):
        """
        Waits the VM is ready after start or reboot.
        """
        # Give the VM a time to reboot
        self.oTestDriver.sleep(30);

        # Waiting the VM is ready.
        # To do it, one will try to open the guest session and start the guest process in loop

        cAttempt = 0;

        while cAttempt < 30:
            fRc, oGuestSession = self.createSession(oSession, 'Session for user: vbox',
                                            'vbox', 'password', 10 * 1000, False);
            if fRc:
                (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Start a guest process',
                                                          30 * 1000, '/sbin/ifconfig',
                                                          ['ifconfig',],
                                                          False, False);
                fRc = self.closeSession(oGuestSession, False) and fRc and True; # pychecker hack.

            if fRc:
                break;

            self.oTestDriver.sleep(10);
            cAttempt += 1;

        return fRc;

    def rebootVMAndCheckReady(self, oSession):
        """
        Reboot the VM and wait the VM is ready.
        """

        fRc, oGuestSession = self.createSession(oSession, 'Session for user: vbox',
                                            'vbox', 'password', 10 * 1000, True);
        if not fRc:
            return fRc;

        (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Reboot the VM',
                                                  30 * 1000, '/usr/bin/sudo',
                                                  ['sudo', 'reboot'],
                                                  False, True);
        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.
        fRc = fRc and self.waitVMisReady(oSession);
        return fRc;

    def installAdditions(self, oSession, oVM):
        """
        Install guest additions in the guest.
        """
        fRc, oGuestSession = self.createSession(oSession, 'Session for user: vbox',
                                            'vbox', 'password', 10 * 1000, True);
        if not fRc:
            return fRc;

        # Install Kernel headers, which are required for actually installing the Linux Additions.
        if oVM.OSTypeId.startswith('Debian') \
        or oVM.OSTypeId.startswith('Ubuntu'):
            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Installing Kernel headers',
                                                  5 * 60 *1000, '/usr/bin/apt-get',
                                                  ['/usr/bin/apt-get', 'install', '-y',
                                                   'linux-headers-generic'],
                                                  False, True);
            if not fRc:
                reporter.error('Error installing Kernel headers');
            else:
                (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Installing Guest Additions depdendencies',
                                                          5 * 60 *1000, '/usr/bin/apt-get',
                                                          ['/usr/bin/apt-get', 'install', '-y', 'build-essential',
                                                           'perl'], False, True);
                if not fRc:
                    reporter.error('Error installing additional installer dependencies');

        elif oVM.OSTypeId.startswith('OL') \
        or   oVM.OSTypeId.startswith('Oracle') \
        or   oVM.OSTypeId.startswith('RHEL') \
        or   oVM.OSTypeId.startswith('Redhat') \
        or   oVM.OSTypeId.startswith('Cent'):
            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Installing Kernel headers',
                                                  5 * 60 *1000, '/usr/bin/yum',
                                                  ['/usr/bin/yum', '-y', 'install', 'kernel-headers'],
                                                  False, True);
            if not fRc:
                reporter.error('Error installing Kernel headers');
            else:
                (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Installing Guest Additions depdendencies',
                                                          5 * 60 *1000, '/usr/bin/yum',
                                                          ['/usr/bin/yum', '-y', 'install', 'make', 'automake', 'gcc',
                                                           'kernel-devel', 'dkms', 'bzip2', 'perl'], False, True);
                if not fRc:
                    reporter.error('Error installing additional installer dependencies');

        else:
            reporter.error('Installing Linux Additions for kind "%s" is not supported yet' % oVM.sKind);
            fRc = False;

        if fRc:
            #
            # The actual install.
            # Also tell the installer to produce the appropriate log files.
            #

            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Installing guest additions',
                                                      5 * 60 *1000, '/usr/bin/sudo',
                                                      ['/usr/bin/sudo', '/bin/sh',
                                                       '/media/cdrom/VBoxLinuxAdditions.run'],
                                                      False, True);
            if fRc:
                # Due to the GA updates as separate process the above function returns before
                # the actual installation finished. So just wait until the GA installed
                fRc = self.waitVMisReady(oSession);

                # Download log files.
                # Ignore errors as all files above might not be present for whatever reason.
                #
                if fRc:
                    asLogFile = [];
                    asLogFile.append('/var/log/vboxadd-install.log');
                    self.downloadFiles(oSession, asLogFile, fIgnoreErrors = True);

        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.
        if fRc:
            fRc = self.rebootVMAndCheckReady(oSession);

        return fRc;

    def installVirtualBox(self, oSession):
        """
        Install VirtualBox in the guest.
        """
        if self.sTestBuild is None:
            return False;

        fRc, oGuestSession = self.createSession(oSession, 'Session for user: vbox',
                                            'vbox', 'password', 10 * 1000, True);
        if not fRc:
            return fRc;

        fRc = self.uploadFile(oSession, self.sTestBuild,
                              '/tmp/' + os.path.basename(self.sTestBuild));

        if fRc:
            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession,
                                                      'Allowing execution for the vbox installer',
                                                      30 * 1000, '/usr/bin/sudo',
                                                      ['/usr/bin/sudo', '/bin/chmod', '755',
                                                       '/tmp/' + os.path.basename(self.sTestBuild)],
                                                      False, True);
        if fRc:
            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Installing VBox',
                                                      240 * 1000, '/usr/bin/sudo',
                                                      ['/usr/bin/sudo',
                                                       '/tmp/' + os.path.basename(self.sTestBuild),],
                                                      False, True);

        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.
        return fRc;

    def configureAutostart(self, oSession, sDefaultPolicy = 'allow',
                           asUserAllow = (), asUserDeny = ()):
        """
        Configures the autostart feature in the guest.
        """

        fRc, oGuestSession = self.createSession(oSession, 'Session for confiure autostart',
                                                'vbox', 'password', 10 * 1000, True);
        if not fRc:
            return fRc;

        # Create autostart database directory writeable for everyone
        (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Creating autostart database',
                                                  30 * 1000, '/usr/bin/sudo',
                                                  ['/usr/bin/sudo', '/bin/mkdir', '-m', '1777', '/etc/vbox/autostart.d'],
                                                  False, True);
        # Create /etc/default/virtualbox
        sVBoxCfg =   'VBOXAUTOSTART_CONFIG=/etc/vbox/autostart.cfg\n' \
                   + 'VBOXAUTOSTART_DB=/etc/vbox/autostart.d\n';
        fRc = fRc and self.uploadString(oSession, sVBoxCfg, '/tmp/virtualbox');
        if fRc:
            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Moving to destination',
                                                      30 * 1000, '/usr/bin/sudo',
                                                      ['/usr/bin/sudo', '/bin/mv', '/tmp/virtualbox',
                                                       '/etc/default/virtualbox'],
                                                      False, True);
        if fRc:
            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Setting permissions',
                                                      30 * 1000, '/usr/bin/sudo',
                                                      ['/usr/bin/sudo', '/bin/chmod', '644',
                                                       '/etc/default/virtualbox'],
                                                      False, True);

        sVBoxCfg = self._createAutostartCfg(sDefaultPolicy, asUserAllow, asUserDeny);
        fRc = fRc and self.uploadString(oSession, sVBoxCfg, '/tmp/autostart.cfg');
        if fRc:
            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Moving to destination',
                                                      30 * 1000, '/usr/bin/sudo',
                                                      ['/usr/bin/sudo', '/bin/mv', '/tmp/autostart.cfg',
                                                       '/etc/vbox/autostart.cfg'],
                                                      False, True);
        if fRc:
            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Setting permissions',
                                                      30 * 1000, '/usr/bin/sudo',
                                                      ['/usr/bin/sudo', '/bin/chmod', '644',
                                                       '/etc/vbox/autostart.cfg'],
                                                      False, True);
        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.
        return fRc;

    def createUser(self, oSession, sUser):
        """
        Create a new user with the given name
        """

        fRc, oGuestSession = self.createSession(oSession, 'Creating new user',
                                                'vbox', 'password', 10 * 1000, True);
        if fRc:
            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Creating new user',
                                                      30 * 1000, '/usr/bin/sudo',
                                                      ['/usr/bin/sudo', '/usr/sbin/useradd', '-m', '-U',
                                                       sUser], False, True);
        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.
        return fRc;

    # pylint: enable=too-many-arguments

    def createTestVM(self, oSession, sUser, sVmName):
        """
        Create a test VM in the guest and enable autostart.
        Due to the sUser is created whithout password,
        all calls will be perfomed using 'sudo -u sUser'
        """

        fRc, oGuestSession = self.createSession(oSession, 'Session for create VM for user: %s' % (sUser,),
                                                'vbox', 'password', 10 * 1000, True);
        if not fRc:
            return fRc;

        (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Configuring autostart database',
                                                  30 * 1000, '/usr/bin/sudo',
                                                  ['/usr/bin/sudo', '-u', sUser, '-H', '/opt/VirtualBox/VBoxManage',
                                                   'setproperty', 'autostartdbpath', '/etc/vbox/autostart.d'],
                                                  False, True);
        if fRc:
            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Create VM ' + sVmName,
                                                      30 * 1000, '/usr/bin/sudo',
                                                      ['/usr/bin/sudo', '-u', sUser, '-H',
                                                       '/opt/VirtualBox/VBoxManage', 'createvm',
                                                       '--name', sVmName, '--register'], False, True);
        if fRc:
            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Enabling autostart for test VM',
                                                      30 * 1000, '/usr/bin/sudo',
                                                      ['/usr/bin/sudo', '-u', sUser, '-H',
                                                       '/opt/VirtualBox/VBoxManage', 'modifyvm',
                                                      sVmName, '--autostart-enabled', 'on'], False, True);
        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.
        return fRc;

    def checkForRunningVM(self, oSession, sUser, sVmName):
        """
        Check for VM running in the guest after autostart.
        Due to the sUser is created whithout password,
        all calls will be perfomed using 'sudo -u sUser'
        """

        fRc, oGuestSession = self.createSession(oSession, 'Session for checking running VMs for user: %s' % (sUser,),
                                                'vbox', 'password', 10 * 1000, True);
        if not fRc:
            return fRc;

        (fRc, _, _, aBuf) = self.guestProcessExecute(oGuestSession, 'Check for running VM',
                                                     30 * 1000, '/usr/bin/sudo',
                                                     ['/usr/bin/sudo', '-u', sUser, '-H',
                                                      '/opt/VirtualBox/VBoxManage',
                                                      'list', 'runningvms'], True, True);
        if fRc:
            bufWrapper = VBoxManageStdOutWrapper();
            bufWrapper.write(aBuf);
            fRc = bufWrapper.sVmRunning == sVmName;

        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.
        return fRc;


class tdAutostartOsDarwin(tdAutostartOs):
    """
    Autostart support methods for Darwin guests.
    """

    def __init__(self, oTestDriver, sTestBuildDir, fpApiVer, sGuestAdditionsIso):
        _ = sTestBuildDir;
        tdAutostartOs.__init__(self, oTestDriver, fpApiVer, sGuestAdditionsIso);
        raise base.GenError('Testing the autostart functionality for Darwin is not implemented');


class tdAutostartOsSolaris(tdAutostartOs):
    """
    Autostart support methods for Solaris guests.
    """

    def __init__(self, oTestDriver, sTestBuildDir, fpApiVer, sGuestAdditionsIso):
        _ = sTestBuildDir;
        tdAutostartOs.__init__(self, oTestDriver, fpApiVer, sGuestAdditionsIso);
        raise base.GenError('Testing the autostart functionality for Solaris is not implemented');


class tdAutostartOsWin(tdAutostartOs):
    """
    Autostart support methods for Windows guests.
    """

    def __init__(self, oTestDriver, sTestBuildDir, fpApiVer, sGuestAdditionsIso):
        tdAutostartOs.__init__(self, oTestDriver, fpApiVer, sGuestAdditionsIso);
        self.sTestBuild = self._findFile('^VirtualBox-.*\\.(exe|msi)$', sTestBuildDir);
        if not self.sTestBuild:
            raise base.GenError("VirtualBox install package not found");
        return;

    def waitVMisReady(self, oSession):
        """
        Waits the VM is ready after start or reboot.
        """
        # Give the VM a time to reboot
        self.oTestDriver.sleep(30);

        # Waiting the VM is ready.
        # To do it, one will try to open the guest session and start the guest process in loop

        cAttempt = 0;

        while cAttempt < 10:
            fRc, oGuestSession = self.createSession(oSession, 'Session for user: vbox',
                                            'vbox', 'password', 10 * 1000, False);
            if fRc:
                (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Start a guest process',
                                                          30 * 1000, 'C:\\Windows\\System32\\ipconfig.exe',
                                                          ['C:\\Windows\\System32\\ipconfig.exe',],
                                                          False, False);
                fRc = self.closeSession(oGuestSession, False) and fRc and True; # pychecker hack.

            if fRc:
                break;

            self.oTestDriver.sleep(10);
            cAttempt += 1;

        return fRc;

    def rebootVMAndCheckReady(self, oSession):
        """
        Reboot the VM and wait the VM is ready.
        """

        fRc, oGuestSession = self.createSession(oSession, 'Session for user: vbox',
                                            'vbox', 'password', 10 * 1000, True);
        if not fRc:
            return fRc;

        (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Reboot the VM',
                                                  30 * 1000, 'C:\\Windows\\System32\\shutdown.exe',
                                                  ['C:\\Windows\\System32\\shutdown.exe', '/f',
                                                   '/r', '/t', '0'],
                                                  False, True);
        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.

        fRc = fRc and self.waitVMisReady(oSession);
        return fRc;

    def installAdditions(self, oSession, oVM):
        """
        Installs the Windows guest additions using the test execution service.
        """
        fRc, oGuestSession = self.createSession(oSession, 'Session for user: vbox',
                                            'vbox', 'password', 10 * 1000, True);
        if not fRc:
            return fRc;
        #
        # Delete relevant log files.
        #
        # Note! On some guests the files in question still can be locked by the OS, so ignore
        #       deletion errors from the guest side (e.g. sharing violations) and just continue.
        #
        asLogFiles = [];
        fHaveSetupApiDevLog = False;
        if oVM.OSTypeId in ('WindowsNT4',):
            sWinDir = 'C:/WinNT/';
        else:
            sWinDir = 'C:/Windows/';
            asLogFiles = [sWinDir + 'setupapi.log', sWinDir + 'setupact.log', sWinDir + 'setuperr.log'];

            # Apply The SetupAPI logging level so that we also get the (most verbose) setupapi.dev.log file.
            ## @todo !!! HACK ALERT !!! Add the value directly into the testing source image. Later.
            (fHaveSetupApiDevLog, _, _, _) = \
                self.guestProcessExecute(oGuestSession, 'Enabling setupapi.dev.log',
                                         60 * 1000, 'c:\\Windows\\System32\\reg.exe',
                                         ['c:\\Windows\\System32\\reg.exe', 'add',
                                          '"HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Setup"',
                                          '/v', 'LogLevel', '/t', 'REG_DWORD', '/d', '0xFF'],
                                         False, True);
        for sFile in asLogFiles:
            try:    oGuestSession.fsObjRemove(sFile);
            except: pass;

        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.

        try:
            oCurProgress = oSession.o.console.guest.updateGuestAdditions(self.sGuestAdditionsIso, None, None);
        except:
            reporter.maybeErrXcpt(True, 'Updating Guest Additions exception for sSrc="%s":'
                                  % (self.sGuestAdditionsIso,));
            fRc = False;
        else:
            if oCurProgress is not None:
                oWrapperProgress = vboxwrappers.ProgressWrapper(oCurProgress, self.oTestDriver.oVBoxMgr,
                                                                self.oTestDriver, "installAdditions");
                oWrapperProgress.wait();
                if not oWrapperProgress.isSuccess():
                    oWrapperProgress.logResult(fIgnoreErrors = False);
                    fRc = False;
            else:
                fRc = reporter.error('No progress object returned');

        if fRc:
            fRc = self.rebootVMAndCheckReady(oSession);
            if fRc is True:
                # Add the Windows Guest Additions installer files to the files we want to download
                # from the guest.
                sGuestAddsDir = 'C:/Program Files/Oracle/VirtualBox Guest Additions/';
                asLogFiles.append(sGuestAddsDir + 'install.log');
                # Note: There won't be a install_ui.log because of the silent installation.
                asLogFiles.append(sGuestAddsDir + 'install_drivers.log');
                asLogFiles.append('C:/Windows/setupapi.log');

                # Note: setupapi.dev.log only is available since Windows 2000.
                if fHaveSetupApiDevLog:
                    asLogFiles.append('C:/Windows/setupapi.dev.log');

                #
                # Download log files.
                # Ignore errors as all files above might not be present (or in different locations)
                # on different Windows guests.
                #
                self.downloadFiles(oSession, asLogFiles, fIgnoreErrors = True);

        return fRc;

    def installVirtualBox(self, oSession):
        """
        Install VirtualBox in the guest.
        """

        if self.sTestBuild is None:
            return False;

        fRc, oGuestSession = self.createSession(oSession, 'Session for user: vbox',
                                            'vbox', 'password', 10 * 1000, True);
        if not fRc:
            return fRc;

        # Used windows image already contains the C:\Temp
        fRc = fRc and self.uploadFile(oSession, self.sTestBuild,
                                      'C:\\Temp\\' + os.path.basename(self.sTestBuild));

        if fRc:
            if self.sTestBuild.endswith('.msi'):
                (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Installing VBox',
                                                        240 * 1000, 'C:\\Windows\\System32\\msiexec.exe',
                                                        ['msiexec', '/quiet', '/i',
                                                         'C:\\Temp\\' + os.path.basename(self.sTestBuild)],
                                                        False, True);
            else:
                (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Installing VBox',
                                                        240 * 1000, 'C:\\Temp\\' + os.path.basename(self.sTestBuild),
                                                        ['C:\\Temp\\' + os.path.basename(self.sTestBuild), '--silent'],
                                                        False, True);

        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.
        return fRc;

    def configureAutostart(self, oSession, sDefaultPolicy = 'allow',
                           asUserAllow = (), asUserDeny = ()):
        """
        Configures the autostart feature in the guest.
        """

        fRc, oGuestSession = self.createSession(oSession, 'Session for confiure autostart',
                                                'vbox', 'password', 10 * 1000, True);
        if not fRc:
            return fRc;

        # Create autostart database directory writeable for everyone
        (fRc, _, _, _) = \
            self.guestProcessExecute(oGuestSession, 'Setting the autostart environment variable',
                                     30 * 1000, 'C:\\Windows\\System32\\reg.exe',
                                     ['reg', 'add',
                                      'HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment',
                                      '/v', 'VBOXAUTOSTART_CONFIG', '/d',
                                      'C:\\ProgramData\\autostart.cfg', '/f'],
                                     False, True);

        sVBoxCfg = self._createAutostartCfg(sDefaultPolicy, asUserAllow, asUserDeny);
        fRc = fRc and self.uploadString(oSession, sVBoxCfg, 'C:\\ProgramData\\autostart.cfg');
        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.
        return fRc;

    def createTestVM(self, oSession, sUser, sVmName):
        """
        Create a test VM in the guest and enable autostart.
        """

        fRc, oGuestSession = self.createSession(oSession, 'Session for user: %s' % (sUser,),
                                                sUser, 'password', 10 * 1000, True);
        if not fRc:
            return fRc;

        (fRc, _, _, _) = \
            self.guestProcessExecute(oGuestSession, 'Create VM ' + sVmName,
                                     30 * 1000, 'C:\\Program Files\\Oracle\\VirtualBox\\VBoxManage.exe',
                                     ['C:\\Program Files\\Oracle\\VirtualBox\\VBoxManage.exe', 'createvm',
                                      '--name', sVmName, '--register'], False, True);
        if fRc:
            (fRc, _, _, _) = \
                self.guestProcessExecute(oGuestSession, 'Enabling autostart for test VM',
                                         30 * 1000, 'C:\\Program Files\\Oracle\\VirtualBox\\VBoxManage.exe',
                                         ['C:\\Program Files\\Oracle\\VirtualBox\\VBoxManage.exe',
                                          'modifyvm', sVmName, '--autostart-enabled', 'on'], False, True);
        fRc = fRc and self.uploadString(oSession, 'password', 'C:\\ProgramData\\password.cfg');
        if fRc:
            (fRc, _, _, _) = \
                self.guestProcessExecute(oGuestSession, 'Install autostart service for the user',
                                         30 * 1000, 'C:\\Program Files\\Oracle\\VirtualBox\\VBoxAutostartSvc.exe',
                                         ['C:\\Program Files\\Oracle\\VirtualBox\\VBoxAutostartSvc.exe',
                                          'install', '--user=' + sUser,
                                          '--password-file=C:\\ProgramData\\password.cfg'],
                                         False, True);
        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.

        return fRc;

    def checkForRunningVM(self, oSession, sUser, sVmName):
        """
        Check for VM running in the guest after autostart.
        """

        fRc, oGuestSession = self.createSession(oSession, 'Session for user: %s' % (sUser,),
                                                sUser, 'password', 10 * 1000, True);
        if not fRc:
            return fRc;

        (fRc, _, _, aBuf) = self.guestProcessExecute(oGuestSession, 'Check for running VM',
                                                   30 * 1000, 'C:\\Program Files\\Oracle\\VirtualBox\\VBoxManage.exe',
                                                   ['C:\\Program Files\\Oracle\\VirtualBox\\VBoxManage.exe',
                                                    'list', 'runningvms'], True, True);
        if fRc:
            bufWrapper = VBoxManageStdOutWrapper();
            bufWrapper.write(aBuf);
            fRc = bufWrapper.sVmRunning == sVmName;

        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.

        return fRc;

    def createUser(self, oSession, sUser):
        """
        Create a new user with the given name
        """

        fRc, oGuestSession = self.createSession(oSession, 'Creating user %s to run a VM' % sUser,
                                                'vbox', 'password', 10 * 1000, True);
        if not fRc:
            return fRc;

        # Create user
        (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Creating user %s to run a VM' % sUser,
                                                30 * 1000, 'C:\\Windows\\System32\\net.exe',
                                                ['net', 'user', sUser, 'password', '/add' ], False, True);

        # Add the user to Administrators group
        if fRc:
            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession, 'Adding the user %s to Administrators group' % sUser,
                                                      30 * 1000, 'C:\\Windows\\System32\\net.exe',
                                                      ['net', 'localgroup', 'Administrators', sUser, '/add' ], False, True);

        #Allow the user to logon as service
        sSecPolicyEditor = """
' SetLogonAsAServiceRight.vbs
' Sample VBScript to set or grant Logon As A Service Right.
' Author: http://www.morgantechspace.com/
' ------------------------------------------------------'

Dim strUserName,ConfigFileName,OrgStr,RepStr,inputFile,strInputFile,outputFile,obj
strUserName = "%s"
Dim oShell
Set oShell = CreateObject ("WScript.Shell")
oShell.Run "secedit /export /cfg config.inf", 0, true
oShell.Run "secedit /import /cfg config.inf /db database.sdb", 0, true

ConfigFileName = "config.inf"
OrgStr = "SeServiceLogonRight ="
RepStr = "SeServiceLogonRight = " & strUserName & ","
Set inputFile = CreateObject("Scripting.FileSystemObject").OpenTextFile("config.inf", 1,1,-1)
strInputFile = inputFile.ReadAll
inputFile.Close
Set inputFile = Nothing

Set outputFile = CreateObject("Scripting.FileSystemObject").OpenTextFile("config.inf",2,1,-1)
outputFile.Write (Replace(strInputFile,OrgStr,RepStr))
outputFile.Close
Set outputFile = Nothing

oShell.Run "secedit /configure /db database.sdb /cfg config.inf",0,true
set oShell= Nothing

Set obj = CreateObject("Scripting.FileSystemObject")
obj.DeleteFile("config.inf")
obj.DeleteFile("database.sdb")

WScript.Echo "Logon As A Service Right granted to user '"& strUserName &"'"
                           """ % sUser;
        fRc = fRc and self.uploadString(oSession, sSecPolicyEditor, 'C:\\Temp\\adjustsec.vbs');
        if fRc:
            (fRc, _, _, _) = self.guestProcessExecute(oGuestSession,
                                                      'Setting the "Logon as service" policy to the user %s' % sUser,
                                                      30 * 1000, 'C:\\Windows\\System32\\cscript.exe',
                                                      ['cscript.exe', 'C:\\Temp\\adjustsec.vbs', '//Nologo'], False, True);
        try:
            oGuestSession.fsObjRemove('C:\\Temp\\adjustsec.vbs');
        except:
            fRc = reporter.errorXcpt('Removing policy script failed');

        fRc = self.closeSession(oGuestSession, True) and fRc and True; # pychecker hack.
        return fRc;


class tdAutostart(vbox.TestDriver):                                      # pylint: disable=too-many-instance-attributes
    """
    Autostart testcase.
    """

    ksOsLinux   = 'tst-linux'
    ksOsWindows = 'tst-win'
    ksOsDarwin  = 'tst-darwin'
    ksOsSolaris = 'tst-solaris'
    ksOsFreeBSD = 'tst-freebsd'

    def __init__(self):
        vbox.TestDriver.__init__(self);
        self.asRsrcs            = None;
        self.asTestVMsDef       = [self.ksOsWindows, self.ksOsLinux];
        self.asTestVMs          = self.asTestVMsDef;
        self.asSkipVMs          = [];
        self.sTestBuildDir      = None; #'D:/AlexD/TestBox/TestAdditionalFiles';
        self.sGuestAdditionsIso = None; #'D:/AlexD/TestBox/TestAdditionalFiles/VBoxGuestAdditions_6.1.2.iso';

    #
    # Overridden methods.
    #
    def showUsage(self):
        rc = vbox.TestDriver.showUsage(self);
        reporter.log('');
        reporter.log('tdAutostart Options:');
        reporter.log('  --test-build-dir <path>');
        reporter.log('      The directory with VirtualBox distros. The option is mandatory');
        reporter.log('      without any default value. The test raises an exception if the');
        reporter.log('      option is not specified.');
        reporter.log('  --guest-additions-iso <path/to/iso>');
        reporter.log('      The path to fresh VirtualBox Guest Additions iso. The option is');
        reporter.log('      mandatory without any default value. The test raises an exception');
        reporter.log('      if the option is not specified.');
        reporter.log('  --test-vms      <vm1[:vm2[:...]]>');
        reporter.log('      Test the specified VMs in the given order. Use this to change');
        reporter.log('      the execution order or limit the choice of VMs');
        reporter.log('      Default: %s  (all)' % (':'.join(self.asTestVMsDef)));
        reporter.log('  --skip-vms      <vm1[:vm2[:...]]>');
        reporter.log('      Skip the specified VMs when testing.');
        return rc;

    def parseOption(self, asArgs, iArg): # pylint: disable=too-many-branches,too-many-statements
        if asArgs[iArg] == '--test-build-dir':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--test-build-dir" takes a path argument');
            self.sTestBuildDir = asArgs[iArg];
        elif asArgs[iArg] == '--guest-additions-iso':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--guest-additions-iso" takes a path or url to iso argument');
            self.sGuestAdditionsIso = asArgs[iArg];
        elif asArgs[iArg] == '--test-vms':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--test-vms" takes colon separated list');
            self.asTestVMs = asArgs[iArg].split(':');
            for s in self.asTestVMs:
                if s not in self.asTestVMsDef:
                    raise base.InvalidOption('The "--test-vms" value "%s" is not valid; valid values are: %s' \
                        % (s, ' '.join(self.asTestVMsDef)));
        elif asArgs[iArg] == '--skip-vms':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--skip-vms" takes colon separated list');
            self.asSkipVMs = asArgs[iArg].split(':');
            for s in self.asSkipVMs:
                if s not in self.asTestVMsDef:
                    reporter.log('warning: The "--test-vms" value "%s" does not specify any of our test VMs.' % (s));
        else:
            return vbox.TestDriver.parseOption(self, asArgs, iArg);
        return iArg + 1;

    def completeOptions(self):
        # Remove skipped VMs from the test list.
        if self.sTestBuildDir is None:
            raise base.InvalidOption('--test-build-dir is not specified')
        if self.sGuestAdditionsIso is None:
            raise base.InvalidOption('--guest-additions-iso is not specified')

        for sVM in self.asSkipVMs:
            try:    self.asTestVMs.remove(sVM);
            except: pass;

        return vbox.TestDriver.completeOptions(self);

    def getResourceSet(self):
        # Construct the resource list the first time it's queried.
        if self.asRsrcs is None:
            self.asRsrcs = [];
            if self.ksOsLinux in self.asTestVMs:
                self.asRsrcs.append('6.0/ub1804piglit/ub1804piglit.vdi');
            if self.ksOsWindows in self.asTestVMs:
                self.asRsrcs.append('6.0/windows7piglit/windows7piglit.vdi');
            #disabled
            #if self.ksOsSolaris in self.asTestVMs:
            #    self.asRsrcs.append('4.2/autostart/tst-solaris.vdi');

        return self.asRsrcs;

    def actionConfig(self):

        # Make sure vboxapi has been imported so we can use the constants.
        if not self.importVBoxApi():
            return False;

        # Download VBoxGuestAdditions.iso before work
        sDestinationIso = os.path.join(self.sScratchPath, 'VBoxGuestAdditions.iso');
        utils.noxcptDeleteFile(sDestinationIso);
        if not downloadFile(self.sGuestAdditionsIso, sDestinationIso, '', reporter.log, reporter.error):
            raise base.GenError("Could not download VBoxGuestAdditions.iso");
        self.sGuestAdditionsIso = sDestinationIso;

        #
        # Configure the VMs we're going to use.
        #

        fRc = True;

        # Linux VMs
        if self.ksOsLinux in self.asTestVMs:
            # ub1804piglit is created with 2 CPUs.
            oVM = self.createTestVM(self.ksOsLinux, 1, '6.0/ub1804piglit/ub1804piglit.vdi', sKind = 'Ubuntu_64', \
                                    fIoApic = True, eNic0AttachType = vboxcon.NetworkAttachmentType_NAT, \
                                    eNic0Type = vboxcon.NetworkAdapterType_Am79C973, cMbRam = 4096, \
                                    cCpus = 2, sDvdImage = self.sGuestAdditionsIso);
            if oVM is None:
                return False;
        # Windows VMs
        if self.ksOsWindows in self.asTestVMs:
            # windows7piglit is created with PAE enabled and 2 CPUs.
            oVM = self.createTestVM(self.ksOsWindows, 1, '6.0/windows7piglit/windows7piglit.vdi', sKind = 'Windows7_64', \
                                    fIoApic = True, eNic0AttachType = vboxcon.NetworkAttachmentType_NAT, \
                                    sHddControllerType = "SATA Controller", cMbRam = 4096, fPae = True, cCpus = 2, \
                                    sDvdImage = self.sGuestAdditionsIso);
            if oVM is None:
                return False;

            # additional options used during the windows7piglit creation
            oSession = self.openSession(oVM);
            if oSession is not None:
                fRc = fRc and oSession.setVRamSize(256);
                fRc = fRc and oSession.setVideoControllerType(vboxcon.GraphicsControllerType_VBoxSVGA)
                fRc = fRc and oSession.setAccelerate3DEnabled(True);
                fRc = fRc and oSession.enableUsbOhci(True);
                fRc = fRc and oSession.enableUsbHid(True);
                fRc = fRc and oSession.saveSettings();
                fRc = oSession.close() and fRc and True; # pychecker hack.
                oSession = None;
            else:
                fRc = False;

        # disabled
        # Solaris VMs
        #if self.ksOsSolaris in self.asTestVMs:
        #    oVM = self.createTestVM(self.ksOsSolaris, 1, '4.2/autostart/tst-solaris.vdi', sKind = 'Solaris_64', \
        #                            fIoApic = True, eNic0AttachType = vboxcon.NetworkAttachmentType_NAT, \
        #                            sHddControllerType = "SATA Controller");
        #    if oVM is None:
        #        return False;

        return fRc;

    def actionExecute(self):
        """
        Execute the testcase.
        """
        fRc = self.testAutostart();
        return fRc;

    #
    # Test execution helpers.
    #
    def testAutostartRunProgs(self, oSession, sVmName, oVM):
        """
        Test VirtualBoxs autostart feature in a VM.
        """
        reporter.testStart('Autostart ' + sVmName);

        oGuestOsHlp = None              # type: tdAutostartOs
        if sVmName == self.ksOsLinux:
            oGuestOsHlp = tdAutostartOsLinux(self, self.sTestBuildDir, self.fpApiVer,
                                             self.sGuestAdditionsIso);   # pylint: disable=redefined-variable-type
        elif sVmName == self.ksOsSolaris:
            oGuestOsHlp = tdAutostartOsSolaris(self, self.sTestBuildDir, self.fpApiVer,
                                               self.sGuestAdditionsIso); # pylint: disable=redefined-variable-type
        elif sVmName == self.ksOsDarwin:
            oGuestOsHlp = tdAutostartOsDarwin(self, self.sTestBuildDir, self.fpApiVer,
                                              self.sGuestAdditionsIso);  # pylint: disable=redefined-variable-type
        elif sVmName == self.ksOsWindows:
            oGuestOsHlp = tdAutostartOsWin(self, self.sTestBuildDir, self.fpApiVer,
                                           self.sGuestAdditionsIso);     # pylint: disable=redefined-variable-type

        sTestUserAllow = 'test1';
        sTestUserDeny = 'test2';
        sTestVmName = 'TestVM';

        if oGuestOsHlp is not None:
            #wait the VM is ready after starting
            fRc = oGuestOsHlp.waitVMisReady(oSession);
            #install fresh guest additions
            fRc = fRc and oGuestOsHlp.installAdditions(oSession, oVM);
            # Create two new users
            fRc = fRc and oGuestOsHlp.createUser(oSession, sTestUserAllow);
            fRc = fRc and oGuestOsHlp.createUser(oSession, sTestUserDeny);
            if fRc is True:
                # Install VBox first
                fRc = oGuestOsHlp.installVirtualBox(oSession);
                if fRc is True:
                    fRc = oGuestOsHlp.configureAutostart(oSession, 'allow', (sTestUserAllow,), (sTestUserDeny,));
                    if fRc is True:
                        # Create a VM with autostart enabled in the guest for both users
                        fRc = oGuestOsHlp.createTestVM(oSession, sTestUserAllow, sTestVmName);
                        fRc = fRc and oGuestOsHlp.createTestVM(oSession, sTestUserDeny, sTestVmName);
                        if fRc is True:
                            # Reboot the guest
                            fRc = oGuestOsHlp.rebootVMAndCheckReady(oSession);
                            if fRc is True:
                                # Fudge factor - Allow the guest to finish starting up.
                                self.sleep(30);
                                fRc = oGuestOsHlp.checkForRunningVM(oSession, sTestUserAllow, sTestVmName);
                                if fRc is True:
                                    fRc = oGuestOsHlp.checkForRunningVM(oSession, sTestUserDeny, sTestVmName);
                                    if fRc is True:
                                        reporter.error('Test VM is running inside the guest for denied user');
                                    fRc = not fRc;
                                else:
                                    reporter.error('Test VM is not running inside the guest for allowed user');
                            else:
                                reporter.log('Rebooting the guest failed');
                        else:
                            reporter.log('Creating test VM failed');
                    else:
                        reporter.log('Configuring autostart in the guest failed');
                else:
                    reporter.log('Installing VirtualBox in the guest failed');
            else:
                reporter.log('Creating test users failed');
        else:
            reporter.log('Guest OS helper not created for VM %s' % (sVmName));
            fRc = False;

        reporter.testDone();
        return fRc;

    def testAutostartOneCfg(self, sVmName):
        """
        Runs the specified VM thru test #1.

        Returns a success indicator on the general test execution. This is not
        the actual test result.
        """
        oVM = self.getVmByName(sVmName);

        # Reconfigure the VM
        fRc = True;
        oSession = self.openSession(oVM);
        if oSession is not None:
            fRc = fRc and oSession.enableVirtEx(True);
            fRc = fRc and oSession.enableNestedPaging(True);
            fRc = fRc and oSession.enableNestedHwVirt(True);
            fRc = fRc and oSession.saveSettings();
            fRc = oSession.close() and fRc and True; # pychecker hack.
            oSession = None;
        else:
            fRc = False;

        # Start up.
        if fRc is True:
            self.logVmInfo(oVM);
            oSession = self.startVmByName(sVmName);
            if oSession is not None:
                fRc = self.testAutostartRunProgs(oSession, sVmName, oVM);
                self.terminateVmBySession(oSession)
            else:
                fRc = False;
        return fRc;

    def testAutostartForOneVM(self, sVmName):
        """
        Runs one VM thru the various configurations.
        """
        reporter.testStart(sVmName);
        fRc = True;
        self.testAutostartOneCfg(sVmName);
        reporter.testDone();
        return fRc;

    def testAutostart(self):
        """
        Executes autostart test.
        """

        # Loop thru the test VMs.
        for sVM in self.asTestVMs:
            # run test on the VM.
            if not self.testAutostartForOneVM(sVM):
                fRc = False;
            else:
                fRc = True;

        return fRc;



if __name__ == '__main__':
    sys.exit(tdAutostart().main(sys.argv));

