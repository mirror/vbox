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
#include <QTextDocument>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIDetailsGenerator.h"
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIInformationConfiguration.h"
#include "UIInformationDataItem.h"
#include "UIInformationItem.h"
#include "UIInformationView.h"
#include "UIInformationModel.h"


const unsigned iColumCount = 3;

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
    qDeleteAll(m_tableItems);
    m_tableItems.clear();
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
        m_pTableWidget->setRowCount(20);
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
    QTextDocument textDocument;

    int iMaxColumn1Length = 0;
    int iTableRow = 0;

    insertTitleRow(iTableRow++, m_strGeneralTitle, UIIconPool::iconSet(":/machine_16px.png"));
    UITextTable generalTextTable = UIDetailsGenerator::generateMachineInformationGeneral(m_machine,
                                                                                         UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Default);
    foreach (const UITextTableLine &line, generalTextTable)
    {
        textDocument.setHtml(line.string2());
        insertInfoRow(iTableRow++, line.string1(), textDocument.toPlainText(), fontMetrics, iMaxColumn1Length);
    }

    insertTitleRow(iTableRow++, m_strSystemTitle, UIIconPool::iconSet(":/chipset_16px.png"));
    UITextTable systemTextTable = UIDetailsGenerator::generateMachineInformationSystem(m_machine,
                                                                                       UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Default);
    foreach (const UITextTableLine &line, systemTextTable)
    {
        textDocument.setHtml(line.string2());
        insertInfoRow(iTableRow++, line.string1(), textDocument.toPlainText(), fontMetrics, iMaxColumn1Length);
    }



    m_pTableWidget->resizeColumnToContents(0);
    m_pTableWidget->setColumnWidth(1, 1.5 * iMaxColumn1Length);
    m_pTableWidget->resizeColumnToContents(2);

}

void UIInformationConfiguration::insertTitleRow(int iRow, const QString &strTitle, const QIcon &icon)
{
    QTableWidgetItem *pIconItem = new QTableWidgetItem(icon, "");
    m_pTableWidget->setItem(iRow, 0, pIconItem);
    QTableWidgetItem *pTitleItem = new QTableWidgetItem(strTitle);
    QFont font = pTitleItem->font();
    font.setBold(true);
    pTitleItem->setFont(font);
    m_pTableWidget->setItem(iRow, 1, pTitleItem);
    m_tableItems << pIconItem;
    m_tableItems << pTitleItem;
}

void UIInformationConfiguration::insertInfoRow(int iRow, const QString strText1, const QString &strText2,
                                               QFontMetrics &fontMetrics, int &iMaxColumn1Length)
{
    iMaxColumn1Length = qMax(iMaxColumn1Length, fontMetrics.width(strText1));
    QTableWidgetItem *pCol1 = new QTableWidgetItem(strText1);
    QTableWidgetItem *pCol2 = new QTableWidgetItem(strText2);
    m_tableItems << pCol1;
    m_tableItems << pCol2;
    m_pTableWidget->setItem(iRow, 1, pCol1);
    m_pTableWidget->setItem(iRow, 2, pCol2);
}
