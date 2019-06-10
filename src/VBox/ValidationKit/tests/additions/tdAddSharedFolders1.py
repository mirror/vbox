#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
VirtualBox Validation Kit - Shared Folders #1.
"""

__copyright__ = \
"""
Copyright (C) 2010-2019 Oracle Corporation

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
import os
import shutil
import sys

# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0];
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksValidationKitDir);

# Validation Kit imports.
from testdriver import reporter;
from testdriver import base;
from common     import utils;


class SubTstDrvAddSharedFolders1(base.SubTestDriverBase):
    """
    Sub-test driver for executing shared folders tests.
    """

    def __init__(self, oTstDrv):
        base.SubTestDriverBase.__init__(self, oTstDrv, 'add-shared-folders', 'Shared Folders');

        self.asTestsDef  = [ 'fsperf', ];
        self.asTests     = self.asTestsDef;
        self.asExtraArgs = [];

    def parseOption(self, asArgs, iArg):
        if asArgs[iArg] == '--add-shared-folders-tests': # 'add' as in 'additions', not the verb.
            iArg += 1;
            iNext = self.oTstDrv.requireMoreArgs(1, asArgs, iArg);
            if asArgs[iArg] == 'all':
                self.asTests = self.asTestsDef;
            else:
                self.asTests = asArgs[iArg].split(':');
                for s in self.asTests:
                    if s not in self.asTestsDef:
                        raise base.InvalidOption('The "--add-shared-folders-tests" value "%s" is not valid; valid values are: %s'
                                                 % (s, ' '.join(self.asTestsDef)));
            return iNext;
        elif asArgs[iArg] == '--add-shared-folders-extra-arg':
            iArg += 1;
            iNext = self.oTstDrv.requireMoreArgs(1, asArgs, iArg);
            self.asExtraArgs.append(asArgs[iArg]);
            return iNext;
        return iArg;

    def showUsage(self):
        base.SubTestDriverBase.showUsage(self);
        reporter.log('  --add-shared-folders-tests <t1[:t2[:]]>');
        reporter.log('      Default: all  (%s)' % (':'.join(self.asTestsDef)));
        reporter.log('  --add-shared-folders-extra-arg <fsperf-arg>');
        reporter.log('      Adds an extra FsPerf argument.  Can be repeated.');

        return True;

    def testIt(self, oTestVm, oSession, oTxsSession):
        """
        Executes the test.

        Returns fRc, oTxsSession.  The latter may have changed.
        """
        reporter.log("Active tests: %s" % (self.asTests,));

        #
        # Skip the test if before 6.0
        #
        if self.oTstDrv.fpApiVer < 6.0:
            reporter.log('Requires 6.0 or later (for now)');
            return (None, oTxsSession);

        #
        # Create the host directory to share. Empty except for a 'candle.dir' subdir
        # that we use to check that it mounted correctly.
        #
        sSharedFolder1 = os.path.join(self.oTstDrv.sScratchPath, 'shfl1');
        reporter.log2('Creating shared host folder "%s"...' % (sSharedFolder1,));
        if os.path.exists(sSharedFolder1):
            try:    shutil.rmtree(sSharedFolder1);
            except: return (reporter.errorXcpt('shutil.rmtree(%s)' % (sSharedFolder1,)), oTxsSession);
        try:    os.mkdir(sSharedFolder1);
        except: return (reporter.errorXcpt('os.mkdir(%s)' % (sSharedFolder1,)), oTxsSession);
        try:    os.mkdir(os.path.join(sSharedFolder1, 'candle.dir'));
        except: return (reporter.errorXcpt('os.mkdir(%s)' % (sSharedFolder1,)), oTxsSession);

        # Guess a free mount point inside the guest.
        if oTestVm.isWindows() or oTestVm.isOS2():
            sMountPoint1 = 'V:';
            sGuestSlash  = '\\';
        else:
            sMountPoint1 = '/mnt/shfl1';
            sGuestSlash  = '/';

        #
        # Automount a shared folder in the guest.
        #
        reporter.testStart('Automount');

        reporter.log2('Creating shared folder shfl1...');
        try:
            oConsole = oSession.o.console;
            oConsole.createSharedFolder('shfl1', sSharedFolder1, True, True, sMountPoint1);
        except:
            reporter.errorXcpt('createSharedFolder(shfl1,%s,True,True,%s)' % (sSharedFolder1,sMountPoint1));
            reporter.testDone();
            return (False, oTxsSession);

        # Check whether we can see the shared folder now.  Retry for 30 seconds.
        msStart = base.timestampMilli();
        while True:
            fRc = oTxsSession.syncIsDir(sMountPoint1 + sGuestSlash + 'candle.dir');
            reporter.log2('candle.dir check -> %s' % (fRc,));
            if fRc is not False:
                break;
            if base.timestampMilli() - msStart > 30000:
                reporter.error('Shared folder mounting timed out!');
                break;
            self.oTstDrv.sleep(1);

        reporter.testDone();
        if fRc is not True:
            return (False, oTxsSession); # skip the remainder if we cannot auto mount the folder.

        #
        # Run FsPerf inside the guest.
        #
        fSkip = 'fsperf' not in self.asTests;
        if fSkip is False:
            cMbFree = utils.getDiskUsage(sSharedFolder1);
            if cMbFree >= 16:
                reporter.log2('Free space: %u MBs' % (cMbFree,));
            else:
                reporter.log('Skipping FsPerf because only %u MB free on %s' % (cMbFree, sSharedFolder1,));
                fSkip = True;
        if fSkip is False:
            # Common arguments:
            asArgs = ['FsPerf', '-d', sMountPoint1 + sGuestSlash + 'fstestdir-1', '-s8'];

            # Skip part of mmap on older windows systems without CcCoherencyFlushAndPurgeCache (>= w7).
            reporter.log2('oTestVm.sGuestOsType=%s' % (oTestVm.sGuestOsType,));
            if   oTestVm.getNonCanonicalGuestOsType() \
              in [ 'WindowsNT3x', 'WindowsNT4', 'Windows2000', 'WindowsXP', 'WindowsXP_64', 'Windows2003',
                   'Windows2003_64', 'WindowsVista', 'WindowsVista_64', 'Windows2008', 'Windows2008_64']:
                asArgs.append('--no-mmap-coherency');

            # Configure I/O block sizes according to guest memory size:
            cbMbRam = 128;
            try:    cbMbRam = oSession.o.machine.memorySize;
            except: reporter.errorXcpt();
            reporter.log2('cbMbRam=%s' % (cbMbRam,));
            asArgs.append('--set-block-size=1');
            asArgs.append('--add-block-size=512');
            asArgs.append('--add-block-size=4096');
            asArgs.append('--add-block-size=16384');
            asArgs.append('--add-block-size=65536');
            asArgs.append('--add-block-size=1048576');       #   1 MiB
            if cbMbRam >= 512:
                asArgs.append('--add-block-size=33554432');  #  32 MiB
            if cbMbRam >= 768:
                asArgs.append('--add-block-size=134217728'); # 128 MiB

            # Putting lots (10000) of files in a single directory causes issues on OS X
            # (HFS+ presumably, though could be slow disks) and some linuxes (slow disks,
            # maybe ext2/3?).  So, generally reduce the file count to 4096 everywhere
            # since we're not here to test the host file systems, and 3072 on macs.
            if utils.getHostOs() in [ 'darwin', ]:
                asArgs.append('--many-files=3072');
            elif utils.getHostOs() in [ 'linux', ]:
                asArgs.append('--many-files=4096');

            # Add the extra arguments from the command line and kick it off:
            asArgs.extend(self.asExtraArgs);
            reporter.log2('Starting guest FsPerf (%s)...' % (asArgs,));
            fRc = self.oTstDrv.txsRunTest(oTxsSession, 'FsPerf', 30 * 60 * 1000,
                                          '${CDROM}/vboxvalidationkit/${OS/ARCH}/FsPerf${EXESUFF}', asArgs);
            reporter.log2('FsPerf -> %s' % (fRc,));

            sTestDir = os.path.join(sSharedFolder1, 'fstestdir-1');
            if os.path.exists(sTestDir):
                fRc = reporter.errorXcpt('test directory lingers: %s' % (sTestDir,));
                try:    shutil.rmtree(sTestDir);
                except: fRc = reporter.errorXcpt('shutil.rmtree(%s)' % (sTestDir,));
        else:
            reporter.testStart('FsPerf');
            reporter.testDone(fSkip or fRc is None);

        return (fRc, oTxsSession);



if __name__ == '__main__':
    reporter.error('Cannot run standalone, use tdAddBasic1.py');
    sys.exit(1);

