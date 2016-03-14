/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationRuntime class implementation.
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
# include "UIInformationRuntime.h"
# include "UIInformationItem.h"
# include "UIInformationView.h"
# include "UIExtraDataManager.h"
# include "UIInformationModel.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIInformationRuntime::UIInformationRuntime(QWidget *pParent, const CMachine &machine, const CConsole &console)
    : QWidget(pParent)
    , m_machine(machine)
    , m_console(console)
    , m_pMainLayout(0)
    , m_pModel(0)
    , m_pView(0)
{
    /* Prepare main-layout: */
    prepareLayout();

    /* Prepare model: */
    prepareModel();

    /* Prepare view: */
    prepareView();
}

void UIInformationRuntime::prepareLayout()
{
    /* Create main-layout: */
    m_pMainLayout = new QVBoxLayout(this);
    AssertPtrReturnVoid(m_pMainLayout);
    {
        /* Configure main-layout: */
        m_pMainLayout->setContentsMargins(2, 0, 0, 0);
        m_pMainLayout->setSpacing(0);
        /* Set main-layout: */
        setLayout(m_pMainLayout);
    }
}

void UIInformationRuntime::prepareModel()
{
    /* Create model: */
    m_pModel = new UIInformationModel(this, m_machine, m_console);
    AssertPtrReturnVoid(m_pModel);

    /* Create runtime-attributes item: */
    UIInformationDataRuntimeAttributes *pGeneral = new UIInformationDataRuntimeAttributes(m_machine, m_console, m_pModel);
    AssertPtrReturnVoid(pGeneral);
    {
        /* Add runtime-attributes item to model: */
        m_pModel->addItem(pGeneral);
    }

    /* Create network-statistics item: */
    UIInformationDataNetworkStatistics *pNetwork = new UIInformationDataNetworkStatistics(m_machine, m_console, m_pModel);
    AssertPtrReturnVoid(pNetwork);
    {
        /* Add network-statistics item to model: */
        m_pModel->addItem(pNetwork);
    }

    /* Create storage-statistics item: */
    UIInformationDataStorageStatistics *pStorage = new UIInformationDataStorageStatistics(m_machine, m_console, m_pModel);
    AssertPtrReturnVoid(pStorage);
    {
        /* Add storage-statistics item to model: */
        m_pModel->addItem(pStorage);
    }
}

void UIInformationRuntime::prepareView()
{
    /* Create view: */
    m_pView = new UIInformationView;
    AssertPtrReturnVoid(m_pView);
    {
        /* Configure view: */
        m_pView->setResizeMode(QListView::Adjust);

        /* Create information-delegate item: */
        UIInformationItem* pItem = new UIInformationItem(m_pView);
        AssertPtrReturnVoid(pItem);
        {
            /* Set item-delegate: */
            m_pView->setItemDelegate(pItem);
        }
        /* Connect datachanged signal: */
        connect(m_pModel, SIGNAL(dataChanged(const QModelIndex, const QModelIndex)),
                m_pView, SLOT(updateData(const QModelIndex,const QModelIndex)));

        /* Set model: */
        m_pView->setModel(m_pModel);
        /* Layout view: */
        m_pMainLayout->addWidget(m_pView);
    }
}

