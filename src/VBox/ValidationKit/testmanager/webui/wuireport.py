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
from testmanager.webui.wuicontentbase   import WuiContentBase;
from testmanager.webui.wuihlpgraph       import WuiHlpGraphDataTable, WuiHlpBarGraph;
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

    def generateReportBody(self):
        # Mockup.
        self._sTitle = 'Success rate';
        return '<p>Graph showing COUNT(idFailureReason) grouped by time period.</p>' \
               '<p>New reasons per period, tracked down to build revision.</p>' \
               '<p>Show graph content in table form.</p>';


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
        sHtml += oSuccessRate.show()[1];

        return sHtml;

