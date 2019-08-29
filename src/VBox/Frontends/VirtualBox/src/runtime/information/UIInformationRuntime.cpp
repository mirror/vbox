/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationRuntime class implementation.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QApplication>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QGridLayout>
#include <QScrollArea>
#include <QStyle>
#include <QXmlStreamReader>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QTimer>

/* GUI includes: */
#include "UICommon.h"
#include "UIConverter.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIInformationRuntime.h"
#include "UISession.h"

/* COM includes: */
#include "CGuest.h"
#include "CPerformanceCollector.h"
#include "CPerformanceMetric.h"
#include "CVRDEServerInfo.h"

/* External includes: */
# include <math.h>

const ULONG iPeriod = 1;
const int iMaximumQueueSize = 120;
const int iMetricSetupCount = 1;
const int iDecimalCount = 2;

enum InfoLine
{
    InfoLine_Resolution,
    InfoLine_Uptime,
    InfoLine_ClipboardMode,
    InfoLine_DnDMode,
    InfoLine_ExecutionEngine,
    InfoLine_NestedPaging,
    InfoLine_UnrestrictedExecution,
    InfoLine_Paravirtualization,
    InfoLine_GuestAdditions,
    InfoLine_GuestOSType,
    InfoLine_RemoteDesktop,
    InfoLine_Max
};


QString formatNumber(quint64 number)
{
    if (number <= 0)
        return QString();
    /* See https://en.wikipedia.org/wiki/Metric_prefix for metric suffices:*/
    char suffices[] = {'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'};
    int zeroCount = (int)log10(number);
    if (zeroCount < 3)
        return QString::number(number);
    int h = 3 * (zeroCount / 3);
    char result[128];
    sprintf(result, "%.2f", number / (float)pow(10, h));
    return QString("%1%2").arg(result).arg(suffices[h / 3 - 1]);
}

/*********************************************************************************************************************************
*   UIRuntimeInfoWidget definition.                                                                                     *
******************************************************************1***************************************************************/
class UIRuntimeInfoWidget : public QIWithRetranslateUI<QTableWidget>
{

    Q_OBJECT;

public:

    UIRuntimeInfoWidget(QWidget *pParent, const CMachine &machine, const CConsole &console);

protected:

    virtual void retranslateUi() /* override */;
    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const /* override */;

private:

    void runTimeAttributes();
    void insertInfoLine(InfoLine enmInfoLine, const QString& strLabel, const QString &strInfo);
    void computeMinimumWidth();

    CMachine m_machine;
    CConsole m_console;

    /** @name Cached translated strings.
      * @{ */
        QString m_strTableTitle;
        QString m_strScreenResolutionLabel;
        QString m_strUptimeLabel;
        QString m_strClipboardModeLabel;
        QString m_strDragAndDropLabel;
        QString m_strExcutionEngineLabel;
        QString m_strNestedPagingLabel;
        QString m_strUnrestrictedExecutionLabel;
        QString m_strParavirtualizationLabel;
        QString m_strGuestAdditionsLabel;
        QString m_strGuestOSTypeLabel;
        QString m_strRemoteDesktopLabel;
    /** @} */

    int m_iFontHeight;
    /** Computed by computing the maximum length line. Used to avoid having horizontal scroll bars. */
    int m_iMinimumWidth;
};

/*********************************************************************************************************************************
*   UIChart definition.                                                                                     *
*********************************************************************************************************************************/

class UIChart : public QIWithRetranslateUI<QWidget>
{

    Q_OBJECT;

signals:



public:

    UIChart(QWidget *pParent, UIMetric *pMetric);
    void setFontSize(int iFontSize);
    int  fontSize() const;
    void setTextList(const QStringList &textList);
    const QStringList &textList() const;

    bool withPieChart() const;
    void setWithPieChart(bool fWithPieChart);

    bool useGradientLineColor() const;
    void setUseGradientLineColor(bool fUseGradintLineColor);

    QColor dataSeriesColor(int iDataSeriesIndex);
    void setDataSeriesColor(int iDataSeriesIndex, const QColor &color);

    QString XAxisLabel();
    void setXAxisLabel(const QString &strLabel);

protected:

    virtual void paintEvent(QPaintEvent *pEvent) /* override */;
    virtual QSize minimumSizeHint() const /* override */;
    virtual QSize sizeHint() const  /* override */;
    virtual void retranslateUi()  /* override */;

private slots:

    void sltCreateContextMenu(const QPoint &point);
    void sltResetMetric();

private:

    virtual void computeFontSize();
    /** @name Drawing helper functions.
     * @{ */
       void drawXAxisLabels(QPainter &painter, int iXSubAxisCount);
       void drawPieChart(QPainter &painter, quint64 iMaximum, int iDataIndex, const QRectF &chartRect, int iAlpha);
       void drawCombinedPieCharts(QPainter &painter, quint64 iMaximum);
       void drawDoughnutChart(QPainter &painter, quint64 iMaximum, int iDataIndex,
                              const QRectF &chartRect, const QRectF &innerRect, int iAlpha);

       /** Drawing an overlay rectangle over the charts to indicate that they are disabled. */
       void drawDisabledChartRectangle(QPainter &painter);
    /** @} */

    UIMetric *m_pMetric;
    QSize m_size;
    QFont m_font;
    int m_iMarginLeft;
    int m_iMarginRight;
    int m_iMarginTop;
    int m_iMarginBottom;
    QStringList m_textList;
    QRect m_lineChartRect;
    int m_iPieChartRadius;
    int m_iPieChartSpacing;
    bool m_fWithPieChart;
    bool m_fUseGradientLineColor;
    QColor m_dataSeriesColor[DATA_SERIES_SIZE];
    QString m_strXAxisLabel;
    QString m_strGAWarning;
    QString m_strResetActionLabel;
};

/*********************************************************************************************************************************
*   UIRuntimeInfoWidget implementation.                                                                                     *
*********************************************************************************************************************************/

UIRuntimeInfoWidget::UIRuntimeInfoWidget(QWidget *pParent, const CMachine &machine, const CConsole &console)
    : QIWithRetranslateUI<QTableWidget>(pParent)
    , m_machine(machine)
    , m_console(console)
    , m_iMinimumWidth(0)

{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_iFontHeight = QFontMetrics(font()).height();

    setColumnCount(2);
    verticalHeader()->hide();
    horizontalHeader()->hide();
    setShowGrid(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setFocusPolicy(Qt::NoFocus);
    setSelectionMode(QAbstractItemView::NoSelection);

    retranslateUi();
    /* Add the title row: */
    QTableWidgetItem *pTitleItem = new QTableWidgetItem(UIIconPool::iconSet(":/state_running_16px.png"), m_strTableTitle);
    QFont titleFont(font());
    titleFont.setBold(true);
    pTitleItem->setFont(titleFont);
    insertRow(0);
    setItem(0, 0, pTitleItem);

    /* Make the API calls and populate the table: */
    runTimeAttributes();
    computeMinimumWidth();
}

void UIRuntimeInfoWidget::retranslateUi()
{
    m_strTableTitle = QApplication::translate("UIVMInformationDialog", "Runtime Attributes");
    m_strScreenResolutionLabel = QApplication::translate("UIVMInformationDialog", "Screen Resolution");
    m_strUptimeLabel = QApplication::translate("UIVMInformationDialog", "VM Uptime");
    m_strClipboardModeLabel = QApplication::translate("UIVMInformationDialog", "Clipboard Mode");
    m_strDragAndDropLabel = QApplication::translate("UIVMInformationDialog", "Drag and Drop Mode");
    m_strExcutionEngineLabel = QApplication::translate("UIVMInformationDialog", "VM Execution Engine");
    m_strNestedPagingLabel = QApplication::translate("UIVMInformationDialog", "Nested Paging");
    m_strUnrestrictedExecutionLabel = QApplication::translate("UIVMInformationDialog", "Unrestricted Execution");
    m_strParavirtualizationLabel = QApplication::translate("UIVMInformationDialog", "Paravirtualization Interface");
    m_strGuestAdditionsLabel = QApplication::translate("UIVMInformationDialog", "Guest Additions");
    m_strGuestOSTypeLabel = QApplication::translate("UIVMInformationDialog", "Guest OS Type");
    m_strRemoteDesktopLabel = QApplication::translate("UIVMInformationDialog", "Remote Desktop Server Port");
}

QSize UIRuntimeInfoWidget::sizeHint() const
{
    return QSize(m_iMinimumWidth, m_iMinimumWidth);
}

QSize UIRuntimeInfoWidget::minimumSizeHint() const
{
    return QSize(m_iMinimumWidth, m_iMinimumWidth);
}

void UIRuntimeInfoWidget::insertInfoLine(InfoLine enmInfoLine, const QString& strLabel, const QString &strInfo)
{
    int iMargin = 0.2 * qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    int iRow = rowCount();
    insertRow(iRow);
    setItem(iRow, 0, new QTableWidgetItem(strLabel, enmInfoLine));
    setItem(iRow, 1, new QTableWidgetItem(strInfo, enmInfoLine));
    setRowHeight(iRow, 2 * iMargin + m_iFontHeight);
}

void UIRuntimeInfoWidget::runTimeAttributes()
{
    ULONG cGuestScreens = m_machine.GetMonitorCount();
    QVector<QString> aResolutions(cGuestScreens);
    for (ULONG iScreen = 0; iScreen < cGuestScreens; ++iScreen)
    {
        /* Determine resolution: */
        ULONG uWidth = 0;
        ULONG uHeight = 0;
        ULONG uBpp = 0;
        LONG xOrigin = 0;
        LONG yOrigin = 0;
        KGuestMonitorStatus monitorStatus = KGuestMonitorStatus_Enabled;
        m_console.GetDisplay().GetScreenResolution(iScreen, uWidth, uHeight, uBpp, xOrigin, yOrigin, monitorStatus);
        QString strResolution = QString("%1x%2").arg(uWidth).arg(uHeight);
        if (uBpp)
            strResolution += QString("x%1").arg(uBpp);
        strResolution += QString(" @%1,%2").arg(xOrigin).arg(yOrigin);
        if (monitorStatus == KGuestMonitorStatus_Disabled)
        {
            strResolution += QString(" ");
            strResolution += QString(QApplication::translate("UIVMInformationDialog", "turned off"));
        }
        aResolutions[iScreen] = strResolution;
    }

    /* Determine uptime: */
    CMachineDebugger debugger = m_console.GetDebugger();
    uint32_t uUpSecs = (debugger.GetUptime() / 5000) * 5;
    char szUptime[32];
    uint32_t uUpDays = uUpSecs / (60 * 60 * 24);
    uUpSecs -= uUpDays * 60 * 60 * 24;
    uint32_t uUpHours = uUpSecs / (60 * 60);
    uUpSecs -= uUpHours * 60 * 60;
    uint32_t uUpMins  = uUpSecs / 60;
    uUpSecs -= uUpMins * 60;
    RTStrPrintf(szUptime, sizeof(szUptime), "%dd %02d:%02d:%02d",
                uUpDays, uUpHours, uUpMins, uUpSecs);
    QString strUptime = QString(szUptime);

    /* Determine clipboard mode: */
    QString strClipboardMode = gpConverter->toString(m_machine.GetClipboardMode());
    /* Determine Drag&Drop mode: */
    QString strDnDMode = gpConverter->toString(m_machine.GetDnDMode());

    /* Determine virtualization attributes: */
    QString strVirtualization = debugger.GetHWVirtExEnabled() ?
        QApplication::translate("UIVMInformationDialog", "Active") :
        QApplication::translate("UIVMInformationDialog", "Inactive");

    QString strExecutionEngine;
    switch (debugger.GetExecutionEngine())
    {
        case KVMExecutionEngine_HwVirt:
            strExecutionEngine = "VT-x/AMD-V";  /* no translation */
            break;
        case KVMExecutionEngine_RawMode:
            strExecutionEngine = "raw-mode";    /* no translation */
            break;
        case KVMExecutionEngine_NativeApi:
            strExecutionEngine = "native API";  /* no translation */
            break;
        default:
            AssertFailed();
            RT_FALL_THRU();
        case KVMExecutionEngine_NotSet:
            strExecutionEngine = QApplication::translate("UIVMInformationDialog", "not set");
            break;
    }
    QString strNestedPaging = debugger.GetHWVirtExNestedPagingEnabled() ?
        QApplication::translate("UIVMInformationDialog", "Active"):
        QApplication::translate("UIVMInformationDialog", "Inactive");

    QString strUnrestrictedExecution = debugger.GetHWVirtExUXEnabled() ?
        QApplication::translate("UIVMInformationDialog", "Active"):
        QApplication::translate("UIVMInformationDialog", "Inactive");

        QString strParavirtProvider = gpConverter->toString(m_machine.GetEffectiveParavirtProvider());

    /* Guest information: */
    CGuest guest = m_console.GetGuest();
    QString strGAVersion = guest.GetAdditionsVersion();
    if (strGAVersion.isEmpty())
        strGAVersion = tr("Not Detected", "guest additions");
    else
    {
        ULONG uRevision = guest.GetAdditionsRevision();
        if (uRevision != 0)
            strGAVersion += QString(" r%1").arg(uRevision);
    }
    QString strOSType = guest.GetOSTypeId();
    if (strOSType.isEmpty())
        strOSType = tr("Not Detected", "guest os type");
    else
        strOSType = uiCommon().vmGuestOSTypeDescription(strOSType);

    /* VRDE information: */
    int iVRDEPort = m_console.GetVRDEServerInfo().GetPort();
    QString strVRDEInfo = (iVRDEPort == 0 || iVRDEPort == -1)?
        tr("Not Available", "details report (VRDE server port)") :
        QString("%1").arg(iVRDEPort);

    /* Searching for longest string: */
    QStringList values;
    for (ULONG iScreen = 0; iScreen < cGuestScreens; ++iScreen)
        values << aResolutions[iScreen];
    values << strUptime
           << strExecutionEngine << strNestedPaging << strUnrestrictedExecution
           << strGAVersion << strOSType << strVRDEInfo;
    int iMaxLength = 0;
    foreach (const QString &strValue, values)
        iMaxLength = iMaxLength < QApplication::fontMetrics().width(strValue)
                                  ? QApplication::fontMetrics().width(strValue) : iMaxLength;

    /* Summary: */
    for (ULONG iScreen = 0; iScreen < cGuestScreens; ++iScreen)
    {
        QString strLabel = cGuestScreens > 1 ?
            QString("%1 %2:").arg(m_strScreenResolutionLabel).arg(QString::number(iScreen)) :
            QString("%1:").arg(m_strScreenResolutionLabel);
        insertInfoLine(InfoLine_Resolution, strLabel, aResolutions[iScreen]);
    }

    insertInfoLine(InfoLine_Uptime, QString("%1:").arg(m_strUptimeLabel), strUptime);
    insertInfoLine(InfoLine_ClipboardMode, QString("%1:").arg(m_strClipboardModeLabel), strClipboardMode);
    insertInfoLine(InfoLine_DnDMode, QString("%1:").arg(m_strDragAndDropLabel), strDnDMode);
    insertInfoLine(InfoLine_ExecutionEngine, QString("%1:").arg(m_strExcutionEngineLabel), strExecutionEngine);
    insertInfoLine(InfoLine_NestedPaging, QString("%1:").arg(m_strNestedPagingLabel), strNestedPaging);
    insertInfoLine(InfoLine_UnrestrictedExecution, QString("%1:").arg(m_strUnrestrictedExecutionLabel), strUnrestrictedExecution);
    insertInfoLine(InfoLine_Paravirtualization, QString("%1:").arg(m_strParavirtualizationLabel), strParavirtProvider);
    insertInfoLine(InfoLine_GuestAdditions, QString("%1:").arg(m_strGuestAdditionsLabel), strGAVersion);
    insertInfoLine(InfoLine_GuestOSType, QString("%1:").arg(m_strGuestOSTypeLabel), strOSType);
    insertInfoLine(InfoLine_RemoteDesktop, QString("%1:").arg(m_strRemoteDesktopLabel), strVRDEInfo);

    resizeColumnToContents(0);
    resizeColumnToContents(1);
}

void UIRuntimeInfoWidget::computeMinimumWidth()
{
    m_iMinimumWidth = 0;
    for (int j = 0; j < columnCount(); ++j)
        m_iMinimumWidth += columnWidth(j);
}


/*********************************************************************************************************************************
*   UIChart implementation.                                                                                     *
*********************************************************************************************************************************/

UIChart::UIChart(QWidget *pParent, UIMetric *pMetric)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pMetric(pMetric)
    , m_size(QSize(50, 50))
    , m_fWithPieChart(false)
    , m_fUseGradientLineColor(false)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &UIChart::customContextMenuRequested,
            this, &UIChart::sltCreateContextMenu);

    m_dataSeriesColor[0] = QColor(Qt::red);
    m_dataSeriesColor[1] = QColor(Qt::blue);

    m_iMarginLeft = 1 * qApp->QApplication::style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    m_iMarginRight = 6 * qApp->QApplication::style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    m_iMarginTop = 0.3 * qApp->QApplication::style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    m_iMarginBottom = 2 * qApp->QApplication::style()->pixelMetric(QStyle::PM_LayoutTopMargin);

    float fAppIconSize = qApp->style()->pixelMetric(QStyle::PM_LargeIconSize);
    m_size = QSize(14 * fAppIconSize,  4 * fAppIconSize);
    m_iPieChartSpacing = 2;
    m_iPieChartRadius = m_size.height() - (m_iMarginTop + m_iMarginBottom + 2 * m_iPieChartSpacing);

    retranslateUi();
}

void UIChart::setFontSize(int iFontSize)
{
    m_font.setPixelSize(iFontSize);
}

int UIChart::fontSize() const
{
    return m_font.pixelSize();
}

void UIChart::setTextList(const QStringList &textList)
{
    m_textList = textList;
    computeFontSize();
}

const QStringList &UIChart::textList() const
{
    return m_textList;
}

bool UIChart::withPieChart() const
{
    return m_fWithPieChart;
}

void UIChart::setWithPieChart(bool fWithPieChart)
{
    if (m_fWithPieChart == fWithPieChart)
        return;
    m_fWithPieChart = fWithPieChart;
    update();
}

bool UIChart::useGradientLineColor() const
{
    return m_fUseGradientLineColor;
}

void UIChart::setUseGradientLineColor(bool fUseGradintLineColor)
{
    if (m_fUseGradientLineColor == fUseGradintLineColor)
        return;
    m_fUseGradientLineColor = fUseGradintLineColor;
    update();
}


QColor UIChart::dataSeriesColor(int iDataSeriesIndex)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return QColor();
    return m_dataSeriesColor[iDataSeriesIndex];
}

void UIChart::setDataSeriesColor(int iDataSeriesIndex, const QColor &color)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return;
    if (m_dataSeriesColor[iDataSeriesIndex] == color)
        return;
    m_dataSeriesColor[iDataSeriesIndex] = color;
    update();
}

QString UIChart::XAxisLabel()
{
    return m_strXAxisLabel;
}

void UIChart::setXAxisLabel(const QString &strLabel)
{
    m_strXAxisLabel = strLabel;
}

QSize UIChart::minimumSizeHint() const
{
    return m_size;
}

QSize UIChart::sizeHint() const
{
    return m_size;
}

void UIChart::retranslateUi()
{
    m_strGAWarning = QApplication::translate("UIVMInformationDialog", "No guest additions! This metric requires guest additions to work properly.");
    m_strResetActionLabel = QApplication::translate("UIVMInformationDialog", "Reset");
}

void UIChart::computeFontSize()
{
    int iFontSize = 24;

    //const QString &strText = m_pMetric->name();
    foreach (const QString &strText, m_textList)
    {
        m_font.setPixelSize(iFontSize);

        do{
            int iWidth = QFontMetrics(m_font).width(strText);
            if (iWidth + m_iMarginLeft + m_iMarginRight > m_size.width())
                --iFontSize;
            else
                break;
            m_font.setPixelSize(iFontSize);
        }while(iFontSize > 1);
    }
}

void UIChart::paintEvent(QPaintEvent *pEvent)
{
    Q_UNUSED(pEvent);
    if (!m_pMetric || iMaximumQueueSize <= 1)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    /* Draw a rectanglar grid over which we will draw the line graphs: */
    QPoint chartTopLeft(m_iMarginLeft, m_iMarginTop);
    QSize chartSize(width() - (m_iMarginLeft + m_iMarginRight), height() - (m_iMarginTop + m_iMarginBottom));

    m_lineChartRect = QRect(chartTopLeft, chartSize);
    QColor mainAxisColor(120, 120, 120);
    QColor subAxisColor(200, 200, 200);
    /* Draw the main axes: */
    painter.setPen(mainAxisColor);
    painter.drawRect(m_lineChartRect);

    /* draw Y subaxes: */
    painter.setPen(subAxisColor);
    int iYSubAxisCount = 3;
    for (int i = 0; i < iYSubAxisCount; ++i)
    {
        float fSubAxisY = m_iMarginTop + (i + 1) * m_lineChartRect.height() / (float) (iYSubAxisCount + 1);
        painter.drawLine(m_lineChartRect.left(), fSubAxisY,
                         m_lineChartRect.right(), fSubAxisY);
    }

    /* draw X subaxes: */
    int iXSubAxisCount = 5;
    for (int i = 0; i < iXSubAxisCount; ++i)
    {
        float fSubAxisX = m_lineChartRect.left() + (i + 1) * m_lineChartRect.width() / (float) (iXSubAxisCount + 1);
        painter.drawLine(fSubAxisX, m_lineChartRect.top(), fSubAxisX, m_lineChartRect.bottom());
    }

    /* Draw XAxis tick labels: */
    painter.setPen(mainAxisColor);
    drawXAxisLabels(painter, iXSubAxisCount);


    /* Draw a half-transparent rectangle over the whole widget to indicate the it is disabled: */
    if (!isEnabled())
    {
        drawDisabledChartRectangle(painter);
        return;
    }

    quint64 iMaximum = m_pMetric->maximum();
    if (iMaximum == 0)
        return;
    /* Draw the data lines: */
    float fBarWidth = m_lineChartRect.width() / (float) (iMaximumQueueSize - 1);
    float fH = m_lineChartRect.height() / (float)iMaximum;
    for (int k = 0; k < DATA_SERIES_SIZE; ++k)
    {
        if (m_fUseGradientLineColor)
        {
            QLinearGradient gradient(0, 0, 0, m_lineChartRect.height());
            gradient.setColorAt(0, Qt::black);
            gradient.setColorAt(1, m_dataSeriesColor[k]);
            painter.setPen(QPen(gradient, 2.5));
        }

        const QQueue<quint64> *data = m_pMetric->data(k);
        if (!m_fUseGradientLineColor)
            painter.setPen(QPen(m_dataSeriesColor[k], 2.5));
        for (int i = 0; i < data->size() - 1; ++i)
        {
            int j = i + 1;
            float fHeight = fH * data->at(i);
            float fX = (width() - m_iMarginRight) - ((data->size() -i - 1) * fBarWidth);
            float fHeight2 = fH * data->at(j);
            float fX2 = (width() - m_iMarginRight) - ((data->size() -j - 1) * fBarWidth);
            QLineF bar(fX, height() - (fHeight + m_iMarginBottom), fX2, height() - (fHeight2 + m_iMarginBottom));
            painter.drawLine(bar);
        }
    }

    QFontMetrics fontMetrics(painter.font());
    int iFontHeight = fontMetrics.height();

    /* Draw YAxis tick labels: */
    painter.setPen(mainAxisColor);
    for (int i = 0; i < iYSubAxisCount + 2; ++i)
    {
        int iTextY = 0.5 * iFontHeight + m_iMarginTop + i * m_lineChartRect.height() / (float) (iYSubAxisCount + 1);
        QString strValue;
        quint64 iValue = (iYSubAxisCount + 1 - i) * (iMaximum / (float) (iYSubAxisCount + 1));
        if (m_pMetric->unit().compare("%", Qt::CaseInsensitive) == 0)
            strValue = QString::number(iValue);
        else if (m_pMetric->unit().compare("kb", Qt::CaseInsensitive) == 0)
            strValue = uiCommon().formatSize(_1K * (quint64)iValue, iDecimalCount);
        else if (m_pMetric->unit().compare("b", Qt::CaseInsensitive) == 0 ||
                 m_pMetric->unit().compare("b/s", Qt::CaseInsensitive) == 0)
            strValue = uiCommon().formatSize(iValue, iDecimalCount);
        else if (m_pMetric->unit().compare("times", Qt::CaseInsensitive) == 0)
            strValue = formatNumber(iValue);

        painter.drawText(width() - 0.9 * m_iMarginRight, iTextY, strValue);
    }

    if (m_fWithPieChart)
        drawCombinedPieCharts(painter, iMaximum);
}

void UIChart::drawXAxisLabels(QPainter &painter, int iXSubAxisCount)
{
    QFontMetrics fontMetrics(painter.font());
    int iFontHeight = fontMetrics.height();

    int iTotalSeconds = iPeriod * iMaximumQueueSize;
    for (int i = 0; i < iXSubAxisCount + 2; ++i)
    {
        int iTextX = m_lineChartRect.left() + i * m_lineChartRect.width() / (float) (iXSubAxisCount + 1);
        QString strCurrentSec = QString::number(iTotalSeconds - i * iTotalSeconds / (float)(iXSubAxisCount + 1));
        int iTextWidth = fontMetrics.width(strCurrentSec);
        if (i == 0)
        {
            strCurrentSec += " " + m_strXAxisLabel;
            painter.drawText(iTextX, m_lineChartRect.bottom() + iFontHeight, strCurrentSec);
        }
        else
            painter.drawText(iTextX - 0.5 * iTextWidth, m_lineChartRect.bottom() + iFontHeight, strCurrentSec);
    }
}

void UIChart::drawPieChart(QPainter &painter, quint64 iMaximum, int iDataIndex, const QRectF &chartRect, int iAlpha)
{
    if (!m_pMetric)
        return;
    /* First draw a doughnut shaped chart for the 1st data series */
    const QQueue<quint64> *data = m_pMetric->data(iDataIndex);
    if (!data || data->isEmpty())
        return;

    /* Draw a whole non-filled circle: */
    painter.setPen(QPen(QColor(100, 100, 100, iAlpha), 1));
    painter.drawArc(chartRect, 0, 3600 * 16);
    painter.setPen(Qt::NoPen);
    /* Prepare the gradient for the pie chart: */
    QConicalGradient pieGradient;
    pieGradient.setCenter(chartRect.center());
    pieGradient.setAngle(90);
    pieGradient.setColorAt(0, QColor(0, 0, 0, iAlpha));
    QColor pieColor(m_dataSeriesColor[iDataIndex]);
    pieColor.setAlpha(iAlpha);
    pieGradient.setColorAt(1, pieColor);

    float fAngle = 360.f * data->back() / (float)iMaximum;

    QPainterPath dataPath;
    dataPath.moveTo(chartRect.center());
    dataPath.arcTo(chartRect, 90.f/*startAngle*/,
                   -1.f * fAngle /*sweepLength*/);
    painter.setBrush(pieGradient);
    painter.drawPath(dataPath);
}

void UIChart::drawDoughnutChart(QPainter &painter, quint64 iMaximum, int iDataIndex,
                                const QRectF &chartRect, const QRectF &innerRect, int iAlpha)
{
    const QQueue<quint64> *data = m_pMetric->data(iDataIndex);
    if (!data || data->isEmpty())
        return;

    /* Draw a whole non-filled circle: */
    painter.setPen(QPen(QColor(100, 100, 100, iAlpha), 1));
    painter.drawArc(chartRect, 0, 3600 * 16);
    painter.setPen(Qt::NoPen);

    /* First draw a white filled circle and that the arc for data: */
    QPointF center(chartRect.center());
    QPainterPath fillPath;
    fillPath.moveTo(center);
    fillPath.arcTo(chartRect, 90/*startAngle*/,
                   -1 * 360 /*sweepLength*/);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, iAlpha));
    painter.drawPath(fillPath);

    /* Prepare the gradient for the pie chart: */
    QConicalGradient pieGradient;
    pieGradient.setCenter(chartRect.center());
    pieGradient.setAngle(90);
    pieGradient.setColorAt(0, QColor(0, 0, 0, iAlpha));
    QColor pieColor(m_dataSeriesColor[iDataIndex]);
    pieColor.setAlpha(iAlpha);
    pieGradient.setColorAt(1, pieColor);


    float fAngle = 360.f * data->back() / (float)iMaximum;

    QPainterPath subPath1;
    subPath1.moveTo(chartRect.center());
    subPath1.arcTo(chartRect, 90.f/*startAngle*/,
                   -1.f * fAngle /*sweepLength*/);
    subPath1.closeSubpath();

    QPainterPath subPath2;
    subPath2.moveTo(innerRect.center());
    subPath2.arcTo(innerRect, 90.f/*startAngle*/,
                   -1.f * fAngle /*sweepLength*/);
    subPath2.closeSubpath();

    QPainterPath dataPath = subPath1.subtracted(subPath2);

    painter.setBrush(pieGradient);
    painter.drawPath(dataPath);

}

void UIChart::drawCombinedPieCharts(QPainter &painter, quint64 iMaximum)
{
    if (!m_pMetric)
        return;

    QRectF chartRect(QPointF(m_iPieChartSpacing + m_iMarginLeft, m_iPieChartSpacing + m_iMarginTop),
                     QSizeF(m_iPieChartRadius, m_iPieChartRadius));

    int iAlpha = 80;

    /* First draw a doughnut shaped chart for the 1st data series */
    // int iDataIndex = 0;
    // const QQueue<quint64> *data = m_pMetric->data(iDataIndex);
    // if (!data || data->isEmpty())
    //     return;
    bool fData0 = m_pMetric->data(0) && !m_pMetric->data(0)->isEmpty();
    bool fData1 = m_pMetric->data(0) && !m_pMetric->data(1)->isEmpty();

    if (fData0 && fData1)
    {
        QRectF innerRect(QPointF(chartRect.left() + 0.25 * chartRect.width(), chartRect.top() + 0.25 * chartRect.height()),
                         QSizeF(0.5 * chartRect.width(), 0.5 * chartRect.height()));

        /* Draw a doughnut shaped chart and then pie chart inside it: */
        drawDoughnutChart(painter, iMaximum, 0 /* iDataIndex */, chartRect, innerRect, iAlpha);
        drawPieChart(painter, iMaximum, 1 /* iDataIndex */, innerRect, iAlpha);

    }
    else if (fData0 && !fData1)
        drawPieChart(painter, iMaximum, 0 /* iDataIndex */, chartRect, iAlpha);
    else if (!fData0 && fData1)
        drawPieChart(painter, iMaximum, 1 /* iDataIndex */, chartRect, iAlpha);
}

void UIChart::drawDisabledChartRectangle(QPainter &painter)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(60, 60, 60, 80));
    painter.drawRect(QRect(0, 0, width(), height()));
    painter.setPen(QColor(20, 20, 20, 180));
    QFont font = painter.font();
    font.setBold(true);
    /** @todo make this size dynamic. aka. autoscale the font. */
    font.setPixelSize(16);
    painter.setFont(font);
    painter.drawText(2 * m_iMarginLeft, 15 * m_iMarginTop, m_strGAWarning);
}


void UIChart::sltCreateContextMenu(const QPoint &point)
{
    QMenu menu;
    QAction *pResetAction = menu.addAction(m_strResetActionLabel);
    connect(pResetAction, &QAction::triggered, this, &UIChart::sltResetMetric);
    menu.exec(mapToGlobal(point));
}

void UIChart::sltResetMetric()
{
    if (m_pMetric)
        m_pMetric->reset();
}

/*********************************************************************************************************************************
*   UIMetric implementation.                                                                                     *
*********************************************************************************************************************************/

UIMetric::UIMetric(const QString &strName, const QString &strUnit, int iMaximumQueueSize)
    : m_strName(strName)
    , m_strUnit(strUnit)
    , m_iMaximum(0)
    , m_iMaximumQueueSize(iMaximumQueueSize)
    , m_fRequiresGuestAdditions(false)
    , m_fIsInitialized(false)
{
    m_iTotal[0] = 0;
    m_iTotal[1] = 0;
}

UIMetric::UIMetric()
    : m_iMaximumQueueSize(0)
{
}

const QString &UIMetric::name() const
{
    return m_strName;
}

void UIMetric::setMaximum(quint64 iMaximum)
{
    m_iMaximum = iMaximum;
}

quint64 UIMetric::maximum() const
{
    return m_iMaximum;
}

void UIMetric::setUnit(QString strUnit)
{
    m_strUnit = strUnit;

}
const QString &UIMetric::unit() const
{
    return m_strUnit;
}

void UIMetric::addData(int iDataSeriesIndex, quint64 fData)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return;
    m_data[iDataSeriesIndex].enqueue(fData);
    if (m_data[iDataSeriesIndex].size() > iMaximumQueueSize)
        m_data[iDataSeriesIndex].dequeue();
}

const QQueue<quint64> *UIMetric::data(int iDataSeriesIndex) const
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return 0;
    return &m_data[iDataSeriesIndex];
}

void UIMetric::setTotal(int iDataSeriesIndex, quint64 iTotal)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return;
    m_iTotal[iDataSeriesIndex] = iTotal;
}

quint64 UIMetric::total(int iDataSeriesIndex) const
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return 0;
    return m_iTotal[iDataSeriesIndex];
}

bool UIMetric::requiresGuestAdditions() const
{
    return m_fRequiresGuestAdditions;
}

void UIMetric::setRequiresGuestAdditions(bool fRequiresGAs)
{
    m_fRequiresGuestAdditions = fRequiresGAs;
}

const QStringList &UIMetric::deviceTypeList() const
{
    return m_deviceTypeList;
}

void UIMetric::setDeviceTypeList(const QStringList &list)
{
    m_deviceTypeList = list;
    composeQueryString();
}

const QStringList &UIMetric::metricDataSubString() const
{
    return m_metricDataSubString;
}

void UIMetric::setQueryPrefix(const QString &strPrefix)
{
    m_strQueryPrefix = strPrefix;
    composeQueryString();

}

void UIMetric::setMetricDataSubString(const QStringList &list)
{
    m_metricDataSubString = list;
    composeQueryString();
}

const QString &UIMetric::queryString() const
{
    return m_strQueryString;
}

bool UIMetric::isInitialized() const
{
    return m_fIsInitialized;
}

void UIMetric::setIsInitialized(bool fIsInitialized)
{
    m_fIsInitialized = fIsInitialized;
}

void UIMetric::reset()
{
    m_fIsInitialized = false;
    for (int i = 0; i < DATA_SERIES_SIZE; ++i)
    {
        m_iTotal[i] = 0;
        m_data[i].clear();
    }
    m_iMaximum = 0;
}

void UIMetric::composeQueryString()
{
    /* Compose of if both m_metricDataSubString and m_deviceTypeList are not empty: */
    if (m_deviceTypeList.isEmpty() || m_metricDataSubString.isEmpty())
        return;
    m_strQueryString.clear();
    foreach (const QString &strDeviceName, m_deviceTypeList)
    {
        foreach (const QString &strSubString, m_metricDataSubString)
        {
            m_strQueryString += QString("*%1*%2*%3*|").arg(m_strQueryPrefix).arg(strDeviceName).arg(strSubString);
        }
    }
}


/*********************************************************************************************************************************
*   UIInformationRuntime implementation.                                                                                     *
*********************************************************************************************************************************/

UIInformationRuntime::UIInformationRuntime(QWidget *pParent, const CMachine &machine, const CConsole &console, const UISession *pSession)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fGuestAdditionsAvailable(false)
    , m_machine(machine)
    , m_console(console)
    , m_pMainLayout(0)
    , m_pRuntimeInfoWidget(0)
    , m_pTimer(0)
    , m_strCPUMetricName("CPU Load")
    , m_strRAMMetricName("RAM Usage")
    , m_strDiskMetricName("Disk Usage")
    , m_strNetworkMetricName("Network")
    , m_strDiskIOMetricName("DiskIO")
    , m_strVMExitMetricName("VMExits")
    , m_iTimeStep(0)
{
    if (!m_console.isNull())
        m_comGuest = m_console.GetGuest();
    m_fGuestAdditionsAvailable = guestAdditionsAvailable(6 /* minimum major version */);

    connect(pSession, &UISession::sigAdditionsStateChange, this, &UIInformationRuntime::sltGuestAdditionsStateChange);
    prepareMetrics();
    prepareObjects();
    enableDisableGuestAdditionDependedWidgets(m_fGuestAdditionsAvailable);
    retranslateUi();
}

UIInformationRuntime::~UIInformationRuntime()
{
}

void UIInformationRuntime::retranslateUi()
{
    foreach (UIChart *pChart, m_charts)
        pChart->setXAxisLabel(QApplication::translate("UIVMInformationDialog", "Seconds"));

    /* Translate the chart info labels: */
    int iMaximum = 0;
    m_strCPUInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "CPU Load");
    iMaximum = qMax(iMaximum, m_strCPUInfoLabelTitle.length());
    m_strCPUInfoLabelGuest = QApplication::translate("UIVMInformationDialog", "Guest Load");
    iMaximum = qMax(iMaximum, m_strCPUInfoLabelGuest.length());
    m_strCPUInfoLabelVMM = QApplication::translate("UIVMInformationDialog", "VMM Load");
    iMaximum = qMax(iMaximum, m_strCPUInfoLabelVMM.length());
    m_strRAMInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "RAM Usage");
    iMaximum = qMax(iMaximum, m_strRAMInfoLabelTitle.length());
    m_strRAMInfoLabelTotal = QApplication::translate("UIVMInformationDialog", "Total");
    iMaximum = qMax(iMaximum, m_strRAMInfoLabelTotal.length());
    m_strRAMInfoLabelFree = QApplication::translate("UIVMInformationDialog", "Free");
    iMaximum = qMax(iMaximum, m_strRAMInfoLabelFree.length());
    m_strRAMInfoLabelUsed = QApplication::translate("UIVMInformationDialog", "Used");
    iMaximum = qMax(iMaximum, m_strRAMInfoLabelUsed.length());
    m_strNetworkInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "Network Rate");
    iMaximum = qMax(iMaximum, m_strNetworkInfoLabelTitle.length());
    m_strNetworkInfoLabelReceived = QApplication::translate("UIVMInformationDialog", "Receive Rate");
    iMaximum = qMax(iMaximum, m_strNetworkInfoLabelReceived.length());
    m_strNetworkInfoLabelTransmitted = QApplication::translate("UIVMInformationDialog", "Transmit Rate");
    iMaximum = qMax(iMaximum, m_strNetworkInfoLabelTransmitted.length());
    m_strNetworkInfoLabelReceivedTotal = QApplication::translate("UIVMInformationDialog", "Total Received");
    iMaximum = qMax(iMaximum, m_strNetworkInfoLabelReceivedTotal.length());
    m_strNetworkInfoLabelTransmittedTotal = QApplication::translate("UIVMInformationDialog", "Total Transmitted");
    iMaximum = qMax(iMaximum, m_strNetworkInfoLabelReceivedTotal.length());
    m_strDiskIOInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "Disk IO Rate");
    iMaximum = qMax(iMaximum, m_strDiskIOInfoLabelTitle.length());
    m_strDiskIOInfoLabelWritten = QApplication::translate("UIVMInformationDialog", "Write Rate");
    iMaximum = qMax(iMaximum, m_strDiskIOInfoLabelWritten.length());
    m_strDiskIOInfoLabelRead = QApplication::translate("UIVMInformationDialog", "Read Rate");
    iMaximum = qMax(iMaximum, m_strDiskIOInfoLabelRead.length());
    m_strDiskIOInfoLabelWrittenTotal = QApplication::translate("UIVMInformationDialog", "Total Written");
    iMaximum = qMax(iMaximum, m_strDiskIOInfoLabelWrittenTotal.length());
    m_strDiskIOInfoLabelReadTotal = QApplication::translate("UIVMInformationDialog", "Total Read");
    iMaximum = qMax(iMaximum, m_strDiskIOInfoLabelReadTotal.length());
    m_strVMExitInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "VM Exits");
    iMaximum = qMax(iMaximum, m_strVMExitInfoLabelTitle.length());
    m_strVMExitLabelCurrent = QApplication::translate("UIVMInformationDialog", "Current");
    iMaximum = qMax(iMaximum, m_strVMExitLabelCurrent.length());
    m_strVMExitLabelTotal = QApplication::translate("UIVMInformationDialog", "Total");
    iMaximum = qMax(iMaximum, m_strVMExitLabelTotal.length());


    /* Compute the maximum label string length and set it as a fixed width to labels to prevent always changing widths: */
    /* Add m_iDecimalCount plus 3 characters for the number and 3 for unit string: */
    iMaximum += (iDecimalCount + 6);
    if (!m_infoLabels.isEmpty())
    {
        QLabel *pLabel = m_infoLabels.begin().value();
        if (pLabel)
        {
            QFontMetrics labelFontMetric(pLabel->font());
            int iWidth = iMaximum * labelFontMetric.width('X');
            foreach (QLabel *pInfoLabel, m_infoLabels)
                pInfoLabel->setFixedWidth(iWidth);
        }
    }

}

void UIInformationRuntime::prepareObjects()
{
    m_pMainLayout = new QVBoxLayout(this);
    if (!m_pMainLayout)
        return;
    m_pMainLayout->setSpacing(0);

    m_pTimer = new QTimer(this);
    if (m_pTimer)
    {
        connect(m_pTimer, &QTimer::timeout, this, &UIInformationRuntime::sltTimeout);
        m_pTimer->start(1000 * iPeriod);
    }

    QScrollArea *pScrollArea = new QScrollArea;
    m_pMainLayout->addWidget(pScrollArea);
    QWidget *pContainerWidget = new QWidget;
    QGridLayout *pContainerLayout = new QGridLayout;
    pContainerWidget->setLayout(pContainerLayout);
    pContainerLayout->setSpacing(10);
    pContainerWidget->show();
    pScrollArea->setWidget(pContainerWidget);
    pScrollArea->setWidgetResizable(true);

    QStringList chartOrder;
    chartOrder << m_strCPUMetricName << m_strRAMMetricName <<
        m_strDiskMetricName << m_strNetworkMetricName << m_strDiskIOMetricName << m_strVMExitMetricName;
    int iRow = 0;
    foreach (const QString &strMetricName, chartOrder)
    {
        QHBoxLayout *pChartLayout = new QHBoxLayout;
        pChartLayout->setSpacing(0);

        if (!m_subMetrics.contains(strMetricName))
            continue;
        QLabel *pLabel = new QLabel;
        pLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        pChartLayout->addWidget(pLabel);
        m_infoLabels.insert(strMetricName, pLabel);

        UIChart *pChart = new UIChart(this, &(m_subMetrics[strMetricName]));
        m_charts.insert(strMetricName, pChart);
        pChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        pChartLayout->addWidget(pChart);
        pContainerLayout->addLayout(pChartLayout, iRow, 0, 1, 2);
        ++iRow;
    }

    /* Configure charts: */
    if (m_charts.contains(m_strCPUMetricName) && m_charts[m_strCPUMetricName])
        m_charts[m_strCPUMetricName]->setWithPieChart(true);

    UIRuntimeInfoWidget *m_pRuntimeInfoWidget = new UIRuntimeInfoWidget(0, m_machine, m_console);
    pContainerLayout->addWidget(m_pRuntimeInfoWidget, iRow, 0, 2, 2);
    m_pRuntimeInfoWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);


    QWidget *bottomSpacerWidget = new QWidget(this);
    bottomSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    bottomSpacerWidget->setVisible(true);
    // QPalette pal = bottomSpacerWidget->palette();
    // pal.setColor(QPalette::Background, Qt::green);
    // bottomSpacerWidget->setAutoFillBackground(true);
    // bottomSpacerWidget->setPalette(pal);
    pContainerLayout->addWidget(bottomSpacerWidget, iRow, 0, 1, 2);
}

void UIInformationRuntime::sltTimeout()
{

    if (m_performanceMonitor.isNull())
        return;
    ++m_iTimeStep;
    QVector<QString> allNames;// = new ArrayList<IUnknown>();
    QVector<CUnknown> allObjects;// = new ArrayList<IUnknown>();

    QVector<QString>  aReturnNames;
    QVector<CUnknown>  aReturnObjects;
    QVector<QString>  aReturnUnits;
    QVector<ULONG>  aReturnScales;
    QVector<ULONG>  aReturnSequenceNumbers;
    QVector<ULONG>  aReturnDataIndices;
    QVector<ULONG>  aReturnDataLengths;

    QVector<LONG> returnData = m_performanceMonitor.QueryMetricsData(m_nameList,
                                                                     m_objectList,
                                                                     aReturnNames,
                                                                     aReturnObjects,
                                                                     aReturnUnits,
                                                                     aReturnScales,
                                                                     aReturnSequenceNumbers,
                                                                     aReturnDataIndices,
                                                                     aReturnDataLengths);
    quint64 iTotalRAM = 0;
    quint64 iFreeRAM = 0;

    for (int i = 0; i < aReturnNames.size(); ++i)
    {
        if (aReturnDataLengths[i] == 0)
            continue;
        /* Read the last of the return data disregarding the rest since we are caching the data in GUI side: */
        float fData = returnData[aReturnDataIndices[i] + aReturnDataLengths[i] - 1] / (float)aReturnScales[i];
        if (aReturnNames[i].contains("RAM", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
        {
            if (aReturnNames[i].contains("Total", Qt::CaseInsensitive))
                iTotalRAM = (quint64)fData;
            if (aReturnNames[i].contains("Free", Qt::CaseInsensitive))
                iFreeRAM = (quint64)fData;
        }
    }
    if (m_subMetrics.contains(m_strRAMMetricName))
        updateRAMGraphsAndMetric(iTotalRAM, iFreeRAM);

    /* Update the CPU load chart with values we get from IMachineDebugger::getCPULoad(..): */
    if (m_subMetrics.contains(m_strCPUMetricName))
    {
        ULONG aPctExecuting;
        ULONG aPctHalted;
        ULONG aPctOther;
        m_machineDebugger.GetCPULoad(0x7fffffff, aPctExecuting, aPctHalted, aPctOther);
        updateCPUGraphsAndMetric(aPctExecuting, aPctOther);
    }

    /* Collect the data from IMachineDebugger::getStats(..): */
    quint64 uNetworkTotalReceive = 0;
    quint64 uNetworkTotalTransmit = 0;

    quint64 uDiskIOTotalWritten = 0;
    quint64 uDiskIOTotalRead = 0;

   quint64 uTotalVMExits = 0;

    QVector<DebuggerMetricData> xmlData = getTotalCounterFromDegugger(m_strQueryString);
    for (QMap<QString, UIMetric>::iterator iterator =  m_subMetrics.begin();
         iterator != m_subMetrics.end(); ++iterator)
    {
        UIMetric &metric = iterator.value();
        const QStringList &deviceTypeList = metric.deviceTypeList();
        foreach (const QString &strDeviceType, deviceTypeList)
        {
            foreach (const DebuggerMetricData &data, xmlData)
            {
                if (data.m_strName.contains(strDeviceType, Qt::CaseInsensitive))
                {
                    if (metric.name() == m_strNetworkMetricName)
                    {
                        if (data.m_strName.contains("receive", Qt::CaseInsensitive))
                            uNetworkTotalReceive += data.m_counter;
                        else if (data.m_strName.contains("transmit", Qt::CaseInsensitive))
                            uNetworkTotalTransmit += data.m_counter;
                    }
                    else if (metric.name() == m_strDiskIOMetricName)
                    {
                        if (data.m_strName.contains("written", Qt::CaseInsensitive))
                            uDiskIOTotalWritten += data.m_counter;
                        else if (data.m_strName.contains("read", Qt::CaseInsensitive))
                            uDiskIOTotalRead += data.m_counter;
                    }
                    else if (metric.name() == m_strVMExitMetricName)
                    {
                        if (data.m_strName.contains("RecordedExits", Qt::CaseInsensitive))
                            uTotalVMExits += data.m_counter;
                    }
                }
            }
        }
    }
    updateNetworkGraphsAndMetric(uNetworkTotalReceive, uNetworkTotalTransmit);
    updateDiskIOGraphsAndMetric(uDiskIOTotalWritten, uDiskIOTotalRead);
    updateVMExitMetric(uTotalVMExits);
}

void UIInformationRuntime::sltGuestAdditionsStateChange()
{
    bool fGuestAdditionsAvailable = guestAdditionsAvailable(6 /* minimum major version */);
    if (m_fGuestAdditionsAvailable == fGuestAdditionsAvailable)
        return;
    m_fGuestAdditionsAvailable = fGuestAdditionsAvailable;
    enableDisableGuestAdditionDependedWidgets(m_fGuestAdditionsAvailable);
}

void UIInformationRuntime::prepareMetrics()
{
    m_performanceMonitor = uiCommon().virtualBox().GetPerformanceCollector();
    m_machineDebugger = m_console.GetDebugger();

    if (m_performanceMonitor.isNull())
        return;

    m_nameList << "Guest/RAM/Usage*";
    m_objectList = QVector<CUnknown>(m_nameList.size(), CUnknown());
    m_performanceMonitor.SetupMetrics(m_nameList, m_objectList, iPeriod, iMetricSetupCount);
    {
        QVector<CPerformanceMetric> metrics = m_performanceMonitor.GetMetrics(m_nameList, m_objectList);
        for (int i = 0; i < metrics.size(); ++i)
        {
            QString strName(metrics[i].GetMetricName());
            if (!strName.contains(':'))
            {
                if (strName.contains("RAM", Qt::CaseInsensitive) && strName.contains("Free", Qt::CaseInsensitive))
                {
                    UIMetric newMetric(m_strRAMMetricName, metrics[i].GetUnit(), iMaximumQueueSize);
                    newMetric.setRequiresGuestAdditions(true);
                    m_subMetrics.insert(m_strRAMMetricName, newMetric);
                }
            }
        }
    }

    m_subMetrics.insert(m_strCPUMetricName, UIMetric(m_strCPUMetricName, "%", iMaximumQueueSize));
    {

        /* Network metric: */
        UIMetric networkMetric(m_strNetworkMetricName, "B", iMaximumQueueSize);
        networkMetric.setQueryPrefix("Devices");
        QStringList networkDeviceList;
        networkDeviceList << "E1k" <<"VNet" << "PCNet";
        networkMetric.setDeviceTypeList(networkDeviceList);
        QStringList networkMetricDataSubStringList;
        networkMetricDataSubStringList << "ReceiveBytes" << "TransmitBytes";
        networkMetric.setMetricDataSubString(networkMetricDataSubStringList);
        m_subMetrics.insert(m_strNetworkMetricName, networkMetric);
    }

    /* Disk IO metric */
    {
        UIMetric diskIOMetric(m_strDiskIOMetricName, "B", iMaximumQueueSize);
        diskIOMetric.setQueryPrefix("Devices");
        QStringList diskTypeList;
        diskTypeList << "LSILOGICSCSI" << "BUSLOGIC"
                     << "AHCI" <<  "PIIX3IDE" << "I82078" << "LSILOGICSAS" << "MSD" << "NVME";
        diskIOMetric.setDeviceTypeList(diskTypeList);
        QStringList diskIODataSubStringList;
        diskIODataSubStringList << "WrittenBytes" << "ReadBytes";
        diskIOMetric.setMetricDataSubString(diskIODataSubStringList);
        m_subMetrics.insert(m_strDiskIOMetricName, diskIOMetric);
    }

    /* VM exits metric */
    {
        UIMetric VMExitsMetric(m_strVMExitMetricName, "times", iMaximumQueueSize);
        VMExitsMetric.setQueryPrefix("PROF");
        QStringList typeList;
        typeList << "CPU";
        VMExitsMetric.setDeviceTypeList(typeList);
        QStringList subStringList;
        subStringList << "RecordedExits";
        VMExitsMetric.setMetricDataSubString(subStringList);
        m_subMetrics.insert(m_strVMExitMetricName, VMExitsMetric);
    }

    for (QMap<QString, UIMetric>::const_iterator iterator =  m_subMetrics.begin();
         iterator != m_subMetrics.end(); ++iterator)
    {
        if (iterator.value().queryString().isEmpty())
            continue;
        m_strQueryString += iterator.value().queryString();
    }

}


bool UIInformationRuntime::guestAdditionsAvailable(int iMinimumMajorVersion)
{
    if (m_comGuest.isNull())
        return false;
    bool fGuestAdditionsStatus = m_comGuest.GetAdditionsStatus(m_comGuest.GetAdditionsRunLevel());
    if (fGuestAdditionsStatus)
    {
        QStringList versionStrings = m_comGuest.GetAdditionsVersion().split('.', QString::SkipEmptyParts);
        if (!versionStrings.isEmpty())
        {
            bool fConvert = false;
            int iMajorVersion = versionStrings[0].toInt(&fConvert);
            if (fConvert && iMajorVersion >= iMinimumMajorVersion)
                return true;
        }
    }
    return false;
}

void UIInformationRuntime::enableDisableGuestAdditionDependedWidgets(bool fEnable)
{
    for (QMap<QString, UIMetric>::const_iterator iterator =  m_subMetrics.begin();
         iterator != m_subMetrics.end(); ++iterator)
    {
        if (!iterator.value().requiresGuestAdditions())
            continue;
        if (m_charts.contains(iterator.key()) && m_charts[iterator.key()])
        {
            m_charts[iterator.key()]->setEnabled(fEnable);
            m_charts[iterator.key()]->update();
        }
        if (m_infoLabels.contains(iterator.key()) && m_infoLabels[iterator.key()])
        {
            m_infoLabels[iterator.key()]->setEnabled(fEnable);
            m_infoLabels[iterator.key()]->update();
        }
    }
}

void UIInformationRuntime::updateCPUGraphsAndMetric(ULONG iExecutingPercentage, ULONG iOtherPercentage)
{
    UIMetric &CPUMetric = m_subMetrics[m_strCPUMetricName];
    CPUMetric.addData(0, iExecutingPercentage);
    CPUMetric.addData(1, iOtherPercentage);
    CPUMetric.setMaximum(100);
    if (m_infoLabels.contains(m_strCPUMetricName)  && m_infoLabels[m_strCPUMetricName])
    {
        QString strInfo;
        if (m_infoLabels[m_strCPUMetricName]->isEnabled())
            strInfo = QString("<b>%1</b></b><br/><font color=\"%2\">%3: %4%5</font><br/><font color=\"%6\">%7: %8%9</font>")
                .arg(m_strCPUInfoLabelTitle)
                .arg(dataColorString(m_strCPUMetricName, 0))
                .arg(m_strCPUInfoLabelGuest).arg(QString::number(iExecutingPercentage)).arg(CPUMetric.unit())
                .arg(dataColorString(m_strCPUMetricName, 1))
                .arg(m_strCPUInfoLabelVMM).arg(QString::number(iOtherPercentage)).arg(CPUMetric.unit());
        else
            strInfo = QString("<b>%1</b><br/>%2%3").arg(m_strCPUInfoLabelTitle).arg("--").arg("%");
        m_infoLabels[m_strCPUMetricName]->setText(strInfo);
    }

    if (m_charts.contains(m_strCPUMetricName))
        m_charts[m_strCPUMetricName]->update();
}

void UIInformationRuntime::updateRAMGraphsAndMetric(quint64 iTotalRAM, quint64 iFreeRAM)
{
    UIMetric &RAMMetric = m_subMetrics[m_strRAMMetricName];
    RAMMetric.setMaximum(iTotalRAM);
    RAMMetric.addData(0, iTotalRAM - iFreeRAM);
    if (m_infoLabels.contains(m_strRAMMetricName)  && m_infoLabels[m_strRAMMetricName])
    {
        QString strInfo;
        if (m_infoLabels[m_strRAMMetricName]->isEnabled())
            strInfo = QString("<b>%1</b><br/>%2: %3<br/>%4: %5<br/>%6: %7").arg(m_strRAMInfoLabelTitle).arg(m_strRAMInfoLabelTotal).arg(uiCommon().formatSize(_1K * iTotalRAM, iDecimalCount))
                .arg(m_strRAMInfoLabelFree).arg(uiCommon().formatSize(_1K * (iFreeRAM), iDecimalCount))
                .arg(m_strRAMInfoLabelUsed).arg(uiCommon().formatSize(_1K * (iTotalRAM - iFreeRAM), iDecimalCount));
        else
            strInfo = QString("<b>%1</b><br/>%2: %3<br/>%4: %5<br/>%6: %7").arg(m_strRAMInfoLabelTitle).arg(m_strRAMInfoLabelTotal).arg("---").arg(m_strRAMInfoLabelFree).arg("---").arg(m_strRAMInfoLabelUsed).arg("---");
        m_infoLabels[m_strRAMMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strRAMMetricName))
        m_charts[m_strRAMMetricName]->update();
}

void UIInformationRuntime::updateNetworkGraphsAndMetric(quint64 iReceiveTotal, quint64 iTransmitTotal)
{
    UIMetric &NetMetric = m_subMetrics[m_strNetworkMetricName];

    quint64 iReceiveRate = iReceiveTotal - NetMetric.total(0);
    quint64 iTransmitRate = iTransmitTotal - NetMetric.total(1);

    NetMetric.setTotal(0, iReceiveTotal);
    NetMetric.setTotal(1, iTransmitTotal);

    /* Do not set data and maximum if the metric has not been initialized  since we need to initialize totals "(t-1)" first: */
    if (!NetMetric.isInitialized())
    {
        NetMetric.setIsInitialized(true);
        return;
    }

    NetMetric.addData(0, iReceiveRate);
    NetMetric.addData(1, iTransmitRate);
    quint64 iMaximum = qMax(NetMetric.maximum(), qMax(iReceiveRate, iTransmitRate));
    NetMetric.setMaximum(iMaximum);

    if (m_infoLabels.contains(m_strNetworkMetricName)  && m_infoLabels[m_strNetworkMetricName])
    {
        QString strInfo;
        if (m_infoLabels[m_strNetworkMetricName]->isEnabled())
            strInfo = QString("<b>%1</b></b><br/><font color=\"%2\">%3: %4<br/>%5 %6</font><br/><font color=\"%7\">%8: %9<br/>%10 %11</font>")
                .arg(m_strNetworkInfoLabelTitle)
                .arg(dataColorString(m_strNetworkMetricName, 0)).arg(m_strNetworkInfoLabelReceived).arg(uiCommon().formatSize((quint64)iReceiveRate, iDecimalCount))
                .arg(m_strNetworkInfoLabelReceivedTotal).arg(uiCommon().formatSize((quint64)iReceiveTotal, iDecimalCount))
                .arg(dataColorString(m_strNetworkMetricName, 1)).arg(m_strNetworkInfoLabelTransmitted).arg(uiCommon().formatSize((quint64)iTransmitRate, iDecimalCount))
                .arg(m_strNetworkInfoLabelTransmittedTotal).arg(uiCommon().formatSize((quint64)iTransmitTotal, iDecimalCount));

        else
            strInfo = QString("<b>%1</b><br/>%2: %3<br/>%4: %5").
                arg(m_strNetworkInfoLabelTitle)
                .arg(m_strNetworkInfoLabelReceived).arg("---")
                .arg(m_strNetworkInfoLabelTransmitted).arg("---");
        m_infoLabels[m_strNetworkMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strNetworkMetricName))
        m_charts[m_strNetworkMetricName]->update();
}

void UIInformationRuntime::updateDiskIOGraphsAndMetric(quint64 uDiskIOTotalWritten, quint64 uDiskIOTotalRead)
{
    UIMetric &diskMetric = m_subMetrics[m_strDiskIOMetricName];

    quint64 iWriteRate = uDiskIOTotalWritten - diskMetric.total(0);
    quint64 iReadRate = uDiskIOTotalRead - diskMetric.total(1);

    diskMetric.setTotal(0, uDiskIOTotalWritten);
    diskMetric.setTotal(1, uDiskIOTotalRead);

    /* Do not set data and maximum if the metric has not been initialized  since we need to initialize totals "(t-1)" first: */
    if (!diskMetric.isInitialized())
    {
        diskMetric.setIsInitialized(true);
        return;
    }
    diskMetric.addData(0, iWriteRate);
    diskMetric.addData(1, iReadRate);
    quint64 iMaximum = qMax(diskMetric.maximum(), qMax(iWriteRate, iReadRate));
    diskMetric.setMaximum(iMaximum);

    if (m_infoLabels.contains(m_strDiskIOMetricName)  && m_infoLabels[m_strDiskIOMetricName])
    {
        QString strInfo;
        if (m_infoLabels[m_strDiskIOMetricName]->isEnabled())
            strInfo = QString("<b>%1</b></b><br/><font color=\"%2\">%3: %4<br/>%5 %6</font><br/><font color=\"%7\">%8: %9<br/>%10 %11</font>")
                .arg(m_strDiskIOInfoLabelTitle)
                .arg(dataColorString(m_strDiskIOMetricName, 0)).arg(m_strDiskIOInfoLabelWritten).arg(uiCommon().formatSize((quint64)iWriteRate, iDecimalCount))
                .arg(m_strDiskIOInfoLabelWrittenTotal).arg(uiCommon().formatSize((quint64)uDiskIOTotalWritten, iDecimalCount))
                .arg(dataColorString(m_strDiskIOMetricName, 1)).arg(m_strDiskIOInfoLabelRead).arg(uiCommon().formatSize((quint64)iReadRate, iDecimalCount))
                .arg(m_strDiskIOInfoLabelReadTotal).arg(uiCommon().formatSize((quint64)uDiskIOTotalRead, iDecimalCount));

        else
            strInfo = QString("<b>%1</b><br/>%2: %3<br/>%4: %5").
                arg(m_strDiskIOInfoLabelTitle)
                .arg(m_strDiskIOInfoLabelWritten).arg("---")
                .arg(m_strDiskIOInfoLabelRead).arg("---");
        m_infoLabels[m_strDiskIOMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strDiskIOMetricName))
        m_charts[m_strDiskIOMetricName]->update();

}


void UIInformationRuntime::updateVMExitMetric(quint64 uTotalVMExits)
{
    if (uTotalVMExits <= 0)
        return;

    UIMetric &VMExitMetric = m_subMetrics[m_strVMExitMetricName];

    quint64 iRate = uTotalVMExits - VMExitMetric.total(0);

    VMExitMetric.setTotal(0, uTotalVMExits);

    /* Do not set data and maximum if the metric has not been initialized  since we need to initialize totals "(t-1)" first: */
    if (!VMExitMetric.isInitialized())
    {
        VMExitMetric.setIsInitialized(true);
        return;
    }

    VMExitMetric.addData(0, iRate);
    quint64 iMaximum = qMax(VMExitMetric.maximum(), iRate);
    VMExitMetric.setMaximum(iMaximum);

    if (m_infoLabels.contains(m_strVMExitMetricName)  && m_infoLabels[m_strVMExitMetricName])
    {
        QString strInfo;
        if (m_infoLabels[m_strVMExitMetricName]->isEnabled())
            strInfo = QString("<b>%1</b></b><br/>%2: %3 %4<br/>%5: %6 %7")
                .arg(m_strVMExitInfoLabelTitle)
                .arg(m_strVMExitLabelCurrent).arg(formatNumber(iRate)).arg(VMExitMetric.unit())
                .arg(m_strVMExitLabelTotal).arg(formatNumber(uTotalVMExits)).arg(VMExitMetric.unit());
        else
            strInfo = QString("<b>%1</b><br/>%2%3").arg(m_strVMExitInfoLabelTitle).arg("--").arg("%");
        m_infoLabels[m_strVMExitMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strVMExitMetricName))
        m_charts[m_strVMExitMetricName]->update();
}

QString UIInformationRuntime::dataColorString(const QString &strChartName, int iDataIndex)
{
    if (!m_charts.contains(strChartName))
        return QColor(Qt::red).name(QColor::HexRgb);
    UIChart *pChart = m_charts[strChartName];
    if (!pChart)
        return QColor(Qt::red).name(QColor::HexRgb);
    return pChart->dataSeriesColor(iDataIndex).name(QColor::HexRgb);
}

QVector<DebuggerMetricData> UIInformationRuntime::getTotalCounterFromDegugger(const QString &strQuery)
{
    QVector<DebuggerMetricData> xmlData;
    if (strQuery.isEmpty())
        return xmlData;
    CMachineDebugger debugger = m_console.GetDebugger();

    QString strStats = debugger.GetStats(strQuery, false);
    QXmlStreamReader xmlReader;
    xmlReader.addData(strStats);
    quint64 iTotal = 0;
    if (xmlReader.readNextStartElement())
    {
        while (xmlReader.readNextStartElement())
        {
            if (xmlReader.name() == "Counter")
            {
                QXmlStreamAttributes attributes = xmlReader.attributes();
                quint64 iCounter = attributes.value("c").toULongLong();
                iTotal += iCounter;
                xmlReader.skipCurrentElement();
                xmlData.push_back(DebuggerMetricData(*(attributes.value("name").string()), iCounter));
            }
            else if (xmlReader.name() == "U64")
            {
                QXmlStreamAttributes attributes = xmlReader.attributes();
                quint64 iCounter = attributes.value("val").toULongLong();
                iTotal += iCounter;
                xmlReader.skipCurrentElement();
                xmlData.push_back(DebuggerMetricData(*(attributes.value("name").string()), iCounter));
            }
            else
                xmlReader.skipCurrentElement();

        }
    }
    return xmlData;
}
#include "UIInformationRuntime.moc"
