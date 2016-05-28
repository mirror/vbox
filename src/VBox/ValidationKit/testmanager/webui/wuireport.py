# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager WUI - Reports.
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
from common                             import webutils;
from testmanager.webui.wuicontentbase   import WuiContentBase, WuiSvnLinkWithTooltip;
from testmanager.webui.wuihlpgraph      import WuiHlpGraphDataTable, WuiHlpBarGraph;
from testmanager.core.report            import ReportModelBase;


class WuiReportBase(WuiContentBase):
    """
    Base class for the reports.
    """

    def __init__(self, oModel, dParams, fSubReport = False, fnDPrint = None, oDisp = None):
        WuiContentBase.__init__(self, fnDPrint = fnDPrint, oDisp = oDisp);
        self._oModel        = oModel;
        self._dParams       = dParams;
        self._fSubReport    = fSubReport;
        self._sTitle        = None;

    def generateNavigator(self, sWhere):
        """
        Generates the navigator (manipulate _dParams).
        Returns HTML.
        """
        assert sWhere == 'top' or sWhere == 'bottom';

        return '';

    def generateReportBody(self):
        """
        This is overridden by the child class to generate the report.
        Returns HTML.
        """
        return '<h3>Must override generateReportBody!</h3>';

    def show(self):
        """
        Generate the report.
        Returns (sTitle, HTML).
        """

        sTitle  = self._sTitle if self._sTitle is not None else type(self).__name__;
        sReport = self.generateReportBody();
        if not self._fSubReport:
            sReport = self.generateNavigator('top') + sReport + self.generateNavigator('bottom');
            sTitle = self._oModel.sSubject + ' - ' + sTitle; ## @todo add subject to title in a proper way!

        return (sTitle, sReport);


class WuiReportSuccessRate(WuiReportBase):
    """
    Generates a report displaying the success rate over time.
    """

    def generateReportBody(self):
        self._sTitle = 'Success rate';

        adPeriods = self._oModel.getSuccessRates();

        sReport = '';

        oTable = WuiHlpGraphDataTable('Period', [ 'Succeeded', 'Skipped', 'Failed' ]);

        #for i in range(len(adPeriods) - 1, -1, -1):
        for i, dStatuses in enumerate(adPeriods):
            cSuccess  = dStatuses[ReportModelBase.ksTestStatus_Success] + dStatuses[ReportModelBase.ksTestStatus_Skipped];
            cTotal    = cSuccess + dStatuses[ReportModelBase.ksTestStatus_Failure];
            sPeriod   = self._oModel.getPeriodDesc(i);
            if cTotal > 0:
                iPctSuccess = dStatuses[ReportModelBase.ksTestStatus_Success] * 100 / cTotal;
                iPctSkipped = dStatuses[ReportModelBase.ksTestStatus_Skipped] * 100 / cTotal;
                iPctFailure = dStatuses[ReportModelBase.ksTestStatus_Failure] * 100 / cTotal;
                oTable.addRow(sPeriod, [ iPctSuccess, iPctSkipped, iPctFailure ],
                              [ '%s%% (%d)' % (iPctSuccess, dStatuses[ReportModelBase.ksTestStatus_Success]),
                                '%s%% (%d)' % (iPctSkipped, dStatuses[ReportModelBase.ksTestStatus_Skipped]),
                                '%s%% (%d)' % (iPctFailure, dStatuses[ReportModelBase.ksTestStatus_Failure]), ]);
            else:
                oTable.addRow(sPeriod, [ 0, 0, 0 ], [ '0%', '0%', '0%' ]);

        cTotalNow  = adPeriods[0][ReportModelBase.ksTestStatus_Success];
        cTotalNow += adPeriods[0][ReportModelBase.ksTestStatus_Skipped];
        cSuccessNow = cTotalNow;
        cTotalNow += adPeriods[0][ReportModelBase.ksTestStatus_Failure];
        sReport += '<p>Current success rate: ';
        if cTotalNow > 0:
            sReport += '%s%% (thereof %s%% skipped)</p>\n' \
                     % ( cSuccessNow * 100 / cTotalNow, adPeriods[0][ReportModelBase.ksTestStatus_Skipped] * 100 / cTotalNow);
        else:
            sReport += 'N/A</p>\n'

        oGraph = WuiHlpBarGraph('success-rate', oTable, self._oDisp);
        oGraph.setRangeMax(100);
        sReport += oGraph.renderGraph();

        return sReport;


class WuiReportFailureReasons(WuiReportBase):
    """
    Generates a report displaying the failure reasons over time.
    """

    def _formatEdgeOccurence(self, oTransient):
        """
        Helper for formatting the transients.
        oTransient is of type ReportFailureReasonTransient.
        """
        sHtml = u'<li>';
        if oTransient.fEnter:   sHtml += 'Since ';
        else:                   sHtml += 'Till ';
        sHtml += WuiSvnLinkWithTooltip(oTransient.iRevision, oTransient.sRepository, fBracketed = 'False').toHtml();
        sHtml += u', %s: ' % (self.formatTsShort(oTransient.tsDone),);
        sHtml += u'%s / %s' % (webutils.escapeElem(oTransient.oReason.oCategory.sShort),
                               webutils.escapeElem(oTransient.oReason.sShort),);
        sHtml += u'</li>\n';

        return sHtml;


    def generateReportBody(self):
        self._sTitle = 'Failure reasons';

        sHtml = u'';

        #
        # The array of periods we get have the oldest period first [0].
        #
        oSet = self._oModel.getFailureReasons();

        #
        # List failure reasons starting or stopping to appear within the data set.
        #
        dtFirstLast = {};
        for iPeriod, oPeriod in enumerate(oSet.aoPeriods):
            for oRow in oPeriod.aoRows:
                tIt = dtFirstLast.get(oRow.idFailureReason, (iPeriod, iPeriod));
                #sHtml += u'<!-- %d: %d,%d -- %d -->\n' % (oRow.idFailureReason, tIt[0], tIt[1], iPeriod);
                dtFirstLast[oRow.idFailureReason] = (tIt[0], iPeriod);

        sHtml += '<!-- \n';
        for iPeriod, oPeriod in enumerate(oSet.aoPeriods):
            sHtml += ' iPeriod=%d tsStart=%s tsEnd=%s\n' % (iPeriod, oPeriod.tsStart, oPeriod.tsEnd,);
            sHtml += '             tsMin=%s tsMax=%s\n' % (oPeriod.tsMin, oPeriod.tsMax,);
            sHtml += '              %d / %s\n' % (oPeriod.iPeriod, oPeriod.sDesc,)
        sHtml += '-->\n';

        sHtml += u'<h4>Changes:</h4>\n' \
                 u'<ul>\n';
        if len(oSet.aoEnterInfo) == 0 and len(oSet.aoLeaveInfo) == 0:
            sHtml += u'<li> No changes</li>\n';
        else:
            for oTransient in oSet.aoEnterInfo:
                sHtml += self._formatEdgeOccurence(oTransient);
            for oTransient in oSet.aoLeaveInfo:
                sHtml += self._formatEdgeOccurence(oTransient);
        sHtml += u'</ul>\n';

        #
        # Graph.
        #
        if True: # pylint: disable=W0125
            aidSorted = sorted(oSet.dReasons, key = lambda idReason: oSet.dTotals[idReason], reverse = True);
        else:
            aidSorted = sorted(oSet.dReasons,
                               key = lambda idReason: '%s / %s' % (oSet.dReasons[idReason].oCategory.sShort,
                                                                   oSet.dReasons[idReason].sShort,));

        asNames = [];
        for idReason in aidSorted:
            oReason = oSet.dReasons[idReason];
            asNames.append('%s / %s' % (oReason.oCategory.sShort, oReason.sShort,) )
        oTable = WuiHlpGraphDataTable('Period', asNames);

        for iPeriod, oPeriod in enumerate(reversed(oSet.aoPeriods)):
            aiValues = [];
            for idReason in aidSorted:
                oRow = oPeriod.dById.get(idReason, None);
                iValue = oRow.cHits if oRow is not None else 0;
                aiValues.append(iValue);
            oTable.addRow(oPeriod.sDesc, aiValues);

        oGraph = WuiHlpBarGraph('failure-reason', oTable, self._oDisp);
        oGraph.setRangeMax(max(oSet.cMaxRowHits + 1, 3));
        sHtml += oGraph.renderGraph();

        #
        # Table form necessary?
        #
        #sHtml += u'<p>TODO: Show graph content in table form.</p>';

        return sHtml;


class WuiReportSummary(WuiReportBase):
    """
    Summary report.
    """

    def generateReportBody(self):
        self._sTitle = 'Summary';
        sHtml = '<p>This will display several reports and listings useful to get an overview of %s (id=%s).</p>' \
             % (self._oModel.sSubject, self._oModel.aidSubjects,);

        oSuccessRate = WuiReportSuccessRate(self._oModel, self._dParams, fSubReport = True,
                                            fnDPrint = self._fnDPrint, oDisp = self._oDisp);



        oFailureReasons = WuiReportFailureReasons(self._oModel, self._dParams, fSubReport = True,
                                                  fnDPrint = self._fnDPrint, oDisp = self._oDisp);
        for oReport in [oSuccessRate, oFailureReasons, ]:
            (sTitle, sContent) = oReport.show();
            sHtml += '<br>'; # drop this layout hack
            sHtml += '<div>';
            sHtml += '<h3>%s</h3>\n' % (webutils.escapeElem(sTitle),);
            sHtml += sContent;
            sHtml += '</div>';

        return sHtml;

