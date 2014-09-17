#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Storage benchmark.
"""

__copyright__ = \
"""
Copyright (C) 2012-2014 Oracle Corporation

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
import array;
import os;
import sys;
import StringIO;

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

class IozoneStdOutWrapper(object):
    """ Parser for iozone standard output """
    def __init__(self):
        self.fpInitWriter    = 0.0;
        self.fpRewriter      = 0.0;
        self.fpReader        = 0.0;
        self.fpRereader      = 0.0;
        self.fpReverseReader = 0.0;
        self.fpStrideReader  = 0.0;
        self.fpRandomReader  = 0.0;
        self.fpMixedWorkload = 0.0;
        self.fpRandomWriter  = 0.0;
        self.fpPWrite        = 0.0;
        self.fpPRead         = 0.0;

    def read(self, cb):
        """file.read"""
        _ = cb;
        return "";

    def write(self, sText):
        """iozone stdout write"""
        if isinstance(sText, array.array):
            try:
                sText = sText.tostring();
            except:
                pass;
        try:
            asLines = sText.splitlines();
            for sLine in asLines:
                sLine = sLine.strip();
                if sLine.startswith('Children') is True:
                    # Extract the value
                    idxValue = sLine.rfind('=');
                    if idxValue is -1:
                        raise Exception('IozoneStdOutWrapper: Invalid state');

                    idxValue += 1;
                    while sLine[idxValue] == ' ':
                        idxValue += 1;

                    idxValueEnd = idxValue;
                    while sLine[idxValueEnd] == '.' or sLine[idxValueEnd].isdigit():
                        idxValueEnd += 1;

                    fpValue = float(sLine[idxValue:idxValueEnd]);

                    if sLine.rfind('initial writers') is not -1:
                        self.fpInitWriter = fpValue;
                    elif sLine.rfind('rewriters') is not -1:
                        self.fpRewriter = fpValue;
                    elif sLine.rfind('re-readers') is not -1:
                        self.fpRereader = fpValue;
                    elif sLine.rfind('reverse readers') is not -1:
                        self.fpReverseReader = fpValue;
                    elif sLine.rfind('stride readers') is not -1:
                        self.fpStrideReader = fpValue;
                    elif sLine.rfind('random readers') is not -1:
                        self.fpRandomReader = fpValue;
                    elif sLine.rfind('mixed workload') is not -1:
                        self.fpMixedWorkload = fpValue;
                    elif sLine.rfind('random writers') is not -1:
                        self.fpRandomWriter = fpValue;
                    elif sLine.rfind('pwrite writers') is not -1:
                        self.fpPWrite = fpValue;
                    elif sLine.rfind('pread readers') is not -1:
                        self.fpPRead = fpValue;
                    elif sLine.rfind('readers') is not -1:
                        self.fpReader = fpValue;
                    else:
                        reporter.log('Unknown test returned %s' % sLine);
        except:
            pass;
        return None;

    def getInitWriter(self):
        """Get value for initial writers"""
        return self.fpInitWriter;

    def getRewriter(self):
        """Get value for re-writers"""
        return self.fpRewriter;

    def getReader(self):
        """Get value for initial readers"""
        return self.fpReader;

    def getRereader(self):
        """Get value for re-writers"""
        return self.fpRereader;

    def getReverseReader(self):
        """Get value for reverse readers"""
        return self.fpReverseReader;

    def getStrideReader(self):
        """Get value for stride readers"""
        return self.fpStrideReader;

    def getRandomReader(self):
        """Get value for random readers"""
        return self.fpRandomReader;

    def getMixedWorkload(self):
        """Get value for mixed workload"""
        return self.fpMixedWorkload;

    def getRandomWriter(self):
        """Get value for random writers"""
        return self.fpRandomWriter;

    def getPWrite(self):
        """Get value for pwrite writers"""
        return self.fpPWrite;

    def getPRead(self):
        """Get value for pread readers"""
        return self.fpPRead;

class FioWrapper(object):
    """ Fio stdout parser and config file creator """
    def __init__(self, sRecordSize, sTestsetSize, sQueueDepth, sPath):

        self.configBuf = StringIO.StringIO();
        self.configBuf.write('[global]\n');
        self.configBuf.write('bs=' + sRecordSize + '\n');
        self.configBuf.write('ioengine=libaio\n');
        self.configBuf.write('iodepth=' + sQueueDepth + '\n');
        self.configBuf.write('size=' + sTestsetSize + '\n');
        self.configBuf.write('direct=1\n');
        self.configBuf.write('directory=' + sPath + '\n');

        self.configBuf.write('[seq-write]\n');
        self.configBuf.write('rw=write\n');
        self.configBuf.write('stonewall\n');

        self.configBuf.write('[rand-write]\n');
        self.configBuf.write('rw=randwrite\n');
        self.configBuf.write('stonewall\n');

        self.configBuf.write('[seq-read]\n');
        self.configBuf.write('rw=read\n');
        self.configBuf.write('stonewall\n');

        self.configBuf.write('[rand-read]\n');
        self.configBuf.write('rw=randread\n');
        self.configBuf.write('stonewall\n');
        return;

    def getConfig(self):
        """fio stdin feeder, gives the config file based on the given config values"""
        return self.configBuf.getvalue();

    def write(self, sText):
        """fio stdout write"""
        if isinstance(sText, array.array):
            try:
                sText = sText.tostring();
            except:
                pass;
        try:
            asLines = sText.splitlines();
            for sLine in asLines:
                reporter.log(sLine);
        except:
            pass;
        return None;


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
        self.asDirsDef         = ['/run/media/alexander/OWCSSD/alexander', \
                                  '/run/media/alexander/CrucialSSD/alexander', \
                                  '/run/media/alexander/HardDisk/alexander', \
                                  '/home/alexander'];
        self.asDirs            = self.asDirsDef;
        self.asIscsiTargetsDef = ['aurora|iqn.2011-03.home.aurora:aurora.storagebench|1'];
        self.asIscsiTargets    = self.asIscsiTargetsDef;

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
        reporter.log('  --disk-dirs     <path1[:path2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(self.asDirs)));
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
        elif asArgs[iArg] == '--disk-dirs':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--disk-dirs" takes a colon separated list of directories');
            self.asDirs = asArgs[iArg].split(':');
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

    def test1RunTestProgs(self, oSession, oTxsSession, fRc, sTestName):
        """
        Runs all the test programs on the test machine.
        """
        reporter.testStart(sTestName);

        # Prepare test disk, just create filesystem without partition
        reporter.testStart('mkfs.ext4');
        fRc = self.txsRunTest(oTxsSession, 'Create FS', 60000, \
            '/sbin/mkfs.ext4',
                ('mkfs.ext4', '-F', '/dev/vboxtest'));
        reporter.testDone();

        reporter.testStart('mount');
        fRc = self.txsRunTest(oTxsSession, 'Mount FS', 30000, \
            '/bin/mount',
                ('mount', '/dev/vboxtest', '/mnt'));
        reporter.testDone();

        reporter.testStart('iozone');
        if fRc and 'iozone' in self.asTests:
            oStdOut = IozoneStdOutWrapper();
            fRc = self.txsRunTestRedirectStd(oTxsSession, '2G', 3600000, \
                '/usr/bin/iozone',
                    ('iozone', '-r', '64k', '-s', '2g', '-t', '1', '-T', '-I', '-H', '32','-F', '/mnt/iozone'), \
                (), '', '/dev/null', oStdOut, '/dev/null', '/dev/null');
            if fRc is True:
                reporter.log("Initial writer: " + str(oStdOut.getInitWriter()));
                reporter.log("Rewriter:       " + str(oStdOut.getRewriter()));
                reporter.log("Initial reader: " + str(oStdOut.getReader()));
                reporter.log("Re-reader:      " + str(oStdOut.getRereader()));
                reporter.log("Reverse reader: " + str(oStdOut.getReverseReader()));
                reporter.log("Stride reader:  " + str(oStdOut.getStrideReader()));
                reporter.log("Random reader:  " + str(oStdOut.getRandomReader()));
                reporter.log("Mixed Workload: " + str(oStdOut.getMixedWorkload()));
                reporter.log("Random writer:  " + str(oStdOut.getRandomWriter()));
                reporter.log("pwrite Writer:  " + str(oStdOut.getPWrite()));
                reporter.log("pread Reader:   " + str(oStdOut.getPRead()));
            reporter.testDone();
        else:
            reporter.testDone(fSkipped = True);

        reporter.testStart('fio');
        if fRc and 'fio' in self.asTests:
            oFioWrapper = FioWrapper('64k', '2g', '32', '/mnt');
            fRc = self.txsUploadString(oSession, oTxsSession, oFioWrapper.getConfig(), '${SCRATCH}/aio-test');
            fRc = fRc and self.txsRunTestRedirectStd(oTxsSession, '2G', 3600000, \
                '/usr/bin/fio', ('fio', '${SCRATCH}/aio-test'), \
                (), '', '/dev/null', oFioWrapper, '/dev/null', '/dev/null');
        else:
            reporter.testDone(fSkipped = True);

        reporter.testDone(not fRc);
        return fRc;

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

            if sDiskFormat == "iSCSI":
                listNames = [];
                listValues = [];
                listValues = sDiskPath.split('|');
                listNames.append('TargetAddress');
                listNames.append('TargetName');
                listNames.append('LUN');

                oHd = oSession.oVBox.createHardDisk(sDiskFormat, sDiskPath);
                oHd.type = vboxcon.MediumType_Normal;
                oHd.setProperties(listNames, listValues);

                # Attach it.
                if fRc is True:
                    try:
                        if oSession.fpApiVer >= 4.0:
                            oSession.o.machine.attachDevice(_ControllerTypeToName(eStorageController), \
                                                            1, 0, vboxcon.DeviceType_HardDisk, oHd);
                        else:
                            oSession.o.machine.attachDevice(_ControllerTypeToName(eStorageController), \
                                                            1, 0, vboxcon.DeviceType_HardDisk, oHd.id);
                    except:
                        reporter.errorXcpt('attachDevice("%s",%s,%s,HardDisk,"%s") failed on "%s"' \
                                           % (_ControllerTypeToName(eStorageController), 1, 0, oHd.id, oSession.sName) );
                        fRc = False;
                    else:
                        reporter.log('attached "%s" to %s' % (sDiskPath, oSession.sName));
            else:
                fRc = fRc and oSession.createAndAttachHd(sDiskPath, sDiskFormat, _ControllerTypeToName(eStorageController), \
                                                         cb = 10*1024*1024*1024, iPort = 1, fImmutable = False);
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

                self.test1RunTestProgs(oSession, oTxsSession, fRc, 'Disk benchmark');

                # cleanup.
                self.removeTask(oTxsSession);
                self.terminateVmBySession(oSession)

                # Remove disk
                oSession = self.openSession(oVM);
                if oSession is not None:
                    try:
                        oSession.o.machine.detachDevice(_ControllerTypeToName(eStorageController), 1, 0);

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

    def test1OneVM(self, sVmName):
        """
        Runs one VM thru the various configurations.
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
            else:
                eStorageCtrl = None;

            for sDiskFormat in self.asDiskFormats:
                reporter.testStart('%s' % (sDiskFormat));

                if sDiskFormat == "iSCSI":
                    asPaths = self.asIscsiTargets;
                else:
                    asPaths = self.asDirs;

                for sDir in asPaths:
                    reporter.testStart('%s' % (sDir));

                    if sDiskFormat == "iSCSI":
                        sPath = sDir;
                    else:
                        sPath = sDir + "/test.disk";

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
                reporter.testDone();
            reporter.testDone();
        reporter.testDone();
        return fRc;

    def test1(self):
        """
        Executes test #1.
        """

        # Loop thru the test VMs.
        for sVM in self.asTestVMs:
            # run test on the VM.
            if not self.test1OneVM(sVM):
                fRc = False;
            else:
                fRc = True;

        return fRc;



if __name__ == '__main__':
    sys.exit(tdStorageBenchmark().main(sys.argv));

