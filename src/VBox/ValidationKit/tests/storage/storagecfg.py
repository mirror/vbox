# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Storage test configuration API.
"""

__copyright__ = \
"""
Copyright (C) 2016 Oracle Corporation

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
import re;

class StorageDisk(object):
    """
    Class representing a disk for testing.
    """

    def __init__(self, sPath):
        self.sPath   = sPath;
        self.fUsed   = False;

    def getPath(self):
        """
        Return the disk path.
        """
        return self.sPath;

    def isUsed(self):
        """
        Returns whether the disk is currently in use.
        """
        return self.fUsed;

    def setUsed(self, fUsed):
        """
        Sets the used flag for the disk.
        """
        if fUsed:
            if self.fUsed:
                return False;

            self.fUsed = True;
        else:
            self.fUsed = fUsed;

        return True;

class StorageConfigOs(object):
    """
    Base class for a single hosts OS storage configuration.
    """

    def _getDisksMatchingRegExpWithPath(self, sPath, sRegExp):
        """
        Adds new disks to the config matching the given regular expression.
        """

        lstDisks = [];
        oRegExp = re.compile(sRegExp);
        asFiles = os.listdir(sPath);
        for sFile in asFiles:
            if oRegExp.match(os.path.basename(sFile)) and os.path.exists(sPath + '/' + sFile):
                lstDisks.append(StorageDisk(sPath + '/' + sFile));

        return lstDisks;

class StorageConfigOsSolaris(StorageConfigOs):
    """
    Class implementing the Solaris specifics for a storage configuration.
    """

    def __init__(self):
        StorageConfigOs.__init__(self);

    def getDisksMatchingRegExp(self, sRegExp):
        """
        Returns a list of disks matching the regular expression.
        """
        return self._getDisksMatchingRegExpWithPath('/dev/dsk', sRegExp);

    def getMntBase(self):
        """
        Returns the mountpoint base for the host.
        """
        return '/pools';

    def createStoragePool(self, oExec, sPool, asDisks, sRaidLvl):
        """
        Creates a new storage pool with the given disks and the given RAID level.
        """
        sZPoolRaid = None
        if sRaidLvl == 'raid5' or sRaidLvl is None:
            sZPoolRaid = 'raidz';

        fRc = True;
        if sZPoolRaid is not None:
            fRc = oExec.execBinary('zpool', ('create', '-f', sPool, sZPoolRaid,) + tuple(asDisks));
        else:
            fRc = False;

        return fRc;

    def createVolume(self, oExec, sPool, sVol, sMountPoint, cbVol = None):
        """
        Creates and mounts a filesystem at the given mountpoint using the
        given pool and volume IDs.
        """
        fRc = True;
        if cbVol is not None:
            fRc, _ = oExec.execBinary('zfs', ('create', '-o', 'mountpoint='+sMountPoint, '-V', cbVol, sPool + '/' + sVol));
        else:
            fRc, _ = oExec.execBinary('zfs', ('create', '-o', 'mountpoint='+sMountPoint, sPool + '/' + sVol));

        return fRc;

    def destroyVolume(self, oExec, sPool, sVol):
        """
        Destroys the given volume.
        """
        fRc, _ = oExec.execBinary('zfs', ('destroy', sPool + '/' + sVol));
        return fRc;

    def destroyPool(self, oExec, sPool):
        """
        Destroys the given storage pool.
        """
        fRc, _ = oExec.execBinary('zpool', ('destroy', sPool));
        return fRc;

class StorageConfigOsLinux(StorageConfigOs):
    """
    Class implementing the Linux specifics for a storage configuration.
    """

    def __init__(self):
        StorageConfigOs.__init__(self);

    def getDisksMatchingRegExp(self, sRegExp):
        """
        Returns a list of disks matching the regular expression.
        """
        return self._getDisksMatchingRegExpWithPath('/dev/', sRegExp);

    def getMntBase(self):
        """
        Returns the mountpoint base for the host.
        """
        return '/media';

    def createStoragePool(self, oExec, sPool, asDisks, sRaidLvl):
        """
        Creates a new storage pool with the given disks and the given RAID level.
        """
        fRc = True;
        _ = oExec;
        _ = sPool;
        _ = asDisks;
        _ = sRaidLvl;
        return fRc;

    def createVolume(self, oExec, sPool, sVol, sMountPoint, cbVol = None):
        """
        Creates and mounts a filesystem at the given mountpoint using the
        given pool and volume IDs.
        """
        fRc = True;
        _ = oExec;
        _ = sPool;
        _ = sVol;
        _ = sMountPoint;
        _ = cbVol;
        return fRc;

    def destroyVolume(self, oExec, sPool, sVol):
        """
        Destroys the given volume.
        """
        fRc, _ = oExec.execBinary('lvremove', (sPool + '/' + sVol,));
        return fRc;

    def destroyPool(self, oExec, sPool):
        """
        Destroys the given storage pool.
        """
        fRc, _ = oExec.execBinary('vgremove', (sPool,));
        return fRc;

class StorageCfg(object):
    """
    Storage configuration helper class taking care of the different host OS.
    """

    kdStorageCfgs = {
        'testboxstor1.de.oracle.com': ('solaris', 'c[3-9]t\dd0\Z')
    };

    def __init__(self, oExec, sHostname = None):
        self.oExec    = oExec;
        self.lstDisks = [ ]; # List of disks present in the system.
        self.dPools   = { }; # Dictionary of storage pools.
        self.dVols    = { }; # Dictionary of volumes.
        self.iPoolId  = 0;
        self.iVolId   = 0;

        sOs, oDiskCfg = self.kdStorageCfgs.get(sHostname);

        fRc = True;
        oStorOs = None;
        if sOs == 'solaris':
            oStorOs = StorageConfigOsSolaris();
        elif sOs == 'linux':
            oStorOs = StorageConfigOsLinux(); # pylint: disable=R0204
        else:
            fRc = False;

        if fRc:
            self.oStorOs = oStorOs;
            if isinstance(oDiskCfg, basestring):
                self.lstDisks = oStorOs.getDisksMatchingRegExp(oDiskCfg);
            else:
                # Assume a list of of disks and add.
                for sDisk in oDiskCfg:
                    self.lstDisks.append(StorageDisk(sDisk));

    def __del__(self):
        self.cleanup();

    def cleanup(self):
        """
        Cleans up any created storage configs.
        """

        # Destroy all volumes first.
        for sMountPoint in self.dVols.keys():
            self.destroyVolume(sMountPoint);

        # Destroy all pools.
        for sPool in self.dPools.keys():
            self.destroyStoragePool(sPool);

        self.dVols.clear();
        self.dPools.clear();
        self.iPoolId = 0;
        self.iVolId  = 0;

    def getRawDisk(self):
        """
        Returns a raw disk device from the list of free devices for use.
        """
        for oDisk in self.lstDisks:
            if oDisk.isUsed() is False:
                oDisk.setUsed(True);
                return oDisk.getPath();

        return None;

    def getUnusedDiskCount(self):
        """
        Returns the number of unused disks.
        """

        cDisksUnused = 0;
        for oDisk in self.lstDisks:
            if not oDisk.isUsed():
                cDisksUnused += 1;

        return cDisksUnused;

    def createStoragePool(self, cDisks = 0, sRaidLvl = None):
        """
        Create a new storage pool
        """
        lstDisks = [ ];
        fRc = True;
        sPool = None;

        if cDisks == 0:
            cDisks = self.getUnusedDiskCount();

        for oDisk in self.lstDisks:
            if not oDisk.isUsed():
                oDisk.setUsed(True);
                lstDisks.append(oDisk);
                if len(lstDisks) == cDisks:
                    break;

        # Enough drives to satisfy the request?
        if len(lstDisks) == cDisks:
            # Create a list of all device paths
            lstDiskPaths = [ ];
            for oDisk in lstDisks:
                lstDiskPaths.append(oDisk.getPath());

            # Find a name for the pool
            sPool = 'pool' + str(self.iPoolId);
            self.iPoolId += 1;

            fRc = self.oStorOs.createStoragePool(self.oExec, sPool, lstDiskPaths, sRaidLvl);
            if fRc:
                self.dPools[sPool] = lstDisks;
            else:
                self.iPoolId -= 1;
        else:
            fRc = False;

        # Cleanup in case of error.
        if not fRc:
            for oDisk in lstDisks:
                oDisk.setUsed(False);

        return fRc, sPool;

    def destroyStoragePool(self, sPool):
        """
        Destroys the storage pool with the given ID.
        """

        lstDisks = self.dPools.get(sPool);
        if lstDisks is not None:
            fRc = self.oStorOs.destroyPool(self.oExec, sPool);
            if fRc:
                # Mark disks as unused
                self.dPools.pop(sPool);
                for oDisk in lstDisks:
                    oDisk.setUsed(False);
        else:
            fRc = False;

        return fRc;

    def createVolume(self, sPool, cbVol = None):
        """
        Creates a new volume from the given pool returning the mountpoint.
        """

        fRc = True;
        sMountPoint = None;
        if self.dPools.has_key(sPool):
            sVol = 'vol' + str(self.iVolId);
            sMountPoint = self.oStorOs.getMntBase() + '/' + sVol;
            self.iVolId += 1;
            fRc = self.oStorOs.createVolume(self.oExec, sPool, sVol, sMountPoint, cbVol);
            if fRc:
                self.dVols[sMountPoint] = (sVol, sPool);
            else:
                self.iVolId -= 1;
        else:
            fRc = False;

        return fRc, sMountPoint;

    def destroyVolume(self, sMountPoint):
        """
        Destroy the volume at the given mount point.
        """

        sVol, sPool = self.dVols.get(sMountPoint);
        fRc = True;
        if sVol is not None:
            fRc = self.oStorOs.destroyVolume(self.oExec, sPool, sVol);
            if fRc:
                self.dVols.pop(sMountPoint);
        else:
            fRc = False;

        return fRc;

