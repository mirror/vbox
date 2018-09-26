/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooser class implementation.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
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

/* GUI includes: */
# include "UIChooser.h"
# include "UIChooserModel.h"
# include "UIChooserView.h"
# include "UIVirtualBoxManagerWidget.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIChooser::UIChooser(UIVirtualBoxManagerWidget *pParent)
    : QWidget(pParent)
    , m_pManagerWidget(pParent)
    , m_pMainLayout(0)
    , m_pChooserModel(0)
    , m_pChooserView(0)
{
    /* Prepare: */
    prepare();
}

UIChooser::~UIChooser()
{
    /* Cleanup: */
    cleanup();
}

UIActionPool *UIChooser::actionPool() const
{
    return managerWidget()->actionPool();
}

UIVirtualMachineItem *UIChooser::currentItem() const
{
    return m_pChooserModel->currentMachineItem();
}

QList<UIVirtualMachineItem*> UIChooser::currentItems() const
{
    return m_pChooserModel->currentMachineItems();
}

bool UIChooser::isGroupItemSelected() const
{
    return m_pChooserModel->isGroupItemSelected();
}

bool UIChooser::isGlobalItemSelected() const
{
    return m_pChooserModel->isGlobalItemSelected();
}

bool UIChooser::isMachineItemSelected() const
{
    return m_pChooserModel->isMachineItemSelected();
}

bool UIChooser::isSingleGroupSelected() const
{
    return m_pChooserModel->isSingleGroupSelected();
}

bool UIChooser::isAllItemsOfOneGroupSelected() const
{
    return m_pChooserModel->isAllItemsOfOneGroupSelected();
}

bool UIChooser::isGroupSavingInProgress() const
{
    return m_pChooserModel->isGroupSavingInProgress();
}

void UIChooser::sltHandleToolbarResize(const QSize &newSize)
{
    /* Pass height to a model: */
    model()->setGlobalItemHeightHint(newSize.height());
}

void UIChooser::prepare()
{
    /* Prepare palette: */
    preparePalette();
    /* Prepare layout: */
    prepareLayout();
    /* Prepare model: */
    prepareModel();
    /* Prepare view: */
    prepareView();
    /* Prepare connections: */
    prepareConnections();

    /* Load settings: */
    loadSettings();
}

void UIChooser::preparePalette()
{
    /* Setup palette: */
    setAutoFillBackground(true);
    QPalette pal = palette();
    QColor bodyColor = pal.color(QPalette::Active, QPalette::Midlight).darker(110);
    pal.setColor(QPalette::Window, bodyColor);
    setPalette(pal);
}

void UIChooser::prepareLayout()
{
    /* Create main-layout: */
    m_pMainLayout = new QVBoxLayout(this);
    if (m_pMainLayout)
    {
        /* Configure main-layout: */
        m_pMainLayout->setContentsMargins(0, 0, 0, 0);
        m_pMainLayout->setSpacing(0);
    }
}

void UIChooser::prepareModel()
{
    /* Create chooser-model: */
    m_pChooserModel = new UIChooserModel(this);
}

void UIChooser::prepareView()
{
    /* Setup chooser-view: */
    m_pChooserView = new UIChooserView(this);
    if (m_pChooserView)
    {
        /* Configure chooser-view. */
        m_pChooserView->setScene(m_pChooserModel->scene());
        m_pChooserView->show();
        setFocusProxy(m_pChooserView);

        /* Add into layout: */
        m_pMainLayout->addWidget(m_pChooserView);
    }
}

void UIChooser::prepareConnections()
{
    /* Setup chooser-model connections: */
    connect(m_pChooserModel, &UIChooserModel::sigRootItemMinimumWidthHintChanged,
            m_pChooserView, &UIChooserView::sltMinimumWidthHintChanged);
    connect(m_pChooserModel, &UIChooserModel::sigRootItemMinimumHeightHintChanged,
            m_pChooserView, &UIChooserView::sltMinimumHeightHintChanged);
    connect(m_pChooserModel, &UIChooserModel::sigFocusChanged,
            m_pChooserView, &UIChooserView::sltFocusChanged);
    connect(m_pChooserModel, &UIChooserModel::sigToolMenuRequested,
            this, &UIChooser::sigToolMenuRequested);

    /* Setup chooser-view connections: */
    connect(m_pChooserView, &UIChooserView::sigResized,
            m_pChooserModel, &UIChooserModel::sltHandleViewResized);
}

void UIChooser::loadSettings()
{
    /* Init model: */
    m_pChooserModel->init();
}

void UIChooser::saveSettings()
{
    /* Deinit model: */
    m_pChooserModel->deinit();
}

void UIChooser::cleanup()
{
    /* Save settings: */
    saveSettings();
}
