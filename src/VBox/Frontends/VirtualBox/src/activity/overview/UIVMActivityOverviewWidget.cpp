/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMActivityOverviewWidget class implementation.
 */

/*
 * Copyright (C) 2009-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
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
#include "QIToolBar.h"
#include "UIActionPoolManager.h"
#include "UICloudEntityKey.h"
#include "UICloudNetworkingStuff.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIExtraDataDefs.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UITranslator.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualMachineItemCloud.h"
#include "UIVMActivityMonitor.h"
#include "UIVMActivityOverviewWidget.h"

#ifdef VBOX_WS_MAC
# include "UIWindowMenuManager.h"
#endif /* VBOX_WS_MAC */

/* COM includes: */
#include "CCloudClient.h"
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

/** Draws a doughnut shaped chart for the passed data values and can have a text drawn in the center. */


/*********************************************************************************************************************************
*   Class UIVMActivityOverviewDoughnutChart definition.                                                                          *
*********************************************************************************************************************************/

class UIVMActivityOverviewDoughnutChart : public QWidget
{

    Q_OBJECT;

public:

    UIVMActivityOverviewDoughnutChart(QWidget *pParent = 0);
    void updateData(quint64 iData0, quint64 iData1);
    void setChartColors(const QColor &color0, const QColor &color1);
    void setChartCenterString(const QString &strCenter);
    void setDataMaximum(quint64 iMax);

protected:

    virtual void paintEvent(QPaintEvent *pEvent) RT_OVERRIDE;

private:

    quint64 m_iData0;
    quint64 m_iData1;
    quint64 m_iDataMaximum;
    int     m_iMargin;
    QColor  m_color0;
    QColor  m_color1;
    /** If not empty this text is drawn at the center of the doughnut chart. */
    QString m_strCenter;
};


/*********************************************************************************************************************************
*   Class UIVMActivityOverviewHostStats definition.                                                                              *
*********************************************************************************************************************************/
/** A simple container to store host related performance values. */
class UIVMActivityOverviewHostStats
{

public:

    UIVMActivityOverviewHostStats();
    quint64 m_iCPUUserLoad;
    quint64 m_iCPUKernelLoad;
    quint64 m_iCPUFreq;
    quint64 m_iRAMTotal;
    quint64 m_iRAMFree;
    quint64 m_iFSTotal;
    quint64 m_iFSFree;
};


/*********************************************************************************************************************************
*   Class UIVMActivityOverviewHostStatsWidget definition.                                                                        *
*********************************************************************************************************************************/
/** A container QWidget to layout host stats. related widgets. */
class UIVMActivityOverviewHostStatsWidget : public QIWithRetranslateUI<QWidget>
{

    Q_OBJECT;

public:

    UIVMActivityOverviewHostStatsWidget(QWidget *pParent = 0);
    void setHostStats(const UIVMActivityOverviewHostStats &hostStats);

protected:

    virtual void retranslateUi() RT_OVERRIDE;

private:

    void prepare();
    void addVerticalLine(QHBoxLayout *pLayout);
    void updateLabels();

    UIVMActivityOverviewDoughnutChart  *m_pHostCPUChart;
    UIVMActivityOverviewDoughnutChart  *m_pHostRAMChart;
    UIVMActivityOverviewDoughnutChart  *m_pHostFSChart;
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
    UIVMActivityOverviewHostStats       m_hostStats;
};


/*********************************************************************************************************************************
*   Class UIVMActivityOverviewTableView definition.                                                                              *
*********************************************************************************************************************************/
/** A QTableView extension so manage the column width a bit better than what Qt offers out of box. */
class UIVMActivityOverviewTableView : public QTableView
{
    Q_OBJECT;

signals:

    void sigSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

public:

    UIVMActivityOverviewTableView(QWidget *pParent = 0);
    void setMinimumColumnWidths(const QMap<int, int>& widths);
    void updateColumVisibility();
    int selectedItemIndex() const;
    bool hasSelection() const;

protected:

    virtual void resizeEvent(QResizeEvent *pEvent) RT_OVERRIDE;
    virtual void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) RT_OVERRIDE;
    virtual void mousePressEvent(QMouseEvent *pEvent) RT_OVERRIDE;

private slots:



private:

    /** Resizes all the columns in response to resizeEvent. Columns cannot be narrower than m_minimumColumnWidths values. */
    void resizeHeaders();
    /** Value is in pixels. Columns cannot be narrower than this width. */
    QMap<int, int> m_minimumColumnWidths;
};


/*********************************************************************************************************************************
 *   Class UIActivityOverviewItem definition.                                                                           *
 *********************************************************************************************************************************/
/** Each instance of UIActivityOverviewItem corresponds to a vm. they are owned my the model. */
class UIActivityOverviewItem : public QObject
{

    Q_OBJECT;

public:

    UIActivityOverviewItem(QObject *pParent, const QUuid &uid, const QString &strVMName);

    UIActivityOverviewItem();
    virtual ~UIActivityOverviewItem();
    bool operator==(const UIActivityOverviewItem& other) const;
    int columnLength(int iColumnIndex) const;
    const QUuid &machineId() const;

    virtual QString machineStateString() const = 0;
    virtual bool isRunning() const = 0;
    virtual bool isCloudVM() const = 0;
    virtual void setMachineState(int iState) = 0;

    QString columnData(int iColumnIndex) const;

    quint64  m_uTotalRAM;
    quint64  m_uFreeRAM;
    quint64  m_uUsedRAM;
    float    m_fRAMUsagePercentage;

protected:

    QUuid         m_VMuid;
    QString       m_strVMName;

    /** The strings of each column for the item. We update this during performance query
      * instead of model's data function to know the string length earlier. */
    QMap<int, QString> m_columnData;
};

Q_DECLARE_METATYPE(UIActivityOverviewItem);

/*********************************************************************************************************************************
 *   Class UIActivityOverviewItemLocal definition.                                                                           *
 *********************************************************************************************************************************/
/* A UIActivityOverviewItem derivation to show local vms in the table view: */
class UIActivityOverviewItemLocal : public UIActivityOverviewItem
{
    Q_OBJECT;
public:

    UIActivityOverviewItemLocal(QObject *pParent, const QUuid &uid, const QString &strVMName);
    UIActivityOverviewItemLocal();
    ~UIActivityOverviewItemLocal();

    bool isWithGuestAdditions();
    void resetDebugger();
    void updateColumnData();

    virtual bool isRunning() const override;
    virtual bool isCloudVM() const override;
    virtual QString machineStateString() const override;
    virtual void setMachineState(int iState) override;

private:

    CSession         m_comSession;
    CGuest           m_comGuest;
    KMachineState    m_enmMachineState;
    quint64          m_uVMExitTotal;
    quint64          m_uDiskWriteTotal;
    quint64          m_uDiskReadTotal;
    quint64          m_uNetworkDownTotal;
    quint64          m_uNetworkUpTotal;
    CMachineDebugger m_comDebugger;
};

/*********************************************************************************************************************************
 *   Class UIActivityOverviewItemCloud definition.                                                                           *
 *********************************************************************************************************************************/
/* A UIActivityOverviewItem derivation to show cloud vms in the table view: */
class UIActivityOverviewItemCloud : public UIActivityOverviewItem
{
    Q_OBJECT;
public:

    UIActivityOverviewItemCloud(QObject *pParent, const QUuid &uid, const QString &strVMName, CCloudMachine &comCloudMachine);

    UIActivityOverviewItemCloud();
    ~UIActivityOverviewItemCloud();
    void updateMachineState();
    virtual bool isRunning() const override;
    virtual bool isCloudVM() const override;
    virtual QString machineStateString() const override;
    virtual void setMachineState(int iState) override;

protected:

private slots:

    void sltTimeout();
    void sltMetricNameListingComplete(QVector<QString> metricNameList);
    void sltMetricDataReceived(KMetricType enmMetricType,
                               const QVector<QString> &data, const QVector<QString> &timeStamps);
private:

    void getMetricList();
    void resetColumData();

    QTimer *m_pTimer;
    CCloudMachine m_comCloudMachine;
    KCloudMachineState m_enmMachineState;
    QVector<KMetricType> m_availableMetricTypes;
};


/*********************************************************************************************************************************
*   Class UIVMActivityOverviewProxyModel definition.                                                                             *
*********************************************************************************************************************************/
class UIActivityOverviewProxyModel : public QSortFilterProxyModel
{

    Q_OBJECT;

public:

    UIActivityOverviewProxyModel(QObject *parent = 0);
    void dataUpdate();
    void setNotRunningVMVisibility(bool fShow);
    void setCloudVMVisibility(bool fShow);

protected:

    virtual bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const /* override*/;
    bool filterAcceptsRow(int iSourceRow, const QModelIndex &sourceParent) const /* override*/;

private:

    bool m_fShowNotRunningVMs;
    bool m_fShowCloudVMs;
};


/*********************************************************************************************************************************
*   Class UIActivityOverviewModel definition.                                                                                    *
*********************************************************************************************************************************/
class UIActivityOverviewModel : public QAbstractTableModel
{
    Q_OBJECT;

signals:

    void sigDataUpdate();
    void sigHostStatsUpdate(const UIVMActivityOverviewHostStats &stats);

public:

    UIActivityOverviewModel(QObject *parent = 0);
    int      rowCount(const QModelIndex &parent = QModelIndex()) const RT_OVERRIDE;
    int      columnCount(const QModelIndex &parent = QModelIndex()) const RT_OVERRIDE;
    QVariant data(const QModelIndex &index, int role) const RT_OVERRIDE;
    void clearData();
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    void setColumnCaptions(const QMap<int, QString>& captions);
    void setColumnVisible(const QMap<int, bool>& columnVisible);
    bool columnVisible(int iColumnId) const;
    void setShouldUpdate(bool fShouldUpdate);
    const QMap<int, int> dataLengths() const;
    QUuid itemUid(int iIndex);
    int itemIndex(const QUuid &uid);
    bool isVMRunning(int rowIndex) const;
    bool isCloudVM(int rowIndex) const;
    void setCloudMachineItems(const QList<UIVirtualMachineItemCloud*> &cloudItems);

private slots:

    void sltMachineStateChanged(const QUuid &uId, const KMachineState state);
    void sltMachineRegistered(const QUuid &uId, bool fRegistered);
    void sltLocalVMUpdateTimeout();

private:

    void initialize();
    void initializeItems();
    void setupPerformanceCollector();
    void queryPerformanceCollector();
    void addItem(const QUuid& uMachineId, const QString& strMachineName, KMachineState enmState);
    void removeItem(const QUuid& uMachineId);
    void getHostRAMStats();

    QVector<UIActivityOverviewItem*> m_itemList;
    QMap<int, QString> m_columnTitles;
    QTimer *m_pLocalVMUpdateTimer;

    /** @name The following are used during UIPerformanceCollector::QueryMetricsData(..)
     * @{ */
       QVector<QString> m_nameList;
       QVector<CUnknown> m_objectList;
    /** @} */
    CPerformanceCollector m_performanceCollector;
    QMap<int, bool> m_columnVisible;
    UIVMActivityOverviewHostStats m_hostStats;
    /** Maximum length of string length of data displayed in column. Updated in UIActivityOverviewModel::data(..). */
    mutable QMap<int, int> m_columnDataMaxLength;
};


/*********************************************************************************************************************************
*   UIVMActivityOverviewDelegate definition.                                                                                     *
*********************************************************************************************************************************/
/** A QItemDelegate child class to disable dashed lines drawn around selected cells in QTableViews */
class UIVMActivityOverviewDelegate : public QItemDelegate
{

    Q_OBJECT;

public:

    UIVMActivityOverviewDelegate(QObject *pParent = 0)
        : QItemDelegate(pParent){}

protected:

    virtual void drawFocus ( QPainter * /*painter*/, const QStyleOptionViewItem & /*option*/, const QRect & /*rect*/ ) const {}
};


/*********************************************************************************************************************************
*   Class UIVMActivityOverviewDoughnutChart implementation.                                                                      *
*********************************************************************************************************************************/
UIVMActivityOverviewDoughnutChart::UIVMActivityOverviewDoughnutChart(QWidget *pParent /* = 0 */)
    :QWidget(pParent)
    , m_iData0(0)
    , m_iData1(0)
    , m_iDataMaximum(0)
    , m_iMargin(3)
{
}

void UIVMActivityOverviewDoughnutChart::updateData(quint64 iData0, quint64 iData1)
{
    m_iData0 = iData0;
    m_iData1 = iData1;
    update();
}

void UIVMActivityOverviewDoughnutChart::setChartColors(const QColor &color0, const QColor &color1)
{
    m_color0 = color0;
    m_color1 = color1;
}

void UIVMActivityOverviewDoughnutChart::setChartCenterString(const QString &strCenter)
{
    m_strCenter = strCenter;
}

void UIVMActivityOverviewDoughnutChart::setDataMaximum(quint64 iMax)
{
    m_iDataMaximum = iMax;
}

void UIVMActivityOverviewDoughnutChart::paintEvent(QPaintEvent *pEvent)
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
*   Class UIVMActivityOverviewHostStatsWidget implementation.                                                                    *
*********************************************************************************************************************************/

UIVMActivityOverviewHostStatsWidget::UIVMActivityOverviewHostStatsWidget(QWidget *pParent /* = 0 */)
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
}

void UIVMActivityOverviewHostStatsWidget::setHostStats(const UIVMActivityOverviewHostStats &hostStats)
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
            QString strCenter = QString("%1%\n%2").arg(iUsedRamPer).arg(UIVMActivityOverviewWidget::tr("Used"));
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
            quint64 iUsedFSPer = 100 * (iUsedFS / (float) m_hostStats.m_iFSTotal);
            QString strCenter = QString("%1%\n%2").arg(iUsedFSPer).arg(UIVMActivityOverviewWidget::tr("Used"));
            m_pHostFSChart->setChartCenterString(strCenter);
        }
    }

    updateLabels();
}

void UIVMActivityOverviewHostStatsWidget::retranslateUi()
{
    updateLabels();
}

void UIVMActivityOverviewHostStatsWidget::addVerticalLine(QHBoxLayout *pLayout)
{
    QFrame *pLine = new QFrame;
    pLine->setFrameShape(QFrame::VLine);
    pLine->setFrameShadow(QFrame::Sunken);
    pLayout->addWidget(pLine);
}

void UIVMActivityOverviewHostStatsWidget::prepare()
{
    QHBoxLayout *pLayout = new QHBoxLayout;
    setLayout(pLayout);
    int iMinimumSize =  3 * QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize);

    /* CPU Stuff: */
    {
        /* Host CPU Labels: */
        QWidget *pCPULabelContainer = new QWidget;
        pCPULabelContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
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
        m_pHostCPUChart = new UIVMActivityOverviewDoughnutChart;
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

        m_pHostRAMChart = new UIVMActivityOverviewDoughnutChart;
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

        m_pHostFSChart = new UIVMActivityOverviewDoughnutChart;
        if (m_pHostFSChart)
        {
            m_pHostFSChart->setMinimumSize(iMinimumSize, iMinimumSize);
            pLayout->addWidget(m_pHostFSChart);
            m_pHostFSChart->setChartColors(m_RAMUsedColor, m_RAMFreeColor);
        }

    }
    pLayout->addStretch(2);
}

void UIVMActivityOverviewHostStatsWidget::updateLabels()
{
    if (m_pCPUTitleLabel)
        m_pCPUTitleLabel->setText(QString("<b>%1</b>").arg(UIVMActivityOverviewWidget::tr("Host CPU Load")));
    if (m_pCPUUserLabel)
    {
        QString strColor = QColor(m_CPUUserColor).name(QColor::HexRgb);
        m_pCPUUserLabel->setText(QString("<font color=\"%1\">%2: %3%</font>").arg(strColor).arg(UIVMActivityOverviewWidget::tr("User")).arg(QString::number(m_hostStats.m_iCPUUserLoad)));
    }
    if (m_pCPUKernelLabel)
    {
        QString strColor = QColor(m_CPUKernelColor).name(QColor::HexRgb);
        m_pCPUKernelLabel->setText(QString("<font color=\"%1\">%2: %3%</font>").arg(strColor).arg(UIVMActivityOverviewWidget::tr("Kernel")).arg(QString::number(m_hostStats.m_iCPUKernelLoad)));
    }
    if (m_pCPUTotalLabel)
        m_pCPUTotalLabel->setText(QString("%1: %2%").arg(UIVMActivityOverviewWidget::tr("Total")).arg(m_hostStats.m_iCPUUserLoad + m_hostStats.m_iCPUKernelLoad));
    if (m_pRAMTitleLabel)
        m_pRAMTitleLabel->setText(QString("<b>%1</b>").arg(UIVMActivityOverviewWidget::tr("Host RAM Usage")));
    if (m_pRAMFreeLabel)
    {
        QString strRAM = UITranslator::formatSize(m_hostStats.m_iRAMFree);
        QString strColor = QColor(m_RAMFreeColor).name(QColor::HexRgb);
        m_pRAMFreeLabel->setText(QString("<font color=\"%1\">%2: %3</font>").arg(strColor).arg(UIVMActivityOverviewWidget::tr("Free")).arg(strRAM));
    }
    if (m_pRAMUsedLabel)
    {
        QString strRAM = UITranslator::formatSize(m_hostStats.m_iRAMTotal - m_hostStats.m_iRAMFree);
        QString strColor = QColor(m_RAMUsedColor).name(QColor::HexRgb);
        m_pRAMUsedLabel->setText(QString("<font color=\"%1\">%2: %3</font>").arg(strColor).arg(UIVMActivityOverviewWidget::tr("Used")).arg(strRAM));
    }
    if (m_pRAMTotalLabel)
    {
        QString strRAM = UITranslator::formatSize(m_hostStats.m_iRAMTotal);
        m_pRAMTotalLabel->setText(QString("%1: %2").arg(UIVMActivityOverviewWidget::tr("Total")).arg(strRAM));
    }
    if (m_pFSTitleLabel)
        m_pFSTitleLabel->setText(QString("<b>%1</b>").arg(UIVMActivityOverviewWidget::tr("Host File System")));
    if (m_pFSFreeLabel)
    {
        QString strFS = UITranslator::formatSize(m_hostStats.m_iFSFree);
        QString strColor = QColor(m_RAMFreeColor).name(QColor::HexRgb);
        m_pFSFreeLabel->setText(QString("<font color=\"%1\">%2: %3</font>").arg(strColor).arg(UIVMActivityOverviewWidget::tr("Free")).arg(strFS));
    }
    if (m_pFSUsedLabel)
    {
        QString strFS = UITranslator::formatSize(m_hostStats.m_iFSTotal - m_hostStats.m_iFSFree);
        QString strColor = QColor(m_RAMUsedColor).name(QColor::HexRgb);
        m_pFSUsedLabel->setText(QString("<font color=\"%1\">%2: %3</font>").arg(strColor).arg(UIVMActivityOverviewWidget::tr("Used")).arg(strFS));
    }
    if (m_pFSTotalLabel)
    {
        QString strFS = UITranslator::formatSize(m_hostStats.m_iFSTotal);
        m_pFSTotalLabel->setText(QString("%1: %2").arg(UIVMActivityOverviewWidget::tr("Total")).arg(strFS));
    }
}



/*********************************************************************************************************************************
*   Class UIVMActivityOverviewTableView implementation.                                                                          *
*********************************************************************************************************************************/

UIVMActivityOverviewTableView::UIVMActivityOverviewTableView(QWidget *pParent /* = 0 */)
    :QTableView(pParent)
{
}

void UIVMActivityOverviewTableView::setMinimumColumnWidths(const QMap<int, int>& widths)
{
    m_minimumColumnWidths = widths;
    resizeHeaders();
}

void UIVMActivityOverviewTableView::updateColumVisibility()
{
    UIActivityOverviewProxyModel *pProxyModel = qobject_cast<UIActivityOverviewProxyModel *>(model());
    if (!pProxyModel)
        return;
    UIActivityOverviewModel *pModel = qobject_cast<UIActivityOverviewModel *>(pProxyModel->sourceModel());
    QHeaderView *pHeader = horizontalHeader();

    if (!pModel || !pHeader)
        return;
    for (int i = (int)VMActivityOverviewColumn_Name; i < (int)VMActivityOverviewColumn_Max; ++i)
    {
        if (!pModel->columnVisible(i))
            pHeader->hideSection(i);
        else
            pHeader->showSection(i);
    }
    resizeHeaders();
}

int UIVMActivityOverviewTableView::selectedItemIndex() const
{
    UIActivityOverviewProxyModel *pModel = qobject_cast<UIActivityOverviewProxyModel*>(model());
    if (!pModel)
        return -1;

    QItemSelectionModel *pSelectionModel =  selectionModel();
    if (!pSelectionModel)
        return -1;
    QModelIndexList selectedItemIndices = pSelectionModel->selectedRows();
    if (selectedItemIndices.isEmpty())
        return -1;

    /* just use the the 1st index: */
    QModelIndex modelIndex = pModel->mapToSource(selectedItemIndices[0]);

    if (!modelIndex.isValid())
        return -1;
    return modelIndex.row();
}

bool UIVMActivityOverviewTableView::hasSelection() const
{
    if (!selectionModel())
        return false;
    return selectionModel()->hasSelection();
}

void UIVMActivityOverviewTableView::resizeEvent(QResizeEvent *pEvent)
{
    resizeHeaders();
    QTableView::resizeEvent(pEvent);
}

void UIVMActivityOverviewTableView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    emit sigSelectionChanged(selected, deselected);
    QTableView::selectionChanged(selected, deselected);
}

void UIVMActivityOverviewTableView::mousePressEvent(QMouseEvent *pEvent)
{
    if (!indexAt(pEvent->position().toPoint()).isValid())
        clearSelection();
    QTableView::mousePressEvent(pEvent);
}

void UIVMActivityOverviewTableView::resizeHeaders()
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
        int iMinWidth = m_minimumColumnWidths.value((VMActivityOverviewColumn)i, 0);
        pHeader->resizeSection(i, iWidth < iMinWidth ? iMinWidth : iWidth);
    }
}


/*********************************************************************************************************************************
*   Class UIActivityOverviewItem implementation.                                                                                 *
*********************************************************************************************************************************/
UIActivityOverviewItem::UIActivityOverviewItem(QObject *pParent, const QUuid &uid, const QString &strVMName)
    : QObject(pParent)
    , m_uTotalRAM(0)
    , m_uFreeRAM(0)
    , m_uUsedRAM(0)
    , m_fRAMUsagePercentage(0)
    , m_VMuid(uid)
    , m_strVMName(strVMName)
{
    m_columnData[VMActivityOverviewColumn_Name] = m_strVMName;
}

UIActivityOverviewItem::UIActivityOverviewItem()
    : QObject()
    , m_uTotalRAM(0)
    , m_uUsedRAM(0)
    , m_fRAMUsagePercentage(0)
    , m_VMuid(QUuid())
{
}

UIActivityOverviewItem::~UIActivityOverviewItem()
{
}

bool UIActivityOverviewItem::operator==(const UIActivityOverviewItem& other) const
{
    if (m_VMuid == other.m_VMuid)
        return true;
    return false;
}

QString UIActivityOverviewItem::columnData(int iColumnIndex) const
{
    return m_columnData.value(iColumnIndex, QString());
}

int UIActivityOverviewItem::columnLength(int iColumnIndex) const
{
    return m_columnData.value(iColumnIndex, QString()).length();
}

const QUuid &UIActivityOverviewItem::machineId() const
{
    return m_VMuid;
}


/*********************************************************************************************************************************
*   Class UIVMActivityOverviewHostStats implementation.                                                                          *
*********************************************************************************************************************************/

UIVMActivityOverviewHostStats::UIVMActivityOverviewHostStats()
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
*   Class UIActivityOverviewItemCloud implementation.                                                                            *
*********************************************************************************************************************************/

UIActivityOverviewItemCloud::UIActivityOverviewItemCloud(QObject *pParent, const QUuid &uid, const QString &strVMName, CCloudMachine &comCloudMachine)
    : UIActivityOverviewItem(pParent, uid, strVMName)
    , m_comCloudMachine(comCloudMachine)
{
    updateMachineState();
    m_pTimer = new QTimer(this);
    if (m_pTimer)
    {
        connect(m_pTimer, &QTimer::timeout, this, &UIActivityOverviewItemCloud::sltTimeout);
        m_pTimer->setInterval(60 * 1000);
    }
    resetColumData();
}

UIActivityOverviewItemCloud::UIActivityOverviewItemCloud()
{
}

UIActivityOverviewItemCloud::~UIActivityOverviewItemCloud()
{
}

void UIActivityOverviewItemCloud::updateMachineState()
{
    if (m_comCloudMachine.isOk())
        setMachineState(m_comCloudMachine.GetState());
}

bool UIActivityOverviewItemCloud::isRunning() const
{
    return m_enmMachineState == KCloudMachineState_Running;
}

bool UIActivityOverviewItemCloud::isCloudVM() const
{
    return true;
}

QString UIActivityOverviewItemCloud::machineStateString() const
{
    if (!m_comCloudMachine.isOk())
        return QString();
    return gpConverter->toString(m_comCloudMachine.GetState());
}

void UIActivityOverviewItemCloud::sltTimeout()
{
    int iDataSize = 1;
    foreach (const KMetricType &enmMetricType, m_availableMetricTypes)
    {
        UIProgressTaskReadCloudMachineMetricData *pTask = new UIProgressTaskReadCloudMachineMetricData(this, m_comCloudMachine,
                                                                                                       enmMetricType, iDataSize);
        connect(pTask, &UIProgressTaskReadCloudMachineMetricData::sigMetricDataReceived,
                this, &UIActivityOverviewItemCloud::sltMetricDataReceived);
        pTask->start();
    }
}

void UIActivityOverviewItemCloud::sltMetricDataReceived(KMetricType enmMetricType,
                                                        const QVector<QString> &data, const QVector<QString> &timeStamps)
{
    Q_UNUSED(timeStamps);
    AssertReturnVoid(!data.isEmpty());

    if (data[0].toFloat() < 0)
        return;

    int iDecimalCount = 2;
    QLocale locale;
    if (enmMetricType == KMetricType_CpuUtilization)
    {
        //QString QLocale::toString(double i, char f = 'g', int prec = 6) const

        // m_columnData[VMActivityOverviewColumn_CPUGuestLoad] =
        //     QString("%1%").arg(QString::number(data[0].toFloat(), 'f', iDecimalCount));

        m_columnData[VMActivityOverviewColumn_CPUGuestLoad] =
            QString("%1%").arg(locale.toString(data[0].toFloat(), 'f', iDecimalCount));
}
    else if (enmMetricType == KMetricType_MemoryUtilization)
    {
         if (m_uTotalRAM != 0)
         {
             quint64 uUsedRAM = (quint64)data[0].toFloat() * (m_uTotalRAM / 100.f);
             m_columnData[VMActivityOverviewColumn_RAMUsedAndTotal] =
                 QString("%1/%2").arg(UITranslator::formatSize(_1K * uUsedRAM, iDecimalCount)).
                 arg(UITranslator::formatSize(_1K * m_uTotalRAM, iDecimalCount));
         }
         m_columnData[VMActivityOverviewColumn_RAMUsedPercentage] =
             QString("%1%").arg(QString::number(data[0].toFloat(), 'f', iDecimalCount));
    }
    else if (enmMetricType == KMetricType_NetworksBytesOut)
        m_columnData[VMActivityOverviewColumn_NetworkUpRate] =
            UITranslator::formatSize((quint64)data[0].toFloat(), iDecimalCount);
    else if (enmMetricType == KMetricType_NetworksBytesIn)
        m_columnData[VMActivityOverviewColumn_NetworkDownRate] =
            UITranslator::formatSize((quint64)data[0].toFloat(), iDecimalCount);
    else if (enmMetricType == KMetricType_DiskBytesRead)
        m_columnData[VMActivityOverviewColumn_DiskIOReadRate] =
            UITranslator::formatSize((quint64)data[0].toFloat(), iDecimalCount);
    else if (enmMetricType == KMetricType_DiskBytesWritten)
        m_columnData[VMActivityOverviewColumn_DiskIOWriteRate] =
            UITranslator::formatSize((quint64)data[0].toFloat(), iDecimalCount);

    sender()->deleteLater();
}

void UIActivityOverviewItemCloud::setMachineState(int iState)
{
    if (iState <= KCloudMachineState_Invalid || iState >= KCloudMachineState_Max)
        return;
    KCloudMachineState enmState = static_cast<KCloudMachineState>(iState);
    if (m_enmMachineState == enmState)
        return;
    m_enmMachineState = enmState;
    if (isRunning())
    {
        getMetricList();
        if (m_uTotalRAM == 0)
            m_uTotalRAM = UIMonitorCommon::determineTotalRAMAmount(m_comCloudMachine);
    }
    else
    {
        if (m_pTimer)
            m_pTimer->stop();
    }
}

void UIActivityOverviewItemCloud::resetColumData()
{
    for (int i = (int) VMActivityOverviewColumn_CPUGuestLoad;
         i < (int)VMActivityOverviewColumn_Max; ++i)
        m_columnData[i] = UIVMActivityOverviewWidget::tr("N/A");
}

void UIActivityOverviewItemCloud::getMetricList()
{
    if (!isRunning())
        return;
    UIProgressTaskReadCloudMachineMetricList *pReadListProgressTask =
        new UIProgressTaskReadCloudMachineMetricList(this, m_comCloudMachine);
    AssertPtrReturnVoid(pReadListProgressTask);
    connect(pReadListProgressTask, &UIProgressTaskReadCloudMachineMetricList::sigMetricListReceived,
            this, &UIActivityOverviewItemCloud::sltMetricNameListingComplete);
    pReadListProgressTask->start();
}

void UIActivityOverviewItemCloud::sltMetricNameListingComplete(QVector<QString> metricNameList)
{
    AssertReturnVoid(m_pTimer);
    m_availableMetricTypes.clear();
    foreach (const QString &strName, metricNameList)
        m_availableMetricTypes << gpConverter->fromInternalString<KMetricType>(strName);

    if (!m_availableMetricTypes.isEmpty())
    {
        /* Dont wait 60 secs: */
        sltTimeout();
        m_pTimer->start();
    }
    else
    {
        m_pTimer->stop();
        resetColumData();
    }

    if (sender())
        sender()->deleteLater();

}


/*********************************************************************************************************************************
*   Class UIActivityOverviewItemLocal implementation.                                                                            *
*********************************************************************************************************************************/
UIActivityOverviewItemLocal::UIActivityOverviewItemLocal(QObject *pParent, const QUuid &uid, const QString &strVMName)
    : UIActivityOverviewItem(pParent, uid, strVMName)
    , m_uVMExitTotal(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
{
    if (m_enmMachineState == KMachineState_Running)
        resetDebugger();
}

UIActivityOverviewItemLocal::UIActivityOverviewItemLocal()
    : m_uVMExitTotal(0)
    , m_uDiskWriteTotal(0)
    , m_uDiskReadTotal(0)
    , m_uNetworkDownTotal(0)
    , m_uNetworkUpTotal(0)
{
}

UIActivityOverviewItemLocal::~UIActivityOverviewItemLocal()
{
    if (!m_comSession.isNull())
        m_comSession.UnlockMachine();
}

bool UIActivityOverviewItemLocal::isWithGuestAdditions()
{
    if (m_comGuest.isNull())
        return false;
    return m_comGuest.GetAdditionsStatus(m_comGuest.GetAdditionsRunLevel());
}

void UIActivityOverviewItemLocal::resetDebugger()
{
    m_comSession = uiCommon().openSession(m_VMuid, KLockType_Shared);
    if (!m_comSession.isNull())
    {
        CConsole comConsole = m_comSession.GetConsole();
        if (!comConsole.isNull())
        {
            m_comGuest = comConsole.GetGuest();
            m_comDebugger = comConsole.GetDebugger();
        }
    }
}

bool UIActivityOverviewItemLocal::isRunning() const
{
    return m_enmMachineState == KMachineState_Running;
}

bool UIActivityOverviewItemLocal::isCloudVM() const
{
    return false;
}

void UIActivityOverviewItemLocal::setMachineState(int iState)
{
    if (iState <= KMachineState_Null || iState >= KMachineState_Max)
        return;
    KMachineState enmState = static_cast<KMachineState>(iState);
    if (m_enmMachineState == enmState)
        return;
    m_enmMachineState = enmState;
    if (m_enmMachineState == KMachineState_Running)
        resetDebugger();
}

void UIActivityOverviewItemLocal::updateColumnData()
{
    AssertReturnVoid(m_comDebugger.isOk());

    int iDecimalCount = 2;

    /* CPU Load: */
    ULONG aPctHalted;
    ULONG  uCPUGuestLoad;
    ULONG uCPUVMMLoad;
    m_comDebugger.GetCPULoad(0x7fffffff, uCPUGuestLoad, aPctHalted, uCPUVMMLoad);
    m_columnData[VMActivityOverviewColumn_CPUGuestLoad] =
        QString("%1%").arg(QString::number(uCPUGuestLoad));
    m_columnData[VMActivityOverviewColumn_CPUVMMLoad] =
        QString("%1%").arg(QString::number(uCPUVMMLoad));

    /* RAM Utilization: */
    if (isWithGuestAdditions())
        m_columnData[VMActivityOverviewColumn_RAMUsedAndTotal] =
            QString("%1/%2").arg(UITranslator::formatSize(_1K * m_uUsedRAM, iDecimalCount)).
            arg(UITranslator::formatSize(_1K * m_uTotalRAM, iDecimalCount));
    else
        m_columnData[VMActivityOverviewColumn_RAMUsedAndTotal] = UIVMActivityOverviewWidget::tr("N/A");

    /* RAM Utilization %: */
    if (isWithGuestAdditions())
        m_columnData[VMActivityOverviewColumn_RAMUsedPercentage] =
            QString("%1%").arg(QString::number(m_fRAMUsagePercentage, 'f', 2));
    else
        m_columnData[VMActivityOverviewColumn_RAMUsedPercentage] = UIVMActivityOverviewWidget::tr("N/A");

    /* Network rate: */
    quint64 uPrevDownTotal = m_uNetworkDownTotal;
    quint64 uPrevUpTotal = m_uNetworkUpTotal;
    UIMonitorCommon::getNetworkLoad(m_comDebugger, m_uNetworkDownTotal, m_uNetworkUpTotal);
    quint64 uNetworkDownRate = m_uNetworkDownTotal - uPrevDownTotal;
    quint64 uNetworkUpRate = m_uNetworkUpTotal - uPrevUpTotal;
    m_columnData[VMActivityOverviewColumn_NetworkUpRate] =
        QString("%1").arg(UITranslator::formatSize(uNetworkUpRate, iDecimalCount));
    m_columnData[VMActivityOverviewColumn_NetworkDownRate] =
        QString("%1").arg(UITranslator::formatSize(uNetworkDownRate, iDecimalCount));
    m_columnData[VMActivityOverviewColumn_NetworkUpTotal] =
        QString("%1").arg(UITranslator::formatSize(m_uNetworkUpTotal, iDecimalCount));
    m_columnData[VMActivityOverviewColumn_NetworkDownTotal] =
        QString("%1").arg(UITranslator::formatSize(m_uNetworkDownTotal, iDecimalCount));


    /* IO rate: */
    quint64 uPrevWriteTotal = m_uDiskWriteTotal;
    quint64 uPrevReadTotal = m_uDiskReadTotal;
    UIMonitorCommon::getDiskLoad(m_comDebugger, m_uDiskWriteTotal, m_uDiskReadTotal);
    quint64 uDiskWriteRate = m_uDiskWriteTotal - uPrevWriteTotal;
    quint64 uDiskReadRate = m_uDiskReadTotal - uPrevReadTotal;
    m_columnData[VMActivityOverviewColumn_DiskIOReadRate] =
        QString("%1").arg(UITranslator::formatSize(uDiskReadRate, iDecimalCount));
    m_columnData[VMActivityOverviewColumn_DiskIOWriteRate] =
        QString("%1").arg(UITranslator::formatSize(uDiskWriteRate, iDecimalCount));
    m_columnData[VMActivityOverviewColumn_DiskIOReadTotal] =
        QString("%1").arg(UITranslator::formatSize(m_uDiskReadTotal, iDecimalCount));
    m_columnData[VMActivityOverviewColumn_DiskIOWriteTotal] =
        QString("%1").arg(UITranslator::formatSize(m_uDiskWriteTotal, iDecimalCount));

    /* VM Exits: */
    quint64 uPrevVMExitsTotal = m_uVMExitTotal;
    UIMonitorCommon::getVMMExitCount(m_comDebugger, m_uVMExitTotal);
    quint64 uVMExitRate = m_uVMExitTotal - uPrevVMExitsTotal;
    m_columnData[VMActivityOverviewColumn_VMExits] =
        QString("%1/%2").arg(UITranslator::addMetricSuffixToNumber(uVMExitRate)).
        arg(UITranslator::addMetricSuffixToNumber(m_uVMExitTotal));
}

QString UIActivityOverviewItemLocal::machineStateString() const
{
    return gpConverter->toString(m_enmMachineState);
}


/*********************************************************************************************************************************
*   Class UIVMActivityOverviewProxyModel implementation.                                                                         *
*********************************************************************************************************************************/
UIActivityOverviewProxyModel::UIActivityOverviewProxyModel(QObject *parent /* = 0 */)
    :QSortFilterProxyModel(parent)
{
}

void UIActivityOverviewProxyModel::dataUpdate()
{
    if (sourceModel())
        emit dataChanged(index(0,0), index(sourceModel()->rowCount(), sourceModel()->columnCount()));
    invalidate();
}

void UIActivityOverviewProxyModel::setNotRunningVMVisibility(bool fShow)
{
    if (m_fShowNotRunningVMs == fShow)
        return;
    m_fShowNotRunningVMs = fShow;
    invalidateFilter();
}

void UIActivityOverviewProxyModel::setCloudVMVisibility(bool fShow)
{
    if (m_fShowCloudVMs == fShow)
        return;
    m_fShowCloudVMs = fShow;
    invalidateFilter();
}

bool UIActivityOverviewProxyModel::lessThan(const QModelIndex &sourceLeftIndex, const QModelIndex &sourceRightIndex) const
{
    UIActivityOverviewModel *pModel = qobject_cast<UIActivityOverviewModel*>(sourceModel());
    if (pModel)
    {
        /* Keep running vm always on top of the list: */
        bool fLeftRunning = pModel->isVMRunning(sourceLeftIndex.row());
        bool fRightRunning = pModel->isVMRunning(sourceRightIndex.row());
        if (fLeftRunning && !fRightRunning)
        {
            if (sortOrder() == Qt::AscendingOrder)
                return true;
            else
                return false;
        }
        if (!fLeftRunning && fRightRunning)
        {
            if (sortOrder() == Qt::AscendingOrder)
                return false;
            else
                return true;
        }
    }
    return QSortFilterProxyModel::lessThan(sourceLeftIndex, sourceRightIndex);
}

bool UIActivityOverviewProxyModel::filterAcceptsRow(int iSourceRow, const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent);
    if (m_fShowNotRunningVMs && m_fShowCloudVMs)
        return true;
    UIActivityOverviewModel *pModel = qobject_cast<UIActivityOverviewModel*>(sourceModel());
    if (!pModel)
        return true;

    if (!m_fShowNotRunningVMs && !pModel->isVMRunning(iSourceRow))
        return false;
    if (!m_fShowCloudVMs && pModel->isCloudVM(iSourceRow))
        return false;
    return true;
}


/*********************************************************************************************************************************
*   Class UIActivityOverviewModel implementation.                                                                                *
*********************************************************************************************************************************/
UIActivityOverviewModel::UIActivityOverviewModel(QObject *parent /*= 0*/)
    :QAbstractTableModel(parent)
    , m_pLocalVMUpdateTimer(new QTimer(this))
{
    initialize();
}

void UIActivityOverviewModel::initialize()
{
    for (int i = 0; i < (int)VMActivityOverviewColumn_Max; ++i)
        m_columnDataMaxLength[i] = 0;

    initializeItems();
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIActivityOverviewModel::sltMachineStateChanged);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineRegistered,
            this, &UIActivityOverviewModel::sltMachineRegistered);
    if (m_pLocalVMUpdateTimer)
    {
        connect(m_pLocalVMUpdateTimer, &QTimer::timeout, this, &UIActivityOverviewModel::sltLocalVMUpdateTimeout);
        m_pLocalVMUpdateTimer->start(1000);
    }
}

int UIActivityOverviewModel::rowCount(const QModelIndex &parent /* = QModelIndex() */) const
{
    Q_UNUSED(parent);
    return m_itemList.size();
}

int UIActivityOverviewModel::columnCount(const QModelIndex &parent /* = QModelIndex() */) const
{
    Q_UNUSED(parent);
    return VMActivityOverviewColumn_Max;
}

void UIActivityOverviewModel::setShouldUpdate(bool fShouldUpdate)
{
    if (m_pLocalVMUpdateTimer)
    {
        if (fShouldUpdate)
            m_pLocalVMUpdateTimer->start();
        else
            m_pLocalVMUpdateTimer->stop();
    }
}

const QMap<int, int> UIActivityOverviewModel::dataLengths() const
{
    return m_columnDataMaxLength;
}

QUuid UIActivityOverviewModel::itemUid(int iIndex)
{
    if (iIndex >= m_itemList.size() || !m_itemList[iIndex])
        return QUuid();
    return m_itemList[iIndex]->machineId();
}

int UIActivityOverviewModel::itemIndex(const QUuid &uid)
{
    for (int i = 0; i < m_itemList.size(); ++i)
    {
        if (!m_itemList[i])
            continue;
        if (m_itemList[i]->machineId() == uid)
            return i;
    }
    return -1;
}

bool UIActivityOverviewModel::isVMRunning(int rowIndex) const
{
    if (rowIndex >= m_itemList.size() || rowIndex < 0 || !m_itemList[rowIndex])
        return false;
    return m_itemList[rowIndex]->isRunning();
}

bool UIActivityOverviewModel::isCloudVM(int rowIndex) const
{
    if (rowIndex >= m_itemList.size() || rowIndex < 0 || !m_itemList[rowIndex])
        return false;
    return m_itemList[rowIndex]->isCloudVM();
}

void UIActivityOverviewModel::setCloudMachineItems(const QList<UIVirtualMachineItemCloud*> &cloudItems)
{
    QVector<QUuid> newIds;
    foreach (const UIVirtualMachineItemCloud* pItem, cloudItems)
    {
        if (!pItem)
            continue;
        QUuid id = pItem->machineId();
        if (id.isNull())
            continue;
        newIds << id;
    }
    QVector<UIActivityOverviewItem*> originalItemList = m_itemList;

    /* Remove m_itemList items that are not in @cloudItems: */
    QMutableVectorIterator<UIActivityOverviewItem*> iterator(m_itemList);
    while (iterator.hasNext())
    {
        UIActivityOverviewItem *pItem = iterator.next();
        if (!pItem->isCloudVM())
            continue;
        if (pItem && !newIds.contains(pItem->machineId()))
            iterator.remove();
    }

    /* Add items that are not in m_itemList: */
    foreach (const UIVirtualMachineItemCloud* pItem, cloudItems)
    {
        if (!pItem)
            continue;
        CCloudMachine comMachine = pItem->machine();
        if (!comMachine.isOk())
            continue;
        QUuid id = comMachine.GetId();
        /* Linearly search for the vm with th same id. I cannot make QVector::contain work since we store pointers: */
        bool fFound = false;
        for (int i = 0; i < m_itemList.size() && !fFound; ++i)
        {
            if (m_itemList[i] && m_itemList[i]->machineId() == id)
                fFound = true;
        }
        if (!fFound)
            m_itemList.append(new UIActivityOverviewItemCloud(this, id, comMachine.GetName(), comMachine));
    }

    /* Update cloud machine states: */
    for (int i = 0; i < m_itemList.size(); ++i)
    {
        if (!m_itemList[i] || !m_itemList[i]->isCloudVM())
            continue;
        UIActivityOverviewItemCloud *pItem = qobject_cast<UIActivityOverviewItemCloud*>(m_itemList[i]);
        if (!pItem)
            continue;
        pItem->updateMachineState();
    }
}

QVariant UIActivityOverviewModel::data(const QModelIndex &index, int role) const
{
    if (!isVMRunning(index.row()))
    {
        if (role == Qt::FontRole)
        {
            QFont font = qApp->font();
            font.setItalic(true);
            return font;
        }
        if (role == Qt::ForegroundRole)
        {
            /* For some reason QColor::lighter(..) is not working here: */
            QColor color = qApp->palette().color(QPalette::Active, QPalette::WindowText);
            int h, s, l;
            color.getHsl(&h, &s, &l);
            l = 100;
            return QColor::fromHsl(h, s, l);
        }
    }
    if (!index.isValid() || role != Qt::DisplayRole || index.row() >= rowCount())
        return QVariant();

    UIActivityOverviewItem *pItem = m_itemList[index.row()];
    AssertPtrReturn(pItem, QVariant());

    if (index.column() == VMActivityOverviewColumn_Name)
        return pItem->columnData(index.column());

    if (!pItem->isRunning())
        return pItem->machineStateString();

    return pItem->columnData(index.column());
}

void UIActivityOverviewModel::clearData()
{
    /* We have a request to detach COM stuff,
     * first of all we are removing all the items,
     * this will detach COM wrappers implicitly: */
    qDeleteAll(m_itemList);
    m_itemList.clear();
    /* Detaching perf. collector finally,
     * please do not use it after all: */
    m_performanceCollector.detach();
}

QVariant UIActivityOverviewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        return m_columnTitles.value((VMActivityOverviewColumn)section, QString());;
    return QVariant();
}

void UIActivityOverviewModel::setColumnCaptions(const QMap<int, QString>& captions)
{
    m_columnTitles = captions;
}

void UIActivityOverviewModel::initializeItems()
{
    foreach (const CMachine &comMachine, uiCommon().virtualBox().GetMachines())
    {
        if (!comMachine.isNull())
            addItem(comMachine.GetId(), comMachine.GetName(), comMachine.GetState());
    }
    setupPerformanceCollector();
}

void UIActivityOverviewModel::sltMachineStateChanged(const QUuid &uId, const KMachineState state)
{
    int iIndex = itemIndex(uId);
    if (iIndex != -1 && iIndex < m_itemList.size())
    {
        UIActivityOverviewItemLocal *pItem = qobject_cast<UIActivityOverviewItemLocal*>(m_itemList[iIndex]);
        if (pItem)
        {
            pItem->setMachineState(state);
            if (state == KMachineState_Running)
                pItem->resetDebugger();
        }
    }
}

void UIActivityOverviewModel::sltMachineRegistered(const QUuid &uId, bool fRegistered)
{
    if (fRegistered)
    {
        CMachine comMachine = uiCommon().virtualBox().FindMachine(uId.toString());
        if (!comMachine.isNull())
            addItem(uId, comMachine.GetName(), comMachine.GetState());
    }
    else
        removeItem(uId);
    emit sigDataUpdate();
}

void UIActivityOverviewModel::getHostRAMStats()
{
    CHost comHost = uiCommon().host();
    m_hostStats.m_iRAMTotal = _1M * (quint64)comHost.GetMemorySize();
    m_hostStats.m_iRAMFree = _1M * (quint64)comHost.GetMemoryAvailable();
}

void UIActivityOverviewModel::sltLocalVMUpdateTimeout()
{
    /* Host's RAM usage is obtained from IHost not from IPerformanceCollector: */
    getHostRAMStats();

    /* Use IPerformanceCollector to update VM RAM usage and Host CPU and file IO stats: */
    queryPerformanceCollector();

    for (int i = 0; i < m_itemList.size(); ++i)
    {
        UIActivityOverviewItemLocal *pItem = qobject_cast<UIActivityOverviewItemLocal*>(m_itemList[i]);
        if (!pItem || !pItem->isRunning())
            continue;
        pItem->updateColumnData();
    }

    for (int i = 0; i < (int)VMActivityOverviewColumn_Max; ++i)
    {
        for (int j = 0; j < m_itemList.size(); ++j)
            if (m_columnDataMaxLength.value(i, 0) < m_itemList[j]->columnLength(i))
                m_columnDataMaxLength[i] = m_itemList[j]->columnLength(i);
    }

    emit sigDataUpdate();
    emit sigHostStatsUpdate(m_hostStats);
}

void UIActivityOverviewModel::setupPerformanceCollector()
{
    m_nameList.clear();
    m_objectList.clear();
    /* Initialize and configure CPerformanceCollector: */
    const ULONG iPeriod = 1;
    const int iMetricSetupCount = 1;
    if (m_performanceCollector.isNull())
        m_performanceCollector = uiCommon().virtualBox().GetPerformanceCollector();
    for (int i = 0; i < m_itemList.size(); ++i)
        m_nameList << "Guest/RAM/Usage*";
    /* This is for the host: */
    m_nameList << "CPU*";
    m_nameList << "FS*";
    m_objectList = QVector<CUnknown>(m_nameList.size(), CUnknown());
    m_performanceCollector.SetupMetrics(m_nameList, m_objectList, iPeriod, iMetricSetupCount);
}

void UIActivityOverviewModel::queryPerformanceCollector()
{
    QVector<QString>  aReturnNames;
    QVector<CUnknown>  aReturnObjects;
    QVector<QString>  aReturnUnits;
    QVector<ULONG>  aReturnScales;
    QVector<ULONG>  aReturnSequenceNumbers;
    QVector<ULONG>  aReturnDataIndices;
    QVector<ULONG>  aReturnDataLengths;

    QVector<LONG> returnData = m_performanceCollector.QueryMetricsData(m_nameList,
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
                    int iIndex = itemIndex(comMachine.GetId());
                    if (iIndex == -1 || iIndex >= m_itemList.size() || !m_itemList[iIndex])
                        continue;

                    if (aReturnNames[i].contains("Total", Qt::CaseInsensitive))
                        m_itemList[iIndex]->m_uTotalRAM = fData;
                    else
                        m_itemList[iIndex]->m_uFreeRAM = fData;
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
        if (!m_itemList[i] || m_itemList[i]->isCloudVM())
            continue;
        m_itemList[i]->m_uUsedRAM = m_itemList[i]->m_uTotalRAM - m_itemList[i]->m_uFreeRAM;
        if (m_itemList[i]->m_uTotalRAM != 0)
            m_itemList[i]->m_fRAMUsagePercentage = 100.f * (m_itemList[i]->m_uUsedRAM / (float)m_itemList[i]->m_uTotalRAM);
    }
}

void UIActivityOverviewModel::addItem(const QUuid& uMachineId, const QString& strMachineName, KMachineState enmState)
{
    UIActivityOverviewItemLocal *pItem = new UIActivityOverviewItemLocal(this, uMachineId, strMachineName);
    pItem->setMachineState(enmState);
    m_itemList.append(pItem);
}

void UIActivityOverviewModel::removeItem(const QUuid& uMachineId)
{
    int iIndex = itemIndex(uMachineId);
    if (iIndex == -1)
        return;
    delete m_itemList[iIndex];
    m_itemList.remove(iIndex);
}

void UIActivityOverviewModel::setColumnVisible(const QMap<int, bool>& columnVisible)
{
    m_columnVisible = columnVisible;
}

bool UIActivityOverviewModel::columnVisible(int iColumnId) const
{
    return m_columnVisible.value(iColumnId, true);
}


/*********************************************************************************************************************************
*   Class UIVMActivityOverviewWidget implementation.                                                                             *
*********************************************************************************************************************************/

UIVMActivityOverviewWidget::UIVMActivityOverviewWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                                 bool fShowToolbar /* = true */, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_fShowToolbar(fShowToolbar)
    , m_pToolBar(0)
    , m_pTableView(0)
    , m_pProxyModel(0)
    , m_pModel(0)
    , m_pColumnVisibilityToggleMenu(0)
    , m_pHostStatsWidget(0)
    , m_fIsCurrentTool(true)
    , m_iSortIndicatorWidth(0)
    , m_fShowNotRunningVMs(false)
    , m_fShowCloudVMs(true)
{
    prepare();
}

QMenu *UIVMActivityOverviewWidget::menu() const
{
    return NULL;
}

QMenu *UIVMActivityOverviewWidget::columnVisiblityToggleMenu() const
{
    return m_pColumnVisibilityToggleMenu;
}

bool UIVMActivityOverviewWidget::isCurrentTool() const
{
    return m_fIsCurrentTool;
}

void UIVMActivityOverviewWidget::setIsCurrentTool(bool fIsCurrentTool)
{
    m_fIsCurrentTool = fIsCurrentTool;
    if (m_pModel)
        m_pModel->setShouldUpdate(fIsCurrentTool);
}

void UIVMActivityOverviewWidget::setCloudMachineItems(const QList<UIVirtualMachineItemCloud*> &cloudItems)
{
    if (m_pModel)
        m_pModel->setCloudMachineItems(cloudItems);
}

void UIVMActivityOverviewWidget::retranslateUi()
{
    m_columnTitles[VMActivityOverviewColumn_Name] = UIVMActivityOverviewWidget::tr("VM Name");
    m_columnTitles[VMActivityOverviewColumn_CPUGuestLoad] = UIVMActivityOverviewWidget::tr("CPU Guest");
    m_columnTitles[VMActivityOverviewColumn_CPUVMMLoad] = UIVMActivityOverviewWidget::tr("CPU VMM");
    m_columnTitles[VMActivityOverviewColumn_RAMUsedAndTotal] = UIVMActivityOverviewWidget::tr("RAM Used/Total");
    m_columnTitles[VMActivityOverviewColumn_RAMUsedPercentage] = UIVMActivityOverviewWidget::tr("RAM %");
    m_columnTitles[VMActivityOverviewColumn_NetworkUpRate] = UIVMActivityOverviewWidget::tr("Network Up Rate");
    m_columnTitles[VMActivityOverviewColumn_NetworkDownRate] = UIVMActivityOverviewWidget::tr("Network Down Rate");
    m_columnTitles[VMActivityOverviewColumn_NetworkUpTotal] = UIVMActivityOverviewWidget::tr("Network Up Total");
    m_columnTitles[VMActivityOverviewColumn_NetworkDownTotal] = UIVMActivityOverviewWidget::tr("Network Down Total");
    m_columnTitles[VMActivityOverviewColumn_DiskIOReadRate] = UIVMActivityOverviewWidget::tr("Disk Read Rate");
    m_columnTitles[VMActivityOverviewColumn_DiskIOWriteRate] = UIVMActivityOverviewWidget::tr("Disk Write Rate");
    m_columnTitles[VMActivityOverviewColumn_DiskIOReadTotal] = UIVMActivityOverviewWidget::tr("Disk Read Total");
    m_columnTitles[VMActivityOverviewColumn_DiskIOWriteTotal] = UIVMActivityOverviewWidget::tr("Disk Write Total");
    m_columnTitles[VMActivityOverviewColumn_VMExits] = UIVMActivityOverviewWidget::tr("VM Exits");

    updateColumnsMenu();

    if (m_pModel)
        m_pModel->setColumnCaptions(m_columnTitles);

    computeMinimumColumnWidths();
}

void UIVMActivityOverviewWidget::showEvent(QShowEvent *pEvent)
{
    if (m_pVMActivityMonitorAction && m_pTableView)
        m_pVMActivityMonitorAction->setEnabled(m_pTableView->hasSelection());

    QIWithRetranslateUI<QWidget>::showEvent(pEvent);
}

void UIVMActivityOverviewWidget::prepare()
{
    /* Try to guest the sort indicator's width: */
    int iIndicatorMargin = 3;
    QIcon sortIndicator = qApp->QApplication::style()->standardIcon(QStyle::SP_TitleBarUnshadeButton);
    QList<QSize> iconSizes = sortIndicator.availableSizes();
    foreach(const QSize &msize, iconSizes)
        m_iSortIndicatorWidth = qMax(m_iSortIndicatorWidth, msize.width());
    if (m_iSortIndicatorWidth == 0)
        m_iSortIndicatorWidth = 20;
    m_iSortIndicatorWidth += 2 * iIndicatorMargin;

    prepareWidgets();
    loadSettings();
    prepareActions();
    retranslateUi();
    updateModelColumVisibilityCache();
    uiCommon().setHelpKeyword(this, "vm-activity-overview-widget");
    connect(&uiCommon(), &UICommon::sigAskToCommitData,
            this, &UIVMActivityOverviewWidget::sltSaveSettings);
    connect(&uiCommon(), &UICommon::sigAskToDetachCOM,
            this, &UIVMActivityOverviewWidget::sltClearCOMData);
    sltCloudVMVisibility(m_fShowCloudVMs);
}

void UIVMActivityOverviewWidget::prepareWidgets()
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

    m_pHostStatsWidget = new UIVMActivityOverviewHostStatsWidget;
    if (m_pHostStatsWidget)
        layout()->addWidget(m_pHostStatsWidget);

    m_pModel = new UIActivityOverviewModel(this);
    m_pProxyModel = new UIActivityOverviewProxyModel(this);
    m_pTableView = new UIVMActivityOverviewTableView();
    if (m_pTableView && m_pModel && m_pProxyModel)
    {
        layout()->addWidget(m_pTableView);
        m_pProxyModel->setSourceModel(m_pModel);
        m_pProxyModel->setNotRunningVMVisibility(m_fShowNotRunningVMs);
        m_pProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        m_pTableView->setModel(m_pProxyModel);
        m_pTableView->setItemDelegate(new UIVMActivityOverviewDelegate(this));
        m_pTableView->setSelectionMode(QAbstractItemView::SingleSelection);
        m_pTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_pTableView->setShowGrid(false);
        m_pTableView->setContextMenuPolicy(Qt::CustomContextMenu);
        m_pTableView->horizontalHeader()->setHighlightSections(false);
        m_pTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_pTableView->verticalHeader()->setVisible(false);
        m_pTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        /* Minimize the row height: */
        m_pTableView->verticalHeader()->setDefaultSectionSize(m_pTableView->verticalHeader()->minimumSectionSize());
        m_pTableView->setAlternatingRowColors(true);
        m_pTableView->setSortingEnabled(true);
        m_pTableView->sortByColumn(0, Qt::AscendingOrder);

        connect(m_pModel, &UIActivityOverviewModel::sigDataUpdate,
                this, &UIVMActivityOverviewWidget::sltHandleDataUpdate);
        connect(m_pModel, &UIActivityOverviewModel::sigHostStatsUpdate,
                this, &UIVMActivityOverviewWidget::sltHandleHostStatsUpdate);
        connect(m_pTableView, &UIVMActivityOverviewTableView::customContextMenuRequested,
                this, &UIVMActivityOverviewWidget::sltHandleTableContextMenuRequest);
        connect(m_pTableView, &UIVMActivityOverviewTableView::sigSelectionChanged,
                this, &UIVMActivityOverviewWidget::sltHandleTableSelectionChanged);
        updateModelColumVisibilityCache();
    }
}

void UIVMActivityOverviewWidget::updateColumnsMenu()
{
    UIMenu *pMenu = m_pActionPool->action(UIActionIndexMN_M_VMActivityOverview_M_Columns)->menu();
    if (!pMenu)
        return;
    pMenu->clear();
    for (int i = 0; i < VMActivityOverviewColumn_Max; ++i)
    {
        QAction *pAction = pMenu->addAction(m_columnTitles[i]);
        pAction->setCheckable(true);
        if (i == (int)VMActivityOverviewColumn_Name)
            pAction->setEnabled(false);
        pAction->setData(i);
        pAction->setChecked(columnVisible(i));
        connect(pAction, &QAction::toggled, this, &UIVMActivityOverviewWidget::sltHandleColumnAction);
    }
}

void UIVMActivityOverviewWidget::prepareActions()
{
    updateColumnsMenu();
    m_pVMActivityMonitorAction =
        m_pActionPool->action(UIActionIndexMN_M_VMActivityOverview_S_SwitchToMachineActivity);

    if (m_pVMActivityMonitorAction)
        connect(m_pVMActivityMonitorAction, &QAction::triggered, this, &UIVMActivityOverviewWidget::sltHandleShowVMActivityMonitor);
}

void UIVMActivityOverviewWidget::prepareToolBar()
{
    /* Create toolbar: */
    m_pToolBar = new QIToolBar(parentWidget());
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

void UIVMActivityOverviewWidget::loadSettings()
{
    /* Load the list of hidden columns: */
    QStringList hiddenColumnList = gEDataManager->VMActivityOverviewHiddenColumnList();
    for (int i = (int)VMActivityOverviewColumn_Name; i < (int)VMActivityOverviewColumn_Max; ++i)
        m_columnVisible[i] = true;
    foreach(const QString& strColumn, hiddenColumnList)
        setColumnVisible((int)gpConverter->fromInternalString<VMActivityOverviewColumn>(strColumn), false);
    /* Load other options: */
    sltNotRunningVMVisibility(gEDataManager->VMActivityOverviewShowAllMachines());
}

void UIVMActivityOverviewWidget::sltSaveSettings()
{
    /* Save the list of hidden columns: */
    QStringList hiddenColumnList;
    for (int i = 0; i < m_columnVisible.size(); ++i)
    {
        if (!columnVisible(i))
            hiddenColumnList << gpConverter->toInternalString((VMActivityOverviewColumn) i);
    }
    gEDataManager->setVMActivityOverviewHiddenColumnList(hiddenColumnList);
    gEDataManager->setVMActivityOverviewShowAllMachines(m_fShowNotRunningVMs);
}

void UIVMActivityOverviewWidget::sltClearCOMData()
{
    if (m_pModel)
        m_pModel->clearData();
}

void UIVMActivityOverviewWidget::sltToggleColumnSelectionMenu(bool fChecked)
{
    (void)fChecked;
    if (!m_pColumnVisibilityToggleMenu)
        return;
    m_pColumnVisibilityToggleMenu->exec(this->mapToGlobal(QPoint(0,0)));
}

void UIVMActivityOverviewWidget::sltHandleColumnAction(bool fChecked)
{
    QAction* pSender = qobject_cast<QAction*>(sender());
    if (!pSender)
        return;
    setColumnVisible(pSender->data().toInt(), fChecked);
}

void UIVMActivityOverviewWidget::sltHandleHostStatsUpdate(const UIVMActivityOverviewHostStats &stats)
{
    if (m_pHostStatsWidget)
        m_pHostStatsWidget->setHostStats(stats);
}

void UIVMActivityOverviewWidget::sltHandleDataUpdate()
{
    computeMinimumColumnWidths();
    if (m_pProxyModel)
        m_pProxyModel->dataUpdate();
}

void UIVMActivityOverviewWidget::sltHandleTableContextMenuRequest(const QPoint &pos)
{
    if (!m_pTableView)
        return;

    QMenu menu;
    if (m_pVMActivityMonitorAction)
        menu.addAction(m_pVMActivityMonitorAction);
    menu.addSeparator();

    QAction *pHideNotRunningAction =
        menu.addAction(UIVMActivityOverviewWidget::tr("List all virtual machines"));
    pHideNotRunningAction->setCheckable(true);
    pHideNotRunningAction->setChecked(m_fShowNotRunningVMs);
    connect(pHideNotRunningAction, &QAction::triggered,
            this, &UIVMActivityOverviewWidget::sltNotRunningVMVisibility);

    QAction *pShowCloudVMsAction =
        menu.addAction(UIVMActivityOverviewWidget::tr("Show cloud virtual machines"));
    pShowCloudVMsAction->setCheckable(true);
    pShowCloudVMsAction->setChecked(m_fShowCloudVMs);
    connect(pShowCloudVMsAction, &QAction::triggered,
            this, &UIVMActivityOverviewWidget::sltCloudVMVisibility);

    menu.exec(m_pTableView->mapToGlobal(pos));
}

void UIVMActivityOverviewWidget::sltHandleTableSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);
    if (!m_pVMActivityMonitorAction || !m_pModel || !m_pProxyModel)
        return;

    if (selected.indexes().empty())
    {
        m_pVMActivityMonitorAction->setEnabled(false);
        return;
    }
    int iMachineIndex = m_pProxyModel->mapToSource(selected.indexes()[0]).row();
    if (!m_pModel->isVMRunning(iMachineIndex))
    {
        m_pVMActivityMonitorAction->setEnabled(false);
        return;
    }
    m_pVMActivityMonitorAction->setEnabled(true);
}

void UIVMActivityOverviewWidget::sltHandleShowVMActivityMonitor()
{
    if (!m_pTableView || !m_pModel)
        return;
    const QUuid uMachineId = m_pModel->itemUid(m_pTableView->selectedItemIndex());
    if (uMachineId.isNull())
        return;
    emit sigSwitchToMachineActivityPane(uMachineId);
}

void UIVMActivityOverviewWidget::sltNotRunningVMVisibility(bool fShow)
{
    m_fShowNotRunningVMs = fShow;
    if (m_pProxyModel)
        m_pProxyModel->setNotRunningVMVisibility(m_fShowNotRunningVMs);
}

void UIVMActivityOverviewWidget::sltCloudVMVisibility(bool fShow)
{
    m_fShowCloudVMs = fShow;
    if (m_pProxyModel)
        m_pProxyModel->setCloudVMVisibility(m_fShowCloudVMs);
}

void UIVMActivityOverviewWidget::setColumnVisible(int iColumnId, bool fVisible)
{
    if (m_columnVisible.contains(iColumnId) && m_columnVisible[iColumnId] == fVisible)
        return;
    m_columnVisible[iColumnId] = fVisible;
    updateModelColumVisibilityCache();
}

void UIVMActivityOverviewWidget::updateModelColumVisibilityCache()
{
    if (m_pModel)
        m_pModel->setColumnVisible(m_columnVisible);
    /* Notify the table view for the changed column visibility: */
    if (m_pTableView)
        m_pTableView->updateColumVisibility();
}

void UIVMActivityOverviewWidget::computeMinimumColumnWidths()
{
    if (!m_pTableView || !m_pModel)
        return;
    QFontMetrics fontMetrics(m_pTableView->font());
    const QMap<int, int> &columnDataStringLengths = m_pModel->dataLengths();
    QMap<int, int> columnWidthsInPixels;
    for (int i = 0; i < (int)VMActivityOverviewColumn_Max; ++i)
    {
        int iColumnStringWidth = columnDataStringLengths.value(i, 0);
        int iColumnTitleWidth = m_columnTitles.value(i, QString()).length();
        int iMax = iColumnStringWidth > iColumnTitleWidth ? iColumnStringWidth : iColumnTitleWidth;
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
        columnWidthsInPixels[i] = iMax * fontMetrics.horizontalAdvance('x') +
#else
        columnWidthsInPixels[i] = iMax * fontMetrics.width('x') +
#endif
            QApplication::style()->pixelMetric(QStyle::PM_LayoutLeftMargin) +
            QApplication::style()->pixelMetric(QStyle::PM_LayoutRightMargin) +
            m_iSortIndicatorWidth;
    }
    m_pTableView->setMinimumColumnWidths(columnWidthsInPixels);
}

bool UIVMActivityOverviewWidget::columnVisible(int iColumnId) const
{
    return m_columnVisible.value(iColumnId, true);
}

#include "UIVMActivityOverviewWidget.moc"
