# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=C0302

"""
VirtualBox USB gadget control class
"""

__copyright__ = \
"""
Copyright (C) 2014 Oracle Corporation

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


# Validation Kit imports.
import testdriver.txsclient as txsclient;
import testdriver.reporter as reporter;

class UsbGadget(object):
    """
    USB Gadget control class using the TesteXecService to talk to the external
    board behaving like a USB device.
    The board needs to run an embedded Linux system with the TXS service running.
    """

    def __init__(self):
        self.oTxsSession = None;
        self.sImpersonation = 'Invalid';
        self.sGadgetType    = 'Invalid';

    def _loadModule(self, sModule):
        """
        Loads the given module on the USB gadget.
        Returns True on success.
        Returns False otherwise.
        """
        fRc = False;
        if self.oTxsSession is not None:
            fRc = self.oTxsSession.syncExecEx('/usr/bin/modprobe', ('/usr/bin/modprobe', sModule));
            # For the ODroid-XU3 gadget we have to do a soft connect for the attached host to recognise the device.
            if self.sGadgetType == 'ODroid-XU3':
                fRc = self.oTxsSession.syncExecEx('/usr/bin/sh', \
                        ('/usr/bin/sh', '-c', 'echo connect > /sys/class/udc/12400000.dwc3/soft_connect'));

        return fRc;

    def _unloadModule(self, sModule):
        """
        Unloads the given module on the USB gadget.
        Returns True on success.
        Returns False otherwise.
        """
        fRc = False;
        if self.oTxsSession is not None:
            # For the ODroid-XU3 gadget we do a soft disconnect before unloading the gadget driver.
            if self.sGadgetType == 'ODroid-XU3':
                fRc = self.oTxsSession.syncExecEx('/usr/bin/sh', \
                        ('/usr/bin/sh', '-c', 'echo disconnect > /sys/class/udc/12400000.dwc3/soft_connect'));
            fRc = self.oTxsSession.syncExecEx('/usr/bin/rmmod', ('/usr/bin/rmmod', sModule));

        return fRc;

    def _clearImpersonation(self):
        """
        Removes the current impersonation of the gadget.
        """
        if self.sImpersonation == 'Invalid':
            self._unloadModule('g_zero');
            self._unloadModule('g_mass_storage');
            self._unloadModule('g_webcam');
            self._unloadModule('g_ether');
            return True;
        elif self.sImpersonation == 'Test':
            return self._unloadModule('g_zero');
        elif self.sImpersonation == 'Msd':
            return self._unloadModule('g_mass_storage');
        elif self.sImpersonation == 'Webcam':
            return self._unloadModule('g_webcam');
        elif self.sImpersonation == 'Network':
            return self._unloadModule('g_ether');
        else:
            reporter.log('Invalid impersonation');

        return False;

    def impersonate(self, sImpersonation):
        """
        Impersonate a given device.
        """

        # Clear any previous impersonation
        self._clearImpersonation();
        self.sImpersonation = sImpersonation;

        if sImpersonation == 'Invalid':
            return False;
        elif sImpersonation == 'Test':
            return self._loadModule('g_zero');
        elif sImpersonation == 'Msd':
            # @todo: Not complete
            return self._loadModule('g_mass_storage');
        elif sImpersonation == 'Webcam':
            # @todo: Not complete
            return self._loadModule('g_webcam');
        elif sImpersonation == 'Network':
            return self._loadModule('g_ether');
        else:
            reporter.log('Invalid impersonation');

        return False;

    def connectTo(self, cMsTimeout, sGadgetType, sHostname, uPort = None):
        """
        Connects to the specified target device.
        Returns True on Success.
        Returns False otherwise.
        """
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

        return fRc;

    def disconnectFrom(self):
        """
        Disconnects from the target device.
        """
        fRc = True;

        if self.oTxsSession is not None:
            self._clearImpersonation();
            fRc = self.oTxsSession.syncDisconnect();

        return fRc;
