# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager - Report models.
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
from testmanager.core.base          import ModelLogicBase, TMExceptionBase;
from testmanager.core.build         import BuildCategoryData;
from testmanager.core.dbobjcache    import DatabaseObjCache;
from testmanager.core.failurereason import FailureReasonLogic;
from testmanager.core.testbox       import TestBoxData;
from common                         import constants;



class ReportModelBase(ModelLogicBase): # pylint: disable=R0903
    """
    Something all report logic(/miner) classes inherit from.
    """

    ## @name Report subjects
    ## @{
    ksSubEverything       = 'Everything';
    ksSubSchedGroup       = 'SchedGroup';
    ksSubTestGroup        = 'TestGroup';
    ksSubTestCase         = 'TestCase';
    ksSubTestCaseArgs     = 'TestCaseArgs';
    ksSubTestBox          = 'TestBox';
    ksSubBuild            = 'Build';
    ## @}
    kasSubjects           = [ ksSubEverything, ksSubSchedGroup, ksSubTestGroup, ksSubTestCase, ksSubTestBox, ksSubBuild, ];


    ## @name TestStatus_T
    # @{
    ksTestStatus_Running    = 'running';
    ksTestStatus_Success    = 'success';
    ksTestStatus_Skipped    = 'skipped';
    ksTestStatus_BadTestBox = 'bad-testbox';
    ksTestStatus_Aborted    = 'aborted';
    ksTestStatus_Failure    = 'failure';
    ksTestStatus_TimedOut   = 'timed-out';
    ksTestStatus_Rebooted   = 'rebooted';
    ## @}


    def __init__(self, oDb, tsNow, cPeriods, cHoursPerPeriod, sSubject, aidSubjects):
        ModelLogicBase.__init__(self, oDb);
        # Public so the report generator can easily access them.
        self.tsNow           = tsNow;               # (Can be None.)
        self.cPeriods        = cPeriods;
        self.cHoursPerPeriod = cHoursPerPeriod;
        self.sSubject        = sSubject;
        self.aidSubjects     = aidSubjects;

    def getExtraSubjectTables(self):
        """
        Returns a string with any extra tables needed by the subject. Each
        table name is prefixed by a comma, so can be appended to a FROM
        statement.
        """
        if self.sSubject == self.ksSubSchedGroup:
            return ', TestBoxes';
        return '';

    def getExtraSubjectWhereExpr(self):
        """
        Returns additional WHERE expression relating to the report subject.  It starts
        with an AND so that it can simply be appended to the WHERE clause.
        """
        if self.sSubject == self.ksSubEverything:
            return '';

        if self.sSubject == self.ksSubSchedGroup:
            sWhere = '     AND TestBoxes.idTestBox    =  TestSets.idTestBox\n' \
                     '     AND TestBoxes.tsEffective  >  TestSets.tsCreated\n' \
                     '     AND TestBoxes.tsExpire     <= TestSets.tsCreated\n' \
                     '     AND TestBoxes.idSchedGroup';
        elif self.sSubject == self.ksSubTestGroup:
            sWhere = '     AND TestSets.idTestGroup';
        elif self.sSubject == self.ksSubTestCase:
            sWhere = '     AND TestSets.idTestCase';
        elif self.sSubject == self.ksSubTestCaseArgs:
            sWhere = '     AND TestSets.idTestCaseArgs';
        elif self.sSubject == self.ksSubTestBox:
            sWhere = '     AND TestSets.idTestBox';
        elif self.sSubject == self.ksSubBuild:
            sWhere = '     AND TestSets.idBuild';
        else:
            raise TMExceptionBase(self.sSubject);

        if len(self.aidSubjects) == 1:
            sWhere += self._oDb.formatBindArgs(' = %s\n', (self.aidSubjects[0],));
        else:
            assert len(self.aidSubjects) > 0;
            sWhere += self._oDb.formatBindArgs(' IN (%s', (self.aidSubjects[0],));
            for i in range(1, len(self.aidSubjects)):
                sWhere += self._oDb.formatBindArgs(', %s', (self.aidSubjects[i],));
            sWhere += ')\n';

        return sWhere;

    def getPeriodStart(self, iPeriod):
        """ Gets the python timestamp for the start of the given period. """
        from datetime import timedelta;
        tsNow = self.tsNow if self.tsNow is not None else self._oDb.getCurrentTimestamp();
        cHoursStart = (self.cPeriods - iPeriod    ) * self.cHoursPerPeriod;
        return tsNow - timedelta(hours = cHoursStart);

    def getPeriodEnd(self, iPeriod):
        """ Gets the python timestamp for the end of the given period. """
        from datetime import timedelta;
        tsNow = self.tsNow if self.tsNow is not None else self._oDb.getCurrentTimestamp();
        cHoursEnd   = (self.cPeriods - iPeriod - 1) * self.cHoursPerPeriod;
        return tsNow - timedelta(hours = cHoursEnd);

    def getExtraWhereExprForPeriod(self, iPeriod):
        """
        Returns additional WHERE expression for getting test sets for the
        specified period.  It starts with an AND so that it can simply be
        appended to the WHERE clause.
        """
        if self.tsNow is None:
            sNow = 'CURRENT_TIMESTAMP';
        else:
            sNow = self._oDb.formatBindArgs('%s::TIMESTAMP', (self.tsNow,));

        cHoursStart = (self.cPeriods - iPeriod    ) * self.cHoursPerPeriod;
        cHoursEnd   = (self.cPeriods - iPeriod - 1) * self.cHoursPerPeriod;
        if cHoursEnd == 0:
            return '     AND TestSets.tsDone >= (%s - interval \'%u hours\')\n' \
                   '     AND TestSets.tsDone <  %s\n' \
                 % (sNow, cHoursStart, sNow);
        return '     AND TestSets.tsDone >= (%s - interval \'%u hours\')\n' \
               '     AND TestSets.tsDone <  (%s - interval \'%u hours\')\n' \
             % (sNow, cHoursStart, sNow, cHoursEnd);

    def getPeriodDesc(self, iPeriod):
        """
        Returns the period description, usually for graph data.
        """
        if iPeriod == 0:
            return 'now';
        if self.cHoursPerPeriod == 24:
            return '%dd ago' % (iPeriod, );
        return '%dh ago' % (iPeriod * self.cHoursPerPeriod, );

    def getStraightPeriodDesc(self, iPeriod):
        """
        Returns the period description, usually for graph data.
        """
        iWickedPeriod = self.cPeriods - iPeriod - 1;
        return self.getPeriodDesc(iWickedPeriod);


class ReportFailureReasonRow(object):
    """ Simpler to return this than muck about with stupid arrays. """
    def __init__(self, aoRow, oReason):
        self.idFailureReason    = aoRow[0];
        self.cHits              = aoRow[1];
        self.tsMin              = aoRow[2];
        self.tsMax              = aoRow[3];
        self.oReason            = oReason; # FailureReasonDataEx

class ReportFailureReasonTransient(object):
    """ Details the first or last occurence of a reason.  """
    def __init__(self, idBuild, iRevision, sRepository, idTestSet, idTestResult, tsDone,  # pylint: disable=R0913
                 oReason, iPeriod, fEnter):
        self.idBuild            = idBuild;      # Build ID.
        self.iRevision          = iRevision;    # SVN revision for build.
        self.sRepository        = sRepository;  # SVN repository for build.
        self.idTestSet          = idTestSet;    # Test set.
        self.idTestResult       = idTestResult; # Test result.
        self.tsDone             = tsDone;       # When the test set was done.
        self.oReason            = oReason;      # FailureReasonDataEx
        self.iPeriod            = iPeriod;      # Data set period.
        self.fEnter             = fEnter;       # True if enter event, False if leave event.

class ReportFailureReasonPeriod(object):
    """ A period in ReportFailureReasonSet. """
    def __init__(self, oSet, iPeriod, sDesc, tsFrom, tsTo):
        self.oSet               = oSet          # Reference to the parent ReportFailureReasonSet.
        self.iPeriod            = iPeriod;
        self.sDesc              = sDesc;
        self.aoRows             = [];           # Rows in order the database returned them.
        self.dById              = {};           # Same as aoRows but indexed by idFailureReason.
        self.cHits              = 0;            # Total number of hits.
        self.dFirst             = {};           # The reasons seen for the first time (idFailureReason key).
        self.dLast              = {};           # The reasons seen for the last time (idFailureReason key).
        self.tsStart            = tsFrom;
        self.tsEnd              = tsTo;
        self.tsMin              = tsTo;
        self.tsMax              = tsFrom;
        self.cWithoutReason     = 0;            # Number of failed test sets without any assigned reason.

class ReportFailureReasonSet(object):
    """ What ReportLazyModel.getFailureReasons returns. """
    def __init__(self):
        self.aoPeriods   = [];  # Periods in ascending order (time wise).
        self.dReasons    = {};  # FailureReasonDataEx objected indexted by idFailureReason
        self.dTotals     = {};  # Totals per reason, indexed by idFailureReason.
        self.cHits       = 0;   # Total number of hits in all periods and all reasons.
        self.cMaxRowHits = 0;   # Max hits in a row.
        self.diFirst     = {};  # The period number a reason was first seen (idFailureReason key).
        self.diLast      = {};  # The period number a reason was last seen (idFailureReason key).
        self.aoEnterInfo = [];  # Array of ReportFailureReasonTransient order by iRevision. Excludes the first period.
        self.aoLeaveInfo = [];  # Array of ReportFailureReasonTransient order in descending order by iRevision.  Excludes last.



class ReportLazyModel(ReportModelBase): # pylint: disable=R0903
    """
    The 'lazy bird' report model class.

    We may want to have several classes, maybe one for each report even. But,
    I'm thinking that's a bit overkill so we'll start with this and split it
    if/when it becomes necessary.
    """


    kdsStatusSimplificationMap = {
        ReportModelBase.ksTestStatus_Running:       ReportModelBase.ksTestStatus_Running,
        ReportModelBase.ksTestStatus_Success:       ReportModelBase.ksTestStatus_Success,
        ReportModelBase.ksTestStatus_Skipped:       ReportModelBase.ksTestStatus_Skipped,
        ReportModelBase.ksTestStatus_BadTestBox:    ReportModelBase.ksTestStatus_Skipped,
        ReportModelBase.ksTestStatus_Aborted:       ReportModelBase.ksTestStatus_Skipped,
        ReportModelBase.ksTestStatus_Failure:       ReportModelBase.ksTestStatus_Failure,
        ReportModelBase.ksTestStatus_TimedOut:      ReportModelBase.ksTestStatus_Failure,
        ReportModelBase.ksTestStatus_Rebooted:      ReportModelBase.ksTestStatus_Failure,
    };

    def getSuccessRates(self):
        """
        Gets the success rates of the subject in the specified period.

        Returns an array of data per period (0 is the oldes, self.cPeriods-1 is
        the latest) where each entry is a status (TestStatus_T) dictionary with
        the number of occurences of each final status (i.e. not running).
        """

        adPeriods = [];
        for iPeriod in xrange(self.cPeriods):
            self._oDb.execute('SELECT   enmStatus, COUNT(TestSets.idTestSet)\n'
                              'FROM     TestSets' + self.getExtraSubjectTables() +'\n'
                              'WHERE    enmStatus <> \'running\'\n'
                              + self.getExtraSubjectWhereExpr()
                              + self.getExtraWhereExprForPeriod(iPeriod)
                              +
                              'GROUP BY enmStatus\n');

            dRet = \
            {
                self.ksTestStatus_Skipped: 0,
                self.ksTestStatus_Failure: 0,
                self.ksTestStatus_Success: 0,
            };

            for aoRow in self._oDb.fetchAll():
                sKey = self.kdsStatusSimplificationMap[aoRow[0]]
                if sKey in dRet:
                    dRet[sKey] += aoRow[1];
                else:
                    dRet[sKey]  = aoRow[1];

            assert len(dRet) == 3;

            adPeriods.insert(0, dRet);

        return adPeriods;


    def getFailureReasons(self):
        """
        Gets the failure reasons of the subject in the specified period.

        Returns an array of data per period (0 is the oldes, self.cPeriods-1 is
        the latest) where each entry is a dicationary using failure reason ID as
        key.  The dictionary contains a tuple where the first element is the
        number of occurences and the second is the corresponding
        FailureReasonDataEx object from the cache.

        Note that reason IDs may not be present in every period, we only return
        those with actual occurences.
        """

        # Create a temporary table
        sTsNow   = 'CURRENT_TIMESTAMP' if self.tsNow is None else self._oDb.formatBindArgs('%s::TIMESTAMP', (self.tsNow,));
        sTsFirst = '(%s - interval \'%s hours\')' \
                 % (sTsNow, self.cHoursPerPeriod * self.cPeriods,);
        self._oDb.execute('CREATE TEMPORARY TABLE TmpReasons ON COMMIT DROP AS\n'
                          'SELECT   TestResultFailures.idFailureReason AS idFailureReason,\n'
                          '         TestResultFailures.idTestResult    AS idTestResult,\n'
                          '         TestSets.idTestSet                 AS idTestSet,\n'
                          '         TestSets.tsDone                    AS tsDone,\n'
                          '         TestSets.tsCreated                 AS tsCreated,\n'
                          '         TestSets.idBuild                   AS idBuild\n'
                          'FROM     TestResultFailures,\n'
                          '         TestResults,\n'
                          '         TestSets' + self.getExtraSubjectTables() + '\n'
                          'WHERE    TestResultFailures.idTestResult = TestResults.idTestResult\n'
                          '     AND TestResultFailures.tsExpire     = \'infinity\'::TIMESTAMP\n'
                          '     AND TestResultFailures.tsEffective >= ' + sTsFirst + '\n'
                          '     AND TestResults.enmStatus          <> \'running\'\n'
                          '     AND TestResults.enmStatus          <> \'success\'\n'
                          '     AND TestResults.tsCreated          >= ' + sTsFirst + '\n'
                          '     AND TestResults.tsCreated          <  ' + sTsNow + '\n'
                          '     AND TestResults.idTestSet           = TestSets.idTestSet\n'
                          '     AND TestSets.tsDone                >= ' + sTsFirst + '\n'
                          '     AND TestSets.tsDone                <  ' + sTsNow + '\n'
                          + self.getExtraSubjectWhereExpr());
        self._oDb.execute('SELECT idFailureReason FROM TmpReasons;');


        # Retrieve the period results.
        oFailureReasonLogic = FailureReasonLogic(self._oDb);
        oSet = ReportFailureReasonSet();
        for iPeriod in xrange(self.cPeriods):
            self._oDb.execute('SELECT   idFailureReason,\n'
                              '         COUNT(idTestResult),\n'
                              '         MIN(tsDone),\n'
                              '         MAX(tsDone)\n'
                              'FROM     TmpReasons\n'
                              'WHERE    TRUE\n'
                              + self.getExtraWhereExprForPeriod(iPeriod).replace('TestSets.', '') +
                              'GROUP BY idFailureReason\n');
            aaoRows = self._oDb.fetchAll()

            oPeriod = ReportFailureReasonPeriod(oSet, iPeriod, self.getStraightPeriodDesc(iPeriod),
                                                self.getPeriodStart(iPeriod), self.getPeriodEnd(iPeriod));
            oSet.aoPeriods.append(oPeriod);
            for aoRow in aaoRows:
                oReason = oFailureReasonLogic.cachedLookup(aoRow[0]);
                oPeriodRow = ReportFailureReasonRow(aoRow, oReason);
                oPeriod.aoRows.append(oPeriodRow);
                oPeriod.dById[oPeriodRow.idFailureReason] = oPeriodRow;
                oPeriod.cHits += oPeriodRow.cHits;
                if oPeriodRow.idFailureReason in oSet.dReasons:
                    oSet.dTotals[oPeriodRow.idFailureReason]  += oPeriodRow.cHits;
                else:
                    oSet.dTotals[oPeriodRow.idFailureReason]   = oPeriodRow.cHits;
                    oSet.dReasons[oPeriodRow.idFailureReason]  = oReason;
                    oSet.diFirst[oPeriodRow.idFailureReason]   = iPeriod;
                    oPeriod.dFirst[oPeriodRow.idFailureReason] = oReason;
                if oPeriodRow.cHits > oSet.cMaxRowHits:
                    oSet.cMaxRowHits = oPeriodRow.cHits;
                if oPeriodRow.tsMin < oPeriod.tsMin:
                    oPeriod.tsMin = oPeriodRow.tsMin;
                if oPeriodRow.tsMax > oPeriod.tsMax:
                    oPeriod.tsMax = oPeriodRow.tsMax;
            oSet.cHits += oPeriod.cHits;

            # Count how many test sets we've got without any reason associated with them.
            self._oDb.execute('SELECT   COUNT(TestSets.idTestSet)\n'
                              'FROM     TestSets\n'
                              '         LEFT OUTER JOIN TestResultFailures\n'
                              '                      ON     TestSets.idTestSet             = TestResultFailures.idTestSet\n'
                              '                         AND TestResultFailures.tsEffective = \'infinity\'::TIMESTAMP\n'
                              'WHERE    TestSets.enmStatus          <> \'running\'\n'
                              '     AND TestSets.enmStatus          <> \'success\'\n'
                              + self.getExtraWhereExprForPeriod(iPeriod) +
                              '     AND TestResultFailures.idTestSet IS NULL\n');
            oPeriod.cWithoutReason = self._oDb.fetchOne()[0];


        #
        # construct the diLast and dLast bits.
        #
        for iPeriod in xrange(self.cPeriods - 1, 0, -1):
            oPeriod = oSet.aoPeriods[iPeriod];
            for oRow in oPeriod.aoRows:
                if oRow.idFailureReason not in oSet.diLast:
                    oSet.diLast[oRow.idFailureReason]   = iPeriod;
                    oPeriod.dLast[oRow.idFailureReason] = oRow.oReason;

        #
        # For reasons entering after the first period, look up the build and
        # test set it first occured with.
        #
        for iPeriod in xrange(1, self.cPeriods):
            oPeriod = oSet.aoPeriods[iPeriod];
            for oReason in oPeriod.dFirst.values():
                oSet.aoEnterInfo.append(self._getEdgeFailureReasonOccurence(oReason, iPeriod, fEnter = True));
        oSet.aoEnterInfo = sorted(oSet.aoEnterInfo, key = lambda oTrans: oTrans.iRevision);

        # Ditto for reasons leaving before the last.
        for iPeriod in xrange(self.cPeriods - 1):
            oPeriod = oSet.aoPeriods[iPeriod];
            for oReason in oPeriod.dLast.values():
                oSet.aoLeaveInfo.append(self._getEdgeFailureReasonOccurence(oReason, iPeriod, fEnter = False));
        oSet.aoLeaveInfo = sorted(oSet.aoLeaveInfo, key = lambda oTrans: oTrans.iRevision, reverse = True);

        self._oDb.execute('DROP TABLE TmpReasons\n');
        return oSet;


    def _getEdgeFailureReasonOccurence(self, oReason, iPeriod, fEnter = True):
        """
        Helper for the failure reason report that finds the oldest or newest build
        (SVN rev) and test set (start time) it occured with.

        If fEnter is set the oldest occurence is return, if fEnter clear the newest
        is is returned.

        Returns ReportFailureReasonTransient instant.

        """
        sSorting = 'ASC' if fEnter else 'DESC';
        self._oDb.execute('SELECT   TmpReasons.idTestResult,\n'
                          '         TmpReasons.idTestSet,\n'
                          '         TmpReasons.tsDone,\n'
                          '         TmpReasons.idBuild,\n'
                          '         Builds.iRevision,\n'
                          '         BuildCategories.sRepository\n'
                          'FROM     TmpReasons,\n'
                          '         Builds,\n'
                          '         BuildCategories\n'
                          'WHERE    TmpReasons.idFailureReason  = %s\n'
                          '     AND TmpReasons.idBuild          = Builds.idBuild\n'
                          '     AND Builds.tsExpire             > TmpReasons.tsCreated\n'
                          '     AND Builds.tsEffective         <= TmpReasons.tsCreated\n'
                          '     AND Builds.idBuildCategory      = BuildCategories.idBuildCategory\n'
                          'ORDER BY Builds.iRevision ' + sSorting + ',\n'
                          '         TmpReasons.tsCreated ' + sSorting + '\n'
                          'LIMIT 1\n'
                          , ( oReason.idFailureReason, ));
        aoRow = self._oDb.fetchOne();
        if aoRow is None:
            return ReportFailureReasonTransient(-1, -1, 'internal-error', -1, -1,
                                                self._oDb.getCurrentTimestamp(), oReason, iPeriod, fEnter);
        return ReportFailureReasonTransient(idBuild = aoRow[3], iRevision = aoRow[4], sRepository = aoRow[5],
                                            idTestSet = aoRow[1], idTestResult = aoRow[0], tsDone = aoRow[2],
                                            oReason = oReason, iPeriod = iPeriod, fEnter = fEnter);




class ReportGraphModel(ReportModelBase): # pylint: disable=R0903
    """
    Extended report model used when generating the more complicated graphs
    detailing results, time elapsed and values over time.
    """

    ## @name Subject ID types.
    ## These prefix the values in the aidSubjects array.  The prefix is
    ## followed by a colon and then a list of string IDs.  Following the prefix
    ## is one or more string table IDs separated by colons.  These are used to
    ## drill down the exact test result we're looking for, by matching against
    ## TestResult::idStrName (in the db).
    ## @{
    ksTypeResult  = 'result';
    ksTypeElapsed = 'elapsed';
    ## The last string table ID gives the name of the value.
    ksTypeValue   = 'value';
    ## List of types.
    kasTypes = (ksTypeResult, ksTypeElapsed, ksTypeValue);
    ## @}

    class SampleSource(object):
        """ A sample source. """
        def __init__(self, sType, aidStrTests, idStrValue):
            self.sType       = sType;
            self.aidStrTests = aidStrTests;
            self.idStrValue  = idStrValue;

        def getTestResultTables(self):
            """ Retrieves the list of TestResults tables to join with."""
            sRet = '';
            for i in range(len(self.aidStrTests)):
                sRet += '         TestResults TR%u,\n' % (i,);
            return sRet;

        def getTestResultConditions(self):
            """ Retrieves the join conditions for the TestResults tables."""
            sRet = '';
            cItems = len(self.aidStrTests);
            for i in range(cItems - 1):
                sRet += '     AND TR%u.idStrName = %u\n' \
                        '     AND TR%u.idTestResultParent = TR%u.idTestResult\n' \
                      % ( i, self.aidStrTests[cItems - i - 1], i, i + 1 );
            sRet += '     AND TR%u.idStrName = %u\n' % (cItems - 1, self.aidStrTests[0]);
            return sRet;

    class DataSeries(object):
        """ A data series. """
        def __init__(self, oCache, idBuildCategory, idTestBox, idTestCase, idTestCaseArgs, iUnit):
            _ = oCache;
            self.idBuildCategory    = idBuildCategory;
            self.oBuildCategory     = oCache.getBuildCategory(idBuildCategory);
            self.idTestBox          = idTestBox;
            self.oTestBox           = oCache.getTestBox(idTestBox);
            self.idTestCase         = idTestCase;
            self.idTestCaseArgs     = idTestCaseArgs;
            if idTestCase is not None:
                self.oTestCase      = oCache.getTestCase(idTestCase);
                self.oTestCaseArgs  = None;
            else:
                self.oTestCaseArgs  = oCache.getTestCaseArgs(idTestCaseArgs);
                self.oTestCase      = oCache.getTestCase(self.oTestCaseArgs.idTestCase);
            self.iUnit              = iUnit;
            # Six parallel arrays.
            self.aiRevisions        = []; # The X values.
            self.aiValues           = []; # The Y values.
            self.aiErrorBarBelow    = []; # The Y value minimum errorbars, relative to the Y value (positive).
            self.aiErrorBarAbove    = []; # The Y value maximum errorbars, relative to the Y value (positive).
            self.acSamples          = []; # The number of samples at this X value.
            self.aoRevInfo          = []; # VcsRevisionData objects for each revision. Empty/SQL-NULL objects if no info.

    class DataSeriesCollection(object):
        """ A collection of data series corresponding to one input sample source. """
        def __init__(self, oSampleSrc, asTests, sValue = None):
            self.sType       = oSampleSrc.sType;
            self.aidStrTests = oSampleSrc.aidStrTests;
            self.asTests     = list(asTests);
            self.idStrValue  = oSampleSrc.idStrValue;
            self.sValue      = sValue;
            self.aoSeries    = [];

        def addDataSeries(self, oDataSeries):
            """ Appends a data series to the collection. """
            self.aoSeries.append(oDataSeries);
            return oDataSeries;


    def __init__(self, oDb, tsNow, cPeriods, cHoursPerPeriod, sSubject, aidSubjects, # pylint: disable=R0913
                 aidTestBoxes, aidBuildCats, aidTestCases, fSepTestVars):
        assert(sSubject == self.ksSubEverything); # dummy
        ReportModelBase.__init__(self, oDb, tsNow, cPeriods, cHoursPerPeriod, sSubject, aidSubjects);
        self.aidTestBoxes = aidTestBoxes;
        self.aidBuildCats = aidBuildCats;
        self.aidTestCases = aidTestCases;
        self.fOnTestCase  = not fSepTestVars; # (Separates testcase variations into separate data series.)
        self.oCache       = DatabaseObjCache(self._oDb, self.tsNow, None, self.cPeriods * self.cHoursPerPeriod);


        # Quickly validate and convert the subject "IDs".
        self.aoLookups       = [];
        for sCur in self.aidSubjects:
            asParts = sCur.split(':');
            if len(asParts) < 2:
                raise TMExceptionBase('Invalid graph value "%s"' % (sCur,));

            sType = asParts[0];
            if sType not in ReportGraphModel.kasTypes:
                raise TMExceptionBase('Invalid graph value type "%s" (full: "%s")' % (sType, sCur,));

            aidStrTests = [];
            for sIdStr in asParts[1:]:
                try:    idStr = int(sIdStr);
                except: raise TMExceptionBase('Invalid graph value id "%s" (full: "%s")' % (sIdStr, sCur,));
                if idStr < 0:
                    raise TMExceptionBase('Invalid graph value id "%u" (full: "%s")' % (idStr, sCur,));
                aidStrTests.append(idStr);

            idStrValue = None;
            if sType == ReportGraphModel.ksTypeValue:
                idStrValue = aidStrTests.pop();
            self.aoLookups.append(ReportGraphModel.SampleSource(sType, aidStrTests, idStrValue));

        # done


    def getExtraWhereExprForTotalPeriod(self, sTimestampField):
        """
        Returns additional WHERE expression for getting test sets for the
        specified period.  It starts with an AND so that it can simply be
        appended to the WHERE clause.
        """
        return self.getExtraWhereExprForTotalPeriodEx(sTimestampField, sTimestampField, True);

    def getExtraWhereExprForTotalPeriodEx(self, sStartField = 'tsCreated', sEndField = 'tsDone', fLeadingAnd = True):
        """
        Returns additional WHERE expression for getting test sets for the
        specified period.
        """
        if self.tsNow is None:
            sNow = 'CURRENT_TIMESTAMP';
        else:
            sNow = self._oDb.formatBindArgs('%s::TIMESTAMP', (self.tsNow,));

        sRet = '     AND %s >= (%s - interval \'%u hours\')\n' \
               '     AND %s <=  %s\n' \
             % ( sStartField, sNow, self.cPeriods * self.cHoursPerPeriod,
                 sEndField, sNow);

        if not fLeadingAnd:
            assert sRet[8] == ' ' and sRet[7] == 'D';
            return sRet[9:];
        return sRet;

    def _getEligibleTestSetPeriod(self, sPrefix = 'TestSets.', fLeadingAnd = False):
        """
        Returns additional WHERE expression for getting TestSets rows
        potentially relevant for the selected period.
        """
        if self.tsNow is None:
            sNow = 'CURRENT_TIMESTAMP';
        else:
            sNow = self._oDb.formatBindArgs('%s::TIMESTAMP', (self.tsNow,));

        # The 2nd line is a performance hack on TestSets.  It nudges postgresql
        # into useing the TestSetsCreatedDoneIdx index instead of doing a table
        # scan when we look for eligible bits there.
        # ASSUMES no relevant test runs longer than 7 days!
        sRet = '     AND %stsCreated <= %s\n' \
               '     AND %stsCreated >= (%s - interval \'%u hours\' - interval \'%u days\')\n' \
               '     AND %stsDone    >= (%s - interval \'%u hours\')\n' \
             % ( sPrefix, sNow,
                 sPrefix, sNow,  self.cPeriods * self.cHoursPerPeriod, 7,
                 sPrefix, sNow, self.cPeriods * self.cHoursPerPeriod, );

        if not fLeadingAnd:
            assert sRet[8] == ' ' and sRet[7] == 'D';
            return sRet[9:];
        return sRet;


    def _getNameStrings(self, aidStrTests):
        """ Returns an array of names corresponding to the array of string table entries. """
        return [self.oCache.getTestResultString(idStr) for idStr in aidStrTests];

    def fetchGraphData(self):
        """ returns data """
        sWantedTestCaseId = 'idTestCase' if self.fOnTestCase else 'idTestCaseArgs';

        aoRet = [];
        for oLookup in self.aoLookups:
            #
            # Set up the result collection.
            #
            if oLookup.sType == self.ksTypeValue:
                oCollection = self.DataSeriesCollection(oLookup, self._getNameStrings(oLookup.aidStrTests),
                                                        self.oCache.getTestResultString(oLookup.idStrValue));
            else:
                oCollection = self.DataSeriesCollection(oLookup, self._getNameStrings(oLookup.aidStrTests));

            #
            # Construct the query.
            #
            sQuery  = 'SELECT   Builds.iRevision,\n' \
                      '         TestSets.idBuildCategory,\n' \
                      '         TestSets.idTestBox,\n' \
                      '         TestSets.' + sWantedTestCaseId + ',\n';
            if oLookup.sType == self.ksTypeValue:
                sQuery += '         TestResultValues.iUnit as iUnit,\n' \
                          '         MIN(TestResultValues.lValue),\n' \
                          '         CAST(ROUND(AVG(TestResultValues.lValue)) AS BIGINT),\n' \
                          '         MAX(TestResultValues.lValue),\n' \
                          '         COUNT(TestResultValues.lValue)\n';
            elif oLookup.sType == self.ksTypeElapsed:
                sQuery += '         %u as iUnit,\n' \
                          '         CAST((EXTRACT(EPOCH FROM MIN(TR0.tsElapsed)) * 1000) AS INTEGER),\n' \
                          '         CAST((EXTRACT(EPOCH FROM AVG(TR0.tsElapsed)) * 1000) AS INTEGER),\n' \
                          '         CAST((EXTRACT(EPOCH FROM MAX(TR0.tsElapsed)) * 1000) AS INTEGER),\n' \
                          '         COUNT(TR0.tsElapsed)\n' \
                        % (constants.valueunit.MS,);
            else:
                sQuery += '         %u as iUnit,\n'\
                          '         MIN(TR0.cErrors),\n' \
                          '         CAST(ROUND(AVG(TR0.cErrors)) AS INTEGER),\n' \
                          '         MAX(TR0.cErrors),\n' \
                          '         COUNT(TR0.cErrors)\n' \
                        % (constants.valueunit.OCCURRENCES,);

            if oLookup.sType == self.ksTypeValue:
                sQuery += 'FROM     TestResultValues,\n';
                sQuery += '         TestSets,\n'
                sQuery += oLookup.getTestResultTables();
            else:
                sQuery += 'FROM     ' + oLookup.getTestResultTables().lstrip();
                sQuery += '         TestSets,\n';
            sQuery += '         Builds\n';

            if oLookup.sType == self.ksTypeValue:
                sQuery += 'WHERE    TestResultValues.idStrName = %u\n' % ( oLookup.idStrValue, );
                sQuery += self.getExtraWhereExprForTotalPeriod('TestResultValues.tsCreated');
                sQuery += '     AND TestResultValues.idTestSet = TestSets.idTestSet\n';
                sQuery += self._getEligibleTestSetPeriod(fLeadingAnd = True);
            else:
                sQuery += 'WHERE    ' + (self.getExtraWhereExprForTotalPeriod('TR0.tsCreated').lstrip()[4:]).lstrip();
                sQuery += '     AND TR0.idTestSet = TestSets.idTestSet\n';

            if len(self.aidTestBoxes) == 1:
                sQuery += '     AND TestSets.idTestBox = %u\n' % (self.aidTestBoxes[0],);
            elif len(self.aidTestBoxes) > 0:
                sQuery += '     AND TestSets.idTestBox IN (' + ','.join([str(i) for i in self.aidTestBoxes]) + ')\n';

            if len(self.aidBuildCats) == 1:
                sQuery += '     AND TestSets.idBuildCategory = %u\n' % (self.aidBuildCats[0],);
            elif len(self.aidBuildCats) > 0:
                sQuery += '     AND TestSets.idBuildCategory IN (' + ','.join([str(i) for i in self.aidBuildCats]) + ')\n';

            if len(self.aidTestCases) == 1:
                sQuery += '     AND TestSets.idTestCase = %u\n' % (self.aidTestCases[0],);
            elif len(self.aidTestCases) > 0:
                sQuery += '     AND TestSets.idTestCase IN (' + ','.join([str(i) for i in self.aidTestCases]) + ')\n';

            if oLookup.sType == self.ksTypeElapsed:
                sQuery += '     AND TestSets.enmStatus = \'%s\'::TestStatus_T\n' % (self.ksTestStatus_Success,);

            if oLookup.sType == self.ksTypeValue:
                sQuery += '     AND TestResultValues.idTestResult = TR0.idTestResult\n'
                sQuery += self.getExtraWhereExprForTotalPeriod('TR0.tsCreated'); # For better index matching in some cases.

            if oLookup.sType != self.ksTypeResult:
                sQuery += '     AND TR0.enmStatus = \'%s\'::TestStatus_T\n' % (self.ksTestStatus_Success,);

            sQuery += oLookup.getTestResultConditions();
            sQuery += '     AND TestSets.idBuild = Builds.idBuild\n';

            sQuery += 'GROUP BY TestSets.idBuildCategory,\n' \
                      '         TestSets.idTestBox,\n' \
                      '         TestSets.' + sWantedTestCaseId + ',\n' \
                      '         iUnit,\n' \
                      '         Builds.iRevision\n';
            sQuery += 'ORDER BY TestSets.idBuildCategory,\n' \
                      '         TestSets.idTestBox,\n' \
                      '         TestSets.' + sWantedTestCaseId + ',\n' \
                      '         iUnit,\n' \
                      '         Builds.iRevision\n';

            #
            # Execute it and collect the result.
            #
            sCurRepository   = None;
            dRevisions       = {};
            oLastSeries      = None;
            idLastBuildCat   = -1;
            idLastTestBox    = -1;
            idLastTestCase   = -1;
            iLastUnit        = -1;
            self._oDb.execute(sQuery);
            for aoRow in self._oDb.fetchAll(): # Fetching all here so we can make cache queries below.
                if  aoRow[1] != idLastBuildCat \
                 or aoRow[2] != idLastTestBox \
                 or aoRow[3] != idLastTestCase \
                 or aoRow[4] != iLastUnit:
                    idLastBuildCat = aoRow[1];
                    idLastTestBox  = aoRow[2];
                    idLastTestCase = aoRow[3];
                    iLastUnit      = aoRow[4];
                    if self.fOnTestCase:
                        oLastSeries = self.DataSeries(self.oCache, idLastBuildCat, idLastTestBox,
                                                      idLastTestCase, None, iLastUnit);
                    else:
                        oLastSeries = self.DataSeries(self.oCache, idLastBuildCat, idLastTestBox,
                                                      None, idLastTestCase, iLastUnit);
                    oCollection.addDataSeries(oLastSeries);
                    if oLastSeries.oBuildCategory.sRepository != sCurRepository:
                        if sCurRepository is not None:
                            self.oCache.preloadVcsRevInfo(sCurRepository, dRevisions.keys());
                        sCurRepository = oLastSeries.oBuildCategory.sRepository
                        dRevisions = {};
                oLastSeries.aiRevisions.append(aoRow[0]);
                oLastSeries.aiValues.append(aoRow[6]);
                oLastSeries.aiErrorBarBelow.append(aoRow[6] - aoRow[5]);
                oLastSeries.aiErrorBarAbove.append(aoRow[7] - aoRow[6]);
                oLastSeries.acSamples.append(aoRow[8]);
                dRevisions[aoRow[0]] = 1;

            if sCurRepository is not None:
                self.oCache.preloadVcsRevInfo(sCurRepository, dRevisions.keys());
                del dRevisions;

            #
            # Look up the VCS revision details.
            #
            for oSeries in oCollection.aoSeries:
                for i in range(len(oSeries.aiRevisions)):
                    oSeries.aoRevInfo.append(self.oCache.getVcsRevInfo(sCurRepository, oSeries.aiRevisions[i]));
            aoRet.append(oCollection);

        return aoRet;

    def getEligibleTestBoxes(self):
        """
        Returns a list of TestBoxData objects with eligible testboxes for
        the total period of time defined for this graph.
        """

        # Taking the simple way out now, getting all active testboxes at the
        # time without filtering out on sample sources.

        # 1. Collect the relevant testbox generation IDs.
        self._oDb.execute('SELECT   DISTINCT idTestBox, idGenTestBox\n'
                          'FROM     TestSets\n'
                          'WHERE    ' + self._getEligibleTestSetPeriod(fLeadingAnd = False) +
                          'ORDER BY idTestBox, idGenTestBox DESC');
        idPrevTestBox    = -1;
        asIdGenTestBoxes = [];
        for _ in range(self._oDb.getRowCount()):
            aoRow = self._oDb.fetchOne();
            if aoRow[0] != idPrevTestBox:
                idPrevTestBox = aoRow[0];
                asIdGenTestBoxes.append(str(aoRow[1]));

        # 2. Query all the testbox data in one go.
        aoRet = [];
        if len(asIdGenTestBoxes) > 0:
            self._oDb.execute('SELECT   *\n'
                              'FROM     TestBoxes\n'
                              'WHERE    idGenTestBox in (' + ','.join(asIdGenTestBoxes) + ')\n'
                              'ORDER BY sName');
            for _ in range(self._oDb.getRowCount()):
                aoRet.append(TestBoxData().initFromDbRow(self._oDb.fetchOne()));

        return aoRet;

    def getEligibleBuildCategories(self):
        """
        Returns a list of BuildCategoryData objects with eligible build
        categories for the total period of time defined for this graph.  In
        addition it will add any currently selected categories that aren't
        really relevant to the period, just to simplify the WUI code.

        """

        # Taking the simple way out now, getting all used build cat without
        # any testbox or testcase filtering.

        sSelectedBuildCats = '';
        if len(self.aidBuildCats) > 0:
            sSelectedBuildCats = '   OR idBuildCategory IN (' + ','.join([str(i) for i in self.aidBuildCats]) + ')\n';

        self._oDb.execute('SELECT   DISTINCT *\n'
                          'FROM     BuildCategories\n'
                          'WHERE    idBuildCategory IN (\n'
                          '   SELECT DISTINCT idBuildCategory\n'
                          '   FROM  TestSets\n'
                          '   WHERE ' + self._getEligibleTestSetPeriod(fLeadingAnd = False) +
                          ')\n'
                          + sSelectedBuildCats +
                          'ORDER BY sProduct,\n'
                          '         sBranch,\n'
                          '         asOsArches,\n'
                          '         sType\n');
        aoRet = [];
        for _ in range(self._oDb.getRowCount()):
            aoRet.append(BuildCategoryData().initFromDbRow(self._oDb.fetchOne()));

        return aoRet;

