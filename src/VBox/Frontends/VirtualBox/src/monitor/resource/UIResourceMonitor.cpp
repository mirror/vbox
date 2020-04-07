/* $Id$ */
/** @file
 * VBox Qt GUI - UIResourceMonitor class implementation.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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
#include <QAbstractTableModel>
#include <QCheckBox>
#include <QHeaderView>
#include <QItemDelegate>
#include <QLabel>
#include <QMenuBar>
#include <QPainter>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QSortFilterProxyModel>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UIActionPoolManager.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIExtraDataDefs.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIPerformanceMonitor.h"
#include "UIResourceMonitor.h"
#include "UIMessageCenter.h"
#include "UIToolBar.h"
#include "UIVirtualBoxEventHandler.h"

#ifdef VBOX_WS_MAC
# include "UIWindowMenuManager.h"
#endif /* VBOX_WS_MAC */

/* COM includes: */
#include "CConsole.h"
#include "CMachine.h"
#include "CMachineDebugger.h"
#include "CPerformanceMetric.h"

/* Other VBox includes: */
#include <iprt/cidr.h>

struct ResourceColumn
{
    QString m_strName;
    bool    m_fEnabled;
};

///#define DEBUG_BACKGROUND


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorDoughnutChart definition.                                                                           *
*********************************************************************************************************************************/

class UIVMResourceMonitorDoughnutChart : public QWidget
{

    Q_OBJECT;

public:

    UIVMResourceMonitorDoughnutChart(QWidget *pParent = 0);
    void updateData(quint64 iData0, quint64 iData1);
    void setChartColors(const QColor &color0, const QColor &color1);
    void setChartCenterString(const QString &strCenter);
    void setDataMaximum(quint64 iMax);

protected:

    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

private:

    quint64 m_iData0;
    quint64 m_iData1;
    quint64 m_iDataMaximum;
    int m_iMargin;
    QColor m_color0;
    QColor m_color1;
    /** If not empty this text is drawn at the center of the doughnut chart. */
    QString m_strCenter;
};


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorHostStats definition.                                                                               *
*********************************************************************************************************************************/

class UIVMResourceMonitorHostStats
{

public:

    UIVMResourceMonitorHostStats();
    quint64 m_iCPUUserLoad;
    quint64 m_iCPUKernelLoad;
    quint64 m_iCPUFreq;
    quint64 m_iRAMTotal;
    quint64 m_iRAMFree;
    quint64 m_iFSTotal;
    quint64 m_iFSFree;
};


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorHostStatsWidget definition.                                                                         *
*********************************************************************************************************************************/

class UIVMResourceMonitorHostStatsWidget : public QIWithRetranslateUI<QWidget>
{

    Q_OBJECT;

public:

    UIVMResourceMonitorHostStatsWidget(QWidget *pParent = 0);
    void setHostStats(const UIVMResourceMonitorHostStats &hostStats);

protected:

    virtual void retranslateUi() /* override */;

private:

    void prepare();
    void addVerticalLine(QHBoxLayout *pLayout);
    void updateLabels();

    UIVMResourceMonitorDoughnutChart   *m_pHostCPUChart;
    UIVMResourceMonitorDoughnutChart   *m_pHostRAMChart;
    UIVMResourceMonitorDoughnutChart   *m_pHostFSChart;
    QLabel                             *m_pCPUTitleLabel;
    QLabel                             *m_pCPUUserLabel;
    QLabel                             *m_pCPUKernelLabel;
    QLabel                             *m_pCPUTotalLabel;
    QLabel                             *m_pRAMTitleLabel;
    QLabel                             *m_pRAMUsedLabel;
    QLabel                             *m_pRAMFreeLabel;
    QLabel                             *m_pRAMTotalLabel;
    QLabel                             *m_pFSTitleLabel;
    QLabel                             *m_pFSUsedLabel;
    QLabel                             *m_pFSFreeLabel;
    QLabel                             *m_pFSTotalLabel;
    QColor                              m_CPUUserColor;
    QColor                              m_CPUKernelColor;
    QColor                              m_RAMFreeColor;
    QColor                              m_RAMUsedColor;
    UIVMResourceMonitorHostStats        m_hostStats;
};


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorTableView definition.                                                                               *
*********************************************************************************************************************************/

class UIVMResourceMonitorTableView : public QTableView
{
    Q_OBJECT;

public:

    UIVMResourceMonitorTableView(QWidget *pParent = 0);
    void setMinimumColumnWidths(const QMap<int, int>& widths);
    void updateColumVisibility();

protected:

    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;

private slots:



private:

    void resizeHeader();
    /** Value is in pixels. Columns cannot be narrower than this width. */
    QMap<int, int> m_minimumColumnWidths;
};

/*********************************************************************************************************************************
 *   Class UIVMResourceMonitorItem definition.                                                                           *
 *********************************************************************************************************************************/
class UIResourceMonitorItem
{
public:
    UIResourceMonitorItem(const QUuid &uid, const QString &strVMName);
    UIResourceMonitorItem(const QUuid &uid);
    UIResourceMonitorItem();
    bool operator==(const UIResourceMonitorItem& other) const;
    bool isWithGuestAdditions() const;

    QUuid    m_VMuid;
    QString  m_strVMName;
    quint64  m_uCPUGuestLoad;
    quint64  m_uCPUVMMLoad;

    quint64  m_uTotalRAM;
    quint64  m_uFreeRAM;
    quint64  m_uUsedRAM;
    float    m_fRAMUsagePercentage;

    quint64 m_uNetworkDownRate;
    quint64 m_uNetworkUpRate;
    quint64 m_uNetworkDownTotal;
    quint64 m_uNetworkUpTotal;

    quint64 m_uDiskWriteRate;
    quint64 m_uDiskReadRate;
    quint64 m_uDiskWriteTotal;
    quint64 m_uDiskReadTotal;

    quint64 m_uVMExitRate;
    quint64 m_uVMExitTotal;

    CMachineDebugger m_comDebugger;
    mutable CGuest   m_comGuest;
    /** The strings of each column for the item. We update this during performance query
      * instead of model's data function to know the string length earlier. */
    QMap<int, QString> m_columnData;

private:

    void setupPerformanceCollector();
};

/*********************************************************************************************************************************
 *   Class UIVMResourceMonitorCheckBox definition.                                                                           *
 *********************************************************************************************************************************/

class UIVMResourceMonitorCheckBox : public QCheckBox
{
    Q_OBJECT;

public:

    UIVMResourceMonitorCheckBox(QWidget *parent = 0);
    void setData(const QVariant& data);
    const QVariant data() const;

private:

    QVariant m_data;
};


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorProxyModel definition.                                                                              *
*********************************************************************************************************************************/
class UIResourceMonitorProxyModel : public QSortFilterProxyModel
{

    Q_OBJECT;

public:

    UIResourceMonitorProxyModel(QObject *parent = 0);
    void dataUpdate();
    void reFilter();

protected:

    //virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const /* override */;
    /** Section (column) visibility is controlled by the QHeaderView (see UIVMResourceMonitorTableView::updateColumVisibility)
     *  to have somewhat meaningful column resizing. */
    //virtual bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const /* override */;

private:

};


/*********************************************************************************************************************************
*   Class UIResourceMonitorModel definition.                                                                                     *
*********************************************************************************************************************************/
class UIResourceMonitorModel : public QAbstractTableModel
{
    Q_OBJECT;

signals:

    void sigDataUpdate();
    void sigHostStatsUpdate(const UIVMResourceMonitorHostStats &stats);

public:

    UIResourceMonitorModel(QObject *parent = 0);
    int      rowCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    int      columnCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    QVariant data(const QModelIndex &index, int role) const /* override */;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    void setColumnCaptions(const QMap<int, QString>& captions);
    void setColumnVisible(const QMap<int, bool>& columnVisible);
    bool columnVisible(int iColumnId) const;
    void setShouldUpdate(bool fShouldUpdate);
    const QMap<int, int> dataLengths() const;

private slots:

    void sltMachineStateChanged(const QUuid &uId, const KMachineState state);
    void sltTimeout();

private:

    void initialize();
    void initializeItems();
    void setupPerformanceCollector();
    void queryPerformanceCollector();
    void addItem(const QUuid& uMachineId, const QString& strMachineName);
    void removeItem(const QUuid& uMachineId);
    void getHostRAMStats();

    QVector<UIResourceMonitorItem> m_itemList;
    /* Used to find machines by uid. key is the machine uid and int is the index to m_itemList */
    QMap<QUuid, int>               m_itemMap;
    QMap<int, QString> m_columnCaptions;
    QTimer *m_pTimer;
    /** @name The following are used during UIPerformanceCollector::QueryMetricsData(..)
     * @{ */
    QVector<QString> m_nameList;
    QVector<CUnknown> m_objectList;
    /** @} */
    CPerformanceCollector m_performanceMonitor;
    QMap<int, bool> m_columnVisible;
    /** If true the table data and corresponding view is updated. Possibly set by host widget to true only
     *  when the widget is visible in the main UI. */
    bool m_fShouldUpdate;
    UIVMResourceMonitorHostStats m_hostStats;
    /** Maximum length of string length of data displayed in column. Updated in UIResourceMonitorModel::data(..). */
    mutable QMap<int, int> m_columnDataMaxLength;
};


/*********************************************************************************************************************************
*   UIVMResourceMonitorDelegate definition.                                                                                      *
*********************************************************************************************************************************/
/** A QItemDelegate child class to disable dashed lines drawn around selected cells in QTableViews */
class UIVMResourceMonitorDelegate : public QItemDelegate
{

    Q_OBJECT;

protected:

    virtual void drawFocus ( QPainter * /*painter*/, const QStyleOptionViewItem & /*option*/, const QRect & /*rect*/ ) const {}
};


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorDoughnutChart implementation.                                                                       *
*********************************************************************************************************************************/

UIVMResourceMonitorDoughnutChart::UIVMResourceMonitorDoughnutChart(QWidget *pParent /* = 0 */)
    :QWidget(pParent)
    , m_iData0(0)
    , m_iData1(0)
    , m_iDataMaximum(0)
    , m_iMargin(3)
{
}

void UIVMResourceMonitorDoughnutChart::updateData(quint64 iData0, quint64 iData1)
{
    m_iData0 = iData0;
    m_iData1 = iData1;
    update();
}

void UIVMResourceMonitorDoughnutChart::setChartColors(const QColor &color0, const QColor &color1)
{
    m_color0 = color0;
    m_color1 = color1;
}

void UIVMResourceMonitorDoughnutChart::setChartCenterString(const QString &strCenter)
{
    m_strCenter = strCenter;
}

void UIVMResourceMonitorDoughnutChart::setDataMaximum(quint64 iMax)
{
    m_iDataMaximum = iMax;
}

void UIVMResourceMonitorDoughnutChart::paintEvent(QPaintEvent *pEvent)
{
    QWidget::paintEvent(pEvent);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int iFrameHeight = height()- 2 * m_iMargin;
    QRectF outerRect = QRectF(QPoint(m_iMargin,m_iMargin), QSize(iFrameHeight, iFrameHeight));
    QRectF innerRect = UIMonitorCommon::getScaledRect(outerRect, 0.6f, 0.6f);
    UIMonitorCommon::drawCombinedDoughnutChart(m_iData0, m_color0,
                                               m_iData1, m_color1,
                                               painter, m_iDataMaximum,
                                               outerRect, innerRect, 80);
    if (!m_strCenter.isEmpty())
    {
        float mul = 1.f / 1.4f;
        QRectF textRect =  UIMonitorCommon::getScaledRect(innerRect, mul, mul);
        painter.setPen(Qt::black);
        painter.drawText(textRect, Qt::AlignCenter, m_strCenter);
    }
}


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorHostStatsWidget implementation.                                                                     *
*********************************************************************************************************************************/

UIVMResourceMonitorHostStatsWidget::UIVMResourceMonitorHostStatsWidget(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pHostCPUChart(0)
    , m_pHostRAMChart(0)
    , m_pHostFSChart(0)
    , m_pCPUTitleLabel(0)
    , m_pCPUUserLabel(0)
    , m_pCPUKernelLabel(0)
    , m_pCPUTotalLabel(0)
    , m_pRAMTitleLabel(0)
    , m_pRAMUsedLabel(0)
    , m_pRAMFreeLabel(0)
    , m_pRAMTotalLabel(0)
    , m_pFSTitleLabel(0)
    , m_pFSUsedLabel(0)
    , m_pFSFreeLabel(0)
    , m_pFSTotalLabel(0)
    , m_CPUUserColor(Qt::red)
    , m_CPUKernelColor(Qt::blue)
    , m_RAMFreeColor(Qt::blue)
    , m_RAMUsedColor(Qt::red)
{
    prepare();
    retranslateUi();
#ifdef DEBUG_BACKGROUND
    QPalette pal = palette();
    pal.setColor(QPalette::Background, Qt::red);
    setAutoFillBackground(true);
    setPalette(pal);
#endif
}

void UIVMResourceMonitorHostStatsWidget::setHostStats(const UIVMResourceMonitorHostStats &hostStats)
{
    m_hostStats = hostStats;
    if (m_pHostCPUChart)
    {
        m_pHostCPUChart->updateData(m_hostStats.m_iCPUUserLoad, m_hostStats.m_iCPUKernelLoad);
        QString strCenter = QString("%1\nMHz").arg(m_hostStats.m_iCPUFreq);
        m_pHostCPUChart->setChartCenterString(strCenter);
    }
    if (m_pHostRAMChart)
    {
        quint64 iUsedRAM = m_hostStats.m_iRAMTotal - m_hostStats.m_iRAMFree;
        m_pHostRAMChart->updateData(iUsedRAM, m_hostStats.m_iRAMFree);
        m_pHostRAMChart->setDataMaximum(m_hostStats.m_iRAMTotal);
        if (m_hostStats.m_iRAMTotal != 0)
        {
            quint64 iUsedRamPer = 100 * (iUsedRAM / (float) m_hostStats.m_iRAMTotal);
            QString strCenter = QString("%1%\n%2").arg(iUsedRamPer).arg(tr("Used"));
            m_pHostRAMChart->setChartCenterString(strCenter);
        }
    }
    if (m_pHostFSChart)
    {
        quint64 iUsedFS = m_hostStats.m_iFSTotal - m_hostStats.m_iFSFree;
        m_pHostFSChart->updateData(iUsedFS, m_hostStats.m_iFSFree);
        m_pHostFSChart->setDataMaximum(m_hostStats.m_iFSTotal);
        if (m_hostStats.m_iFSTotal != 0)
        {
            quint64 iUsedRamPer = 100 * (iUsedFS / (float) m_hostStats.m_iFSTotal);
            QString strCenter = QString("%1%\n%2").arg(iUsedRamPer).arg(tr("Used"));
            m_pHostFSChart->setChartCenterString(strCenter);
        }
    }

    updateLabels();
}

void UIVMResourceMonitorHostStatsWidget::retranslateUi()
{
    updateLabels();
}

void UIVMResourceMonitorHostStatsWidget::addVerticalLine(QHBoxLayout *pLayout)
{
    QFrame *pLine = new QFrame;
    pLine->setFrameShape(QFrame::VLine);
    pLine->setFrameShadow(QFrame::Sunken);
    pLayout->addWidget(pLine);
}

void UIVMResourceMonitorHostStatsWidget::prepare()
{
    QHBoxLayout *pLayout = new QHBoxLayout;
    setLayout(pLayout);
    int iMinimumSize =  3 * QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize);

    /* CPU Stuff: */
    {
        /* Host CPU Labels: */
        QWidget *pCPULabelContainer = new QWidget;
        pCPULabelContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

#ifdef DEBUG_BACKGROUND
        QPalette pal = pCPULabelContainer->palette();
        pal.setColor(QPalette::Background, Qt::yellow);
        pCPULabelContainer->setAutoFillBackground(true);
        pCPULabelContainer->setPalette(pal);
#endif
        pLayout->addWidget(pCPULabelContainer);
        QVBoxLayout *pCPULabelsLayout = new QVBoxLayout;
        pCPULabelsLayout->setContentsMargins(0, 0, 0, 0);
        pCPULabelContainer->setLayout(pCPULabelsLayout);
        m_pCPUTitleLabel = new QLabel;
        pCPULabelsLayout->addWidget(m_pCPUTitleLabel);
        m_pCPUUserLabel = new QLabel;
        pCPULabelsLayout->addWidget(m_pCPUUserLabel);
        m_pCPUKernelLabel = new QLabel;
        pCPULabelsLayout->addWidget(m_pCPUKernelLabel);
        m_pCPUTotalLabel = new QLabel;
        pCPULabelsLayout->addWidget(m_pCPUTotalLabel);
        pCPULabelsLayout->setAlignment(Qt::AlignTop);
        pCPULabelsLayout->setSpacing(0);
        /* Host CPU chart widget: */
        m_pHostCPUChart = new UIVMResourceMonitorDoughnutChart;
        if (m_pHostCPUChart)
        {
            m_pHostCPUChart->setMinimumSize(iMinimumSize, iMinimumSize);
            m_pHostCPUChart->setDataMaximum(100);
            pLayout->addWidget(m_pHostCPUChart);
            m_pHostCPUChart->setChartColors(m_CPUUserColor, m_CPUKernelColor);
        }
    }
    addVerticalLine(pLayout);
    /* RAM Stuff: */
    {
        QWidget *pRAMLabelContainer = new QWidget;
        pRAMLabelContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

        pLayout->addWidget(pRAMLabelContainer);
        QVBoxLayout *pRAMLabelsLayout = new QVBoxLayout;
        pRAMLabelsLayout->setContentsMargins(0, 0, 0, 0);
        pRAMLabelsLayout->setSpacing(0);
        pRAMLabelContainer->setLayout(pRAMLabelsLayout);
        m_pRAMTitleLabel = new QLabel;
        pRAMLabelsLayout->addWidget(m_pRAMTitleLabel);
        m_pRAMUsedLabel = new QLabel;
        pRAMLabelsLayout->addWidget(m_pRAMUsedLabel);
        m_pRAMFreeLabel = new QLabel;
        pRAMLabelsLayout->addWidget(m_pRAMFreeLabel);
        m_pRAMTotalLabel = new QLabel;
        pRAMLabelsLayout->addWidget(m_pRAMTotalLabel);

        m_pHostRAMChart = new UIVMResourceMonitorDoughnutChart;
        if (m_pHostRAMChart)
        {
            m_pHostRAMChart->setMinimumSize(iMinimumSize, iMinimumSize);
            pLayout->addWidget(m_pHostRAMChart);
            m_pHostRAMChart->setChartColors(m_RAMUsedColor, m_RAMFreeColor);
        }
    }
    addVerticalLine(pLayout);
    /* FS Stuff: */
    {
        QWidget *pFSLabelContainer = new QWidget;
        pLayout->addWidget(pFSLabelContainer);
        pFSLabelContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        QVBoxLayout *pFSLabelsLayout = new QVBoxLayout;
        pFSLabelsLayout->setContentsMargins(0, 0, 0, 0);
        pFSLabelsLayout->setSpacing(0);
        pFSLabelContainer->setLayout(pFSLabelsLayout);
        m_pFSTitleLabel = new QLabel;
        pFSLabelsLayout->addWidget(m_pFSTitleLabel);
        m_pFSUsedLabel = new QLabel;
        pFSLabelsLayout->addWidget(m_pFSUsedLabel);
        m_pFSFreeLabel = new QLabel;
        pFSLabelsLayout->addWidget(m_pFSFreeLabel);
        m_pFSTotalLabel = new QLabel;
        pFSLabelsLayout->addWidget(m_pFSTotalLabel);

        m_pHostFSChart = new UIVMResourceMonitorDoughnutChart;
        if (m_pHostFSChart)
        {
            m_pHostFSChart->setMinimumSize(iMinimumSize, iMinimumSize);
            pLayout->addWidget(m_pHostFSChart);
            m_pHostFSChart->setChartColors(m_RAMUsedColor, m_RAMFreeColor);
        }

    }
    pLayout->addStretch(2);
}

void UIVMResourceMonitorHostStatsWidget::updateLabels()
{
    if (m_pCPUTitleLabel)
        m_pCPUTitleLabel->setText(QString("<b>%1</b>").arg(tr("Host CPU Load")));
    if (m_pCPUUserLabel)
    {
        QString strColor = QColor(m_CPUUserColor).name(QColor::HexRgb);
        m_pCPUUserLabel->setText(QString("<font color=\"%1\">%2: %3%</font>").arg(strColor).arg(tr("User")).arg(QString::number(m_hostStats.m_iCPUUserLoad)));
    }
    if (m_pCPUKernelLabel)
    {
        QString strColor = QColor(m_CPUKernelColor).name(QColor::HexRgb);
        m_pCPUKernelLabel->setText(QString("<font color=\"%1\">%2: %3%</font>").arg(strColor).arg(tr("Kernel")).arg(QString::number(m_hostStats.m_iCPUKernelLoad)));
    }
    if (m_pCPUTotalLabel)
        m_pCPUTotalLabel->setText(QString("%1: %2%").arg(tr("Total")).arg(m_hostStats.m_iCPUUserLoad + m_hostStats.m_iCPUKernelLoad));
    if (m_pRAMTitleLabel)
        m_pRAMTitleLabel->setText(QString("<b>%1</b>").arg(tr("Host RAM Usage")));
    if (m_pRAMFreeLabel)
    {
        QString strRAM = uiCommon().formatSize(m_hostStats.m_iRAMFree);
        QString strColor = QColor(m_RAMFreeColor).name(QColor::HexRgb);
        m_pRAMFreeLabel->setText(QString("<font color=\"%1\">%2: %3</font>").arg(strColor).arg(tr("Free")).arg(strRAM));
    }
    if (m_pRAMUsedLabel)
    {
        QString strRAM = uiCommon().formatSize(m_hostStats.m_iRAMTotal - m_hostStats.m_iRAMFree);
        QString strColor = QColor(m_RAMUsedColor).name(QColor::HexRgb);
        m_pRAMUsedLabel->setText(QString("<font color=\"%1\">%2: %3</font>").arg(strColor).arg(tr("Used")).arg(strRAM));
    }
    if (m_pRAMTotalLabel)
    {
        QString strRAM = uiCommon().formatSize(m_hostStats.m_iRAMTotal);
        m_pRAMTotalLabel->setText(QString("%1: %2").arg(tr("Total")).arg(strRAM));
    }
    if (m_pFSTitleLabel)
        m_pFSTitleLabel->setText(QString("<b>%1</b>").arg(tr("Host File System")));
    if (m_pFSFreeLabel)
    {
        QString strFS = uiCommon().formatSize(m_hostStats.m_iFSFree);
        QString strColor = QColor(m_RAMFreeColor).name(QColor::HexRgb);
        m_pFSFreeLabel->setText(QString("<font color=\"%1\">%2: %3</font>").arg(strColor).arg(tr("Free")).arg(strFS));
    }
    if (m_pFSUsedLabel)
    {
        QString strFS = uiCommon().formatSize(m_hostStats.m_iFSTotal - m_hostStats.m_iFSFree);
        QString strColor = QColor(m_RAMUsedColor).name(QColor::HexRgb);
        m_pFSUsedLabel->setText(QString("<font color=\"%1\">%2: %3</font>").arg(strColor).arg(tr("Used")).arg(strFS));
    }
    if (m_pFSTotalLabel)
    {
        QString strFS = uiCommon().formatSize(m_hostStats.m_iFSTotal);
        m_pFSTotalLabel->setText(QString("%1: %2").arg(tr("Total")).arg(strFS));
    }
}



/*********************************************************************************************************************************
*   Class UIVMResourceMonitorTableView implementation.                                                                           *
*********************************************************************************************************************************/

UIVMResourceMonitorTableView::UIVMResourceMonitorTableView(QWidget *pParent /* = 0 */)
    :QTableView(pParent)
{
}

void UIVMResourceMonitorTableView::setMinimumColumnWidths(const QMap<int, int>& widths)
{
    m_minimumColumnWidths = widths;
    resizeHeader();
}

void UIVMResourceMonitorTableView::updateColumVisibility()
{
    UIResourceMonitorProxyModel *pProxyModel = qobject_cast<UIResourceMonitorProxyModel *>(model());
    if (!pProxyModel)
        return;
    UIResourceMonitorModel *pModel = qobject_cast<UIResourceMonitorModel *>(pProxyModel->sourceModel());
    QHeaderView *pHeader = horizontalHeader();

    if (!pModel || !pHeader)
        return;
    for (int i = (int)VMResourceMonitorColumn_Name; i < (int)VMResourceMonitorColumn_Max; ++i)
    {
        if (!pModel->columnVisible(i))
            pHeader->hideSection(i);
        else
            pHeader->showSection(i);
    }
    resizeHeader();
}

void UIVMResourceMonitorTableView::resizeEvent(QResizeEvent *pEvent)
{
    resizeHeader();
    QTableView::resizeEvent(pEvent);
}

void UIVMResourceMonitorTableView::resizeHeader()
{
    QHeaderView* pHeader = horizontalHeader();
    if (!pHeader)
        return;
    int iSectionCount = pHeader->count();
    int iHiddenSectionCount = pHeader->hiddenSectionCount();
    int iWidth = width() / (iSectionCount - iHiddenSectionCount);
    for (int i = 0; i < iSectionCount; ++i)
    {
        if (pHeader->isSectionHidden(i))
            continue;
        int iMinWidth = m_minimumColumnWidths.value((VMResourceMonitorColumn)i, 0);
        pHeader->resizeSection(i, iWidth < iMinWidth ? iMinWidth : iWidth);
    }
}


/*********************************************************************************************************************************
 *   Class UIVMResourceMonitorItem implementation.                                                                           *
 *********************************************************************************************************************************/
UIResourceMonitorItem::UIResourceMonitorItem(const QUuid &uid, const QString &strVMName)
    : m_VMuid(uid)
    , m_strVMName(strVMName)
    , m_uCPUGuestLoad(0)
    , m_uCPUVMMLoad(0)
    , m_uTotalRAM(0)
    , m_uFreeRAM(0)
    , m_uUsedRAM(0)
    , m_fRAMUsagePercentage(0)
    , m_uNetworkDownRate(0)
    , m_uNetworkUpRate(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
    , m_uDiskWriteRate(0)
    , m_uDiskReadRate(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
    , m_uVMExitRate(0)
    , m_uVMExitTotal(0)
{
    CSession comSession = uiCommon().openSession(uid, KLockType_Shared);
    if (!comSession.isNull())
    {
        CConsole comConsole = comSession.GetConsole();
        if (!comConsole.isNull())
        {
            m_comGuest = comConsole.GetGuest();
            m_comDebugger = comConsole.GetDebugger();
        }
    }
}

UIResourceMonitorItem::UIResourceMonitorItem()
    : m_VMuid(QUuid())
    , m_uCPUGuestLoad(0)
    , m_uCPUVMMLoad(0)
    , m_uTotalRAM(0)
    , m_uUsedRAM(0)
    , m_fRAMUsagePercentage(0)
    , m_uNetworkDownRate(0)
    , m_uNetworkUpRate(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
    , m_uDiskWriteRate(0)
    , m_uDiskReadRate(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
    , m_uVMExitRate(0)
    , m_uVMExitTotal(0)
{
}

UIResourceMonitorItem::UIResourceMonitorItem(const QUuid &uid)
    : m_VMuid(uid)
    , m_uCPUGuestLoad(0)
    , m_uCPUVMMLoad(0)
    , m_uTotalRAM(0)
    , m_uUsedRAM(0)
    , m_fRAMUsagePercentage(0)
    , m_uNetworkDownRate(0)
    , m_uNetworkUpRate(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
    , m_uDiskWriteRate(0)
    , m_uDiskReadRate(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
    , m_uVMExitRate(0)
    , m_uVMExitTotal(0)
{
}

bool UIResourceMonitorItem::operator==(const UIResourceMonitorItem& other) const
{
    if (m_VMuid == other.m_VMuid)
        return true;
    return false;
}

bool UIResourceMonitorItem::isWithGuestAdditions() const
{
    if (m_comGuest.isNull())
        return false;
    return m_comGuest.GetAdditionsStatus(m_comGuest.GetAdditionsRunLevel());
}


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorHostStats implementation.                                                                           *
*********************************************************************************************************************************/

UIVMResourceMonitorHostStats::UIVMResourceMonitorHostStats()
    : m_iCPUUserLoad(0)
    , m_iCPUKernelLoad(0)
    , m_iCPUFreq(0)
    , m_iRAMTotal(0)
    , m_iRAMFree(0)
    , m_iFSTotal(0)
    , m_iFSFree(0)
{
}


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorCheckBox implementation.                                                                            *
*********************************************************************************************************************************/

UIVMResourceMonitorCheckBox::UIVMResourceMonitorCheckBox(QWidget *parent /* = 0 */)
    :QCheckBox(parent)
{
}
void UIVMResourceMonitorCheckBox::setData(const QVariant& data)
{
    m_data = data;
}

const QVariant UIVMResourceMonitorCheckBox::data() const
{
    return m_data;
}


/*********************************************************************************************************************************
*   Class UIVMResourceMonitorProxyModel implementation.                                                                          *
*********************************************************************************************************************************/
UIResourceMonitorProxyModel::UIResourceMonitorProxyModel(QObject *parent /* = 0 */)
    :QSortFilterProxyModel(parent)
{
}

void UIResourceMonitorProxyModel::dataUpdate()
{
    if (sourceModel())
        emit dataChanged(index(0,0), index(sourceModel()->rowCount(), sourceModel()->columnCount()));
    invalidate();
}

void UIResourceMonitorProxyModel::reFilter()
{
    emit layoutAboutToBeChanged();
    invalidateFilter();
    emit layoutChanged();
}

/* See the function definition comment: */
// bool UIResourceMonitorProxyModel::filterAcceptsColumn(int iSourceColumn, const QModelIndex &sourceParent) const
// {
//     Q_UNUSED(sourceParent);
//     UIResourceMonitorModel* pModel = qobject_cast<UIResourceMonitorModel*>(sourceModel());
//     if (!pModel)
//         return true;
//     return pModel->columnVisible(iSourceColumn);
// }


/*********************************************************************************************************************************
*   Class UIResourceMonitorModel implementation.                                                                                 *
*********************************************************************************************************************************/
UIResourceMonitorModel::UIResourceMonitorModel(QObject *parent /*= 0*/)
    :QAbstractTableModel(parent)
    , m_pTimer(new QTimer(this))
    , m_fShouldUpdate(true)
{
    initialize();
}

void UIResourceMonitorModel::initialize()
{
    for (int i = 0; i < (int)VMResourceMonitorColumn_Max; ++i)
        m_columnDataMaxLength[i] = 0;

    initializeItems();
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIResourceMonitorModel::sltMachineStateChanged);

    if (m_pTimer)
    {
        connect(m_pTimer, &QTimer::timeout, this, &UIResourceMonitorModel::sltTimeout);
        m_pTimer->start(1000);
    }
}

int UIResourceMonitorModel::rowCount(const QModelIndex &parent /* = QModelIndex() */) const
{
    Q_UNUSED(parent);
    return m_itemList.size();
}

int UIResourceMonitorModel::columnCount(const QModelIndex &parent /* = QModelIndex() */) const
{
    Q_UNUSED(parent);
    return VMResourceMonitorColumn_Max;
}

void UIResourceMonitorModel::setShouldUpdate(bool fShouldUpdate)
{
    m_fShouldUpdate = fShouldUpdate;
}

const QMap<int, int> UIResourceMonitorModel::dataLengths() const
{
    return m_columnDataMaxLength;
}

QVariant UIResourceMonitorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole || index.row() >= rowCount())
        return QVariant();
    return m_itemList[index.row()].m_columnData[index.column()];
}

QVariant UIResourceMonitorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        return m_columnCaptions.value((VMResourceMonitorColumn)section, QString());;
    return QVariant();
}

void UIResourceMonitorModel::setColumnCaptions(const QMap<int, QString>& captions)
{
    m_columnCaptions = captions;
}

void UIResourceMonitorModel::initializeItems()
{
    foreach (const CMachine &comMachine, uiCommon().virtualBox().GetMachines())
    {
        if (!comMachine.isNull())
        {
            if (comMachine.GetState() == KMachineState_Running)
                addItem(comMachine.GetId(), comMachine.GetName());
        }
    }
    setupPerformanceCollector();
}

void UIResourceMonitorModel::sltMachineStateChanged(const QUuid &uId, const KMachineState state)
{
    int iIndex = m_itemMap.value(uId, -1);
    /* Remove the machine in case machine is no longer working. */
    if (iIndex != -1 && state != KMachineState_Running)
    {
        emit layoutAboutToBeChanged();
        removeItem(uId);
        emit layoutChanged();
        setupPerformanceCollector();
    }
    /* Insert the machine if it is working. */
    if (iIndex == -1 && state == KMachineState_Running)
    {
        emit layoutAboutToBeChanged();
        CMachine comMachine = uiCommon().virtualBox().FindMachine(uId.toString());
        if (!comMachine.isNull())
            addItem(uId, comMachine.GetName());
        emit layoutChanged();
        setupPerformanceCollector();
    }
}

void UIResourceMonitorModel::getHostRAMStats()
{
    CHost comHost = uiCommon().host();
    m_hostStats.m_iRAMTotal = _1M * (quint64)comHost.GetMemorySize();
    m_hostStats.m_iRAMFree = _1M * (quint64)comHost.GetMemoryAvailable();
}

void UIResourceMonitorModel::sltTimeout()
{
    if (!m_fShouldUpdate)
        return;
    ULONG aPctExecuting;
    ULONG aPctHalted;
    ULONG aPctVMM;

    // bool fRAMColumns = columnVisible(VMResourceMonitorColumn_RAMUsedAndTotal)
    //     || columnVisible(VMResourceMonitorColumn_RAMUsedPercentage);
    bool fCPUColumns = columnVisible(VMResourceMonitorColumn_CPUVMMLoad) || columnVisible(VMResourceMonitorColumn_CPUGuestLoad);
    bool fNetworkColumns = columnVisible(VMResourceMonitorColumn_NetworkUpRate)
        || columnVisible(VMResourceMonitorColumn_NetworkDownRate)
        || columnVisible(VMResourceMonitorColumn_NetworkUpTotal)
        || columnVisible(VMResourceMonitorColumn_NetworkDownTotal);
    bool fIOColumns = columnVisible(VMResourceMonitorColumn_DiskIOReadRate)
        || columnVisible(VMResourceMonitorColumn_DiskIOWriteRate)
        || columnVisible(VMResourceMonitorColumn_DiskIOReadTotal)
        || columnVisible(VMResourceMonitorColumn_DiskIOWriteTotal);
    bool fVMExitColumn = columnVisible(VMResourceMonitorColumn_VMExits);

    getHostRAMStats();

    /* RAM usage and Host CPU: */
    //if (!m_performanceMonitor.isNull() && fRAMColumns)
    queryPerformanceCollector();

    for (int i = 0; i < m_itemList.size(); ++i)
    {
        if (!m_itemList[i].m_comDebugger.isNull())
        {
            /* CPU Load: */
            if (fCPUColumns)
            {
                m_itemList[i].m_comDebugger.GetCPULoad(0x7fffffff, aPctExecuting, aPctHalted, aPctVMM);
                m_itemList[i].m_uCPUGuestLoad = aPctExecuting;
                m_itemList[i].m_uCPUVMMLoad = aPctVMM;
            }
            /* Network rate: */
            if (fNetworkColumns)
            {
                quint64 uPrevDownTotal = m_itemList[i].m_uNetworkDownTotal;
                quint64 uPrevUpTotal = m_itemList[i].m_uNetworkUpTotal;
                UIMonitorCommon::getNetworkLoad(m_itemList[i].m_comDebugger,
                                                m_itemList[i].m_uNetworkDownTotal, m_itemList[i].m_uNetworkUpTotal);
                m_itemList[i].m_uNetworkDownRate = m_itemList[i].m_uNetworkDownTotal - uPrevDownTotal;
                m_itemList[i].m_uNetworkUpRate = m_itemList[i].m_uNetworkUpTotal - uPrevUpTotal;
            }
            /* IO rate: */
            if (fIOColumns)
            {
                quint64 uPrevWriteTotal = m_itemList[i].m_uDiskWriteTotal;
                quint64 uPrevReadTotal = m_itemList[i].m_uDiskReadTotal;
                UIMonitorCommon::getDiskLoad(m_itemList[i].m_comDebugger,
                                             m_itemList[i].m_uDiskWriteTotal, m_itemList[i].m_uDiskReadTotal);
                m_itemList[i].m_uDiskWriteRate = m_itemList[i].m_uDiskWriteTotal - uPrevWriteTotal;
                m_itemList[i].m_uDiskReadRate = m_itemList[i].m_uDiskReadTotal - uPrevReadTotal;
            }
            /* VM Exits: */
            if (fVMExitColumn)
            {
                quint64 uPrevVMExitsTotal = m_itemList[i].m_uVMExitTotal;
                UIMonitorCommon::getVMMExitCount(m_itemList[i].m_comDebugger, m_itemList[i].m_uVMExitTotal);
                m_itemList[i].m_uVMExitRate = m_itemList[i].m_uVMExitTotal - uPrevVMExitsTotal;
            }
        }
    }
    int iDecimalCount = 2;
    for (int i = 0; i < m_itemList.size(); ++i)
    {
        m_itemList[i].m_columnData[VMResourceMonitorColumn_Name] = m_itemList[i].m_strVMName;
        m_itemList[i].m_columnData[VMResourceMonitorColumn_CPUGuestLoad] =
            QString("%1%").arg(QString::number(m_itemList[i].m_uCPUGuestLoad));
        m_itemList[i].m_columnData[VMResourceMonitorColumn_CPUVMMLoad] =
            QString("%1%").arg(QString::number(m_itemList[i].m_uCPUVMMLoad));

        if (m_itemList[i].isWithGuestAdditions())
            m_itemList[i].m_columnData[VMResourceMonitorColumn_RAMUsedAndTotal] =
                QString("%1/%2").arg(uiCommon().formatSize(_1K * m_itemList[i].m_uUsedRAM, iDecimalCount)).
                arg(uiCommon().formatSize(_1K * m_itemList[i].m_uTotalRAM, iDecimalCount));
        else
            m_itemList[i].m_columnData[VMResourceMonitorColumn_RAMUsedAndTotal] = tr("N/A");

        if (m_itemList[i].isWithGuestAdditions())
            m_itemList[i].m_columnData[VMResourceMonitorColumn_RAMUsedPercentage] =
                QString("%1%").arg(QString::number(m_itemList[i].m_fRAMUsagePercentage, 'f', 2));
        else
            m_itemList[i].m_columnData[VMResourceMonitorColumn_RAMUsedPercentage] = tr("N/A");

        m_itemList[i].m_columnData[VMResourceMonitorColumn_NetworkUpRate] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uNetworkUpRate, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_NetworkDownRate] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uNetworkDownRate, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_NetworkUpTotal] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uNetworkUpTotal, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_NetworkDownTotal] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uNetworkDownTotal, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_DiskIOReadRate] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uDiskReadRate, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_DiskIOWriteRate] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uDiskWriteRate, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_DiskIOReadTotal] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uDiskReadTotal, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_DiskIOWriteTotal] =
            QString("%1").arg(uiCommon().formatSize(m_itemList[i].m_uDiskWriteTotal, iDecimalCount));

        m_itemList[i].m_columnData[VMResourceMonitorColumn_VMExits] =
            QString("%1/%2").arg(UICommon::addMetricSuffixToNumber(m_itemList[i].m_uVMExitRate)).
            arg(UICommon::addMetricSuffixToNumber(m_itemList[i].m_uVMExitTotal));
    }

    for (int i = 0; i < (int)VMResourceMonitorColumn_Max; ++i)
    {
        for (int j = 0; j < m_itemList.size(); ++j)
            if (m_columnDataMaxLength.value(i, 0) < m_itemList[j].m_columnData[i].length())
                m_columnDataMaxLength[i] = m_itemList[j].m_columnData[i].length();
    }
    emit sigDataUpdate();
    emit sigHostStatsUpdate(m_hostStats);
}

void UIResourceMonitorModel::setupPerformanceCollector()
{
    m_nameList.clear();
    m_objectList.clear();
    /* Initialize and configure CPerformanceCollector: */
    const ULONG iPeriod = 1;
    const int iMetricSetupCount = 1;
    if (m_performanceMonitor.isNull())
        m_performanceMonitor = uiCommon().virtualBox().GetPerformanceCollector();
    for (int i = 0; i < m_itemList.size(); ++i)
        m_nameList << "Guest/RAM/Usage*";
    /* This is for the host: */
    m_nameList << "CPU*";
    m_nameList << "FS*";
    m_objectList = QVector<CUnknown>(m_nameList.size(), CUnknown());
    m_performanceMonitor.SetupMetrics(m_nameList, m_objectList, iPeriod, iMetricSetupCount);
}

void UIResourceMonitorModel::queryPerformanceCollector()
{
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
    /* Parse the result we get from CPerformanceCollector to get respective values: */
    for (int i = 0; i < aReturnNames.size(); ++i)
    {
        if (aReturnDataLengths[i] == 0)
            continue;
        /* Read the last of the return data disregarding the rest since we are caching the data in GUI side: */
        float fData = returnData[aReturnDataIndices[i] + aReturnDataLengths[i] - 1] / (float)aReturnScales[i];
        if (aReturnNames[i].contains("RAM", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
        {
            if (aReturnNames[i].contains("Total", Qt::CaseInsensitive) || aReturnNames[i].contains("Free", Qt::CaseInsensitive))
            {
                {
                    CMachine comMachine = (CMachine)aReturnObjects[i];
                    if (comMachine.isNull())
                        continue;
                    int iIndex = m_itemMap.value(comMachine.GetId(), -1);
                    if (iIndex == -1 || iIndex >= m_itemList.size())
                        continue;
                    if (aReturnNames[i].contains("Total", Qt::CaseInsensitive))
                        m_itemList[iIndex].m_uTotalRAM = fData;
                    else
                        m_itemList[iIndex].m_uFreeRAM = fData;
                }
            }
        }
        else if (aReturnNames[i].contains("CPU/Load/User", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
        {
            CHost comHost = (CHost)aReturnObjects[i];
            if (!comHost.isNull())
                m_hostStats.m_iCPUUserLoad = fData;
        }
        else if (aReturnNames[i].contains("CPU/Load/Kernel", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
        {
            CHost comHost = (CHost)aReturnObjects[i];
            if (!comHost.isNull())
                m_hostStats.m_iCPUKernelLoad = fData;
        }
        else if (aReturnNames[i].contains("CPU/MHz", Qt::CaseInsensitive) && !aReturnNames[i].contains(":"))
        {
            CHost comHost = (CHost)aReturnObjects[i];
            if (!comHost.isNull())
                m_hostStats.m_iCPUFreq = fData;
        }
       else if (aReturnNames[i].contains("FS", Qt::CaseInsensitive) &&
                aReturnNames[i].contains("Total", Qt::CaseInsensitive) &&
                !aReturnNames[i].contains(":"))
       {
            CHost comHost = (CHost)aReturnObjects[i];
            if (!comHost.isNull())
                m_hostStats.m_iFSTotal = _1M * fData;
       }
       else if (aReturnNames[i].contains("FS", Qt::CaseInsensitive) &&
                aReturnNames[i].contains("Free", Qt::CaseInsensitive) &&
                !aReturnNames[i].contains(":"))
       {
            CHost comHost = (CHost)aReturnObjects[i];
            if (!comHost.isNull())
                m_hostStats.m_iFSFree = _1M * fData;
       }
    }
    for (int i = 0; i < m_itemList.size(); ++i)
    {
        m_itemList[i].m_uUsedRAM = m_itemList[i].m_uTotalRAM - m_itemList[i].m_uFreeRAM;
        if (m_itemList[i].m_uTotalRAM != 0)
            m_itemList[i].m_fRAMUsagePercentage = 100.f * (m_itemList[i].m_uUsedRAM / (float)m_itemList[i].m_uTotalRAM);
    }
}

void UIResourceMonitorModel::addItem(const QUuid& uMachineId, const QString& strMachineName)
{
    int iIndex = m_itemList.size();
    m_itemList.append(UIResourceMonitorItem(uMachineId, strMachineName));
    m_itemMap[uMachineId] = iIndex;
}

void UIResourceMonitorModel::removeItem(const QUuid& uMachineId)
{
    int iIndex = m_itemMap.value(uMachineId, -1);
    if (iIndex == -1)
        return;
    m_itemList.remove(iIndex);
    m_itemMap.remove(uMachineId);
}

void UIResourceMonitorModel::setColumnVisible(const QMap<int, bool>& columnVisible)
{
    m_columnVisible = columnVisible;
}

bool UIResourceMonitorModel::columnVisible(int iColumnId) const
{
    return m_columnVisible.value(iColumnId, true);
}


/*********************************************************************************************************************************
*   Class UIResourceMonitorWidget implementation.                                                                                *
*********************************************************************************************************************************/

UIResourceMonitorWidget::UIResourceMonitorWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                                 bool fShowToolbar /* = true */, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_fShowToolbar(fShowToolbar)
    , m_pToolBar(0)
    , m_pTableView(0)
    , m_pProxyModel(0)
    , m_pModel(0)
    , m_pHostStatsWidget(0)
    , m_pColumnSelectionMenu(0)
    , m_fIsCurrentTool(true)
{
    /* Prepare: */
    prepare();
}

UIResourceMonitorWidget::~UIResourceMonitorWidget()
{
    saveSettings();
}

QMenu *UIResourceMonitorWidget::menu() const
{
    return NULL;
}

bool UIResourceMonitorWidget::isCurrentTool() const
{
    return m_fIsCurrentTool;
}

void UIResourceMonitorWidget::setIsCurrentTool(bool fIsCurrentTool)
{
    m_fIsCurrentTool = fIsCurrentTool;
    if (m_pModel)
        m_pModel->setShouldUpdate(fIsCurrentTool);
}

void UIResourceMonitorWidget::retranslateUi()
{
    m_columnCaptions[VMResourceMonitorColumn_Name] = tr("VM Name");
    m_columnCaptions[VMResourceMonitorColumn_CPUGuestLoad] = tr("CPU Guest");
    m_columnCaptions[VMResourceMonitorColumn_CPUVMMLoad] = tr("CPU VMM");
    m_columnCaptions[VMResourceMonitorColumn_RAMUsedAndTotal] = tr("RAM Used/Total");
    m_columnCaptions[VMResourceMonitorColumn_RAMUsedPercentage] = tr("RAM %");
    m_columnCaptions[VMResourceMonitorColumn_NetworkUpRate] = tr("Network Up Rate");
    m_columnCaptions[VMResourceMonitorColumn_NetworkDownRate] = tr("Network Down Rate");
    m_columnCaptions[VMResourceMonitorColumn_NetworkUpTotal] = tr("Network Up Total");
    m_columnCaptions[VMResourceMonitorColumn_NetworkDownTotal] = tr("Network Down Total");
    m_columnCaptions[VMResourceMonitorColumn_DiskIOReadRate] = tr("Disk Read Rate");
    m_columnCaptions[VMResourceMonitorColumn_DiskIOWriteRate] = tr("Disk Write Rate");
    m_columnCaptions[VMResourceMonitorColumn_DiskIOReadTotal] = tr("Disk Read Total");
    m_columnCaptions[VMResourceMonitorColumn_DiskIOWriteTotal] = tr("Disk Write Total");
    m_columnCaptions[VMResourceMonitorColumn_VMExits] = tr("VM Exits");
    if (m_pModel)
        m_pModel->setColumnCaptions(m_columnCaptions);
    computeMinimumColumnWidths();
}

void UIResourceMonitorWidget::resizeEvent(QResizeEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::resizeEvent(pEvent);
}

void UIResourceMonitorWidget::showEvent(QShowEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::showEvent(pEvent);
}

void UIResourceMonitorWidget::paintEvent(QPaintEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::paintEvent(pEvent);
}


void UIResourceMonitorWidget::prepare()
{
    loadHiddenColumnList();
    prepareWidgets();
    loadSettings();
    retranslateUi();
    prepareActions();
    updateModelColumVisibilityCache();
}

void UIResourceMonitorWidget::prepareWidgets()
{
    /* Create main-layout: */
    new QVBoxLayout(this);
    if (!layout())
        return;
    /* Configure layout: */
    layout()->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
    layout()->setSpacing(10);
#else
    layout()->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2);
#endif

    if (m_fShowToolbar)
        prepareToolBar();

    m_pHostStatsWidget = new UIVMResourceMonitorHostStatsWidget;
    if (m_pHostStatsWidget)
        layout()->addWidget(m_pHostStatsWidget);

    m_pModel = new UIResourceMonitorModel(this);
    m_pProxyModel = new UIResourceMonitorProxyModel(this);

    m_pTableView = new UIVMResourceMonitorTableView();
    if (m_pTableView && m_pModel && m_pProxyModel)
    {
        layout()->addWidget(m_pTableView);
        // m_pTableView->setContextMenuPolicy(Qt::CustomContextMenu);
        // connect(m_pTableView, &QTableView::customContextMenuRequested,
        //         this, &UIResourceMonitorWidget::sltCreateContextMenu);
        m_pProxyModel->setSourceModel(m_pModel);
        m_pTableView->setModel(m_pProxyModel);
        m_pTableView->setItemDelegate(new UIVMResourceMonitorDelegate);
        m_pTableView->setSelectionMode(QAbstractItemView::NoSelection);
        /* m_pTableView->setSelectionMode(QAbstractItemView::SingleSelection);
           m_pTableView->setSelectionBehavior(QAbstractItemView::SelectRows);*/
        m_pTableView->setShowGrid(false);
        m_pTableView->horizontalHeader()->setHighlightSections(false);
        m_pTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_pTableView->verticalHeader()->setVisible(false);
        m_pTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        /* Minimize the row height: */
        m_pTableView->verticalHeader()->setDefaultSectionSize(m_pTableView->verticalHeader()->minimumSectionSize());
        m_pTableView->setAlternatingRowColors(true);
        m_pTableView->setSortingEnabled(true);
        m_pTableView->sortByColumn(0, Qt::AscendingOrder);
        connect(m_pModel, &UIResourceMonitorModel::sigDataUpdate, this, &UIResourceMonitorWidget::sltHandleDataUpdate);
        connect(m_pModel, &UIResourceMonitorModel::sigHostStatsUpdate, this, &UIResourceMonitorWidget::sltHandleHostStatsUpdate);
        updateModelColumVisibilityCache();
    }
}

void UIResourceMonitorWidget::prepareActions()
{
    connect(m_pActionPool->action(UIActionIndexST_M_VMResourceMonitor_T_Columns), &QAction::toggled,
            this, &UIResourceMonitorWidget::sltToggleColumnSelectionMenu);
    m_pColumnSelectionMenu  = new QFrame(this);
    m_pColumnSelectionMenu->setAutoFillBackground(true);
    m_pColumnSelectionMenu->setFrameStyle(QFrame::Panel | QFrame::Plain);
    m_pColumnSelectionMenu->hide();
    QVBoxLayout* pLayout = new QVBoxLayout(m_pColumnSelectionMenu);
    int iLength = 0;
    for (int i = 0; i < VMResourceMonitorColumn_Max; ++i)
    {
        UIVMResourceMonitorCheckBox* pCheckBox = new UIVMResourceMonitorCheckBox;
        QString strCaption = m_columnCaptions.value((VMResourceMonitorColumn)i, QString());
        pCheckBox->setText(strCaption);
        iLength = strCaption.length() > iLength ? strCaption.length() : iLength;
        if (!pCheckBox)
            continue;
        pLayout->addWidget(pCheckBox);
        pCheckBox->setData(i);
        pCheckBox->setChecked(columnVisible(i));
        if (i == (int)VMResourceMonitorColumn_Name)
            pCheckBox->setEnabled(false);
        connect(pCheckBox, &UIVMResourceMonitorCheckBox::toggled, this, &UIResourceMonitorWidget::sltHandleColumnAction);
    }
    QFontMetrics fontMetrics(m_pColumnSelectionMenu->font());
    int iWidth = iLength * fontMetrics.width('x') +
        QApplication::style()->pixelMetric(QStyle::PM_IndicatorWidth) +
        2 * QApplication::style()->pixelMetric(QStyle::PM_LayoutLeftMargin) +
        2 * QApplication::style()->pixelMetric(QStyle::PM_LayoutRightMargin);
    m_pColumnSelectionMenu->setFixedWidth(iWidth);
}

void UIResourceMonitorWidget::prepareToolBar()
{
    /* Create toolbar: */
    m_pToolBar = new UIToolBar(parentWidget());
    AssertPtrReturnVoid(m_pToolBar);
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

#ifdef VBOX_WS_MAC
        /* Check whether we are embedded into a stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Add into layout: */
            layout()->addWidget(m_pToolBar);
        }
#else
        /* Add into layout: */
        layout()->addWidget(m_pToolBar);
#endif
    }
}

void UIResourceMonitorWidget::loadSettings()
{
}

void UIResourceMonitorWidget::loadHiddenColumnList()
{
    QStringList hiddenColumnList = gEDataManager->VMResourceMonitorHiddenColumnList();
    for (int i = (int)VMResourceMonitorColumn_Name; i < (int)VMResourceMonitorColumn_Max; ++i)
        m_columnVisible[i] = true;
    foreach(const QString& strColumn, hiddenColumnList)
        setColumnVisible((int)gpConverter->fromInternalString<VMResourceMonitorColumn>(strColumn), false);
}

void UIResourceMonitorWidget::saveSettings()
{
    QStringList hiddenColumnList;
    for (int i = 0; i < m_columnVisible.size(); ++i)
    {
        if (!columnVisible(i))
            hiddenColumnList << gpConverter->toInternalString((VMResourceMonitorColumn) i);
    }
    gEDataManager->setVMResourceMonitorHiddenColumnList(hiddenColumnList);
}

void UIResourceMonitorWidget::sltToggleColumnSelectionMenu(bool fChecked)
{
    if (!m_pColumnSelectionMenu)
        return;
    m_pColumnSelectionMenu->setVisible(fChecked);

    if (fChecked)
    {
        m_pColumnSelectionMenu->move(0, 0);
        m_pColumnSelectionMenu->raise();
        m_pColumnSelectionMenu->resize(400, 400);
        m_pColumnSelectionMenu->show();
        m_pColumnSelectionMenu->setFocus();
    }
    else
        m_pColumnSelectionMenu->hide();
    update();
}

void UIResourceMonitorWidget::sltHandleColumnAction(bool fChecked)
{
    UIVMResourceMonitorCheckBox* pSender = qobject_cast<UIVMResourceMonitorCheckBox*>(sender());
    if (!pSender)
        return;
    setColumnVisible(pSender->data().toInt(), fChecked);
}

void UIResourceMonitorWidget::sltHandleHostStatsUpdate(const UIVMResourceMonitorHostStats &stats)
{
    if (m_pHostStatsWidget)
        m_pHostStatsWidget->setHostStats(stats);
}

void UIResourceMonitorWidget::sltHandleDataUpdate()
{
    computeMinimumColumnWidths();
    if (m_pProxyModel)
        m_pProxyModel->dataUpdate();
}

void UIResourceMonitorWidget::setColumnVisible(int iColumnId, bool fVisible)
{
    if (m_columnVisible.contains(iColumnId) && m_columnVisible[iColumnId] == fVisible)
        return;
    m_columnVisible[iColumnId] = fVisible;
    updateModelColumVisibilityCache();
}

void UIResourceMonitorWidget::updateModelColumVisibilityCache()
{
    if (m_pModel)
        m_pModel->setColumnVisible(m_columnVisible);
    /* Notify the table view for the changed column visibility: */
    if (m_pTableView)
        m_pTableView->updateColumVisibility();
}

void UIResourceMonitorWidget::computeMinimumColumnWidths()
{
    if (!m_pTableView || !m_pModel)
        return;
    QFontMetrics fontMetrics(m_pTableView->font());
    const QMap<int, int> &columnDataStringLengths = m_pModel->dataLengths();

    QMap<int, int> columnWidthsInPixels;
    for (int i = 0; i < (int)VMResourceMonitorColumn_Max; ++i)
    {
        int iColumnStringWidth = columnDataStringLengths.value(i, 0);
        int iColumnTitleWidth = m_columnCaptions.value(i, QString()).length();
        int iMax = iColumnStringWidth > iColumnTitleWidth ? iColumnStringWidth : iColumnTitleWidth;
        columnWidthsInPixels[i] = iMax * fontMetrics.width('x') +
            QApplication::style()->pixelMetric(QStyle::PM_LayoutLeftMargin) +
            QApplication::style()->pixelMetric(QStyle::PM_LayoutRightMargin);
    }
    m_pTableView->setMinimumColumnWidths(columnWidthsInPixels);
}

bool UIResourceMonitorWidget::columnVisible(int iColumnId) const
{
    return m_columnVisible.value(iColumnId, true);
}


/*********************************************************************************************************************************
*   Class UIResourceMonitorFactory implementation.                                                                               *
*********************************************************************************************************************************/

UIResourceMonitorFactory::UIResourceMonitorFactory(UIActionPool *pActionPool /* = 0 */)
    : m_pActionPool(pActionPool)
{
}

void UIResourceMonitorFactory::create(QIManagerDialog *&pDialog, QWidget *pCenterWidget)
{
    pDialog = new UIResourceMonitor(pCenterWidget, m_pActionPool);
}


/*********************************************************************************************************************************
*   Class UIResourceMonitor implementation.                                                                                      *
*********************************************************************************************************************************/

UIResourceMonitor::UIResourceMonitor(QWidget *pCenterWidget, UIActionPool *pActionPool)
    : QIWithRetranslateUI<QIManagerDialog>(pCenterWidget)
    , m_pActionPool(pActionPool)
{
}

void UIResourceMonitor::retranslateUi()
{
    setWindowTitle(tr("VM Resource Monitor"));
}

void UIResourceMonitor::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/host_iface_manager_32px.png", ":/host_iface_manager_16px.png"));
}

void UIResourceMonitor::configureCentralWidget()
{
    UIResourceMonitorWidget *pWidget = new UIResourceMonitorWidget(EmbedTo_Dialog, m_pActionPool, true, this);
    AssertPtrReturnVoid(pWidget);
    {
        setWidget(pWidget);
        setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        setWidgetToolbar(pWidget->toolbar());
#endif
        centralWidget()->layout()->addWidget(pWidget);
    }
}

void UIResourceMonitor::configureButtonBox()
{
}

void UIResourceMonitor::finalize()
{
    retranslateUi();
}

UIResourceMonitorWidget *UIResourceMonitor::widget()
{
    return qobject_cast<UIResourceMonitorWidget*>(QIManagerDialog::widget());
}


#include "UIResourceMonitor.moc"
