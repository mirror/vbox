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
const int iMaximumQueueSize = 100;
const int iMetricSetupCount = 1;

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

protected:

    virtual void paintEvent(QPaintEvent *pEvent) /* override */;
    virtual QSize minimumSizeHint() const /* override */;
    virtual QSize sizeHint() const  /* override */;
    virtual void computeFontSize();

    const UISubMetric *m_pSubMetric;
    QSize m_size;
    QFont m_font;
    int m_iMargin;
    QStringList m_textList;
    int m_iPieChartSize;
    QRect m_pieChartRect;

};


/*********************************************************************************************************************************
*   UIChart implementation.                                                                                     *
*********************************************************************************************************************************/

UIChart::UIChart(QWidget *pParent, const UISubMetric *pSubMetric)
    :QWidget(pParent)
    , m_pSubMetric(pSubMetric)
    , m_size(QSize(50, 50))
{
    m_iMargin = qApp->QApplication::style()->pixelMetric(QStyle::PM_LayoutTopMargin);
    m_iPieChartSize = 1.5f * qApp->style()->pixelMetric(QStyle::PM_LargeIconSize);
    m_pieChartRect = QRect(1.5 * m_iMargin, 1.5 * m_iMargin, m_iPieChartSize, m_iPieChartSize);
    m_size = QSize(6 * m_iPieChartSize, 1.8 * m_iPieChartSize);
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
            if (iWidth + 2 * m_iMargin > m_size.width())
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

    float fChartHeight = height() - 2 * m_iMargin;
    float fChartWidth = width() - 2 * m_iMargin;
    painter.setPen(QColor(120, 120, 120));
    painter.drawRect(QRect(m_iMargin, m_iMargin, fChartWidth, fChartHeight));
    painter.setPen(QColor(200, 200, 200));

    int iYParts = 4;
    for (int i = 1; i < iYParts; ++i)
        painter.drawLine(m_iMargin, m_iMargin + i *fChartHeight / (float)iYParts, width() - m_iMargin, m_iMargin + i * fChartHeight / (float)iYParts);


    int iXParts = 5;
    for (int i = 1; i < iXParts; ++i)
        painter.drawLine(m_iMargin + i * fChartWidth / (float)iXParts, m_iMargin, m_iMargin + i * fChartWidth / (float)iXParts, height() - m_iMargin);

    painter.setRenderHint(QPainter::Antialiasing);// | QPainter::TextAntialiasing);
    if (!m_pSubMetric)
        return;
    const QQueue<ULONG> &data = m_pSubMetric->data();

    if (data.isEmpty() || iMaximumQueueSize == 0)
        return;
    float fBarWidth = fChartWidth / (float) iMaximumQueueSize;

    float fMaximum = m_pSubMetric->maximum();
    if (m_pSubMetric->isPercentage())
        fMaximum = 100.f;

    float fH = 0;
    if (fMaximum != 0)
        fH = fChartHeight / fMaximum;

    QLinearGradient gradient(0, 0, 0, fChartHeight);
    gradient.setColorAt(1.0, Qt::green);
    gradient.setColorAt(0.0, Qt::red);
    painter.setPen(QPen(gradient, 2.5));

    for (int i = 0; i < data.size() - 1; ++i)
    {
        int j = i + 1;

        float fHeight = fH * data[i];
        float fX = width() - ((data.size() -i) * fBarWidth);

        float fHeight2 = fH * data[j];
        float fX2 = width() - ((data.size() -j) * fBarWidth);


        QLineF bar(fX, height() - (fHeight + m_iMargin), fX2, height() - (fHeight2 + m_iMargin));
        painter.drawLine(bar);
    }


    /* Draw a whole non-filled circle: */
    painter.setPen(QPen(Qt::gray, 1));
    painter.drawArc(m_pieChartRect, 0, 3600 * 16);

    QPointF center(m_pieChartRect.center());
    QPainterPath fillPath;
    QPainterPath dataPath;
    dataPath.moveTo(center);
    fillPath.moveTo(center);
    float fAngle = 360.f * data.back() / fMaximum;
    dataPath.arcTo(m_pieChartRect, 90/*startAngle*/,
                 -1 * fAngle /*sweepLength*/);
    fillPath.arcTo(m_pieChartRect, 90/*startAngle*/,
                 -1 * 360 /*sweepLength*/);

    /* First draw a white filled circle and that the arc for data: */
    QConicalGradient pieGradient;
    pieGradient.setCenter(m_pieChartRect.center());
    pieGradient.setAngle(90);
    pieGradient.setColorAt(0, Qt::red);
    pieGradient.setColorAt(1, Qt::green);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawPath(fillPath);
    painter.setBrush(pieGradient);
    painter.drawPath(dataPath);
}

/*********************************************************************************************************************************
*   UISubMetric implementation.                                                                                     *
*********************************************************************************************************************************/

UISubMetric::UISubMetric(const QString &strName, const QString &strUnit, int iMaximumQueueSize)
    : m_strName(strName)
    , m_strUnit(strUnit)
    , m_iMaximumQueueSize(iMaximumQueueSize)
{
    if (isPercentage())
        m_fMaximum = 100.f;
}

UISubMetric::UISubMetric()
    : m_iMaximumQueueSize(0)
{
}

const QString &UISubMetric::name() const
{
    return m_strName;
}

void UISubMetric::setMaximum(float fMaximum)
{
    m_fMaximum = fMaximum;
}

float UISubMetric::maximum() const
{
    return m_fMaximum;
}

void UISubMetric::setMinimum(float fMinimum)
{
    m_fMinimum = fMinimum;
}

float UISubMetric::minimum() const
{
    return m_fMinimum;
}

void UISubMetric::setAverage(float fAverage)
{
    m_fAverage = fAverage;
}

float UISubMetric::average() const
{
    return m_fAverage;
}

void UISubMetric::setUnit(QString strUnit)
{
    m_strUnit = strUnit;

}
const QString &UISubMetric::unit() const
{
    return m_strUnit;
}

void UISubMetric::addData(ULONG fData)
{
    m_data.enqueue(fData);
    if (m_data.size() > iMaximumQueueSize)
        m_data.dequeue();
}

const QQueue<ULONG> &UISubMetric::data() const
{
    return m_data;
}

bool UISubMetric::isPercentage() const
{
    return m_strUnit == "%";
}


/*********************************************************************************************************************************
*   UIPerformanceMonitor implementation.                                                                                     *
*********************************************************************************************************************************/

UIPerformanceMonitor::UIPerformanceMonitor(QWidget *pParent, const CMachine &machine, const CConsole &console)
    : QWidget(pParent)
    , m_fGuestAdditionsAvailable(false)
    , m_machine(machine)
    , m_console(console)
    , m_pMainLayout(0)
    , m_pTimer(0)
    , m_strCPUMetricName("CPU Load")
    , m_strRAMMetricName("RAM Usage")
{
    // bool fGuestAdditionsStatus = comGuest.GetAdditionsStatus(comGuest.GetAdditionsRunLevel());
    // if (fGuestAdditionsStatus)
    // {
    //     QStringList versionStrings = comGuest.GetAdditionsVersion().split('.', QString::SkipEmptyParts);
    //     if (!versionStrings.isEmpty())
    //     {
    //         bool fConvert = false;
    //         int iMajorVersion = versionStrings[0].toInt(&fConvert);
    //         if (fConvert && iMajorVersion >= 6)
    //             m_fGuestAdditionsAvailable = true;
    //     }
    // }
    // printf("m_fGuestAdditionsAvailable %d\n", m_fGuestAdditionsAvailable);

    preparePerformaceCollector();
    prepareObjects();
}

UIPerformanceMonitor::~UIPerformanceMonitor()
{
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

    int iRow = 0;
    for (QMap<QString, UISubMetric>::const_iterator iterator =  m_subMetrics.begin();
         iterator != m_subMetrics.end(); ++iterator)
    {
        QLabel *pLabel = new QLabel;
        m_pMainLayout->addWidget(pLabel, iRow, 0);
        m_infoLabels.insert(iterator.key(), pLabel);

        UIChart *pChart = new UIChart(this, &(iterator.value()));
        m_charts.insert(iterator.key(), pChart);
        pChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_pMainLayout->addWidget(pChart, iRow, 1);
        ++iRow;
    }

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

    for (int i = 0; i < aReturnNames.size(); ++i)
    {
        if (aReturnDataLengths[i] == 0)
            continue;

        float fData = 0;
        if (aReturnScales[i] == 1)
            fData = returnData[aReturnDataIndices[i] + aReturnDataLengths[i] - 1];
        else
            fData = returnData[aReturnDataIndices[i] + aReturnDataLengths[i] - 1] / (float)aReturnScales[i];

        if (aReturnNames[i].contains("RAM", Qt::CaseInsensitive))
        {
            if (aReturnNames[i].contains("Total", Qt::CaseInsensitive))
            {
                if (aReturnNames[i].contains(":"))
                    continue;
                else
                {
                    iTotalRAM = (quint64)fData;
                    continue;
                }
            }
            if (aReturnNames[i].contains("Free", Qt::CaseInsensitive))
            {
                if (aReturnNames[i].contains(":"))
                    continue;
                else
                {
                    iFreeRAM = (quint64)fData;
                    continue;
                }

            }
        }
    }

    /* Update the CPU load chart with values we get from IMachineDebugger::getCPULoad(..): */
    if (m_subMetrics.contains(m_strCPUMetricName))
    {
        UISubMetric &CPUMetric = m_subMetrics[m_strCPUMetricName];
        ULONG aPctExecuting;
        ULONG aPctHalted;
        ULONG aPctOther;
        m_machineDebugger.GetCPULoad(0x7fffffff, aPctExecuting, aPctHalted, aPctOther);
        CPUMetric.addData(aPctExecuting);
        QList<UIChart*> charts = m_charts.values(m_strCPUMetricName);
        QString strInfo = QString("%1\n%2%3").arg("CPU Load").arg(QString::number(aPctExecuting)).arg("%");
        if (m_infoLabels.contains(m_strCPUMetricName))
            m_infoLabels[m_strCPUMetricName]->setText(strInfo);
        if (m_charts.contains(m_strCPUMetricName))
            m_charts[m_strCPUMetricName]->update();
    }

    if (m_subMetrics.contains(m_strRAMMetricName))
    {
        UISubMetric &RAMMetric = m_subMetrics[m_strRAMMetricName];
        RAMMetric.setMaximum(iTotalRAM);
        RAMMetric.addData(iTotalRAM - iFreeRAM);
        QString strInfo = QString("%1\n%2: %3\n%4: %5\n%6: %7").arg("RAM Usage").arg("Total").arg(uiCommon().formatSize(_1K * iTotalRAM)).arg("Free:").arg(uiCommon().formatSize(_1K * (iFreeRAM))).arg("Used:").arg(uiCommon().formatSize(_1K * (iTotalRAM - iFreeRAM)));
        if (m_infoLabels.contains(m_strRAMMetricName))
            m_infoLabels[m_strRAMMetricName]->setText(strInfo);

        if (m_charts.contains(m_strRAMMetricName))
        {
            m_charts[m_strRAMMetricName]->update();
        }
    }

    /* Try to compute a global font size so that all texts fit to widgets: */
    // int iMinimumFontSize = 24;
    // foreach (UIChart *pChart, m_charts)
    // {
    //     if (!qobject_cast<UIPieChart*>(pChart))
    //         continue;
    //             iMinimumFontSize = qMin(pChart->fontSize(), iMinimumFontSize);
    // }

    // foreach (UIChart *pChart, m_charts)
    // {
    //     if (!pChart)
    //         continue;
    //     pChart->setFontSize(iMinimumFontSize);
    //     pChart->update();
    // }


    // int iMaxLineNumber = 0;
    // int iMaxLineLength = 0;
    // foreach (UIChart *pChart, m_charts)
    // {
    //     if (!pChart)
    //         continue;
    //     const QStringList &list = pChart->textList();
    //     iMaxLineNumber = qMax(iMaxLineNumber, list.size());
    //     foreach (const QString &strLine, list)
    //         iMaxLineLength = qMax(iMaxLineLength, strLine.length());
    // }

    // foreach (UIChart *pChart, m_charts)
    // {
    //     /* Currently we only care about the pie charts since they are the smallest of charts: */
    //     if (qobject_cast<UIPieChart*>(pChart))
    //         continue;

    // }
}

void UIPerformanceMonitor::preparePerformaceCollector()
{
    m_performanceMonitor = uiCommon().virtualBox().GetPerformanceCollector();
    m_machineDebugger = m_console.GetDebugger();

    if (m_performanceMonitor.isNull())
        return;

    // m_nameList << "Guest/RAM/Usage*";
    m_nameList << "Guest/RAM/Usage*";
    // m_nameList << "Disk*";

    m_nameList << "*";
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
                    QString strRAMMetricName(m_strRAMMetricName);
                    m_subMetrics.insert(strRAMMetricName, UISubMetric(strRAMMetricName, metrics[i].GetUnit(), iMaximumQueueSize));
                }
            }
        }
    }
    m_subMetrics.insert(m_strCPUMetricName, UISubMetric(m_strCPUMetricName, "%", iMaximumQueueSize));
}

#include "UIPerformanceMonitor.moc"
