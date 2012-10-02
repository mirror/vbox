/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooser class implementation
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <QVBoxLayout>
#include <QStatusBar>

/* GUI includes: */
#include "UIGChooser.h"
#include "UIGChooserModel.h"
#include "UIGChooserView.h"
#include "VBoxGlobal.h"

UIGChooser::UIGChooser(QWidget *pParent)
    : QWidget(pParent)
    , m_pMainLayout(0)
    , m_pChooserModel(0)
    , m_pChooserView(0)
    , m_pStatusBar(0)
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

UIGChooser::~UIGChooser()
{
    /* Save: */
    save();
}

UIVMItem* UIGChooser::currentItem() const
{
    return m_pChooserModel->currentItem();
}

QList<UIVMItem*> UIGChooser::currentItems() const
{
    return m_pChooserModel->currentItems();
}

bool UIGChooser::isSingleGroupSelected() const
{
    return m_pChooserModel->isSingleGroupSelected();
}

bool UIGChooser::isAllItemsOfOneGroupSelected() const
{
    return m_pChooserModel->isAllItemsOfOneGroupSelected();
}

void UIGChooser::setStatusBar(QStatusBar *pStatusBar)
{
    /* Old status-bar set? */
    if (m_pStatusBar)
       m_pChooserModel->disconnect(m_pStatusBar);

    /* Connect new status-bar: */
    m_pStatusBar = pStatusBar;
    connect(m_pChooserModel, SIGNAL(sigClearStatusMessage()),
            m_pStatusBar, SLOT(clearMessage()));
    connect(m_pChooserModel, SIGNAL(sigShowStatusMessage(const QString&)),
            m_pStatusBar, SLOT(showMessage(const QString&)));
}

bool UIGChooser::isGroupSavingInProgress() const
{
    return m_pChooserModel->isGroupSavingInProgress();
}

void UIGChooser::preparePalette()
{
    /* Setup palette: */
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(240, 240, 240));
    setPalette(pal);
}

void UIGChooser::prepareLayout()
{
    /* Setup main-layout: */
    m_pMainLayout = new QVBoxLayout(this);
    m_pMainLayout->setContentsMargins(0, 0, 2, 0);
    m_pMainLayout->setSpacing(0);
}

void UIGChooser::prepareModel()
{
    /* Setup chooser-model: */
    m_pChooserModel = new UIGChooserModel(this);
}

void UIGChooser::prepareView()
{
    /* Setup chooser-view: */
    m_pChooserView = new UIGChooserView(this);
    m_pChooserView->setScene(m_pChooserModel->scene());
    m_pChooserView->show();
    setFocusProxy(m_pChooserView);
    m_pMainLayout->addWidget(m_pChooserView);
}

void UIGChooser::prepareConnections()
{
    /* Setup chooser-model connections: */
    connect(m_pChooserModel, SIGNAL(sigRootItemResized(const QSizeF&, int)),
            m_pChooserView, SLOT(sltHandleRootItemResized(const QSizeF&, int)));
    connect(m_pChooserModel, SIGNAL(sigFocusChanged(UIGChooserItem*)),
            m_pChooserView, SLOT(sltFocusChanged(UIGChooserItem*)));

    /* Setup chooser-view connections: */
    connect(m_pChooserView, SIGNAL(sigResized()),
            m_pChooserModel, SLOT(sltHandleViewResized()));
}

void UIGChooser::load()
{
    /* Prepare model: */
    m_pChooserModel->prepare();

    /* Load last selected item: */
    m_pChooserModel->setCurrentItemDefinition(vboxGlobal().virtualBox().GetExtraData(GUI_LastItemSelected));
}

void UIGChooser::save()
{
    /* Save last selected item: */
    vboxGlobal().virtualBox().SetExtraData(GUI_LastItemSelected, m_pChooserModel->currentItemDefinition());

    /* Cleanup model: */
    m_pChooserModel->cleanup();
}

