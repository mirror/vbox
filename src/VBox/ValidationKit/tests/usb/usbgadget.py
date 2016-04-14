# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=C0302

"""
VirtualBox USB gadget control class
"""

__copyright__ = \
"""
Copyright (C) 2014-2016 Oracle Corporation

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

import time;

# Validation Kit imports.
import testdriver.txsclient as txsclient;
import testdriver.reporter as reporter;
from common import utils;

## @name USB gadget type string constants.
## @{
g_ksGadgetTypeInvalid     = 'Invalid';
g_ksGadgetTypeBeaglebone  = 'BeagleBone';
g_ksGadgetTypeODroidXu3   = 'ODroidXu3';
g_ksGadgetTypeDummyHcd    = 'DummyHcd';
## @}

## @name USB gadget configurations.
## @{
g_kdGadgetCfgs = {
    g_ksGadgetTypeBeaglebone: ('musb-hdrc.0.auto'),
    g_ksGadgetTypeODroidXu3:  ('12400000.dwc3'),
    g_ksGadgetTypeDummyHcd:   ('dummy_udc.0')
};
## @}

## @name USB gadget imeprsonation string constants.
## @{
g_ksGadgetImpersonationInvalid = 'Invalid';
g_ksGadgetImpersonationTest    = 'Test';
g_ksGadgetImpersonationMsd     = 'Msd';
g_ksGadgetImpersonationWebcam  = 'Webcam';
g_ksGadgetImpersonationEther   = 'Ether';
## @}

class UsbGadget(object):
    """
    USB Gadget control class using the TesteXecService to talk to the external
    board behaving like a USB device.
    The board needs to run an embedded Linux system with the TXS service running.
    """

    def __init__(self):
        self.oTxsSession    = None;
        self.sImpersonation = g_ksGadgetImpersonationInvalid;
        self.sGadgetType    = g_ksGadgetTypeInvalid;

    def _sudoExecuteSync(self, asArgs):
        """
        Executes a sudo child process synchronously.
        Returns a tuple [True, 0] if the process executed successfully
        and returned 0, otherwise [False, rc] is returned.
        """
        reporter.log('Executing [sudo]: %s' % (asArgs, ));
        reporter.flushall();
        iRc = 0;
        try:
            iRc = utils.sudoProcessCall(asArgs, shell = False, close_fds = False);
        except:
            reporter.errorXcpt();
            return (False, 0);
        reporter.log('Exit code [sudo]: %s (%s)' % (iRc, asArgs));
        return (iRc is 0, iRc);

    def _execLocallyOrThroughTxs(self, sExec, asArgs):
        """
        Executes the given program locally or through TXS based on the
        current config.
        """
        fRc = False;

        if self.oTxsSession is not None:
            fRc = self.oTxsSession.syncExecEx(sExec, (sExec,) + asArgs);
        else:
            fRc, _ = self._sudoExecuteSync([sExec, ] + list(asArgs));
        return fRc;

    def _loadModule(self, sModule):
        """
        Loads the given module on the USB gadget.
        Returns True on success.
        Returns False otherwise.
        """
        return self._execLocallyOrThroughTxs('/usr/bin/modprobe', (sModule, ));

    def _unloadModule(self, sModule):
        """
        Unloads the given module on the USB gadget.
        Returns True on success.
        Returns False otherwise.
        """
        return self._execLocallyOrThroughTxs('/usr/bin/rmmod', (sModule, ));

    def _clearImpersonation(self):
        """
        Removes the current impersonation of the gadget.
        """
        if self.sImpersonation == g_ksGadgetImpersonationInvalid:
            self._unloadModule('g_zero');
            self._unloadModule('g_mass_storage');
            self._unloadModule('g_webcam');
            self._unloadModule('g_ether');
            return True;
        elif self.sImpersonation == g_ksGadgetImpersonationTest:
            return self._unloadModule('g_zero');
        elif self.sImpersonation == g_ksGadgetImpersonationMsd:
            return self._unloadModule('g_mass_storage');
        elif self.sImpersonation == g_ksGadgetImpersonationWebcam:
            return self._unloadModule('g_webcam');
        elif self.sImpersonation == g_ksGadgetImpersonationEther:
            return self._unloadModule('g_ether');
        else:
            reporter.log('Invalid impersonation');

        return False;

    def _doSoftConnect(self, sAction):
        """
        Does a softconnect/-disconnect based on the given action.
        """
        sUdcFile = g_kdGadgetCfgs.get(self.sGadgetType);
        sPath = '/sys/class/udc/' + sUdcFile + '/soft_connect';

        return self._execLocallyOrThroughTxs('/usr/bin/sh', ('-c', 'echo ' + sAction + ' > ' + sPath));

    def _prepareGadget(self):
        """
        Prepares the gadget for use in testing.
        """
        fRc = True;
        if self.sGadgetType is g_ksGadgetTypeDummyHcd:
            fRc = self._loadModule('dummy_hcd');
        return fRc;

    def _cleanupGadget(self):
        """
        Cleans up the gadget to the default state.
        """
        fRc = True;
        if self.sGadgetType is g_ksGadgetTypeDummyHcd:
            fRc = self._unloadModule('dummy_hcd');
        return fRc;

    def disconnectUsb(self):
        """
        Disconnects the USB gadget from the host. (USB connection not network
        connection used for control)
        """
        return self._doSoftConnect('disconnect');

    def connectUsb(self):
        """
        Connect the USB gadget to the host.
        """
        return self._doSoftConnect('connect');

    def impersonate(self, sImpersonation):
        """
        Impersonate a given device.
        """

        # Clear any previous impersonation
        self._clearImpersonation();
        self.sImpersonation = sImpersonation;

        fRc = False;
        if sImpersonation == g_ksGadgetImpersonationTest:
            fRc = self._loadModule('g_zero');
        elif sImpersonation == g_ksGadgetImpersonationMsd:
            # @todo: Not complete
            fRc = self._loadModule('g_mass_storage');
        elif sImpersonation == g_ksGadgetImpersonationWebcam:
            # @todo: Not complete
            fRc = self._loadModule('g_webcam');
        elif sImpersonation == g_ksGadgetImpersonationEther:
            fRc = self._loadModule('g_ether');
        else:
            reporter.log('Invalid impersonation');

        if fRc and self.sGadgetType is g_ksGadgetTypeDummyHcd and self.oTxsSession is not None:
            time.sleep(2); # Fudge
            # For the dummy HCD over USB/IP case we have to bind the device to the USB/IP server
            self._execLocallyOrThroughTxs('/usr/bin/sh', ('-c', '/usr/bin/usbip bind -b 3-1'));

        return fRc;

    def connectTo(self, cMsTimeout, sGadgetType, fUseTxs, sHostname, uPort = None):
        """
        Connects to the specified target device.
        Returns True on Success.
        Returns False otherwise.
        """
        fRc = True;

        if fUseTxs:
            if uPort is None:
                self.oTxsSession = txsclient.openTcpSession(cMsTimeout, sHostname);
            else:
                self.oTxsSession = txsclient.openTcpSession(cMsTimeout, sHostname, uPort = uPort);
            if self.oTxsSession is None:
                return False;

            fDone = self.oTxsSession.waitForTask(30*1000);
            print 'connect: waitForTask -> %s, result %s' % (fDone, self.oTxsSession.getResult());
            if fDone is True and self.oTxsSession.isSuccess():
                fRc = True;
            else:
                fRc = False;

            if fRc is True:
                self.sGadgetType = sGadgetType;
            else:
                self.sGadgetType = g_ksGadgetTypeInvalid;

        if fRc:
            fRc = self._prepareGadget();

        return fRc;

    def disconnectFrom(self):
        """
        Disconnects from the target device.
        """
        fRc = True;

        if self.sGadgetType is not g_ksGadgetTypeInvalid:
            self._clearImpersonation();
            self._cleanupGadget();
            if self.oTxsSession is not None:
                fRc = self.oTxsSession.syncDisconnect();

        return fRc;
