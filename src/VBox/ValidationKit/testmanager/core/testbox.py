# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager - TestBox.
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


# Standard python imports.
import unittest;

# Validation Kit imports.
from testmanager.core.base  import ModelDataBase, ModelDataBaseTestCase, ModelLogicBase, TMExceptionBase, TMInFligthCollision, \
                                   TMInvalidData, TMTooManyRows, TMRowNotFound, ChangeLogEntry, AttributeChangeEntry;


# pylint: disable=C0103
class TestBoxData(ModelDataBase):  # pylint: disable=R0902
    """
    TestBox Data.
    """

    ## LomKind_T
    ksLomKind_None                  = 'none';
    ksLomKind_ILOM                  = 'ilom';
    ksLomKind_ELOM                  = 'elom';
    ksLomKind_AppleXserveLom        = 'apple-xserver-lom';
    kasLomKindValues                = [ ksLomKind_None, ksLomKind_ILOM, ksLomKind_ELOM, ksLomKind_AppleXserveLom];
    kaoLomKindDescs                 = \
    [
        ( ksLomKind_None,           'None',             ''),
        ( ksLomKind_ILOM,           'ILOM',             ''),
        ( ksLomKind_ELOM,           'ELOM',             ''),
        ( ksLomKind_AppleXserveLom, 'Apple Xserve LOM', ''),
    ];


    ## TestBoxCmd_T
    ksTestBoxCmd_None               = 'none';
    ksTestBoxCmd_Abort              = 'abort';
    ksTestBoxCmd_Reboot             = 'reboot';
    ksTestBoxCmd_Upgrade            = 'upgrade';
    ksTestBoxCmd_UpgradeAndReboot   = 'upgrade-and-reboot';
    ksTestBoxCmd_Special            = 'special';
    kasTestBoxCmdValues             =  [ ksTestBoxCmd_None, ksTestBoxCmd_Abort, ksTestBoxCmd_Reboot, ksTestBoxCmd_Upgrade,
                                         ksTestBoxCmd_UpgradeAndReboot, ksTestBoxCmd_Special];
    kaoTestBoxCmdDescs              = \
    [
        ( ksTestBoxCmd_None,              'None',                               ''),
        ( ksTestBoxCmd_Abort,             'Abort current test',                 ''),
        ( ksTestBoxCmd_Reboot,            'Reboot TestBox',                     ''),
        ( ksTestBoxCmd_Upgrade,           'Upgrade TestBox Script',             ''),
        ( ksTestBoxCmd_UpgradeAndReboot,  'Upgrade TestBox Script and reboot',  ''),
        ( ksTestBoxCmd_Special,           'Special (reserved)',                 ''),
    ];


    ksIdAttr    = 'idTestBox';
    ksIdGenAttr = 'idGenTestBox';

    ksParam_idTestBox           = 'TestBox_idTestBox';
    ksParam_tsEffective         = 'TestBox_tsEffective';
    ksParam_tsExpire            = 'TestBox_tsExpire';
    ksParam_uidAuthor           = 'TestBox_uidAuthor';
    ksParam_idGenTestBox        = 'TestBox_idGenTestBox';
    ksParam_ip                  = 'TestBox_ip';
    ksParam_uuidSystem          = 'TestBox_uuidSystem';
    ksParam_sName               = 'TestBox_sName';
    ksParam_sDescription        = 'TestBox_sDescription';
    ksParam_idSchedGroup        = 'TestBox_idSchedGroup';
    ksParam_fEnabled            = 'TestBox_fEnabled';
    ksParam_enmLomKind          = 'TestBox_enmLomKind';
    ksParam_ipLom               = 'TestBox_ipLom';
    ksParam_pctScaleTimeout     = 'TestBox_pctScaleTimeout';
    ksParam_sOs                 = 'TestBox_sOs';
    ksParam_sOsVersion          = 'TestBox_sOsVersion';
    ksParam_sCpuVendor          = 'TestBox_sCpuVendor';
    ksParam_sCpuArch            = 'TestBox_sCpuArch';
    ksParam_sCpuName            = 'TestBox_sCpuName';
    ksParam_lCpuRevision        = 'TestBox_lCpuRevision';
    ksParam_cCpus               = 'TestBox_cCpus';
    ksParam_fCpuHwVirt          = 'TestBox_fCpuHwVirt';
    ksParam_fCpuNestedPaging    = 'TestBox_fCpuNestedPaging';
    ksParam_fCpu64BitGuest      = 'TestBox_fCpu64BitGuest';
    ksParam_fChipsetIoMmu       = 'TestBox_fChipsetIoMmu';
    ksParam_cMbMemory           = 'TestBox_cMbMemory';
    ksParam_cMbScratch          = 'TestBox_cMbScratch';
    ksParam_sReport             = 'TestBox_sReport';
    ksParam_iTestBoxScriptRev   = 'TestBox_iTestBoxScriptRev';
    ksParam_iPythonHexVersion   = 'TestBox_iPythonHexVersion';
    ksParam_enmPendingCmd       = 'TestBox_enmPendingCmd';

    kasAllowNullAttributes      = ['idTestBox', 'tsEffective', 'tsExpire', 'uidAuthor', 'idGenTestBox', 'sDescription',
                                   'ipLom', 'sOs', 'sOsVersion', 'sCpuVendor', 'sCpuArch', 'sCpuName', 'lCpuRevision', 'cCpus',
                                   'fCpuHwVirt', 'fCpuNestedPaging', 'fCpu64BitGuest', 'fChipsetIoMmu', 'cMbMemory',
                                   'cMbScratch', 'sReport', 'iTestBoxScriptRev', 'iPythonHexVersion' ];
    kasValidValues_enmLomKind   = kasLomKindValues;
    kasValidValues_enmPendingCmd = kasTestBoxCmdValues;
    kiMin_pctScaleTimeout       =    11;
    kiMax_pctScaleTimeout       = 19999;
    kcchMax_sReport             = 65535;


    def __init__(self):
        ModelDataBase.__init__(self);

        #
        # Initialize with defaults.
        # See the database for explanations of each of these fields.
        #
        self.idTestBox           = None;
        self.tsEffective         = None;
        self.tsExpire            = None;
        self.uidAuthor           = None;
        self.idGenTestBox        = None;
        self.ip                  = None;
        self.uuidSystem          = None;
        self.sName               = None;
        self.sDescription        = None;
        self.idSchedGroup        = 1;
        self.fEnabled            = False;
        self.enmLomKind          = self.ksLomKind_None;
        self.ipLom               = None;
        self.pctScaleTimeout     = 100;
        self.sOs                 = None;
        self.sOsVersion          = None;
        self.sCpuVendor          = None;
        self.sCpuArch            = None;
        self.sCpuName            = None;
        self.lCpuRevision        = None;
        self.cCpus               = 1;
        self.fCpuHwVirt          = False;
        self.fCpuNestedPaging    = False;
        self.fCpu64BitGuest      = False;
        self.fChipsetIoMmu       = False;
        self.cMbMemory           = 1;
        self.cMbScratch          = 0;
        self.sReport             = None;
        self.iTestBoxScriptRev   = 0;
        self.iPythonHexVersion   = 0;
        self.enmPendingCmd       = self.ksTestBoxCmd_None;

    def initFromDbRow(self, aoRow):
        """
        Internal worker for initFromDbWithId and initFromDbWithGenId as well as
        from TestBoxLogic.  Expecting a SELECT * FROM TestBoxes result.
        """

        if aoRow is None:
            raise TMRowNotFound('TestBox not found.');

        self.idTestBox           = aoRow[0];
        self.tsEffective         = aoRow[1];
        self.tsExpire            = aoRow[2];
        self.uidAuthor           = aoRow[3];
        self.idGenTestBox        = aoRow[4];
        self.ip                  = aoRow[5];
        self.uuidSystem          = aoRow[6];
        self.sName               = aoRow[7];
        self.sDescription        = aoRow[8];
        self.idSchedGroup        = aoRow[9];
        self.fEnabled            = aoRow[10];
        self.enmLomKind          = aoRow[11];
        self.ipLom               = aoRow[12];
        self.pctScaleTimeout     = aoRow[13];
        self.sOs                 = aoRow[14];
        self.sOsVersion          = aoRow[15];
        self.sCpuVendor          = aoRow[16];
        self.sCpuArch            = aoRow[17];
        self.sCpuName            = aoRow[18];
        self.lCpuRevision        = aoRow[19];
        self.cCpus               = aoRow[20];
        self.fCpuHwVirt          = aoRow[21];
        self.fCpuNestedPaging    = aoRow[22];
        self.fCpu64BitGuest      = aoRow[23];
        self.fChipsetIoMmu       = aoRow[24];
        self.cMbMemory           = aoRow[25];
        self.cMbScratch          = aoRow[26];
        self.sReport             = aoRow[27];
        self.iTestBoxScriptRev   = aoRow[28];
        self.iPythonHexVersion   = aoRow[29];
        self.enmPendingCmd       = aoRow[30];
        return self;

    def initFromDbWithId(self, oDb, idTestBox, tsNow = None, sPeriodBack = None):
        """
        Initialize the object from the database.
        """
        oDb.execute(self.formatSimpleNowAndPeriodQuery(oDb,
                                                       'SELECT *\n'
                                                       'FROM   TestBoxes\n'
                                                       'WHERE  idTestBox    = %s\n'
                                                       , ( idTestBox, ), tsNow, sPeriodBack));
        aoRow = oDb.fetchOne()
        if aoRow is None:
            raise TMRowNotFound('idTestBox=%s not found (tsNow=%s sPeriodBack=%s)' % (idTestBox, tsNow, sPeriodBack,));
        return self.initFromDbRow(aoRow);

    def initFromDbWithGenId(self, oDb, idGenTestBox):
        """
        Initialize the object from the database.
        """
        oDb.execute('SELECT *\n'
                    'FROM   TestBoxes\n'
                    'WHERE  idGenTestBox = %s\n'
                    , (idGenTestBox, ) );
        return self.initFromDbRow(oDb.fetchOne());

    def _validateAndConvertWorker(self, asAllowNullAttributes, oDb, enmValidateFor = ModelDataBase.ksValidateFor_Other):
        # Override to do extra ipLom checks.
        dErrors = ModelDataBase._validateAndConvertWorker(self, asAllowNullAttributes, oDb, enmValidateFor);
        if    self.ksParam_ipLom      not in dErrors \
          and self.ksParam_enmLomKind not in dErrors \
          and self.enmLomKind != self.ksLomKind_None \
          and self.ipLom is None:
            dErrors[self.ksParam_ipLom] = 'Light-out-management IP is mandatory and a LOM is selected.'
        return dErrors;

    def formatPythonVersion(self):
        """
        Unbuttons the version number and formats it as a version string.
        """
        if self.iPythonHexVersion is None:
            return 'N/A';
        return 'v%d.%d.%d.%d' \
            % (  self.iPythonHexVersion >> 24,
                (self.iPythonHexVersion >> 16) & 0xff,
                (self.iPythonHexVersion >>  8) & 0xff,
                 self.iPythonHexVersion        & 0xff);

    def getCpuFamily(self):
        """ Returns the CPU family for a x86 or amd64 testboxes."""
        if self.lCpuRevision is None:
            return 0;
        return (self.lCpuRevision >> 24 & 0xff);

    def getCpuModel(self):
        """ Returns the CPU model for a x86 or amd64 testboxes."""
        if self.lCpuRevision is None:
            return 0;
        return (self.lCpuRevision >> 8 & 0xffff);

    def getCpuStepping(self):
        """ Returns the CPU stepping for a x86 or amd64 testboxes."""
        if self.lCpuRevision is None:
            return 0;
        return (self.lCpuRevision & 0xff);

    # The following is a translation of the g_aenmIntelFamily06 array in CPUMR3CpuId.cpp:
    kdIntelFamily06 = {
        0x00: 'P6',
        0x01: 'P6',
        0x03: 'P6_II',
        0x05: 'P6_II',
        0x06: 'P6_II',
        0x07: 'P6_III',
        0x08: 'P6_III',
        0x09: 'P6_M_Banias',
        0x0a: 'P6_III',
        0x0b: 'P6_III',
        0x0d: 'P6_M_Dothan',
        0x0e: 'Core_Yonah',
        0x0f: 'Core2_Merom',
        0x15: 'P6_M_Dothan',
        0x16: 'Core2_Merom',
        0x17: 'Core2_Penryn',
        0x1a: 'Core7_Nehalem',
        0x1c: 'Atom_Bonnell',
        0x1d: 'Core2_Penryn',
        0x1e: 'Core7_Nehalem',
        0x1f: 'Core7_Nehalem',
        0x25: 'Core7_Westmere',
        0x26: 'Atom_Lincroft',
        0x27: 'Atom_Saltwell',
        0x2a: 'Core7_SandyBridge',
        0x2c: 'Core7_Westmere',
        0x2d: 'Core7_SandyBridge',
        0x2e: 'Core7_Nehalem',
        0x2f: 'Core7_Westmere',
        0x35: 'Atom_Saltwell',
        0x36: 'Atom_Saltwell',
        0x37: 'Atom_Silvermont',
        0x3a: 'Core7_IvyBridge',
        0x3c: 'Core7_Haswell',
        0x3d: 'Core7_Broadwell',
        0x3e: 'Core7_IvyBridge',
        0x3f: 'Core7_Haswell',
        0x45: 'Core7_Haswell',
        0x46: 'Core7_Haswell',
        0x47: 'Core7_Broadwell',
        0x4a: 'Atom_Silvermont',
        0x4c: 'Atom_Airmount',
        0x4d: 'Atom_Silvermont',
        0x4e: 'Core7_Skylake',
        0x4f: 'Core7_Broadwell',
        0x55: 'Core7_Skylake',
        0x56: 'Core7_Broadwell',
        0x5a: 'Atom_Silvermont',
        0x5c: 'Atom_Goldmont',
        0x5d: 'Atom_Silvermont',
        0x5e: 'Core7_Skylake',
        0x66: 'Core7_Cannonlake',
    };
    # Also from CPUMR3CpuId.cpp, but the switch.
    kdIntelFamily15 = {
        0x00: 'NB_Willamette',
        0x01: 'NB_Willamette',
        0x02: 'NB_Northwood',
        0x03: 'NB_Prescott',
        0x04: 'NB_Prescott2M',
        0x05: 'NB_Unknown',
        0x06: 'NB_CedarMill',
        0x07: 'NB_Gallatin',
    };

    def queryCpuMicroarch(self):
        """ Try guess the microarch name for the cpu.  Returns None if we cannot. """
        if self.lCpuRevision is None or self.sCpuVendor is None:
            return None;
        uFam = self.getCpuFamily();
        uMod = self.getCpuModel();
        if self.sCpuVendor == 'GenuineIntel':
            if uFam == 6:
                return self.kdIntelFamily06.get(uMod, None);
            if uFam == 15:
                return self.kdIntelFamily15.get(uMod, None);
        elif self.sCpuVendor == 'AuthenticAMD':
            if uFam == 0xf:
                if uMod < 0x10:                             return 'K8_130nm';
                if uMod >= 0x60 and uMod < 0x80:            return 'K8_65nm';
                if uMod >= 0x40:                            return 'K8_90nm_AMDV';
                if uMod in [0x21, 0x23, 0x2b, 0x37, 0x3f]:  return 'K8_90nm_DualCore';
                return 'AMD_K8_90nm';
            if uFam == 0x10:                                return 'K10';
            if uFam == 0x11:                                return 'K10_Lion';
            if uFam == 0x12:                                return 'K10_Llano';
            if uFam == 0x14:                                return 'Bobcat';
            if uFam == 0x15:
                if uMod <= 0x01:                            return 'Bulldozer';
                if uMod in [0x02, 0x10, 0x13]:              return 'Piledriver';
                return None;
            if uFam == 0x16:
                return 'Jaguar';
        elif self.sCpuVendor == 'CentaurHauls':
            if uFam == 0x05:
                if uMod == 0x01: return 'Centaur_C6';
                if uMod == 0x04: return 'Centaur_C6';
                if uMod == 0x08: return 'Centaur_C2';
                if uMod == 0x09: return 'Centaur_C3';
            if uFam == 0x06:
                if uMod == 0x05: return 'VIA_C3_M2';
                if uMod == 0x06: return 'VIA_C3_C5A';
                if uMod == 0x07: return 'VIA_C3_C5B' if self.getCpuStepping() < 8 else 'VIA_C3_C5C';
                if uMod == 0x08: return 'VIA_C3_C5N';
                if uMod == 0x09: return 'VIA_C3_C5XL' if self.getCpuStepping() < 8 else 'VIA_C3_C5P';
                if uMod == 0x0a: return 'VIA_C7_C5J';
                if uMod == 0x0f: return 'VIA_Isaiah';
        return None;

    def getPrettyCpuVersion(self):
        """ Pretty formatting of the family/model/stepping with microarch optimizations. """
        if self.lCpuRevision is None or self.sCpuVendor is None:
            return u'<none>';
        sMarch = self.queryCpuMicroarch();
        if sMarch is not None:
            return '%s m%02X s%02X' % (sMarch, self.getCpuModel(), self.getCpuStepping());
        return 'fam%02X m%02X s%02X' % (self.getCpuFamily(), self.getCpuModel(), self.getCpuStepping());

    def getArchBitString(self):
        """ Returns 32-bit, 64-bit, <none>, or sCpuArch. """
        if self.sCpuArch is None:
            return '<none>';
        if self.sCpuArch in [ 'x86',]:
            return '32-bit';
        if self.sCpuArch in [ 'amd64',]:
            return '64-bit';
        return self.sCpuArch;

    def getPrettyCpuVendor(self):
        """ Pretty vendor name."""
        if self.sCpuVendor is None:
            return '<none>';
        if self.sCpuVendor == 'GenuineIntel':     return 'Intel';
        if self.sCpuVendor == 'AuthenticAMD':     return 'AMD';
        if self.sCpuVendor == 'CentaurHauls':     return 'VIA';
        return self.sCpuVendor;



class TestBoxLogic(ModelLogicBase):
    """
    TestBox logic.
    """


    def __init__(self, oDb):
        ModelLogicBase.__init__(self, oDb);
        self.dCache = None;

    def tryFetchTestBoxByUuid(self, sTestBoxUuid):
        """
        Tries to fetch a testbox by its UUID alone.
        """
        self._oDb.execute('SELECT   *\n'
                          'FROM     TestBoxes\n'
                          'WHERE    uuidSystem = %s\n'
                          '     AND tsExpire = \'infinity\'::timestamp\n'
                          'ORDER BY tsEffective DESC\n',
                          (sTestBoxUuid,));
        if self._oDb.getRowCount() == 0:
            return None;
        if self._oDb.getRowCount() != 1:
            raise TMTooManyRows('Database integrity error: %u hits' % (self._oDb.getRowCount(),));
        oData = TestBoxData();
        oData.initFromDbRow(self._oDb.fetchOne());
        return oData;

    def fetchForListing(self, iStart, cMaxRows, tsNow):
        """
        Fetches testboxes for listing.

        Returns an array (list) of TestBoxData items, empty list if none. The
        TestBoxData instances have an extra oStatus member that is either None or
        a TestBoxStatusData instance, and a member tsCurrent holding
        CURRENT_TIMESTAMP.

        Raises exception on error.
        """
        from testmanager.core.testboxstatus import TestBoxStatusData;

        if tsNow is None:
            self._oDb.execute('SELECT   TestBoxes.*, TestBoxStatuses.*\n'
                              'FROM     TestBoxes\n'
                              'LEFT OUTER JOIN TestBoxStatuses ON (\n'
                              '         TestBoxStatuses.idTestBox = TestBoxes.idTestBox )\n'
                              'WHERE    tsExpire = \'infinity\'::TIMESTAMP\n'
                              'ORDER BY sName\n'
                              'LIMIT %s OFFSET %s\n'
                              , (cMaxRows, iStart,));
        else:
            self._oDb.execute('SELECT   TestBoxes.*, TestBoxStatuses.*\n'
                              'FROM     TestBoxes\n'
                              'LEFT OUTER JOIN TestBoxStatuses ON (\n'
                              '         TestBoxStatuses.idTestBox = TestBoxes.idTestBox )\n'
                              'WHERE    tsExpire     > %s\n'
                              '     AND tsEffective <= %s\n'
                              'ORDER BY sName\n'
                              'LIMIT %s OFFSET %s\n'
                              , (tsNow, tsNow, cMaxRows, iStart,));

        aoRows = [];
        for aoOne in self._oDb.fetchAll():
            oTestBox = TestBoxData().initFromDbRow(aoOne);
            oTestBox.tsCurrent = self._oDb.getCurrentTimestamp();                  # pylint: disable=W0201
            oTestBox.oStatus   = None;                                             # pylint: disable=W0201
            if aoOne[31] is not None:
                oTestBox.oStatus = TestBoxStatusData().initFromDbRow(aoOne[31:]);  # pylint: disable=W0201
            aoRows.append(oTestBox);
        return aoRows;

    def fetchForChangeLog(self, idTestBox, iStart, cMaxRows, tsNow): # pylint: disable=R0914
        """
        Fetches change log entries for a testbox.

        Returns an array of ChangeLogEntry instance and an indicator whether
        there are more entries.
        Raises exception on error.
        """

        if tsNow is None:
            tsNow = self._oDb.getCurrentTimestamp();

        self._oDb.execute('SELECT   TestBoxes.*, Users.sUsername\n'
                          'FROM     TestBoxes\n'
                          'LEFT OUTER JOIN Users \n'
                          '     ON (    TestBoxes.uidAuthor = Users.uid\n'
                          '         AND Users.tsEffective <= TestBoxes.tsEffective\n'
                          '         AND Users.tsExpire    >  TestBoxes.tsEffective)\n'
                          'WHERE    TestBoxes.tsEffective <= %s\n'
                          '     AND TestBoxes.idTestBox = %s\n'
                          'ORDER BY TestBoxes.tsExpire DESC\n'
                          'LIMIT %s OFFSET %s\n'
                          , (tsNow, idTestBox, cMaxRows + 1, iStart,));

        aoRows = [];
        for _ in range(self._oDb.getRowCount()):
            oRow = self._oDb.fetchOne();
            aoRows.append([TestBoxData().initFromDbRow(oRow), oRow[-1],]);

        # Calculate the changes.
        aoEntries = [];
        for i in range(0, len(aoRows) - 1):
            (oNew, sAuthor) = aoRows[i];
            (oOld, _      ) = aoRows[i + 1];
            aoChanges = [];
            for sAttr in oNew.getDataAttributes():
                if sAttr not in [ 'tsEffective', 'tsExpire', 'uidAuthor']:
                    oOldAttr = getattr(oOld, sAttr);
                    oNewAttr = getattr(oNew, sAttr);
                    if oOldAttr != oNewAttr:
                        aoChanges.append(AttributeChangeEntry(sAttr, oNewAttr, oOldAttr, str(oNewAttr), str(oOldAttr)));
            aoEntries.append(ChangeLogEntry(oNew.uidAuthor, sAuthor, oNew.tsEffective, oNew.tsExpire, oNew, oOld, aoChanges));

        # If we're at the end of the log, add the initial entry.
        if len(aoRows) <= cMaxRows and len(aoRows) > 0:
            (oNew, sAuthor) = aoRows[-1];
            aoEntries.append(ChangeLogEntry(oNew.uidAuthor, aoRows[-1][1], oNew.tsEffective, oNew.tsExpire, oNew, None, []));

        return (aoEntries, len(aoRows) > cMaxRows);

    def addEntry(self, oData, uidAuthor, fCommit = False):
        """
        Creates a testbox in the database.
        Returns the testbox ID, testbox generation ID and effective timestamp
        of the created testbox on success.  Throws error on failure.
        """
        dDataErrors = oData.validateAndConvert(self._oDb, oData.ksValidateFor_Add);
        if len(dDataErrors) > 0:
            raise TMInvalidData('Invalid data passed to create(): %s' % (dDataErrors,));

        self._oDb.execute('INSERT INTO TestBoxes (\n'
                          '         idTestBox,\n'
                          '         tsEffective,\n'
                          '         tsExpire,\n'
                          '         uidAuthor,\n'
                          '         idGenTestBox,\n'
                          '         ip,\n'
                          '         uuidSystem,\n'
                          '         sName,\n'
                          '         sDescription,\n'
                          '         idSchedGroup,\n'
                          '         fEnabled,\n'
                          '         enmLomKind,\n'
                          '         ipLom,\n'
                          '         pctScaleTimeout,\n'
                          '         sOs,\n'
                          '         sOsVersion,\n'
                          '         sCpuVendor,\n'
                          '         sCpuArch,\n'
                          '         sCpuName,\n'
                          '         lCpuRevision,\n'
                          '         cCpus,\n'
                          '         fCpuHwVirt,\n'
                          '         fCpuNestedPaging,\n'
                          '         fCpu64BitGuest,\n'
                          '         fChipsetIoMmu,\n'
                          '         cMbMemory,\n'
                          '         cMbScratch,\n'
                          '         sReport,\n'
                          '         iTestBoxScriptRev,\n'
                          '         iPythonHexVersion,\n'
                          '         enmPendingCmd\n'
                          '         )'
                          'VALUES (\n'
                          '         DEFAULT,\n'
                          '         CURRENT_TIMESTAMP,\n'
                          '         DEFAULT,\n'
                          '         %s,\n'          # uidAuthor
                          '         DEFAULT,\n'
                          '         %s,\n'          # ip
                          '         %s,\n'          # uuidSystem
                          '         %s,\n'          # sName
                          '         %s,\n'          # sDescription
                          '         %s,\n'          # idSchedGroup
                          '         %s,\n'          # fEnabled
                          '         %s,\n'          # enmLomKind
                          '         %s,\n'          # ipLom
                          '         %s,\n'          # pctScaleTimeout
                          '         %s,\n'          # sOs
                          '         %s,\n'          # sOsVersion
                          '         %s,\n'          # sCpuVendor
                          '         %s,\n'          # sCpuArch
                          '         %s,\n'          # sCpuName
                          '         %s,\n'          # lCpuRevision
                          '         %s,\n'          # cCpus
                          '         %s,\n'          # fCpuHwVirt
                          '         %s,\n'          # fCpuNestedPaging
                          '         %s,\n'          # fCpu64BitGuest
                          '         %s,\n'          # fChipsetIoMmu
                          '         %s,\n'          # cMbMemory
                          '         %s,\n'          # cMbScratch
                          '         %s,\n'          # sReport
                          '         %s,\n'          # iTestBoxScriptRev
                          '         %s,\n'          # iPythonHexVersion
                          '         %s\n'           # enmPendingCmd
                          '         )\n'
                          'RETURNING idTestBox, idGenTestBox, tsEffective'
                          , (uidAuthor,
                             oData.ip,
                             oData.uuidSystem,
                             oData.sName,
                             oData.sDescription,
                             oData.idSchedGroup,
                             oData.fEnabled,
                             oData.enmLomKind,
                             oData.ipLom,
                             oData.pctScaleTimeout,
                             oData.sOs,
                             oData.sOsVersion,
                             oData.sCpuVendor,
                             oData.sCpuArch,
                             oData.sCpuName,
                             oData.lCpuRevision,
                             oData.cCpus,
                             oData.fCpuHwVirt,
                             oData.fCpuNestedPaging,
                             oData.fCpu64BitGuest,
                             oData.fChipsetIoMmu,
                             oData.cMbMemory,
                             oData.cMbScratch,
                             oData.sReport,
                             oData.iTestBoxScriptRev,
                             oData.iPythonHexVersion,
                             oData.enmPendingCmd
                             )
                          );
        oRow = self._oDb.fetchOne();
        self._oDb.maybeCommit(fCommit);
        return (oRow[0], oRow[1], oRow[2]);

    def editEntry(self, oData, uidAuthor, fCommit = False):
        """
        Data edit update, web UI is the primary user.
        Returns the new generation ID and effective date.
        """

        dDataErrors = oData.validateAndConvert(self._oDb, oData.ksValidateFor_Edit);
        if len(dDataErrors) > 0:
            raise TMInvalidData('Invalid data passed to create(): %s' % (dDataErrors,));

        ## @todo check if the data changed.

        self._oDb.execute('UPDATE ONLY TestBoxes\n'
                          'SET      tsExpire = CURRENT_TIMESTAMP\n'
                          'WHERE    idGenTestBox = %s\n'
                          '     AND tsExpire = \'infinity\'::timestamp\n'
                          'RETURNING tsExpire\n',
                          (oData.idGenTestBox,));
        try:
            tsEffective = self._oDb.fetchOne()[0];

            # Would be easier to do this using an insert or update hook, I think. Much easier.

            ##
            ## @todo The table is growing too fast.  Rows are too long.  Mixing data from here and there.  Split it and
            ##       rethink storage and update strategy!
            ##

            self._oDb.execute('INSERT INTO TestBoxes (\n'
                              '         idGenTestBox,\n'
                              '         idTestBox,\n'
                              '         tsEffective,\n'
                              '         uidAuthor,\n'
                              '         ip,\n'
                              '         uuidSystem,\n'
                              '         sName,\n'
                              '         sDescription,\n'
                              '         idSchedGroup,\n'
                              '         fEnabled,\n'
                              '         enmLomKind,\n'
                              '         ipLom,\n'
                              '         pctScaleTimeout,\n'
                              '         sOs,\n'
                              '         sOsVersion,\n'
                              '         sCpuVendor,\n'
                              '         sCpuArch,\n'
                              '         sCpuName,\n'
                              '         lCpuRevision,\n'
                              '         cCpus,\n'
                              '         fCpuHwVirt,\n'
                              '         fCpuNestedPaging,\n'
                              '         fCpu64BitGuest,\n'
                              '         fChipsetIoMmu,\n'
                              '         cMbMemory,\n'
                              '         cMbScratch,\n'
                              '         sReport,\n'
                              '         iTestBoxScriptRev,\n'
                              '         iPythonHexVersion,\n'
                              '         enmPendingCmd\n'
                              '         )\n'
                              'SELECT   NEXTVAL(\'TestBoxGenIdSeq\'),\n'
                              '         idTestBox,\n'
                              '         %s,\n'          # tsEffective
                              '         %s,\n'          # uidAuthor
                              '         %s,\n'          # ip
                              '         %s,\n'          # uuidSystem
                              '         %s,\n'          # sName
                              '         %s,\n'          # sDescription
                              '         %s,\n'          # idSchedGroup
                              '         %s,\n'          # fEnabled
                              '         %s,\n'          # enmLomKind
                              '         %s,\n'          # ipLom
                              '         %s,\n'          # pctScaleTimeout
                              '         sOs,\n'
                              '         sOsVersion,\n'
                              '         sCpuVendor,\n'
                              '         sCpuArch,\n'
                              '         sCpuName,\n'
                              '         lCpuRevision,\n'
                              '         cCpus,\n'
                              '         fCpuHwVirt,\n'
                              '         fCpuNestedPaging,\n'
                              '         fCpu64BitGuest,\n'
                              '         fChipsetIoMmu,\n'
                              '         cMbMemory,\n'
                              '         cMbScratch,\n'
                              '         sReport,\n'
                              '         iTestBoxScriptRev,\n'
                              '         iPythonHexVersion,\n'
                              '         %s\n'           # enmPendingCmd
                              'FROM     TestBoxes\n'
                              'WHERE    idGenTestBox = %s\n'
                              'RETURNING idGenTestBox, tsEffective'
                              , (tsEffective,
                                 uidAuthor,
                                 oData.ip,
                                 oData.uuidSystem,
                                 oData.sName,
                                 oData.sDescription,
                                 oData.idSchedGroup,
                                 oData.fEnabled,
                                 oData.enmLomKind,
                                 oData.ipLom,
                                 oData.pctScaleTimeout,
                                 oData.enmPendingCmd,
                                 oData.idGenTestBox,
                              ));
            aRow = self._oDb.fetchOne();
            if aRow is None:
                raise TMExceptionBase('Insert failed? oRow=None');
            idGenTestBox = aRow[0];
            tsEffective  = aRow[1];
            self._oDb.maybeCommit(fCommit);
        except:
            self._oDb.rollback();
            raise;

        return (idGenTestBox, tsEffective);

    def updateOnSignOn(self, idTestBox, idGenTestBox, sTestBoxAddr, sOs, sOsVersion, # pylint: disable=R0913,R0914
                       sCpuVendor, sCpuArch, sCpuName, lCpuRevision, cCpus, fCpuHwVirt, fCpuNestedPaging, fCpu64BitGuest,
                       fChipsetIoMmu, cMbMemory, cMbScratch, sReport, iTestBoxScriptRev, iPythonHexVersion):
        """
        Update the testbox attributes automatically on behalf of the testbox script.
        Returns the new generation id on success, raises an exception on failure.
        """
        try:
            # Would be easier to do this using an insert or update hook, I think. Much easier.
            self._oDb.execute('UPDATE ONLY TestBoxes\n'
                              'SET      tsExpire = CURRENT_TIMESTAMP\n'
                              'WHERE    idGenTestBox = %s\n'
                              '     AND tsExpire = \'infinity\'::timestamp\n'
                              'RETURNING tsExpire\n',
                              (idGenTestBox,));
            tsEffective = self._oDb.fetchOne()[0];

            self._oDb.execute('INSERT INTO TestBoxes (\n'
                              '         idGenTestBox,\n'
                              '         idTestBox,\n'
                              '         tsEffective,\n'
                              '         uidAuthor,\n'
                              '         ip,\n'
                              '         uuidSystem,\n'
                              '         sName,\n'
                              '         sDescription,\n'
                              '         idSchedGroup,\n'
                              '         fEnabled,\n'
                              '         enmLomKind,\n'
                              '         ipLom,\n'
                              '         pctScaleTimeout,\n'
                              '         sOs,\n'
                              '         sOsVersion,\n'
                              '         sCpuVendor,\n'
                              '         sCpuArch,\n'
                              '         sCpuName,\n'
                              '         lCpuRevision,\n'
                              '         cCpus,\n'
                              '         fCpuHwVirt,\n'
                              '         fCpuNestedPaging,\n'
                              '         fCpu64BitGuest,\n'
                              '         fChipsetIoMmu,\n'
                              '         cMbMemory,\n'
                              '         cMbScratch,\n'
                              '         sReport,\n'
                              '         iTestBoxScriptRev,\n'
                              '         iPythonHexVersion,\n'
                              '         enmPendingCmd\n'
                              '         )\n'
                              'SELECT   NEXTVAL(\'TestBoxGenIdSeq\'),\n'
                              '         %s,\n'
                              '         %s,\n'
                              '         NULL,\n'            # uidAuthor
                              '         %s,\n'
                              '         uuidSystem,\n'
                              '         sName,\n'
                              '         sDescription,\n'
                              '         idSchedGroup,\n'
                              '         fEnabled,\n'
                              '         enmLomKind,\n'
                              '         ipLom,\n'
                              '         pctScaleTimeout,\n'
                              '         %s,\n'              # sOs
                              '         %s,\n'              # sOsVersion
                              '         %s,\n'              # sCpuVendor
                              '         %s,\n'              # sCpuArch
                              '         %s,\n'              # sCpuName
                              '         %s,\n'              # lCpuRevision
                              '         %s,\n'              # cCpus
                              '         %s,\n'              # fCpuHwVirt
                              '         %s,\n'              # fCpuNestedPaging
                              '         %s,\n'              # fCpu64BitGuest
                              '         %s,\n'              # fChipsetIoMmu
                              '         %s,\n'              # cMbMemory
                              '         %s,\n'              # cMbScratch
                              '         %s,\n'              # sReport
                              '         %s,\n'              # iTestBoxScriptRev
                              '         %s,\n'              # iPythonHexVersion
                              '         enmPendingCmd\n'
                              'FROM     TestBoxes\n'
                              'WHERE    idGenTestBox = %s\n'
                              'RETURNING idGenTestBox'
                              , (idTestBox,
                                 tsEffective,
                                 sTestBoxAddr,
                                 sOs,
                                 sOsVersion,
                                 sCpuVendor,
                                 sCpuArch,
                                 sCpuName,
                                 lCpuRevision,
                                 cCpus,
                                 fCpuHwVirt,
                                 fCpuNestedPaging,
                                 fCpu64BitGuest,
                                 fChipsetIoMmu,
                                 cMbMemory,
                                 cMbScratch,
                                 sReport,
                                 iTestBoxScriptRev,
                                 iPythonHexVersion,
                                 idGenTestBox,
                              ));
            idGenTestBox = self._oDb.fetchOne()[0];
            self._oDb.commit();
        except:
            self._oDb.rollback();
            raise;

        return idGenTestBox;

    def setCommand(self, idTestBox, sOldCommand, sNewCommand, uidAuthor = None, fCommit = False, sComment = None,
                   fNoRollbackOnInFlightCollision = False):
        """
        Sets or resets the pending command on a testbox.
        Returns (idGenTestBox, tsEffective) of the new row.
        """
        _ = sComment;
        try:
            # Would be easier to do this using an insert or update hook, I think. Much easier.
            self._oDb.execute('UPDATE ONLY TestBoxes\n'
                              'SET      tsExpire = CURRENT_TIMESTAMP\n'
                              'WHERE    idTestBox = %s\n'
                              '     AND tsExpire = \'infinity\'::timestamp\n'
                              '     AND enmPendingCmd = %s\n'
                              'RETURNING tsExpire\n',
                              (idTestBox, sOldCommand,));
            if self._oDb.getRowCount() == 0:
                raise TMInFligthCollision();
            tsEffective = self._oDb.fetchOne()[0];

            self._oDb.execute('INSERT INTO TestBoxes (\n'
                              '         idGenTestBox,\n'
                              '         idTestBox,\n'
                              '         tsEffective,\n'
                              '         uidAuthor,\n'
                              '         ip,\n'
                              '         uuidSystem,\n'
                              '         sName,\n'
                              '         sDescription,\n'
                              '         idSchedGroup,\n'
                              '         fEnabled,\n'
                              '         enmLomKind,\n'
                              '         ipLom,\n'
                              '         pctScaleTimeout,\n'
                              '         sOs,\n'
                              '         sOsVersion,\n'
                              '         sCpuVendor,\n'
                              '         sCpuArch,\n'
                              '         sCpuName,\n'
                              '         lCpuRevision,\n'
                              '         cCpus,\n'
                              '         fCpuHwVirt,\n'
                              '         fCpuNestedPaging,\n'
                              '         fCpu64BitGuest,\n'
                              '         fChipsetIoMmu,\n'
                              '         cMbMemory,\n'
                              '         cMbScratch,\n'
                              '         sReport,\n'
                              '         iTestBoxScriptRev,\n'
                              '         iPythonHexVersion,\n'
                              '         enmPendingCmd\n'
                              '         )\n'
                              'SELECT   NEXTVAL(\'TestBoxGenIdSeq\'),\n'
                              '         %s,\n'      # idTestBox
                              '         %s,\n'      # tsEffective
                              '         %s,\n'      # uidAuthor
                              '         ip,\n'
                              '         uuidSystem,\n'
                              '         sName,\n'
                              '         sDescription,\n'
                              '         idSchedGroup,\n'
                              '         fEnabled,\n'
                              '         enmLomKind,\n'
                              '         ipLom,\n'
                              '         pctScaleTimeout,\n'
                              '         sOs,\n'
                              '         sOsVersion,\n'
                              '         sCpuVendor,\n'
                              '         sCpuArch,\n'
                              '         sCpuName,\n'
                              '         lCpuRevision,\n'
                              '         cCpus,\n'
                              '         fCpuHwVirt,\n'
                              '         fCpuNestedPaging,\n'
                              '         fCpu64BitGuest,\n'
                              '         fChipsetIoMmu,\n'
                              '         cMbMemory,\n'
                              '         cMbScratch,\n'
                              '         sReport,\n'
                              '         iTestBoxScriptRev,\n'
                              '         iPythonHexVersion,\n'
                              '         %s\n'       # enmPendingCmd
                              'FROM     TestBoxes\n'
                              'WHERE    idTestBox = %s\n'
                              '     AND tsExpire  = %s\n'
                              '     AND enmPendingCmd = %s\n'
                              'RETURNING idGenTestBox'
                              , (idTestBox,
                                 tsEffective,
                                 uidAuthor,
                                 sNewCommand,
                                 idTestBox,
                                 tsEffective,
                                 sOldCommand,
                              ));
            idGenTestBox = self._oDb.fetchOne()[0];
            if fCommit is True:
                self._oDb.commit();

        except TMInFligthCollision: # This is pretty stupid, but don't want to touch testboxcontroller.py now.
            if not fNoRollbackOnInFlightCollision:
                self._oDb.rollback();
            raise;
        except:
            self._oDb.rollback();
            raise;

        return (idGenTestBox, tsEffective);



    def getAll(self):
        """
        Retrieve list of all registered Test Box records from DB.
        """
        self._oDb.execute('SELECT   *\n'
                          'FROM     TestBoxes\n'
                          'WHERE    tsExpire=\'infinity\'::timestamp;')

        aaoRows = self._oDb.fetchAll()
        aoRet = []
        for aoRow in aaoRows:
            aoRet.append(TestBoxData().initFromDbRow(aoRow))
        return aoRet

    def _historize(self, idTestBox, uidAuthor, tsExpire = None):
        """ Historizes the current entry. """
        if tsExpire is None:
            self._oDb.execute('UPDATE TestBoxes\n'
                              'SET    tsExpire   = CURRENT_TIMESTAMP,\n'
                              '       uidAuthor  = %s\n'
                              'WHERE  idTestBox  = %s\n'
                              '   AND tsExpire = \'infinity\'::TIMESTAMP\n'
                              , (uidAuthor, idTestBox,));
        else:
            self._oDb.execute('UPDATE TestBoxes\n'
                              'SET    tsExpire   = %s,\n'
                              '       uidAuthor  = %s\n'
                              'WHERE  idTestBox  = %s\n'
                              '   AND tsExpire = \'infinity\'::TIMESTAMP\n'
                              , (uidAuthor, tsExpire, idTestBox,));
        return True;


    def removeEntry(self, uidAuthor, idTestBox, fCascade = False, fCommit=False):
        """
        Delete user account
        """
        _ = fCascade;

        fRc = self._historize(idTestBox, uidAuthor, None);
        if fRc:
            self._oDb.maybeCommit(fCommit);

        return fRc


    def cachedLookup(self, idTestBox):
        """
        Looks up the most recent TestBoxData object for idTestBox via
        an object cache.

        Returns a shared TestBoxData object.  None if not found.
        Raises exception on DB error.
        """
        if self.dCache is None:
            self.dCache = self._oDb.getCache('TestBoxData');
        oEntry = self.dCache.get(idTestBox, None);
        if oEntry is None:
            self._oDb.execute('SELECT   *\n'
                              'FROM     TestBoxes\n'
                              'WHERE    idTestBox  = %s\n'
                              '     AND tsExpire   = \'infinity\'::TIMESTAMP\n'
                              , (idTestBox, ));
            if self._oDb.getRowCount() == 0:
                # Maybe it was deleted, try get the last entry.
                self._oDb.execute('SELECT   *\n'
                                  'FROM     TestBoxes\n'
                                  'WHERE    idTestBox = %s\n'
                                  'ORDER BY tsExpire DESC\n'
                                  'LIMIT 1\n'
                                  , (idTestBox, ));
            elif self._oDb.getRowCount() > 1:
                raise self._oDb.integrityException('%s infinity rows for %s' % (self._oDb.getRowCount(), idTestBox));

            if self._oDb.getRowCount() == 1:
                aaoRow = self._oDb.fetchOne();
                oEntry = TestBoxData().initFromDbRow(aaoRow);
                self.dCache[idTestBox] = oEntry;
        return oEntry;



    #
    # The virtual test sheriff interface.
    #

    def hasTestBoxRecentlyBeenRebooted(self, idTestBox, cHoursBack = 2, tsNow = None):
        """
        Checks if the testbox has been rebooted in the specified time period.

        This does not include already pending reboots, though under some
        circumstances it may.  These being the test box entry being edited for
        other reasons.

        Returns True / False.
        """
        if tsNow is None:
            tsNow = self._oDb.getCurrentTimestamp();
        self._oDb.execute('SELECT COUNT(idTestBox)\n'
                          'FROM   TestBoxes\n'
                          'WHERE  idTestBox      = %s\n'
                          '   AND tsExpire       < %s\n'
                          '   AND tsExpire      >= %s - interval \'%s hours\'\n'
                          '   AND enmPendingCmd IN (%s, %s)\n'
                          , ( idTestBox, tsNow, tsNow, cHoursBack,
                              TestBoxData.ksTestBoxCmd_Reboot, TestBoxData.ksTestBoxCmd_UpgradeAndReboot, ));
        return self._oDb.fetchOne()[0] > 0;


    def rebootTestBox(self, idTestBox, uidAuthor, sComment, sOldCommand = TestBoxData.ksTestBoxCmd_None, fCommit = False):
        """
        Issues a reboot command for the given test box.
        Return True on succes, False on in-flight collision.
        May raise DB exception with rollback on other trouble.
        """
        try:
            self.setCommand(idTestBox, sOldCommand, TestBoxData.ksTestBoxCmd_Reboot,
                            uidAuthor = uidAuthor, fCommit = fCommit, sComment = sComment,
                            fNoRollbackOnInFlightCollision = True);
        except TMInFligthCollision:
            return False;
        except:
            raise;
        return True;


    def disableTestBox(self, idTestBox, uidAuthor, sComment, fCommit = False):
        """
        Disables the given test box.

        Raises exception on trouble, without rollback.
        """
        oTestBox = TestBoxData().initFromDbWithId(self._oDb, idTestBox);
        if oTestBox.fEnabled:
            oTestBox.fEnabled = False;
            if sComment is not None:
                _ = sComment; # oTestBox.sComment = sComment;
            self.editEntry(oTestBox, uidAuthor = uidAuthor, fCommit = fCommit);
        return None;


#
# Unit testing.
#

# pylint: disable=C0111
class TestBoxDataTestCase(ModelDataBaseTestCase):
    def setUp(self):
        self.aoSamples = [TestBoxData(),];

if __name__ == '__main__':
    unittest.main();
    # not reached.

