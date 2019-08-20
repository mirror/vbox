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

/* GUI includes: */
#include "UIDetailsGenerator.h"
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIInformationConfiguration.h"
#include "UIVirtualBoxEventHandler.h"

UIInformationConfiguration::UIInformationConfiguration(QWidget *pParent, const CMachine &machine, const CConsole &console)
    : UIInformationWidget(pParent, machine, console)

{
    retranslateUi();
    createTableItems();
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineDataChange,
            this, &UIInformationConfiguration::sltMachineDataChanged);
}

void UIInformationConfiguration::sltMachineDataChanged()
{
    resetTable();
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
}


void UIInformationConfiguration::createTableItems()
{
    if (!m_pTableWidget)
        return;
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
}
