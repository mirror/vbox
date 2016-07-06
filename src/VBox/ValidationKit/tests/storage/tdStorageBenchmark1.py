#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Storage benchmark.
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
import os;
import socket;
import sys;
import StringIO;

# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0];
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksValidationKitDir);

# Validation Kit imports.
from common     import constants;
from testdriver import reporter;
from testdriver import base;
from testdriver import vbox;
from testdriver import vboxcon;

import remoteexecutor;
import storagecfg;

def _ControllerTypeToName(eControllerType):
    """ Translate a controller type to a name. """
    if eControllerType == vboxcon.StorageControllerType_PIIX3 or eControllerType == vboxcon.StorageControllerType_PIIX4:
        sType = "IDE Controller";
    elif eControllerType == vboxcon.StorageControllerType_IntelAhci:
        sType = "SATA Controller";
    elif eControllerType == vboxcon.StorageControllerType_LsiLogicSas:
        sType = "SAS Controller";
    elif eControllerType == vboxcon.StorageControllerType_LsiLogic or eControllerType == vboxcon.StorageControllerType_BusLogic:
        sType = "SCSI Controller";
    else:
        sType = "Storage Controller";
    return sType;

class FioTest(object):
    """
    Flexible I/O tester testcase.
    """

    kdHostIoEngine = {
        'solaris': 'solarisaio',
        'linux':   'libaio'
    };

    def __init__(self, oExecutor, dCfg = None):
        self.oExecutor  = oExecutor;
        self.sCfgFileId = None;
        self.dCfg       = dCfg;

    def prepare(self, cMsTimeout = 30000):
        """ Prepares the testcase """

        sTargetOs = self.dCfg.get('TargetOs', 'linux');
        sIoEngine = self.kdHostIoEngine.get(sTargetOs);
        if sIoEngine is None:
            return False;

        cfgBuf = StringIO.StringIO();
        cfgBuf.write('[global]\n');
        cfgBuf.write('bs=' + self.dCfg.get('RecordSize', '4k') + '\n');
        cfgBuf.write('ioengine=' + sIoEngine + '\n');
        cfgBuf.write('iodepth=' + self.dCfg.get('QueueDepth', '32') + '\n');
        cfgBuf.write('size=' + self.dCfg.get('TestsetSize', '2g') + '\n');
        cfgBuf.write('direct=1\n');
        cfgBuf.write('directory=' + self.dCfg.get('FilePath', '/mnt') + '\n');

        cfgBuf.write('[seq-write]\n');
        cfgBuf.write('rw=write\n');
        cfgBuf.write('stonewall\n');

        cfgBuf.write('[rand-write]\n');
        cfgBuf.write('rw=randwrite\n');
        cfgBuf.write('stonewall\n');

        cfgBuf.write('[seq-read]\n');
        cfgBuf.write('rw=read\n');
        cfgBuf.write('stonewall\n');

        cfgBuf.write('[rand-read]\n');
        cfgBuf.write('rw=randread\n');
        cfgBuf.write('stonewall\n');

        self.sCfgFileId = self.oExecutor.copyString(cfgBuf, 'aio-test', cMsTimeout);
        return self.sCfgFileId is not None;

    def run(self, cMsTimeout = 30000):
        """ Runs the testcase """
        _ = cMsTimeout
        fRc, sOutput = self.oExecutor.execBinary('fio', (self.sCfgFileId,));
        # @todo: Parse output.
        _ = sOutput;
        return fRc;

    def cleanup(self):
        """ Cleans up any leftovers from the testcase. """

    def reportResult(self):
        """
        Reports the test results to the test manager.
        """
        return True;

class IozoneTest(object):
    """
    I/O zone testcase.
    """
    def __init__(self, oExecutor, dCfg = None):
        self.oExecutor = oExecutor;
        self.sResult = None;
        self.lstTests = [ ('initial writers', 'FirstWrite'),
                          ('rewriters',       'Rewrite'),
                          ('re-readers',      'ReRead'),
                          ('stride readers',  'StrideRead'),
                          ('random readers',  'RandomRead'),
                          ('mixed workload',  'MixedWorkload'),
                          ('random writers',  'RandomWrite'),
                          ('pwrite writers',  'PWrite'),
                          ('pread readers',   'PRead'),
                          ('readers',         'FirstRead')];
        self.sRecordSize  = dCfg.get('RecordSize',  '4k');
        self.sTestsetSize = dCfg.get('TestsetSize', '2g');
        self.sQueueDepth  = dCfg.get('QueueDepth',  '32');
        self.sFilePath    = dCfg.get('FilePath',    '/mnt/iozone');

    def prepare(self, cMsTimeout = 30000):
        """ Prepares the testcase """
        _ = cMsTimeout;
        return True; # Nothing to do.

    def run(self, cMsTimeout = 30000):
        """ Runs the testcase """
        fRc, sOutput = self.oExecutor.execBinary('iozone', ('-r', self.sRecordSize, '-s', self.sTestsetSize, \
                                                            '-t', '1', '-T', '-I', \
                                                            '-H', self.sQueueDepth,'-F', self.sFilePath));
        if fRc:
            self.sResult = sOutput;

        _ = cMsTimeout;
        return fRc;

    def cleanup(self):
        """ Cleans up any leftovers from the testcase. """
        return True;

    def reportResult(self):
        """
        Reports the test results to the test manager.
        """

        fRc = True;
        if self.sResult is not None:
            try:
                asLines = self.sResult.splitlines();
                for sLine in asLines:
                    sLine = sLine.strip();
                    if sLine.startswith('Children') is True:
                        # Extract the value
                        idxValue = sLine.rfind('=');
                        if idxValue is -1:
                            raise Exception('IozoneTest: Invalid state');

                        idxValue += 1;
                        while sLine[idxValue] == ' ':
                            idxValue += 1;

                        idxValueEnd = idxValue;
                        while sLine[idxValueEnd] == '.' or sLine[idxValueEnd].isdigit():
                            idxValueEnd += 1;

                        for sNeedle, sTestVal in self.lstTests:
                            if sLine.rfind(sNeedle) is not -1:
                                reporter.testValue(sTestVal, sLine[idxValue:idxValueEnd],
                                                   constants.valueunit.g_asNames[constants.valueunit.KILOBYTES_PER_SEC]);
            except:
                fRc = False;
        else:
            fRc = False;

        return fRc;


class tdStorageBenchmark(vbox.TestDriver):                                      # pylint: disable=R0902
    """
    Storage benchmark.
    """

    def __init__(self):
        vbox.TestDriver.__init__(self);
        self.asRsrcs           = None;
        self.oGuestToGuestVM   = None;
        self.oGuestToGuestSess = None;
        self.oGuestToGuestTxs  = None;
        self.asTestVMsDef      = ['tst-debian'];
        self.asTestVMs         = self.asTestVMsDef;
        self.asSkipVMs         = [];
        self.asVirtModesDef    = ['hwvirt', 'hwvirt-np', 'raw',]
        self.asVirtModes       = self.asVirtModesDef
        self.acCpusDef         = [1, 2,]
        self.acCpus            = self.acCpusDef;
        self.asStorageCtrlsDef = ['AHCI', 'IDE', 'LsiLogicSAS', 'LsiLogic', 'BusLogic'];
        self.asStorageCtrls    = self.asStorageCtrlsDef;
        self.asDiskFormatsDef  = ['VDI', 'VMDK', 'VHD', 'QED', 'Parallels', 'QCOW', 'iSCSI'];
        self.asDiskFormats     = self.asDiskFormatsDef;
        self.asTestsDef        = ['iozone', 'fio'];
        self.asTests           = self.asTestsDef;
        self.asIscsiTargetsDef = ['aurora|iqn.2011-03.home.aurora:aurora.storagebench|1'];
        self.asIscsiTargets    = self.asIscsiTargetsDef;
        self.fTestHost         = False;

    #
    # Overridden methods.
    #
    def showUsage(self):
        rc = vbox.TestDriver.showUsage(self);
        reporter.log('');
        reporter.log('tdStorageBenchmark1 Options:');
        reporter.log('  --virt-modes    <m1[:m2[:]]');
        reporter.log('      Default: %s' % (':'.join(self.asVirtModesDef)));
        reporter.log('  --cpu-counts    <c1[:c2[:]]');
        reporter.log('      Default: %s' % (':'.join(str(c) for c in self.acCpusDef)));
        reporter.log('  --storage-ctrls <type1[:type2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(self.asStorageCtrls)));
        reporter.log('  --disk-formats  <type1[:type2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(self.asDiskFormats)));
        reporter.log('  --iscsi-targets     <target1[:target2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(self.asIscsiTargets)));
        reporter.log('  --tests         <test1[:test2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(self.asTests)));
        reporter.log('  --test-vms      <vm1[:vm2[:...]]>');
        reporter.log('      Test the specified VMs in the given order. Use this to change');
        reporter.log('      the execution order or limit the choice of VMs');
        reporter.log('      Default: %s  (all)' % (':'.join(self.asTestVMsDef)));
        reporter.log('  --skip-vms      <vm1[:vm2[:...]]>');
        reporter.log('      Skip the specified VMs when testing.');
        reporter.log('  --test-host');
        reporter.log('      Do all configured tests on the host first and report the results');
        reporter.log('      to get a baseline');
        return rc;

    def parseOption(self, asArgs, iArg):                                        # pylint: disable=R0912,R0915
        if asArgs[iArg] == '--virt-modes':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--virt-modes" takes a colon separated list of modes');
            self.asVirtModes = asArgs[iArg].split(':');
            for s in self.asVirtModes:
                if s not in self.asVirtModesDef:
                    raise base.InvalidOption('The "--virt-modes" value "%s" is not valid; valid values are: %s' \
                        % (s, ' '.join(self.asVirtModesDef)));
        elif asArgs[iArg] == '--cpu-counts':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--cpu-counts" takes a colon separated list of cpu counts');
            self.acCpus = [];
            for s in asArgs[iArg].split(':'):
                try: c = int(s);
                except: raise base.InvalidOption('The "--cpu-counts" value "%s" is not an integer' % (s,));
                if c <= 0:  raise base.InvalidOption('The "--cpu-counts" value "%s" is zero or negative' % (s,));
                self.acCpus.append(c);
        elif asArgs[iArg] == '--storage-ctrls':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--storage-ctrls" takes a colon separated list of Storage controller types');
            self.asStorageCtrls = asArgs[iArg].split(':');
        elif asArgs[iArg] == '--disk-formats':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--disk-formats" takes a colon separated list of disk formats');
            self.asDiskFormats = asArgs[iArg].split(':');
        elif asArgs[iArg] == '--iscsi-targets':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--iscsi-targets" takes a colon separated list of iscsi targets');
            self.asIscsiTargets = asArgs[iArg].split(':');
        elif asArgs[iArg] == '--tests':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--tests" takes a colon separated list of disk formats');
            self.asTests = asArgs[iArg].split(':');
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
        elif asArgs[iArg] == '--test-host':
            self.fTestHost = True;
        else:
            return vbox.TestDriver.parseOption(self, asArgs, iArg);
        return iArg + 1;

    def completeOptions(self):
        # Remove skipped VMs from the test list.
        for sVM in self.asSkipVMs:
            try:    self.asTestVMs.remove(sVM);
            except: pass;

        return vbox.TestDriver.completeOptions(self);

    def getResourceSet(self):
        # Construct the resource list the first time it's queried.
        if self.asRsrcs is None:
            self.asRsrcs = [];
            if 'tst-debian' in self.asTestVMs:
                self.asRsrcs.append('4.2/storage/debian.vdi');

        return self.asRsrcs;

    def actionConfig(self):

        # Make sure vboxapi has been imported so we can use the constants.
        if not self.importVBoxApi():
            return False;

        #
        # Configure the VMs we're going to use.
        #

        # Linux VMs
        if 'tst-debian' in self.asTestVMs:
            oVM = self.createTestVM('tst-debian', 1, '4.2/storage/debian.vdi', sKind = 'Debian_64', fIoApic = True, \
                                    eNic0AttachType = vboxcon.NetworkAttachmentType_NAT, \
                                    eNic0Type = vboxcon.NetworkAdapterType_Am79C973);
            if oVM is None:
                return False;

        return True;

    def actionExecute(self):
        """
        Execute the testcase.
        """
        fRc = self.test1();
        return fRc;


    #
    # Test execution helpers.
    #

    def test1Benchmark(self, sBenchmark, oTxsSession = None):
        """
        Runs the given benchmark on the test host.
        """
        lstBinaryPaths = ['/bin', '/sbin', '/usr/bin', '/usr/sbin', \
                          '/opt/csw/bin', '/usr/ccs/bin', '/usr/sfw/bin'];

        oExecutor = remoteexecutor.RemoteExecutor(oTxsSession, lstBinaryPaths, self.sScratchPath);
        oTst = None;
        if sBenchmark == 'iozone':
            oTst = IozoneTest(oExecutor);
        elif sBenchmark == 'fio':
            oTst = FioTest(oExecutor); # pylint: disable=R0204

        if oTst is not None:
            reporter.testStart(sBenchmark);

            # Create a basic pool with the default configuration.
            oStorCfg = storagecfg.StorageCfg(oExecutor, socket.gethostname().lower());
            fRc, sPoolId = oStorCfg.createStoragePool(oExecutor);
            if fRc:
                fRc, sMountpoint = oStorCfg.createVolume(sPoolId);
                _ = sMountpoint;
                if fRc:
                    oTst = IozoneTest(oExecutor);
                    fRc = oTst.prepare();
                    if fRc:
                        fRc = oTst.run();
                        if fRc:
                            fRc = oTst.reportResult();
                        else:
                            reporter.testFailure('Running the testcase failed');
                    else:
                        reporter.testFailure('Preparing the testcase failed');

                    oTst.cleanup();
            else:
                reporter.testFailure('Creating a storage pool on the target failed');

            oStorCfg.cleanup();
            reporter.testDone();

        return fRc;

    def test1Benchmarks(self, oTxsSession = None):
        """
        Runs all the configured benchmarks on the target.
        """
        reporter.testStart('Host');
        for sTest in self.asTests:
            self.test1Benchmark(sTest, oTxsSession);
        reporter.testDone();

    def test1(self):
        """
        Executes test #1.
        """

        # Test the host first if requested
        fRc = True;
        if self.fTestHost:
            fRc = self.test1Benchmarks()

        # Loop thru the test VMs.
        #for sVM in self.asTestVMs:
        #    # run test on the VM.
        #    if not self.test1OneVM(sVM):
        #        fRc = False;
        #    else:
        #        fRc = True;

        return fRc;



if __name__ == '__main__':
    sys.exit(tdStorageBenchmark().main(sys.argv));

