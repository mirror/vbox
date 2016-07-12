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
from common     import utils;
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
    elif eControllerType == vboxcon.StorageControllerType_NVMe:
        sType = "NVMe Controller";
    else:
        sType = "Storage Controller";
    return sType;

class FioTest(object):
    """
    Flexible I/O tester testcase.
    """

    kdHostIoEngine = {
        'solaris': ('solarisaio', False),
        'linux':   ('libaio', True)
    };

    def __init__(self, oExecutor, dCfg = None):
        self.oExecutor  = oExecutor;
        self.sCfgFileId = None;
        self.dCfg       = dCfg;

    def prepare(self, cMsTimeout = 30000):
        """ Prepares the testcase """

        sTargetOs = self.dCfg.get('TargetOs', 'linux');
        sIoEngine, fDirectIo = self.kdHostIoEngine.get(sTargetOs);
        if sIoEngine is None:
            return False;

        cfgBuf = StringIO.StringIO();
        cfgBuf.write('[global]\n');
        cfgBuf.write('bs=' + self.dCfg.get('RecordSize', '4k') + '\n');
        cfgBuf.write('ioengine=' + sIoEngine + '\n');
        cfgBuf.write('iodepth=' + self.dCfg.get('QueueDepth', '32') + '\n');
        cfgBuf.write('size=' + self.dCfg.get('TestsetSize', '2g') + '\n');
        if fDirectIo:
            cfgBuf.write('direct=1\n');
        else:
            cfgBuf.write('direct=0\n');
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

        self.sCfgFileId = self.oExecutor.copyString(cfgBuf.getvalue(), 'aio-test', cMsTimeout);
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
                          ('reverse readers', 'ReverseRead'),
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
        self.fDirectIo    = True;

        sTargetOs = dCfg.get('TargetOs');
        if sTargetOs == 'solaris':
            self.fDirectIo = False;

    def prepare(self, cMsTimeout = 30000):
        """ Prepares the testcase """
        _ = cMsTimeout;
        return True; # Nothing to do.

    def run(self, cMsTimeout = 30000):
        """ Runs the testcase """
        tupArgs = ('-r', self.sRecordSize, '-s', self.sTestsetSize, \
                   '-t', '1', '-T', '-F', self.sFilePath + '/iozone.tmp');
        if self.fDirectIo:
            tupArgs += ('-I',);
        fRc, sOutput = self.oExecutor.execBinary('iozone', tupArgs);
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

                        # Get the reported value, cut off after the decimal point
                        # it is not supported by the testmanager yet and is not really
                        # relevant anyway.
                        idxValueEnd = idxValue;
                        while sLine[idxValueEnd].isdigit():
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

    # Global storage configs for the testbox
    kdStorageCfgs = {
        'testboxstor1.de.oracle.com': r'c[3-9]t\dd0\Z',
        'adaris': [ '/dev/sda' ]
    };

    def __init__(self):
        vbox.TestDriver.__init__(self);
        self.asRsrcs           = None;
        self.oGuestToGuestVM   = None;
        self.oGuestToGuestSess = None;
        self.oGuestToGuestTxs  = None;
        self.asTestVMsDef      = ['tst-storage'];
        self.asTestVMs         = self.asTestVMsDef;
        self.asSkipVMs         = [];
        self.asVirtModesDef    = ['hwvirt', 'hwvirt-np', 'raw',]
        self.asVirtModes       = self.asVirtModesDef
        self.acCpusDef         = [1, 2,]
        self.acCpus            = self.acCpusDef;
        self.asStorageCtrlsDef = ['AHCI', 'IDE', 'LsiLogicSAS', 'LsiLogic', 'BusLogic', 'NVMe'];
        self.asStorageCtrls    = self.asStorageCtrlsDef;
        self.asDiskFormatsDef  = ['VDI', 'VMDK', 'VHD', 'QED', 'Parallels', 'QCOW', 'iSCSI'];
        self.asDiskFormats     = self.asDiskFormatsDef;
        self.asTestsDef        = ['iozone', 'fio'];
        self.asTests           = self.asTestsDef;
        self.asIscsiTargetsDef = [ ]; # @todo: Configure one target for basic iSCSI testing
        self.asIscsiTargets    = self.asIscsiTargetsDef;
        self.fTestHost         = False;
        self.fUseScratch       = False;
        self.oStorCfg          = None;

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
        reporter.log('  --use-scratch');
        reporter.log('      Use the scratch directory for testing instead of setting up');
        reporter.log('      fresh volumes on dedicated disks (for development)');
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
        elif asArgs[iArg] == '--use-scratch':
            self.fUseScratch = True;
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
            if 'tst-storage' in self.asTestVMs:
                self.asRsrcs.append('5.0/storage/tst-storage.vdi');

        return self.asRsrcs;

    def actionConfig(self):

        # Make sure vboxapi has been imported so we can use the constants.
        if not self.importVBoxApi():
            return False;

        #
        # Configure the VMs we're going to use.
        #

        # Linux VMs
        if 'tst-storage' in self.asTestVMs:
            oVM = self.createTestVM('tst-storage', 1, '5.0/storage/tst-storage.vdi', sKind = 'ArchLinux_64', fIoApic = True, \
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

    def prepareStorage(self, oStorCfg):
        """
        Prepares the host storage for disk images or direct testing on the host.
        """
        # Create a basic pool with the default configuration.
        sMountPoint = None;
        fRc, sPoolId = oStorCfg.createStoragePool();
        if fRc:
            fRc, sMountPoint = oStorCfg.createVolume(sPoolId);
            if not fRc:
                sMountPoint = None;
                oStorCfg.cleanup();

        return sMountPoint;

    def cleanupStorage(self, oStorCfg):
        """
        Cleans up any created storage space for a test.
        """
        return oStorCfg.cleanup();

    def getGuestDisk(self, oSession, oTxsSession, eStorageController):
        """
        Gets the path of the disk in the guest to use for testing.
        """
        lstDisks = None;

        # The naming scheme for NVMe is different and we don't have
        # to query the guest for unformatted disks here because the disk with the OS
        # is not attached to a NVMe controller.
        if eStorageController == vboxcon.StorageControllerType_NVMe:
            lstDisks = [ '/dev/nvme0n1' ];
        else:
            # Find a unformatted disk (no partition).
            # @todo: This is a hack because LIST and STAT are not yet implemented
            #        in TXS (get to this eventually)
            lstBlkDev = [ '/dev/sda', '/dev/sdb' ];
            for sBlkDev in lstBlkDev:
                fRc = oTxsSession.syncExec('/usr/bin/ls', ('ls', sBlkDev + '1'));
                if not fRc:
                    lstDisks = [ sBlkDev ];
                    break;

        _ = oSession;
        return lstDisks;

    def testBenchmark(self, sTargetOs, sBenchmark, sMountpoint, oExecutor):
        """
        Runs the given benchmark on the test host.
        """
        # Create a basic config
        dCfg = {
            'RecordSize':  '64k',
            'TestsetSize': '100m',
            'QueueDepth':  '32',
            'FilePath': sMountpoint,
            'TargetOs': sTargetOs
        };

        oTst = None;
        if sBenchmark == 'iozone':
            oTst = IozoneTest(oExecutor, dCfg);
        elif sBenchmark == 'fio':
            oTst = FioTest(oExecutor, dCfg); # pylint: disable=R0204

        if oTst is not None:
            reporter.testStart(sBenchmark);
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
        reporter.testDone();

        return fRc;

    def testBenchmarks(self, sTargetOs, sMountPoint, oExecutor):
        """
        Runs all the configured benchmarks on the target.
        """
        for sTest in self.asTests:
            self.testBenchmark(sTargetOs, sTest, sMountPoint, oExecutor);

    def test1OneCfg(self, sVmName, eStorageController, sDiskFormat, sDiskPath, cCpus, fHwVirt, fNestedPaging):
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
            # Attach HD
            fRc = oSession.ensureControllerAttached(_ControllerTypeToName(eStorageController));
            fRc = fRc and oSession.setStorageControllerType(eStorageController, _ControllerTypeToName(eStorageController));

            iDevice = 0;
            if eStorageController == vboxcon.StorageControllerType_PIIX3 or \
               eStorageController == vboxcon.StorageControllerType_PIIX4:
                iDevice = 1; # Master is for the OS.

            if sDiskFormat == "iSCSI":
                listNames = [];
                listValues = [];
                listValues = sDiskPath.split('|');
                listNames.append('TargetAddress');
                listNames.append('TargetName');
                listNames.append('LUN');

                if self.fpApiVer >= 5.0:
                    oHd = oSession.oVBox.createMedium(sDiskFormat, sDiskPath, vboxcon.AccessMode_ReadWrite, \
                                                      vboxcon.DeviceType_HardDisk);
                else:
                    oHd = oSession.oVBox.createHardDisk(sDiskFormat, sDiskPath);
                oHd.type = vboxcon.MediumType_Normal;
                oHd.setProperties(listNames, listValues);

                # Attach it.
                if fRc is True:
                    try:
                        if oSession.fpApiVer >= 4.0:
                            oSession.o.machine.attachDevice(_ControllerTypeToName(eStorageController), \
                                                            0, iDevice, vboxcon.DeviceType_HardDisk, oHd);
                        else:
                            oSession.o.machine.attachDevice(_ControllerTypeToName(eStorageController), \
                                                            0, iDevice, vboxcon.DeviceType_HardDisk, oHd.id);
                    except:
                        reporter.errorXcpt('attachDevice("%s",%s,%s,HardDisk,"%s") failed on "%s"' \
                                           % (_ControllerTypeToName(eStorageController), 1, 0, oHd.id, oSession.sName) );
                        fRc = False;
                    else:
                        reporter.log('attached "%s" to %s' % (sDiskPath, oSession.sName));
            else:
                fRc = fRc and oSession.createAndAttachHd(sDiskPath, sDiskFormat, _ControllerTypeToName(eStorageController), \
                                                         cb = 300*1024*1024*1024, iPort = 0, iDevice = iDevice, \
                                                         fImmutable = False);
            fRc = fRc and oSession.enableVirtEx(fHwVirt);
            fRc = fRc and oSession.enableNestedPaging(fNestedPaging);
            fRc = fRc and oSession.setCpuCount(cCpus);
            fRc = fRc and oSession.saveSettings();
            fRc = oSession.close() and fRc and True; # pychecker hack.
            oSession = None;
        else:
            fRc = False;

        # Start up.
        if fRc is True:
            self.logVmInfo(oVM);
            oSession, oTxsSession = self.startVmAndConnectToTxsViaTcp(sVmName, fCdWait = False, fNatForwardingForTxs = True);
            if oSession is not None:
                self.addTask(oSession);

                # Fudge factor - Allow the guest to finish starting up.
                self.sleep(5);

                # Prepare the storage on the guest
                lstBinaryPaths = ['/bin', '/sbin', '/usr/bin', '/usr/sbin' ];
                oExecVm = remoteexecutor.RemoteExecutor(oTxsSession, lstBinaryPaths, '${SCRATCH}');
                oStorCfgVm = storagecfg.StorageCfg(oExecVm, 'linux', self.getGuestDisk(oSession, oTxsSession, \
                                                                                       eStorageController));

                sMountPoint = self.prepareStorage(oStorCfgVm);
                if sMountPoint is not None:
                    self.testBenchmarks('linux', sMountPoint, oExecVm);
                    self.cleanupStorage(oStorCfgVm);
                else:
                    reporter.testFailure('Failed to prepare storage for the guest benchmark');

                # cleanup.
                self.removeTask(oTxsSession);
                self.terminateVmBySession(oSession)

                # Remove disk
                oSession = self.openSession(oVM);
                if oSession is not None:
                    try:
                        oSession.o.machine.detachDevice(_ControllerTypeToName(eStorageController), 0, iDevice);

                        # Remove storage controller if it is not an IDE controller.
                        if     eStorageController is not vboxcon.StorageControllerType_PIIX3 \
                           and eStorageController is not vboxcon.StorageControllerType_PIIX4:
                            oSession.o.machine.removeStorageController(_ControllerTypeToName(eStorageController));

                        oSession.saveSettings();
                        self.oVBox.deleteHdByLocation(sDiskPath);
                        oSession.saveSettings();
                        oSession.close();
                        oSession = None;
                    except:
                        reporter.errorXcpt('failed to detach/delete disk %s from storage controller' % (sDiskPath));
                else:
                    fRc = False;
            else:
                fRc = False;
        return fRc;

    def testBenchmarkOneVM(self, sVmName):
        """
        Runs one VM thru the various benchmark configurations.
        """
        reporter.testStart(sVmName);
        fRc = True;
        for sStorageCtrl in self.asStorageCtrls:
            reporter.testStart(sStorageCtrl);

            if sStorageCtrl == 'AHCI':
                eStorageCtrl = vboxcon.StorageControllerType_IntelAhci;
            elif sStorageCtrl == 'IDE':
                eStorageCtrl = vboxcon.StorageControllerType_PIIX4;
            elif sStorageCtrl == 'LsiLogicSAS':
                eStorageCtrl = vboxcon.StorageControllerType_LsiLogicSas;
            elif sStorageCtrl == 'LsiLogic':
                eStorageCtrl = vboxcon.StorageControllerType_LsiLogic;
            elif sStorageCtrl == 'BusLogic':
                eStorageCtrl = vboxcon.StorageControllerType_BusLogic;
            elif sStorageCtrl == 'NVMe':
                eStorageCtrl = vboxcon.StorageControllerType_NVMe;
            else:
                eStorageCtrl = None;

            for sDiskFormat in self.asDiskFormats:
                reporter.testStart('%s' % (sDiskFormat));

                if sDiskFormat == "iSCSI":
                    asPaths = self.asIscsiTargets;
                else:
                    if self.fUseScratch:
                        asPaths = [ self.sScratchPath ];
                    else:
                        # Create a new default storage config on the host
                        sMountPoint = self.prepareStorage(self.oStorCfg);
                        if sMountPoint is not None:
                            # Create a directory where every normal user can write to.
                            self.oStorCfg.mkDirOnVolume(sMountPoint, 'test', 0777);
                            asPaths = [ sMountPoint + '/test' ];
                        else:
                            asPaths = [];
                            fRc = False;
                            reporter.testFailure('Failed to prepare storage for VM');

                for sPath in asPaths:
                    reporter.testStart('%s' % (sPath));

                    if sDiskFormat == "iSCSI":
                        sPath = sPath;
                    else:
                        sPath = sPath + "/test.disk";

                    for cCpus in self.acCpus:
                        if cCpus == 1:  reporter.testStart('1 cpu');
                        else:           reporter.testStart('%u cpus' % (cCpus));

                        for sVirtMode in self.asVirtModes:
                            if sVirtMode == 'raw' and cCpus > 1:
                                continue;
                            hsVirtModeDesc = {};
                            hsVirtModeDesc['raw']       = 'Raw-mode';
                            hsVirtModeDesc['hwvirt']    = 'HwVirt';
                            hsVirtModeDesc['hwvirt-np'] = 'NestedPaging';
                            reporter.testStart(hsVirtModeDesc[sVirtMode]);

                            fHwVirt       = sVirtMode != 'raw';
                            fNestedPaging = sVirtMode == 'hwvirt-np';
                            fRc = self.test1OneCfg(sVmName, eStorageCtrl, sDiskFormat, sPath, \
                                                   cCpus, fHwVirt, fNestedPaging)  and  fRc and True; # pychecker hack.
                            reporter.testDone();

                        reporter.testDone();
                    reporter.testDone();

                # Cleanup storage area
                if sDiskFormat != 'iSCSI' and not self.fUseScratch:
                    self.cleanupStorage(self.oStorCfg);

                reporter.testDone();
            reporter.testDone();
        reporter.testDone();
        return fRc;

    def test1(self):
        """
        Executes test #1.
        """

        fRc = True;
        oDiskCfg = self.kdStorageCfgs.get(socket.gethostname().lower());

        # Test the host first if requested
        if oDiskCfg is not None:
            lstBinaryPaths = ['/bin', '/sbin', '/usr/bin', '/usr/sbin', \
                              '/opt/csw/bin', '/usr/ccs/bin', '/usr/sfw/bin'];
            oExecutor = remoteexecutor.RemoteExecutor(None, lstBinaryPaths, self.sScratchPath);
            self.oStorCfg = storagecfg.StorageCfg(oExecutor, utils.getHostOs(), oDiskCfg);

            if self.fTestHost:
                reporter.testStart('Host');
                if self.fUseScratch:
                    sMountPoint = self.sScratchPath;
                else:
                    sMountPoint = self.prepareStorage(self.oStorCfg);
                if sMountPoint is not None:
                    fRc = self.testBenchmarks(utils.getHostOs(), sMountPoint, oExecutor);
                    self.cleanupStorage(self.oStorCfg);
                else:
                    reporter.testFailure('Failed to prepare host storage');
                reporter.testDone();

            if fRc:
                # Loop thru the test VMs.
                for sVM in self.asTestVMs:
                    # run test on the VM.
                    if not self.testBenchmarkOneVM(sVM):
                        fRc = False;
                    else:
                        fRc = True;
        else:
            fRc = False;

        return fRc;

if __name__ == '__main__':
    sys.exit(tdStorageBenchmark().main(sys.argv));

