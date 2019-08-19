/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationConfiguration class implementation.
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

/* COM includes: */

/* Qt includes: */
#include <QApplication>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

/* GUI includes: */
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIInformationConfiguration.h"
#include "UIInformationDataItem.h"
#include "UIInformationItem.h"
#include "UIInformationView.h"
#include "UIInformationModel.h"


const unsigned iColumCount = 3;

class UIInformationTableRow
{
public:

    UIInformationTableRow(UIInformationConfiguration::TableRow row);
    ~UIInformationTableRow();

    QTableWidgetItem *addItem(unsigned iColumn, const QIcon &icon, const QString &strText);
    QTableWidgetItem *addItem(unsigned iColumn, const QString &strText);

private:

    QList<QTableWidgetItem*> m_items;
    UIInformationConfiguration::TableRow m_enmRow;
};

UIInformationTableRow::UIInformationTableRow(UIInformationConfiguration::TableRow enmRow)
    : m_enmRow(enmRow)
{
    m_items.reserve(iColumCount);
}

UIInformationTableRow::~UIInformationTableRow()
{
    for (int i = 0; i < m_items.size(); ++i)
        delete m_items[i];
    m_items.clear();
}


QTableWidgetItem *UIInformationTableRow::addItem(unsigned iColumn, const QIcon &icon, const QString &strText)
{
    if (iColumn >= iColumCount)
        return 0;
    /* Deallocate first if we have something on the column already: */
    if (m_items.size() > (int)iColumn && m_items[iColumn])
        delete m_items[iColumn];
    QTableWidgetItem *pItem = new QTableWidgetItem(icon, strText);
    m_items.insert(iColumn, pItem);
    return pItem;
}

QTableWidgetItem *UIInformationTableRow::addItem(unsigned iColumn, const QString &strText)
{
    if (iColumn >= iColumCount)
        return 0;
    /* Deallocate first if we have something on the column already: */
    if (m_items.size() > (int)iColumn && m_items[iColumn])
        delete m_items[iColumn];
    QTableWidgetItem *pItem = new QTableWidgetItem(strText);
    m_items.insert(iColumn, pItem);
    return pItem;
}

UIInformationConfiguration::UIInformationConfiguration(QWidget *pParent, const CMachine &machine, const CConsole &console)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_machine(machine)
    , m_console(console)
    , m_pMainLayout(0)
    , m_pModel(0)
    , m_pView(0)
    , m_pTableWidget(0)
{
    /* Prepare model: */
    prepareModel();

    /* Prepare view: */
    prepareObjects();
    retranslateUi();
    createTableItems();
}

UIInformationConfiguration::~UIInformationConfiguration()
{
    qDeleteAll(m_rows);
    m_rows.clear();
}

void UIInformationConfiguration::retranslateUi()
{
    m_strGeneralTitle = QApplication::translate("UIVMInformationDialog", "General");
    m_strGeneralName = QApplication::translate("UIVMInformationDialog", "Name");
    m_strGeneralOSType = QApplication::translate("UIVMInformationDialog", "Operating System");
    m_strSystemTitle = QApplication::translate("UIVMInformationDialog", "System");
    m_strError = QApplication::translate("UIVMInformationDialog", "Not Detected");
}

void UIInformationConfiguration::prepareModel()
{
    /* Create information-model: */
    m_pModel = new UIInformationModel(this, m_machine, m_console);
    AssertPtrReturnVoid(m_pModel);
    {
        /* Create general data-item: */
        UIInformationDataItem *pGeneral = new UIInformationDataGeneral(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pGeneral);
        {
            /* Add general data-item to model: */
            m_pModel->addItem(pGeneral);
        }

        /* Create system data-item: */
        UIInformationDataItem *pSystem = new UIInformationDataSystem(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pSystem);
        {
            /* Add system data-item to model: */
            m_pModel->addItem(pSystem);
        }

        /* Create display data-item: */
        UIInformationDataItem *pDisplay = new UIInformationDataDisplay(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pDisplay);
        {
            /* Add display data-item to model: */
            m_pModel->addItem(pDisplay);
        }

        /* Create storage data-item: */
        UIInformationDataItem *pStorage = new UIInformationDataStorage(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pStorage);
        {
            /* Add storage data-item to model: */
            m_pModel->addItem(pStorage);
        }

        /* Create audio data-item: */
        UIInformationDataItem *pAudio = new UIInformationDataAudio(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pAudio);
        {
            /* Add audio data-item to model: */
            m_pModel->addItem(pAudio);
        }

        /* Create network data-item: */
        UIInformationDataItem *pNetwork = new UIInformationDataNetwork(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pNetwork);
        {
            /* Add network data-item to model: */
            m_pModel->addItem(pNetwork);
        }

        /* Create serial-ports data-item: */
        UIInformationDataItem *pSerialPorts = new UIInformationDataSerialPorts(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pSerialPorts);
        {
            /* Add serial-ports data-item to model: */
            m_pModel->addItem(pSerialPorts);
        }

        /* Create usb data-item: */
        UIInformationDataItem *pUSB = new UIInformationDataUSB(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pUSB);
        {
            /* Add usb data-item to model: */
            m_pModel->addItem(pUSB);
        }

        /* Create shared-folders data-item: */
        UIInformationDataItem *pSharedFolders = new UIInformationDataSharedFolders(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pSharedFolders);
        {
            /* Add shared-folders data-item to model: */
            m_pModel->addItem(pSharedFolders);
        }
    }
}

void UIInformationConfiguration::prepareObjects()
{
    /* Create layout: */
    m_pMainLayout = new QVBoxLayout(this);
    if (!m_pMainLayout)
        return;
    m_pMainLayout->setSpacing(0);


    /* Create information-view: */
    m_pView = new UIInformationView;
    AssertPtrReturnVoid(m_pView);
    {
        /* Configure information-view: */
        m_pView->setResizeMode(QListView::Adjust);

        /* Create information-delegate item: */
        UIInformationItem *pItem = new UIInformationItem(m_pView);
        AssertPtrReturnVoid(pItem);
        {
            /* Set item-delegate for information-view: */
            m_pView->setItemDelegate(pItem);
        }
        /* Connect data changed signal: */
        connect(m_pModel, &UIInformationModel::dataChanged, m_pView, &UIInformationView::updateData);

        /* Set model for view: */
        m_pView->setModel(m_pModel);
        /* Add information-view to the layout: */
        m_pMainLayout->addWidget(m_pView);
    }

    m_pTableWidget = new QTableWidget;

    if (m_pTableWidget)
    {
        /* Configure the table by hiding the headers etc.: */
        m_pTableWidget->setRowCount(TableRow_Max);
        m_pTableWidget->setColumnCount(3);
        m_pTableWidget->verticalHeader()->hide();
        m_pTableWidget->horizontalHeader()->hide();
        //m_pTableWidget->setShowGrid(false);
        m_pMainLayout->addWidget(m_pTableWidget);
        m_pTableWidget->hide();
    }
}

void UIInformationConfiguration::updateTable()
{
}

void UIInformationConfiguration::createTableItems()
{
    if (!m_pTableWidget)
        return;
    QFontMetrics fontMetrics(m_pTableWidget->font());

    int iMaxColumn1Length = 0;

    /* General Section: */
    insertTitleRow(TableRow_General_Title, m_strGeneralTitle, UIIconPool::iconSet(":/machine_16px.png"));
    QString strMachineName = m_machine.isNull() ? m_strError : m_machine.GetName();
    insertInfoRow(TableRow_General_Name, m_strGeneralName, strMachineName, fontMetrics, iMaxColumn1Length);
    QString strOSTypeName;
    if (!m_console.isNull())
    {
        CGuest comGuest = m_console.GetGuest();
        if (!comGuest.isNull())
            strOSTypeName = uiCommon().vmGuestOSTypeDescription(comGuest.GetOSTypeId());
    }
    if (strOSTypeName.isEmpty())
        strOSTypeName = m_strError;
    insertInfoRow(TableRow_General_OSType, m_strGeneralOSType, strOSTypeName, fontMetrics, iMaxColumn1Length);
    insertTitleRow(TableRow_System_Title, m_strSystemTitle, UIIconPool::iconSet(":/chipset_16px.png"));


    m_pTableWidget->resizeColumnToContents(0);
    //m_pTableWidget->resizeColumnToContents(1);
    m_pTableWidget->setColumnWidth(1, 1.5 * iMaxColumn1Length);
    m_pTableWidget->resizeColumnToContents(2);

}

void UIInformationConfiguration::insertTitleRow(TableRow enmRow, const QString &strTitle, const QIcon &icon)
{
    UIInformationTableRow *pRow = new UIInformationTableRow(enmRow);
    m_rows.insert(enmRow, pRow);
    m_pTableWidget->setItem(enmRow, 0, pRow->addItem(0, icon, ""));
    QTableWidgetItem *pItem = pRow->addItem(1, strTitle);
    QFont font = pItem->font();
    font.setBold(true);
    pItem->setFont(font);
    m_pTableWidget->setItem(enmRow, 1, pItem);
}

void UIInformationConfiguration::insertInfoRow(TableRow enmRow, const QString &strColumn1, const QString &strColumn2,
                                               QFontMetrics &fontMetrics, int &iMaxColumn1Length)
{
    UIInformationTableRow *pRow = new UIInformationTableRow(enmRow);
    m_rows.insert(enmRow, pRow);
    m_pTableWidget->setItem(enmRow, 1, pRow->addItem(1, strColumn1));
    m_pTableWidget->setItem(enmRow, 2, pRow->addItem(1, strColumn2));
    iMaxColumn1Length = qMax(iMaxColumn1Length, fontMetrics.width(strColumn1));
}
