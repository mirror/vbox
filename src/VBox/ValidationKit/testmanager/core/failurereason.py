# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager - Failure Reasons.
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


# Validation Kit imports.
from testmanager.core.base              import ModelDataBase, ModelLogicBase, TMExceptionBase
from testmanager.core.useraccount       import UserAccountLogic;



class FailureReasonData(ModelDataBase):
    """
    Failure Reason Data.
    """

    ksIdAttr = 'idFailureReason';

    ksParam_idFailureReason   = 'FailureReasonData_idFailureReason'
    ksParam_tsEffective       = 'FailureReasonData_tsEffective'
    ksParam_tsExpire          = 'FailureReasonData_tsExpire'
    ksParam_uidAuthor         = 'FailureReasonData_uidAuthor'
    ksParam_idFailureCategory = 'FailureReasonData_idFailureCategory'
    ksParam_sShort            = 'FailureReasonData_sShort'
    ksParam_sFull             = 'FailureReasonData_sFull'
    ksParam_iTicket           = 'FailureReasonData_iTicket'
    ksParam_asUrls            = 'FailureReasonData_asUrls'

    kasAllowNullAttributes    = [ 'idFailureReason', 'tsEffective', 'tsExpire',
                                  'uidAuthor',       'iTicket',      'asUrls' ]

    def __init__(self):
        ModelDataBase.__init__(self);

        #
        # Initialize with defaults.
        # See the database for explanations of each of these fields.
        #

        self.idFailureReason   = None
        self.tsEffective       = None
        self.tsExpire          = None
        self.uidAuthor         = None
        self.idFailureCategory = None
        self.sShort            = None
        self.sFull             = None
        self.iTicket           = None
        self.asUrls            = None

    def initFromDbRow(self, aoRow):
        """
        Re-initializes the data with a row from a SELECT * FROM FailureReasons.

        Returns self. Raises exception if the row is None or otherwise invalid.
        """

        if aoRow is None:
            raise TMExceptionBase('Failure Reason not found.');

        self.idFailureReason   = aoRow[0]
        self.tsEffective       = aoRow[1]
        self.tsExpire          = aoRow[2]
        self.uidAuthor         = aoRow[3]
        self.idFailureCategory = aoRow[4]
        self.sShort            = aoRow[5]
        self.sFull             = aoRow[6]
        self.iTicket           = aoRow[7]
        self.asUrls            = aoRow[8]

        return self;


class FailureReasonDataEx(FailureReasonData):
    """
    Failure Reason Data, extended version that includes the category.
    """

    def __init__(self):
        FailureReasonData.__init__(self);
        self.oCategory  = None;
        self.oAuthor    = None;

    def initFromDbRowEx(self, aoRow, oCategoryLogic, oUserAccountLogic):
        """
        Re-initializes the data with a row from a SELECT * FROM FailureReasons.

        Returns self. Raises exception if the row is None or otherwise invalid.
        """

        self.initFromDbRow(aoRow);
        self.oCategory  = oCategoryLogic.cachedLookup(self.idFailureCategory);
        self.oAuthor    = oUserAccountLogic.cachedLookup(self.uidAuthor);

        return self;


class FailureReasonLogic(ModelLogicBase): # pylint: disable=R0903
    """
    Failure Reason logic.
    """

    def __init__(self, oDb):
        ModelLogicBase.__init__(self, oDb)
        self.ahCache = None;
        self.oCategoryLogic = None;
        self.oUserAccountLogic = None;

    def fetchForListing(self, iStart, cMaxRows, tsNow):
        """
        Fetches Failure Category records.

        Returns an array (list) of FailureReasonData items, empty list if none.
        Raises exception on error.
        """

        if tsNow is None:
            self._oDb.execute('SELECT   *\n'
                              'FROM     FailureReasons\n'
                              'WHERE    tsExpire = \'infinity\'::TIMESTAMP\n'
                              'ORDER BY idFailureReason DESC\n'
                              'LIMIT %s OFFSET %s\n'
                              , (cMaxRows, iStart,));
        else:
            self._oDb.execute('SELECT   *\n'
                              'FROM     FailureReasons\n'
                              'WHERE    tsExpire     > %s\n'
                              '     AND tsEffective <= %s\n'
                              'ORDER BY idFailureReason DESC\n'
                              'LIMIT %s OFFSET %s\n'
                              , (tsNow, tsNow, cMaxRows, iStart,));

        aoRows = []
        for aoRow in self._oDb.fetchAll():
            aoRows.append(FailureReasonData().initFromDbRow(aoRow))
        return aoRows

    def fetchForCombo(self, sFirstEntry = 'Select a failure reason', tsEffective = None):
        """
        Gets the list of Failure Reasons for a combo box.
        Returns an array of (value [idFailureReason], drop-down-name [sShort],
        hover-text [sFull]) tuples.
        """
        if tsEffective is None:
            self._oDb.execute('SELECT   fr.idFailureReason, CONCAT(fc.sShort, \' / \', fr.sShort) as sComboText, fr.sFull\n'
                              'FROM     FailureReasons fr,\n'
                              '         FailureCategories fc\n'
                              'WHERE    fr.idFailureCategory = fc.idFailureCategory\n'
                              '  AND    fr.tsExpire = \'infinity\'::TIMESTAMP\n'
                              '  AND    fc.tsExpire = \'infinity\'::TIMESTAMP\n'
                              'ORDER BY sComboText')
        else:
            self._oDb.execute('SELECT   fr.idFailureReason, CONCAT(fc.sShort, \' / \', fr.sShort) as sComboText, fr.sFull\n'
                              'FROM     FailureReasons fr,\n'
                              '         FailureCategories fc\n'
                              'WHERE    fr.idFailureCategory = fc.idFailureCategory\n'
                              '  AND    fr.tsExpire     > %s\n'
                              '  AND    fr.tsEffective <= %s\n'
                              '  AND    fc.tsExpire     > %s\n'
                              '  AND    fc.tsEffective <= %s\n'
                              'ORDER BY sComboText'
                              , (tsEffective, tsEffective, tsEffective, tsEffective));
        aoRows = self._oDb.fetchAll();
        return [(-1, sFirstEntry, '')] + aoRows;

    def getById(self, idFailureReason):
        """Get Failure Reason data by idFailureReason"""

        self._oDb.execute('SELECT   *\n'
                          'FROM     FailureReasons\n'
                          'WHERE    tsExpire   = \'infinity\'::timestamp\n'
                          '  AND    idFailureReason = %s;', (idFailureReason,))
        aRows = self._oDb.fetchAll()
        if len(aRows) not in (0, 1):
            raise self._oDb.integrityException(
                'Found more than one failure reasons with the same credentials. Database structure is corrupted.')
        try:
            return FailureReasonData().initFromDbRow(aRows[0])
        except IndexError:
            return None

    def getIdsByCategory(self, idFailureCategory, tsEffective=None):
        """
        Gets the list of Failure Ressons IDs,
        all the items belong to @param idFailureCategory
        """
        if tsEffective is None:
            self._oDb.execute('SELECT   idFailureReason\n'
                              'FROM     FailureReasons\n'
                              'WHERE    tsExpire = \'infinity\'::TIMESTAMP\n'
                              '     AND idFailureCategory = %s\n'
                              'ORDER BY idFailureReason DESC'
                              , (idFailureCategory,))
        else:
            self._oDb.execute('SELECT   idFailureReason\n'
                              'FROM     FailureReasons\n'
                              'WHERE    tsExpire     > %s\n'
                              '     AND tsEffective <= %s\n'
                              '     AND idFailureCategory = %s\n'
                              'ORDER BY idFailureReason DESC'
                              , (tsEffective, tsEffective, idFailureCategory))
        return self._oDb.fetchAll()

    def getAll(self, tsEffective=None):
        """
        Gets the list of all Failure Reasons.
        Returns an array of FailureReasonData instances.
        """
        if tsEffective is None:
            self._oDb.execute('SELECT   *\n'
                              'FROM     FailureReasons\n'
                              'WHERE    tsExpire = \'infinity\'::TIMESTAMP\n'
                              'ORDER BY idFailureReason DESC')
        else:
            self._oDb.execute('SELECT   *\n'
                              'FROM     FailureReasons\n'
                              'WHERE    tsExpire     > %s\n'
                              '     AND tsEffective <= %s\n'
                              'ORDER BY idFailureReason DESC'
                              , (tsEffective, tsEffective))
        aoRet = []
        for aoRow in self._oDb.fetchAll():
            aoRet.append(FailureReasonData().initFromDbRow(aoRow))
        return aoRet

    def addEntry(self, oFailureReasonData, uidAuthor, fCommit=True):
        """Add record to database"""

        # Check if record with the same sShort fiels is already exists
        self._oDb.execute('SELECT *\n'
                          'FROM   FailureReasons\n'
                          'WHERE  tsExpire   = \'infinity\'::TIMESTAMP\n'
                          '   AND sShort = %s\n',
                          (oFailureReasonData.sShort,))
        if len(self._oDb.fetchAll()) != 0:
            raise Exception('Record already exist')

        # Add record
        self._oDb.execute('INSERT INTO FailureReasons (\n'
                          '  uidAuthor, idFailureCategory,'
                          '  sShort, sFull, iTicket, asUrls'
                          ')\n'
                          'VALUES (%s, %s, %s, %s, %s, %s)',
                          (uidAuthor,
                           oFailureReasonData.idFailureCategory,
                           oFailureReasonData.sShort,
                           oFailureReasonData.sFull,
                           oFailureReasonData.iTicket,
                           oFailureReasonData.asUrls))
        if fCommit:
            self._oDb.commit()

        return True

    def remove(self, uidAuthor, idFailureReason, fNeedCommit=True):
        """
        Historize record
        """
        self._oDb.execute('UPDATE FailureReasons\n'
                          'SET    tsExpire    = CURRENT_TIMESTAMP,\n'
                          '       uidAuthor   = %s\n'
                          'WHERE  idFailureReason = %s\n'
                          '   AND tsExpire    = \'infinity\'::TIMESTAMP\n',
                          (uidAuthor, idFailureReason))

        # Also historize Black List records
        self._oDb.execute('UPDATE BuildBlackList\n'
                          'SET    tsExpire    = CURRENT_TIMESTAMP,\n'
                          '       uidAuthor   = %s\n'
                          'WHERE  idFailureReason = %s\n'
                          '   AND tsExpire    = \'infinity\'::TIMESTAMP\n',
                          (uidAuthor, idFailureReason))

        if fNeedCommit:
            self._oDb.commit()

        return True

    def editEntry(self, oFailureReasonData, uidAuthor, fCommit=True):
        """Modify database record"""

        # Check if record exists
        oFailureReasonDataOld = self.getById(oFailureReasonData.idFailureReason)
        if oFailureReasonDataOld is None:
            raise TMExceptionBase(
                'Failure Reason (id: %d) does not exist'
                % oFailureReasonData.idFailureReason)

        # Check if anything has been changed
        if oFailureReasonData.isEqual(oFailureReasonDataOld):
            return True

        # Historize record
        self.remove(
            uidAuthor, oFailureReasonData.idFailureReason, fNeedCommit=False)


        # Add new record (keeping its ID)
        self._oDb.execute('INSERT INTO FailureReasons (\n'
                          '  idFailureReason,'
                          '  uidAuthor,'
                          '  idFailureCategory,'
                          '  sShort,'
                          '  sFull,'
                          '  iTicket,'
                          '  asUrls'
                          ')\n'
                          'VALUES (%s, %s, %s, %s, %s, %s, %s)',
                          (oFailureReasonData.idFailureReason,
                           uidAuthor,
                           oFailureReasonData.idFailureCategory,
                           oFailureReasonData.sShort,
                           oFailureReasonData.sFull,
                           oFailureReasonData.iTicket,
                           oFailureReasonData.asUrls
                           ))

        if fCommit:
            self._oDb.commit()

        return True

    def cachedLookup(self, idFailureReason):
        """
        Looks up the most recent FailureReasonDataEx object for uid idFailureReason
        an object cache.

        Returns a shared FailureReasonData object.  None if not found.
        Raises exception on DB error.
        """
        if self.ahCache is None:
            self.ahCache = self._oDb.getCache('FailureReasonDataEx');
        oEntry = self.ahCache.get(idFailureReason, None);
        if oEntry is None:
            self._oDb.execute('SELECT   *\n'
                              'FROM     FailureReasons\n'
                              'WHERE    idFailureReason = %s\n'
                              '     AND tsExpire = \'infinity\'::TIMESTAMP\n'
                              , (idFailureReason, ));
            if self._oDb.getRowCount() == 0:
                # Maybe it was deleted, try get the last entry.
                self._oDb.execute('SELECT   *\n'
                                  'FROM     FailureReasons\n'
                                  'WHERE    idFailureReason = %s\n'
                                  'ORDER BY tsExpire\n'
                                  'LIMIT 1\n'
                                  , (idFailureReason, ));
            elif self._oDb.getRowCount() > 1:
                raise self._oDb.integrityException('%s infinity rows for %s' % (self._oDb.getRowCount(), idFailureReason));

            if self._oDb.getRowCount() == 1:
                if self.oCategoryLogic is None:
                    from testmanager.core.failurecategory import FailureCategoryLogic;
                    self.oCategoryLogic = FailureCategoryLogic(self._oDb);
                if self.oUserAccountLogic is None:
                    self.oUserAccountLogic = UserAccountLogic(self._oDb);
                oEntry = FailureReasonDataEx().initFromDbRowEx(self._oDb.fetchOne(), self.oCategoryLogic,
                                                               self.oUserAccountLogic);
                self.ahCache[idFailureReason] = oEntry;
        return oEntry;

