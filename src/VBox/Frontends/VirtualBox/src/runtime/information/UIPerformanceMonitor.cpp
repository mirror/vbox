/* $Id$ */
/** @file
 * VBox Qt GUI - UIPerformanceMonitor class implementation.
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
#include <QLabel>
#include <QPainter>
#include <QGridLayout>
#include <QTimer>

/* GUI includes: */
#include "UICommon.h"
#include "UIPerformanceMonitor.h"
#include "UIInformationDataItem.h"
#include "UIInformationItem.h"
#include "UIInformationView.h"
#include "UIExtraDataManager.h"
#include "UIInformationModel.h"
#include "UISession.h"

#include "CGuest.h"
#include "CPerformanceCollector.h"
#include "CPerformanceMetric.h"

const ULONG iPeriod = 1;
const int iMaximumQueueSize = 120;
const int iMetricSetupCount = 1;
const int iDecimalCount = 2;
/*********************************************************************************************************************************
*   UIChart definition.                                                                                     *
*********************************************************************************************************************************/

class UIChart : public QWidget
{

    Q_OBJECT;

public:

    UIChart(QWidget *pParent, const UISubMetric *pSubMetric);
    void setFontSize(int iFontSize);
    int  fontSize() const;
    void setTextList(const QStringList &textList);
    const QStringList &textList() const;

    bool drawPieChart() const;
    void setDrawPieChart(bool fDrawPieChart);

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
    virtual void computeFontSize();

    const UISubMetric *m_pSubMetric;
    QSize m_size;
    QFont m_font;
    int m_iMarginLeft;
    int m_iMarginRight;
    int m_iMarginTop;
    int m_iMarginBottom;
    QStringList m_textList;
    int m_iPieChartSize;
    QRect m_pieChartRect;
    bool m_fDrawPieChart;
    bool m_fUseGradientLineColor;
    QColor m_dataSeriesColor[2];
    QString m_strXAxisLabel;
};


/*********************************************************************************************************************************
*   UIChart implementation.                                                                                     *
*********************************************************************************************************************************/

UIChart::UIChart(QWidget *pParent, const UISubMetric *pSubMetric)
    :QWidget(pParent)
    , m_pSubMetric(pSubMetric)
    , m_size(QSize(50, 50))
    , m_fDrawPieChart(true)
    , m_fUseGradientLineColor(true)
{
    m_dataSeriesColor[0] = QColor(Qt::red);
    m_dataSeriesColor[1] = QColor(Qt::blue);

    m_iMarginLeft = 1 * qApp->QApplication::style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    m_iMarginRight = 6 * qApp->QApplication::style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    m_iMarginTop = 0.3 * qApp->QApplication::style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    m_iMarginBottom = 2 * qApp->QApplication::style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    m_iPieChartSize = 1.5f * qApp->style()->pixelMetric(QStyle::PM_LargeIconSize);
    m_pieChartRect = QRect(1.5 * m_iMarginLeft, 1.5 * m_iMarginTop, m_iPieChartSize, m_iPieChartSize);
    m_size = QSize(6 * m_iPieChartSize, 2 * m_iPieChartSize);
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

bool UIChart::drawPieChart() const
{
    return m_fDrawPieChart;
}

void UIChart::setDrawPieChart(bool fDrawPieChart)
{
    if (m_fDrawPieChart == fDrawPieChart)
        return;
    m_fDrawPieChart = fDrawPieChart;
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
    if (iDataSeriesIndex >= 2)
        return QColor();
    return m_dataSeriesColor[iDataSeriesIndex];
}

void UIChart::setDataSeriesColor(int iDataSeriesIndex, const QColor &color)
{
    if (iDataSeriesIndex >= 2)
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

void UIChart::computeFontSize()
{
    int iFontSize = 24;

    //const QString &strText = m_pSubMetric->name();
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
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    /* Draw a rectanglar grid over which we will draw the line graphs: */
    int iChartHeight = height() - (m_iMarginTop + m_iMarginBottom);
    int iChartWidth = width() - (m_iMarginLeft + m_iMarginRight);
    QColor mainAxisColor(120, 120, 120);
    painter.setPen(mainAxisColor);
    painter.drawRect(QRect(m_iMarginLeft, m_iMarginTop, iChartWidth, iChartHeight));
    painter.setPen(QColor(200, 200, 200));
    /* Y subaxes: */
    int iYSubAxisCount = 3;
    for (int i = 0; i < iYSubAxisCount; ++i)
    {
        float fSubAxisY = m_iMarginTop + (i + 1) * iChartHeight / (float) (iYSubAxisCount + 1);
        painter.drawLine(m_iMarginLeft, fSubAxisY,
                         width() - m_iMarginRight, fSubAxisY);
    }
    /* X subaxes: */
    int iXSubAxisCount = 5;
    for (int i = 0; i < iXSubAxisCount; ++i)
    {
        float fSubAxisX = m_iMarginLeft + (i + 1) * iChartWidth / (float) (iXSubAxisCount + 1);
        painter.drawLine(fSubAxisX, m_iMarginTop, fSubAxisX, height() - m_iMarginBottom);
    }
    /* Draw XAxis tick labels: */
    painter.setPen(mainAxisColor);
    int iTotalSeconds = iPeriod * iMaximumQueueSize;
    QFontMetrics fontMetrics(painter.font());
    int iFontHeight = fontMetrics.height();
    for (int i = 0; i < iXSubAxisCount + 2; ++i)
    {
        int iTextX = m_iMarginLeft + i * iChartWidth / (float) (iXSubAxisCount + 1);
        QString strCurentSec = QString::number(iTotalSeconds - i * iTotalSeconds / (float)(iXSubAxisCount + 1));
        int iTextWidth = fontMetrics.width(strCurentSec);
        if (i == 0)
        {
            strCurentSec += " " + m_strXAxisLabel;
            painter.drawText(iTextX, height() - m_iMarginBottom + iFontHeight, strCurentSec);
        }
        else
            painter.drawText(iTextX - 0.5 * iTextWidth, height() - m_iMarginBottom + iFontHeight, strCurentSec);
    }

    /* Draw a half-transparent rectangle over the whole widget to indicate the it is disabled: */
    if (!isEnabled())
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(60, 60, 60, 50));
        painter.drawRect(QRect(0, 0, width(), height()));
    }

    if (!m_pSubMetric || iMaximumQueueSize <= 1)
        return;

    ULONG iMaximum = m_pSubMetric->maximum();
    if (iMaximum == 0)
        return;
    /* Draw the data lines: */
    if (isEnabled())
    {
        float fBarWidth = iChartWidth / (float) (iMaximumQueueSize - 1);
        float fH = iChartHeight / (float)iMaximum;
        if (m_fUseGradientLineColor)
        {
            QLinearGradient gradient(0, 0, 0, iChartHeight);
            gradient.setColorAt(1.0, Qt::green);
            gradient.setColorAt(0.5, Qt::yellow);
            gradient.setColorAt(0.0, Qt::red);
            painter.setPen(QPen(gradient, 2.5));
        }
        for (int k = 0; k < 2; ++k)
        {
            const QQueue<ULONG> *data = m_pSubMetric->data(k);
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
    }

    /* Draw YAxis tick labels: */
    painter.setPen(mainAxisColor);
    for (int i = 0; i < iYSubAxisCount + 2; ++i)
    {

        int iTextY = 0.5 * iFontHeight + m_iMarginTop + i * iChartHeight / (float) (iYSubAxisCount + 1);
        QString strValue;
        ULONG iValue = (iYSubAxisCount + 1 - i) * (iMaximum / (float) (iYSubAxisCount + 1));
        //printf("%d %u\n", i, iValue);
        if (m_pSubMetric->unit().compare("%", Qt::CaseInsensitive) == 0)
            strValue = QString::number(iValue);
        else if (m_pSubMetric->unit().compare("kb", Qt::CaseInsensitive) == 0)
            strValue = uiCommon().formatSize(_1K * (quint64)iValue, iDecimalCount);
        else if (m_pSubMetric->unit().compare("b", Qt::CaseInsensitive) == 0 ||
                 m_pSubMetric->unit().compare("b/s", Qt::CaseInsensitive) == 0)
            strValue = uiCommon().formatSize(iValue, iDecimalCount);
        painter.drawText(width() - 0.9 * m_iMarginRight, iTextY, strValue);
    }

    if (m_fDrawPieChart)
    {
        /* Draw a whole non-filled circle: */
        painter.setPen(QPen(Qt::gray, 1));
        painter.drawArc(m_pieChartRect, 0, 3600 * 16);

        QPointF center(m_pieChartRect.center());
        QPainterPath fillPath;
        fillPath.moveTo(center);
        fillPath.arcTo(m_pieChartRect, 90/*startAngle*/,
                       -1 * 360 /*sweepLength*/);

        /* First draw a white filled circle and that the arc for data: */
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 175));
        painter.drawPath(fillPath);

        /* Prepare the gradient for the pie chart: */
        QConicalGradient pieGradient;
        pieGradient.setCenter(m_pieChartRect.center());
        pieGradient.setAngle(90);
        pieGradient.setColorAt(0, Qt::red);
        pieGradient.setColorAt(0.5, Qt::yellow);
        pieGradient.setColorAt(1, Qt::green);

        if (isEnabled())
        {
            /* Draw the pie chart for the 0th data series only: */
            const QQueue<ULONG> *data = m_pSubMetric->data(0);
            if (data && !data->isEmpty())
            {
                QPainterPath dataPath;
                dataPath.moveTo(center);
                float fAngle = 360.f * data->back() / (float)iMaximum;
                dataPath.arcTo(m_pieChartRect, 90/*startAngle*/,
                               -1 * fAngle /*sweepLength*/);
                painter.setBrush(pieGradient);
                painter.drawPath(dataPath);
            }
        }
    }


}

/*********************************************************************************************************************************
*   UISubMetric implementation.                                                                                     *
*********************************************************************************************************************************/

UISubMetric::UISubMetric(const QString &strName, const QString &strUnit, int iMaximumQueueSize)
    : m_strName(strName)
    , m_strUnit(strUnit)
    , m_iMaximum(0)
    , m_iMaximumQueueSize(iMaximumQueueSize)
    , m_fRequiresGuestAdditions(false)
{
}

UISubMetric::UISubMetric()
    : m_iMaximumQueueSize(0)
{
}

const QString &UISubMetric::name() const
{
    return m_strName;
}

void UISubMetric::setMaximum(ULONG iMaximum)
{
    m_iMaximum = iMaximum;
}

ULONG UISubMetric::maximum() const
{
    return m_iMaximum;
}

void UISubMetric::setUnit(QString strUnit)
{
    m_strUnit = strUnit;

}
const QString &UISubMetric::unit() const
{
    return m_strUnit;
}

void UISubMetric::addData(int iDataSeriesIndex, ULONG fData)
{
    if (iDataSeriesIndex >= 2)
        return;
    m_data[iDataSeriesIndex].enqueue(fData);
    if (m_data[iDataSeriesIndex].size() > iMaximumQueueSize)
        m_data[iDataSeriesIndex].dequeue();
}

const QQueue<ULONG> *UISubMetric::data(int iDataSeriesIndex) const
{
    if (iDataSeriesIndex >= 2)
        return 0;
    return &m_data[iDataSeriesIndex];
}

bool UISubMetric::requiresGuestAdditions() const
{
    return m_fRequiresGuestAdditions;
}

void UISubMetric::setRequiresGuestAdditions(bool fRequiresGAs)
{
    m_fRequiresGuestAdditions = fRequiresGAs;
}

/*********************************************************************************************************************************
*   UIPerformanceMonitor implementation.                                                                                     *
*********************************************************************************************************************************/

UIPerformanceMonitor::UIPerformanceMonitor(QWidget *pParent, const CMachine &machine, const CConsole &console, const UISession *pSession)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fGuestAdditionsAvailable(false)
    , m_machine(machine)
    , m_console(console)
    , m_pMainLayout(0)
    , m_pTimer(0)
    , m_strCPUMetricName("CPU Load")
    , m_strRAMMetricName("RAM Usage")
    , m_strDiskMetricName("Disk Usage")
    , m_strNetMetricName("Net")
{
    if (!m_console.isNull())
        m_comGuest = m_console.GetGuest();
    m_fGuestAdditionsAvailable = guestAdditionsAvailable(6 /* minimum major version */);

    connect(pSession, &UISession::sigAdditionsStateChange, this, &UIPerformanceMonitor::sltGuestAdditionsStateChange);
    preparePerformaceCollector();
    prepareObjects();
    enableDisableGuestAdditionDependedWidgets(m_fGuestAdditionsAvailable);
    retranslateUi();
}

UIPerformanceMonitor::~UIPerformanceMonitor()
{
}

void UIPerformanceMonitor::retranslateUi()
{
    foreach (UIChart *pChart, m_charts)
        pChart->setXAxisLabel(QApplication::translate("UIVMInformationDialog", "Seconds"));
    int iMaximum = 0;
    m_strCPUInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "CPU Load");
    iMaximum = qMax(iMaximum, m_strCPUInfoLabelTitle.length());

    m_strRAMInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "RAM Usage");
    iMaximum = qMax(iMaximum, m_strRAMInfoLabelTitle.length());
    m_strRAMInfoLabelTotal = QApplication::translate("UIVMInformationDialog", "Total");
    iMaximum = qMax(iMaximum, m_strRAMInfoLabelTotal.length());
    m_strRAMInfoLabelFree = QApplication::translate("UIVMInformationDialog", "Free");
    iMaximum = qMax(iMaximum, m_strRAMInfoLabelFree.length());
    m_strRAMInfoLabelUsed = QApplication::translate("UIVMInformationDialog", "Used");
    iMaximum = qMax(iMaximum, m_strRAMInfoLabelUsed.length());
    m_strNetInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "Network");
    iMaximum = qMax(iMaximum, m_strNetInfoLabelTitle.length());
    m_strNetInfoLabelReceived = QApplication::translate("UIVMInformationDialog", "Received");
    iMaximum = qMax(iMaximum, m_strNetInfoLabelReceived.length());
    m_strNetInfoLabelTransmitted = QApplication::translate("UIVMInformationDialog", "Trasmitted");
    iMaximum = qMax(iMaximum, m_strNetInfoLabelTransmitted.length());
    m_strNetInfoLabelMaximum = QApplication::translate("UIVMInformationDialog", "Maximum");
    iMaximum = qMax(iMaximum, m_strNetInfoLabelMaximum.length());

    /* Compute the maximum label string length and set it as a fixed width to labels to prevent always changing widths: */
    /* Add m_iDecimalCount plus 3 characters for the number and 2 for unit string: */
    iMaximum += (iDecimalCount + 5);
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

void UIPerformanceMonitor::prepareObjects()
{
    m_pMainLayout = new QGridLayout(this);
    if (m_pMainLayout)
    {
        m_pMainLayout->setSpacing(10);
    }

    m_pTimer = new QTimer(this);
    if (m_pTimer)
    {
        connect(m_pTimer, &QTimer::timeout, this, &UIPerformanceMonitor::sltTimeout);
        m_pTimer->start(1000 * iPeriod);
    }

    QStringList chartOder;
    chartOder << m_strCPUMetricName << m_strRAMMetricName << m_strDiskMetricName << m_strNetMetricName;
    int iRow = 0;
    foreach (const QString &strMetricName, chartOder)
    {
        if (!m_subMetrics.contains(strMetricName))
            continue;
        QLabel *pLabel = new QLabel;
        pLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        m_pMainLayout->addWidget(pLabel, iRow, 0);
        m_infoLabels.insert(strMetricName, pLabel);

        UIChart *pChart = new UIChart(this, &(m_subMetrics[strMetricName]));
        m_charts.insert(strMetricName, pChart);
        pChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_pMainLayout->addWidget(pChart, iRow, 1);
        ++iRow;
    }

    /* Configure charts: */
    if (m_charts.contains(m_strNetMetricName) && m_charts[m_strNetMetricName])
    {
        m_charts[m_strNetMetricName]->setDrawPieChart(false);
        m_charts[m_strNetMetricName]->setUseGradientLineColor(false);
    }
    if (m_charts.contains(m_strCPUMetricName) && m_charts[m_strCPUMetricName])
        m_charts[m_strCPUMetricName]->setUseGradientLineColor(false);

    QWidget *bottomSpacerWidget = new QWidget(this);
    bottomSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    bottomSpacerWidget->setVisible(true);
    // QPalette pal = bottomSpacerWidget->palette();
    // pal.setColor(QPalette::Background, Qt::green);
    // bottomSpacerWidget->setAutoFillBackground(true);
    // bottomSpacerWidget->setPalette(pal);

    m_pMainLayout->addWidget(bottomSpacerWidget, iRow, 0, 1, 2);
}

void UIPerformanceMonitor::sltTimeout()
{

    if (m_performanceMonitor.isNull())
        return;
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
    ULONG iReceiveRate = 0;
    ULONG  iTransmitRate = 0;

    for (int i = 0; i < aReturnNames.size(); ++i)
    {
        if (aReturnDataLengths[i] == 0)
            continue;
        if (aReturnNames[i].contains("Disk"), Qt::CaseInsensitive)
            printf("%f\n", 22.2);
        /* Read the last of the return data disregarding the rest since we are caching the data in GUI side: */
        float fData = returnData[aReturnDataIndices[i] + aReturnDataLengths[i] - 1] / (float)aReturnScales[i];
        if (aReturnNames[i].contains("RAM", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
        {
            if (aReturnNames[i].contains("Total", Qt::CaseInsensitive))
                iTotalRAM = (quint64)fData;
            if (aReturnNames[i].contains("Free", Qt::CaseInsensitive))
                iFreeRAM = (quint64)fData;
        }
        else if (aReturnNames[i].contains("Net/Rate", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
        {
            if (aReturnNames[i].contains("Rx", Qt::CaseInsensitive))
                iReceiveRate = fData;
            if (aReturnNames[i].contains("Tx", Qt::CaseInsensitive))
                iTransmitRate = fData;
        }
    }

    (void)iReceiveRate;
    (void)iTransmitRate;
    /* Update the CPU load chart with values we get from IMachineDebugger::getCPULoad(..): */
    if (m_subMetrics.contains(m_strCPUMetricName))
    {
        ULONG aPctExecuting;
        ULONG aPctHalted;
        ULONG aPctOther;
        m_machineDebugger.GetCPULoad(0x7fffffff, aPctExecuting, aPctHalted, aPctOther);
        updateCPUGraphsAndMetric(aPctExecuting, aPctOther);
    }

    if (m_subMetrics.contains(m_strRAMMetricName))
        updateRAMGraphsAndMetric(iTotalRAM, iFreeRAM);
    if (m_subMetrics.contains(m_strNetMetricName))
        updateNewGraphsAndMetric(iReceiveRate, iTransmitRate);

}

void UIPerformanceMonitor::sltGuestAdditionsStateChange()
{
    bool fGuestAdditionsAvailable = guestAdditionsAvailable(6 /* minimum major version */);
    if (m_fGuestAdditionsAvailable == fGuestAdditionsAvailable)
        return;
    m_fGuestAdditionsAvailable = fGuestAdditionsAvailable;
    enableDisableGuestAdditionDependedWidgets(m_fGuestAdditionsAvailable);
}

void UIPerformanceMonitor::preparePerformaceCollector()
{
    m_performanceMonitor = uiCommon().virtualBox().GetPerformanceCollector();
    m_machineDebugger = m_console.GetDebugger();

    if (m_performanceMonitor.isNull())
        return;

    // m_nameList << "Guest/RAM/Usage*";
    m_nameList << "Guest/RAM/Usage*";
    m_nameList << "Net/Rate*";


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
                    UISubMetric newMetric(m_strRAMMetricName, metrics[i].GetUnit(), iMaximumQueueSize);
                    newMetric.setRequiresGuestAdditions(true);
                    m_subMetrics.insert(m_strRAMMetricName, newMetric);
                }
                else if (strName.contains("Net", Qt::CaseInsensitive))
                {
                    UISubMetric newMetric(m_strNetMetricName, metrics[i].GetUnit(), iMaximumQueueSize);
                    newMetric.setRequiresGuestAdditions(true);
                    m_subMetrics.insert(m_strNetMetricName, newMetric);
                }
            }
        }
    }
    m_subMetrics.insert(m_strCPUMetricName, UISubMetric(m_strCPUMetricName, "%", iMaximumQueueSize));
}

bool UIPerformanceMonitor::guestAdditionsAvailable(int iMinimumMajorVersion)
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

void UIPerformanceMonitor::enableDisableGuestAdditionDependedWidgets(bool fEnable)
{
    for (QMap<QString, UISubMetric>::const_iterator iterator =  m_subMetrics.begin();
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

void UIPerformanceMonitor::updateCPUGraphsAndMetric(ULONG iExecutingPercentage, ULONG iOtherPercentage)
{
    UISubMetric &CPUMetric = m_subMetrics[m_strCPUMetricName];
    CPUMetric.addData(0, iExecutingPercentage);
    CPUMetric.addData(1, iOtherPercentage);
    CPUMetric.setMaximum(100);
    if (m_infoLabels.contains(m_strCPUMetricName) && m_infoLabels[m_strCPUMetricName])
    {
        QString strInfo;
        if (m_infoLabels[m_strCPUMetricName]->isEnabled())
            strInfo = QString("<b>%1</b><br/>%2%3").arg(m_strCPUInfoLabelTitle).arg(QString::number(iExecutingPercentage)).arg("%");
        else
            strInfo = QString("<b>%1</b><br/>%2%3").arg(m_strCPUInfoLabelTitle).arg("--").arg("%");
        m_infoLabels[m_strCPUMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strCPUMetricName))
        m_charts[m_strCPUMetricName]->update();
}

void UIPerformanceMonitor::updateRAMGraphsAndMetric(quint64 iTotalRAM, quint64 iFreeRAM)
{
    UISubMetric &RAMMetric = m_subMetrics[m_strRAMMetricName];
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

void UIPerformanceMonitor::updateNewGraphsAndMetric(ULONG iReceiveRate, ULONG iTransmitRate)
{
   UISubMetric &NetMetric = m_subMetrics[m_strNetMetricName];

   NetMetric.addData(0, iReceiveRate);
   NetMetric.addData(1, iTransmitRate);

   ULONG iMaximum = qMax(NetMetric.maximum(), (ULONG)qMax(iReceiveRate, iTransmitRate));
   NetMetric.setMaximum(iMaximum);

    if (m_infoLabels.contains(m_strNetMetricName)  && m_infoLabels[m_strNetMetricName])
    {
        QString strInfo;
        if (m_infoLabels[m_strNetMetricName]->isEnabled())
            strInfo = QString("<b>%1</b></b><br/><font color=\"#FF0000\">%2: %3</font><br/><font color=\"#0000FF\">%4: %5</font><br/>%6: %7").arg(m_strNetInfoLabelTitle)
                .arg(m_strNetInfoLabelReceived).arg(uiCommon().formatSize((quint64)iReceiveRate, iDecimalCount))
                .arg(m_strNetInfoLabelTransmitted).arg(uiCommon().formatSize((quint64)iTransmitRate, iDecimalCount))
                .arg(m_strNetInfoLabelMaximum).arg(uiCommon().formatSize((quint64)iMaximum, iDecimalCount));
        else
            strInfo = QString("<b>%1</b><br/>%2: %3<br/>%4: %5<br/>%6: %7").arg(m_strNetInfoLabelTitle).arg(m_strNetInfoLabelReceived).arg("---").arg(m_strNetInfoLabelTransmitted).arg("---");
        m_infoLabels[m_strNetMetricName]->setText(strInfo);
    }
   if (m_charts.contains(m_strNetMetricName))
       m_charts[m_strNetMetricName]->update();

}

#include "UIPerformanceMonitor.moc"
