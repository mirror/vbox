/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformation class implementation.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QVBoxLayout>
# include <QApplication>

/* GUI includes: */
# include "UIInformation.h"
# include "UIInformationItem.h"
# include "UIInformationView.h"
# include "UIExtraDataManager.h"
# include "UIInformationModel.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIInformation::UIInformation(QWidget *pParent, const CMachine &machine, const CConsole &console)
    : QWidget(pParent)
    , m_machine(machine)
    , m_console(console)
    , m_pMainLayout(0)
    , m_pModel(0)
    , m_pView(0)
{
    /* Prepare main-layout: */
    prepareMainLayout();

    /* Prepare model: */
    prepareModel();

    /* Prepare view: */
    prepareView();
}

void UIInformation::prepareMainLayout()
{
    /* Create main-layout: */
    m_pMainLayout = new QVBoxLayout;
    AssertPtrReturnVoid(m_pMainLayout);
    {
        /* Configure main-layout: */
        m_pMainLayout->setContentsMargins(2, 0, 0, 0);
        m_pMainLayout->setSpacing(0);
        setLayout(m_pMainLayout);
    }
}

void UIInformation::prepareModel()
{
    /* Create information-model: */
    m_pModel = new UIInformationModel(this, m_machine, m_console);
    AssertPtrReturnVoid(m_pModel);
    {
        /* Add data to information-model: */
        /* Create General data-item: */
        UIInformationDataItem *pGeneral = new UIInformationDataGeneral(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pGeneral);
        {
            m_pModel->addItem(pGeneral);
        }

        /* Create System data-item: */
        UIInformationDataItem *pSystem = new UIInformationDataSystem(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pSystem);
        {
            m_pModel->addItem(pSystem);
        }

        /* Create Display data-item: */
        UIInformationDataItem *pDisplay = new UIInformationDataDisplay(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pDisplay);
        {
            m_pModel->addItem(pDisplay);
        }

        /* Create Storage data-item: */
        UIInformationDataItem *pStorage = new UIInformationDataStorage(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pStorage);
        {
            m_pModel->addItem(pStorage);
        }

        /* Create Audio data-item: */
        UIInformationDataItem *pAudio = new UIInformationDataAudio(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pAudio);
        {
            m_pModel->addItem(pAudio);
        }

        /* Create Network data-item: */
        UIInformationDataItem *pNetwork = new UIInformationDataNetwork(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pNetwork);
        {
            m_pModel->addItem(pNetwork);
        }

        /* Create Serial-ports data-item: */
        UIInformationDataItem *pSerialPorts = new UIInformationDataSerialPorts(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pSerialPorts);
        {
            m_pModel->addItem(pSerialPorts);
        }

#ifdef VBOX_WITH_PARALLEL_PORTS
        /* Create Parallel-ports data-item: */
        UIInformationDataItem *pParallelPorts = new UIInformationDataParallelPorts(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pParallelPorts);
        {
            m_pModel->addItem(pParallelPorts);
        }
#endif /* VBOX_WITH_PARALLEL_PORTS */

        /* Create USB data-item: */
        UIInformationDataItem *pUSB = new UIInformationDataUSB(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pUSB);
        {
            m_pModel->addItem(pUSB);
        }

        /* Create Shared-folders data-item: */
        UIInformationDataItem *pSharedFolders = new UIInformationDataSharedFolders(m_machine, m_console, m_pModel);
        AssertPtrReturnVoid(pSharedFolders);
        {
            m_pModel->addItem(pSharedFolders);
        }
    }
}

void UIInformation::prepareView()
{
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
            m_pView->setItemDelegate(pItem);
        }
        /* Connect datachanged signal: */
        connect(m_pModel, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
                m_pView, SLOT(updateData(const QModelIndex&, const QModelIndex&)));

        /* Set model for view: */
        m_pView->setModel(m_pModel);
        /* Add information-view to the main-layout: */
        m_pMainLayout->addWidget(m_pView);
    }
}

