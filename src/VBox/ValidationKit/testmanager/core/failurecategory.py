# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager - Failure Categories.
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
from testmanager.core.base          import ModelDataBase, ModelLogicBase, TMExceptionBase
from testmanager.core.failurereason import FailureReasonLogic

class FailureCategoryData(ModelDataBase):
    """
    Failure Category Data.
    """

    ksParam_idFailureCategory = 'FailureCategory_idFailureCategory'
    ksParam_tsEffective       = 'FailureCategory_tsEffective'
    ksParam_tsExpire          = 'FailureCategory_tsExpire'
    ksParam_uidAuthor         = 'FailureCategory_uidAuthor'
    ksParam_sShort            = 'FailureCategory_sShort'
    ksParam_sFull             = 'FailureCategory_sFull'

    kasAllowNullAttributes    = [ 'idFailureCategory', 'tsEffective', 'tsExpire', 'uidAuthor' ]

    def __init__(self):
        ModelDataBase.__init__(self);

        #
        # Initialize with defaults.
        # See the database for explanations of each of these fields.
        #

        self.idFailureCategory = None
        self.tsEffective       = None
        self.tsExpire          = None
        self.uidAuthor         = None
        self.sShort            = None
        self.sFull             = None

    def initFromDbRow(self, aoRow):
        """
        Re-initializes the data with a row from a SELECT * FROM FailureCategoryes.

        Returns self. Raises exception if the row is None or otherwise invalid.
        """

        if aoRow is None:
            raise TMExceptionBase('Failure Category not found.');

        self.idFailureCategory = aoRow[0]
        self.tsEffective       = aoRow[1]
        self.tsExpire          = aoRow[2]
        self.uidAuthor         = aoRow[3]
        self.sShort            = aoRow[4]
        self.sFull             = aoRow[5]

        return self


class FailureCategoryLogic(ModelLogicBase): # pylint: disable=R0903
    """
    Failure Category logic.
    """

    def __init__(self, oDb):
        ModelLogicBase.__init__(self, oDb)
        self.ahCache = None;

    def fetchForListing(self, iStart, cMaxRows, tsNow):
        """
        Fetches Failure Category records.

        Returns an array (list) of FailureCategoryData items, empty list if none.
        Raises exception on error.
        """

        if tsNow is None:
            self._oDb.execute('SELECT   *\n'
                              'FROM     FailureCategories\n'
                              'WHERE    tsExpire = \'infinity\'::TIMESTAMP\n'
                              'ORDER BY idFailureCategory ASC\n'
                              'LIMIT %s OFFSET %s\n'
                              , (cMaxRows, iStart,));
        else:
            self._oDb.execute('SELECT   *\n'
                              'FROM     FailureCategories\n'
                              'WHERE    tsExpire     > %s\n'
                              '     AND tsEffective <= %s\n'
                              'ORDER BY idFailureCategory ASC\n'
                              'LIMIT %s OFFSET %s\n'
                              , (tsNow, tsNow, cMaxRows, iStart,));

        aoRows = []
        for aoRow in self._oDb.fetchAll():
            aoRows.append(FailureCategoryData().initFromDbRow(aoRow))
        return aoRows

    def getFailureCategoriesForCombo(self, tsEffective = None):
        """
        Gets the list of Failure Categories for a combo box.
        Returns an array of (value [idFailureCategory], drop-down-name [sShort],
        hover-text [sFull]) tuples.
        """
        if tsEffective is None:
            self._oDb.execute('SELECT   idFailureCategory, sShort, sFull\n'
                              'FROM     FailureCategories\n'
                              'WHERE    tsExpire = \'infinity\'::TIMESTAMP\n'
                              'ORDER BY sShort')
        else:
            self._oDb.execute('SELECT   idFailureCategory, sShort, sFull\n'
                              'FROM     FailureCategories\n'
                              'WHERE    tsExpire     > %s\n'
                              '     AND tsEffective <= %s\n'
                              'ORDER BY sShort'
                              , (tsEffective, tsEffective))
        return self._oDb.fetchAll()


    def getById(self, idFailureCategory):
        """Get Failure Category data by idFailureCategory"""

        self._oDb.execute('SELECT   *\n'
                          'FROM     FailureCategories\n'
                          'WHERE    tsExpire   = \'infinity\'::timestamp\n'
                          '  AND    idFailureCategory = %s;', (idFailureCategory,))
        aRows = self._oDb.fetchAll()
        if len(aRows) not in (0, 1):
            raise self._oDb.integrityException(
                'Found more than one failure categories with the same credentials. Database structure is corrupted.')
        try:
            return FailureCategoryData().initFromDbRow(aRows[0])
        except IndexError:
            return None

    def addEntry(self, oFailureCategoryData, uidAuthor, fCommit=True):
        """
        Add Failure Category record
        """

        # Check if record with the same sShort fiels is already exists
        self._oDb.execute('SELECT *\n'
                          'FROM   FailureCategories\n'
                          'WHERE  tsExpire   = \'infinity\'::TIMESTAMP\n'
                          '   AND sShort = %s\n',
                          (oFailureCategoryData.sShort,))
        if len(self._oDb.fetchAll()) != 0:
            raise Exception('Record already exist')

        # Add record
        self._oDb.execute('INSERT INTO FailureCategories (\n'
                          '  uidAuthor, sShort, sFull'
                          ')\n'
                          'VALUES (%s, %s, %s)',
                          (uidAuthor,
                           oFailureCategoryData.sShort,
                           oFailureCategoryData.sFull))
        if fCommit:
            self._oDb.commit()

        return True

    def remove(self, uidAuthor, idFailureCategory, fNeedCommit=True):
        """
        Historize record
        """

        # Historize Failure Reasons records first
        self._oDb.execute('SELECT idFailureReason\n'
                          'FROM   FailureReasons\n'
                          'WHERE  idFailureCategory = %s\n'
                          '   AND tsExpire    = \'infinity\'::TIMESTAMP\n',
                          (idFailureCategory,))
        for iFailureReasonId in self._oDb.fetchAll():
            FailureReasonLogic(self._oDb).remove(uidAuthor, iFailureReasonId, fNeedCommit = False)

        self._oDb.execute('UPDATE FailureCategories\n'
                          'SET    tsExpire    = CURRENT_TIMESTAMP,\n'
                          '       uidAuthor   = %s\n'
                          'WHERE  idFailureCategory = %s\n'
                          '   AND tsExpire    = \'infinity\'::TIMESTAMP\n',
                          (uidAuthor, idFailureCategory))

        if fNeedCommit:
            self._oDb.commit()

        return True

    def editEntry(self, oFailureCategoryData, uidAuthor, fCommit=True):
        """Modify database record"""

        # Check if record exists
        oFailureCategoryDataOld = self.getById(oFailureCategoryData.idFailureCategory)
        if oFailureCategoryDataOld is None:
            raise TMExceptionBase(
                'Failure Category (id: %d) does not exist'
                % oFailureCategoryData.idFailureCategory)

        # Check if anything has been changed
        if oFailureCategoryData.isEqual(oFailureCategoryDataOld):
            return True

        # Historize record
        self.remove(
            uidAuthor, oFailureCategoryData.idFailureCategory, fNeedCommit=False)

        self._oDb.execute('INSERT INTO FailureCategories (\n'
                          '  idFailureCategory, uidAuthor, sShort, sFull'
                          ')\n'
                          'VALUES (%s, %s, %s, %s)',
                          (oFailureCategoryData.idFailureCategory,
                           uidAuthor,
                           oFailureCategoryData.sShort,
                           oFailureCategoryData.sFull))
        if fCommit:
            self._oDb.commit()

        return True

    def cachedLookup(self, idFailureCategory):
        """
        Looks up the most recent FailureCategoryData object for idFailureCategory
        via an object cache.

        Returns a shared FailureCategoryData object.  None if not found.
        Raises exception on DB error.
        """
        if self.ahCache is None:
            self.ahCache = self._oDb.getCache('FailureCategory');

        oEntry = self.ahCache.get(idFailureCategory, None);
        if oEntry is None:
            self._oDb.execute('SELECT   *\n'
                              'FROM     FailureCategories\n'
                              'WHERE    idFailureCategory = %s\n'
                              '     AND tsExpire = \'infinity\'::TIMESTAMP\n'
                              , (idFailureCategory, ));
            if self._oDb.getRowCount() == 0:
                # Maybe it was deleted, try get the last entry.
                self._oDb.execute('SELECT   *\n'
                                  'FROM     FailureCategories\n'
                                  'WHERE    idFailureCategory = %s\n'
                                  'ORDER BY tsExpire\n'
                                  'LIMIT 1\n'
                                  , (idFailureCategory, ));
            elif self._oDb.getRowCount() > 1:
                raise self._oDb.integrityException('%s infinity rows for %s' % (self._oDb.getRowCount(), idFailureCategory));

            if self._oDb.getRowCount() == 1:
                oEntry = FailureCategoryData().initFromDbRow(self._oDb.fetchOne());
                self.ahCache[idFailureCategory] = oEntry;
        return oEntry;

