/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooser class implementation.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
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
# include <QStatusBar>
# include <QStyle>

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

    /* Load: */
    load();
}

UIChooser::~UIChooser()
{
    /* Save: */
    save();
}

UIActionPool* UIChooser::actionPool() const
{
    return managerWidget()->actionPool();
}

bool UIChooser::isGlobalItemSelected() const
{
    return m_pChooserModel->isGlobalItemSelected();
}

bool UIChooser::isMachineItemSelected() const
{
    return m_pChooserModel->isMachineItemSelected();
}

UIVirtualMachineItem *UIChooser::currentItem() const
{
    return m_pChooserModel->currentMachineItem();
}

QList<UIVirtualMachineItem*> UIChooser::currentItems() const
{
    return m_pChooserModel->currentMachineItems();
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

void UIChooser::preparePalette()
{
    /* Setup palette: */
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, pal.color(QPalette::Active, QPalette::Base));
    setPalette(pal);
}

void UIChooser::prepareLayout()
{
    /* Setup main-layout: */
    m_pMainLayout = new QVBoxLayout(this);
    const int iR = qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin) / 9;
    m_pMainLayout->setContentsMargins(0, 0, iR, 0);
    m_pMainLayout->setSpacing(0);
}

void UIChooser::prepareModel()
{
    /* Setup chooser-model: */
    m_pChooserModel = new UIChooserModel(this);
}

void UIChooser::prepareView()
{
    /* Setup chooser-view: */
    m_pChooserView = new UIChooserView(this);
    m_pChooserView->setScene(m_pChooserModel->scene());
    m_pChooserView->show();
    setFocusProxy(m_pChooserView);
    m_pMainLayout->addWidget(m_pChooserView);
}

void UIChooser::prepareConnections()
{
    /* Setup chooser-model connections: */
    connect(m_pChooserModel, SIGNAL(sigRootItemMinimumWidthHintChanged(int)),
            m_pChooserView, SLOT(sltMinimumWidthHintChanged(int)));
    connect(m_pChooserModel, SIGNAL(sigRootItemMinimumHeightHintChanged(int)),
            m_pChooserView, SLOT(sltMinimumHeightHintChanged(int)));
    connect(m_pChooserModel, SIGNAL(sigFocusChanged(UIChooserItem*)),
            m_pChooserView, SLOT(sltFocusChanged(UIChooserItem*)));

    /* Setup chooser-view connections: */
    connect(m_pChooserView, SIGNAL(sigResized()),
            m_pChooserModel, SLOT(sltHandleViewResized()));
}

void UIChooser::load()
{
    /* Prepare model: */
    m_pChooserModel->prepare();
}

void UIChooser::save()
{
    /* Cleanup model: */
    m_pChooserModel->cleanup();
}

