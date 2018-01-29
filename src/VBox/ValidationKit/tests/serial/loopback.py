# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Serial loopback module.
"""

__copyright__ = \
"""
Copyright (C) 2018 Oracle Corporation

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
#import os;
import socket;
import threading;


g_ksLoopbackTcpServ         = 'TcpServ';
g_ksLoopbackTcpClient       = 'TcpClient';
g_ksLoopbackNamedPipeServ   = 'NamedPipeServ';
g_ksLoopbackNamedPipeClient = 'NamedPipeClient';

class SerialLoopbackTcpServ(object):
    """
    Handler for a server TCP style connection.
    """
    def __init__(self, sLocation, iTimeout):
        sHost, sPort = sLocation.split(':');
        self.oConn = None;
        self.oSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM);
        self.oSock.settimeout(iTimeout);
        self.oSock.bind((sHost, int(sPort)));
        self.oSock.listen(1);
        self.iTimeout = iTimeout;

    def __del__(self):
        if self.oConn is not None:
            self.oConn.close();
        self.oSock.close();

    def pumpIo(self):
        """
        Main I/O pumping routine.
        """
        try:
            if self.oConn is None:
                oConn, _ = self.oSock.accept();
                self.oConn = oConn;
            else:
                abData = self.oConn.recv(1024);
                if abData is not None:
                    self.oConn.send(abData);
        except:
            pass;

class SerialLoopbackTcpClient(object):
    """
    Handler for a client TCP style connection.
    """
    def __init__(self, sLocation, iTimeout):
        sHost, sPort = sLocation.split(':');
        self.oConn = socket.socket(socket.AF_INET, socket.SOCK_STREAM);
        self.oConn.connect((sHost, int(sPort)));
        self.oConn.settimeout(iTimeout);
        self.iTimeout = iTimeout;

    def __del__(self):
        self.oConn.close();

    def pumpIo(self):
        """
        Main I/O pumping routine.
        """
        try:
            abData = self.oConn.recv(1024);
            if abData is not None:
                self.oConn.send(abData);
        except:
            pass;

class SerialLoopbackNamedPipeServ(object):
    """
    Handler for a named pipe server style connection.
    """
    def __init__(self, sLocation, iTimeout):
        self.oConn = None;
        self.oSock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM);
        self.oSock.settimeout(iTimeout);
        self.oSock.bind(sLocation);
        self.oSock.listen(1);
        self.iTimeout = iTimeout;

    def __del__(self):
        if self.oConn is not None:
            self.oConn.close();
        self.oSock.close();

    def pumpIo(self):
        """
        Main I/O pumping routine.
        """
        try:
            if self.oConn is None:
                oConn, _ = self.oSock.accept();
                self.oConn = oConn;
            else:
                abData = self.oConn.recv(1024);
                if abData is not None:
                    self.oConn.send(abData);
        except:
            pass;

class SerialLoopbackNamedPipeClient(object):
    """
    Handler for a named pipe client style connection.
    """
    def __init__(self, sLocation, iTimeout):
        self.oConn = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM);
        self.oConn.connect(sLocation);
        self.oConn.settimeout(iTimeout);
        self.iTimeout = iTimeout;

    def __del__(self):
        self.oConn.close();

    def pumpIo(self):
        """
        Main I/O pumping routine.
        """
        try:
            abData = self.oConn.recv(1024);
            if abData is not None:
                self.oConn.send(abData);
        except:
            pass;

class SerialLoopback(object):
    """
    Serial port loopback module working with TCP and named pipes.
    """

    def __init__(self, sType, sLocation):
        self.fShutdown = False;
        self.sType     = sType;
        self.sLocation = sLocation;
        self.oLock     = threading.Lock();
        self.oThread   = threading.Thread(target=self.threadWorker, args=(), name=('SerLoopback'));

        if sType == g_ksLoopbackTcpServ:
            self.oIoPumper = SerialLoopbackTcpServ(sLocation, 0.5);
            self.oThread.start();
        elif sType == g_ksLoopbackNamedPipeServ:
            self.oIoPumper = SerialLoopbackNamedPipeServ(sLocation, 0.5);
            self.oThread.start();

    def connect(self):
        """
        Connects to the server for a client type version.
        """
        fRc = True;
        try:
            if self.sType == g_ksLoopbackTcpClient:
                self.oIoPumper = SerialLoopbackTcpClient(self.sLocation, 0.5);
            elif self.sType == g_ksLoopbackNamedPipeClient:
                self.oIoPumper = SerialLoopbackNamedPipeClient(self.sLocation, 0.5);
        except:
            fRc = False;
        else:
            self.oThread.start();
        return fRc;

    def shutdown(self):
        """
        Shutdown any connection and wait for it to become idle.
        """
        self.oLock.acquire();
        self.fShutdown = True;
        self.oLock.release();

    def isShutdown(self):
        """
        Returns whether the I/O pumping thread should shut down.
        """
        self.oLock.acquire();
        fShutdown = self.fShutdown;
        self.oLock.release();

        return fShutdown;

    def threadWorker(self):
        """
        The threaded worker.
        """
        while not self.isShutdown():
            self.oIoPumper.pumpIo();

