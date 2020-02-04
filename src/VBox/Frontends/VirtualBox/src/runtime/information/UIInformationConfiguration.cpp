/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationConfiguration class implementation.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
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
#include "UIVirtualBoxEventHandler.h"

UIInformationConfiguration::UIInformationConfiguration(QWidget *pParent, const CMachine &machine, const CConsole &console)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_machine(machine)
    , m_console(console)
    , m_pMainLayout(0)
    , m_pTableWidget(0)
    , m_iColumCount(3)
    , m_iRowLeftMargin(0.2 * qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin))
    , m_iRowTopMargin(0.2 * qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin))
    , m_iRowRightMargin(0.2 * qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin))
    , m_iRowBottomMargin(0.2 * qApp->style()->pixelMetric(QStyle::PM_LayoutBottomMargin))
{
    prepareObjects();
    retranslateUi();
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineDataChange,
            this, &UIInformationConfiguration::sltMachineDataChanged);
    connect(&uiCommon(), &UICommon::sigMediumEnumerationFinished,
            this, &UIInformationConfiguration::sltMachineDataChanged);
}

void UIInformationConfiguration::sltMachineDataChanged()
{
    createTableItems();
}

void UIInformationConfiguration::retranslateUi()
{
    m_strGeneralTitle = QApplication::translate("UIVMInformationDialog", "General");
    m_strSystemTitle = QApplication::translate("UIVMInformationDialog", "System");
    m_strDisplayTitle = QApplication::translate("UIVMInformationDialog", "Display");
    m_strStorageTitle = QApplication::translate("UIVMInformationDialog", "Storage");
    m_strAudioTitle = QApplication::translate("UIVMInformationDialog", "Audio");
    m_strNetworkTitle = QApplication::translate("UIVMInformationDialog", "Network");
    m_strSerialPortsTitle = QApplication::translate("UIVMInformationDialog", "Serial Ports");
    m_strUSBTitle = QApplication::translate("UIVMInformationDialog", "USB");
    m_strSharedFoldersTitle = QApplication::translate("UIVMInformationDialog", "Shared Folders");
    createTableItems();
}

void UIInformationConfiguration::createTableItems()
{
    if (!m_pTableWidget)
        return;
    resetTable();
    QFontMetrics fontMetrics(m_pTableWidget->font());
    QTextDocument textDocument;

    int iMaxColumn1Length = 0;

    /* General section: */
    insertTitleRow(m_strGeneralTitle, UIIconPool::iconSet(":/machine_16px.png"), fontMetrics);
    insertInfoRows(UIDetailsGenerator::generateMachineInformationGeneral(m_machine,
                                                                         UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Default),
                   fontMetrics, textDocument, iMaxColumn1Length);

    /* System section: */
    insertTitleRow(m_strSystemTitle, UIIconPool::iconSet(":/chipset_16px.png"), fontMetrics);
    insertInfoRows(UIDetailsGenerator::generateMachineInformationSystem(m_machine,
                                                                         UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Default),
                   fontMetrics, textDocument, iMaxColumn1Length);

    /* Display section: */
    insertTitleRow(m_strDisplayTitle, UIIconPool::iconSet(":/vrdp_16px.png"), fontMetrics);
    insertInfoRows(UIDetailsGenerator::generateMachineInformationDisplay(m_machine,
                                                                         UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_Default),
                   fontMetrics, textDocument, iMaxColumn1Length);

    /* Storage section: */
    insertTitleRow(m_strStorageTitle, UIIconPool::iconSet(":/hd_16px.png"), fontMetrics);
    insertInfoRows(UIDetailsGenerator::generateMachineInformationStorage(m_machine,
                                                                         UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_Default),
                   fontMetrics, textDocument, iMaxColumn1Length);

    /* Audio section: */
    insertTitleRow(m_strAudioTitle, UIIconPool::iconSet(":/sound_16px.png"), fontMetrics);
    insertInfoRows(UIDetailsGenerator::generateMachineInformationAudio(m_machine,
                                                                         UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_Default),
                   fontMetrics, textDocument, iMaxColumn1Length);

    /* Network section: */
    insertTitleRow(m_strNetworkTitle, UIIconPool::iconSet(":/nw_16px.png"), fontMetrics);
    insertInfoRows(UIDetailsGenerator::generateMachineInformationNetwork(m_machine,
                                                                         UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_Default),
                   fontMetrics, textDocument, iMaxColumn1Length);

    /* Serial port section: */
    insertTitleRow(m_strSerialPortsTitle, UIIconPool::iconSet(":/serial_port_16px.png"), fontMetrics);
    insertInfoRows(UIDetailsGenerator::generateMachineInformationSerial(m_machine,
                                                                        UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_Default),
                   fontMetrics, textDocument, iMaxColumn1Length);

    /* USB section: */
    insertTitleRow(m_strUSBTitle, UIIconPool::iconSet(":/usb_16px.png"), fontMetrics);
    insertInfoRows(UIDetailsGenerator::generateMachineInformationUSB(m_machine,
                                                                        UIExtraDataMetaDefs::DetailsElementOptionTypeUsb_Default),
                   fontMetrics, textDocument, iMaxColumn1Length);

    /* Share folders section: */
    insertTitleRow(m_strSharedFoldersTitle, UIIconPool::iconSet(":/sf_16px.png"), fontMetrics);
    insertInfoRows(UIDetailsGenerator::generateMachineInformationSharedFolders(m_machine,
                                                                               UIExtraDataMetaDefs::DetailsElementOptionTypeSharedFolders_Default),
                   fontMetrics, textDocument, iMaxColumn1Length);

    m_pTableWidget->resizeColumnToContents(0);
    /* Resize the column 1 a bit larger than the max string if contains: */
    m_pTableWidget->setColumnWidth(1, 1.5 * iMaxColumn1Length);
    m_pTableWidget->resizeColumnToContents(2);
    m_pTableWidget->horizontalHeader()->setStretchLastSection(true);
}

void UIInformationConfiguration::prepareObjects()
{
    /* Create layout: */
    m_pMainLayout = new QVBoxLayout(this);
    if (!m_pMainLayout)
        return;
    m_pMainLayout->setSpacing(0);

    m_pTableWidget = new QTableWidget;
    if (m_pTableWidget)
    {
        /* Configure the table by hiding the headers etc.: */
        m_pTableWidget->setColumnCount(m_iColumCount);
        m_pTableWidget->setAlternatingRowColors(true);
        m_pTableWidget->verticalHeader()->hide();
        m_pTableWidget->horizontalHeader()->hide();
        m_pTableWidget->setShowGrid(false);
        m_pTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_pTableWidget->setFocusPolicy(Qt::NoFocus);
        m_pTableWidget->setSelectionMode(QAbstractItemView::NoSelection);
        m_pMainLayout->addWidget(m_pTableWidget);
    }
}

void UIInformationConfiguration::insertInfoRows(const UITextTable &table, const QFontMetrics &fontMetrics,
                                                QTextDocument &textDocument, int &iMaxColumn1Length)
{
    foreach (const UITextTableLine &line, table)
    {
        textDocument.setHtml(line.string2());
        insertInfoRow(line.string1(), textDocument.toPlainText(), fontMetrics, iMaxColumn1Length);
    }
}

void UIInformationConfiguration::insertTitleRow(const QString &strTitle, const QIcon &icon, const QFontMetrics &fontMetrics)
{
    int iRow = m_pTableWidget->rowCount();
    m_pTableWidget->insertRow(iRow);
    QSize iconSize;
    icon.actualSize(iconSize);
    m_pTableWidget->setRowHeight(iRow,
                                 qMax(fontMetrics.height() + m_iRowTopMargin + m_iRowBottomMargin, iconSize.height()));
    m_pTableWidget->setItem(iRow, 0, new QTableWidgetItem(icon, ""));
    QTableWidgetItem *pTitleItem = new QTableWidgetItem(strTitle);
    QFont font = pTitleItem->font();
    font.setBold(true);
    pTitleItem->setFont(font);
    m_pTableWidget->setItem(iRow, 1, pTitleItem);
}

void UIInformationConfiguration::insertInfoRow(const QString strText1, const QString &strText2,
                                               const QFontMetrics &fontMetrics, int &iMaxColumn1Length)
{
    int iRow = m_pTableWidget->rowCount();
    m_pTableWidget->insertRow(iRow);
    m_pTableWidget->setRowHeight(iRow, fontMetrics.height() + m_iRowTopMargin + m_iRowBottomMargin);
    iMaxColumn1Length = qMax(iMaxColumn1Length, fontMetrics.width(strText1));
    m_pTableWidget->setItem(iRow, 1, new QTableWidgetItem(strText1));
    m_pTableWidget->setItem(iRow, 2, new QTableWidgetItem(strText2));
}

void UIInformationConfiguration::resetTable()
{
    if (m_pTableWidget)
    {
        m_pTableWidget->clear();
        m_pTableWidget->setRowCount(0);
        m_pTableWidget->setColumnCount(m_iColumCount);
    }
}
