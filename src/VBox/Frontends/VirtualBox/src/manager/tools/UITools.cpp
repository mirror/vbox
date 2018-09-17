/* $Id$ */
/** @file
 * VBox Qt GUI - UITools class implementation.
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
# include "UITools.h"
# include "UIToolsModel.h"
# include "UIToolsView.h"
# include "UIVirtualBoxManagerWidget.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UITools::UITools(UIVirtualBoxManagerWidget *pParent)
    : QWidget(pParent)
    , m_pManagerWidget(pParent)
    , m_pMainLayout(0)
    , m_pToolsModel(0)
    , m_pToolsView(0)
{
    /* Prepare: */
    prepare();
}

UITools::~UITools()
{
    /* Cleanup: */
    cleanup();
}

UIActionPool *UITools::actionPool() const
{
    return managerWidget()->actionPool();
}

void UITools::setToolsClass(UIToolsClass enmClass)
{
    m_pToolsModel->setToolsClass(enmClass);
}

UIToolsClass UITools::toolsClass() const
{
    return m_pToolsModel->toolsClass();
}

UIToolsType UITools::toolsType() const
{
    return m_pToolsModel->toolsType();
}

void UITools::setToolsEnabled(UIToolsClass enmClass, bool fEnabled)
{
    m_pToolsModel->setToolsEnabled(enmClass, fEnabled);
}

bool UITools::areToolsEnabled(UIToolsClass enmClass) const
{
    return m_pToolsModel->areToolsEnabled(enmClass);
}

UIToolsItem *UITools::currentItem() const
{
    return m_pToolsModel->currentItem();
}

void UITools::prepare()
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

void UITools::preparePalette()
{
    /* Setup palette: */
    setAutoFillBackground(true);
    QPalette pal = palette();
    QColor bodyColor = pal.color(QPalette::Active, QPalette::Midlight).darker(110);
    pal.setColor(QPalette::Window, bodyColor);
    setPalette(pal);
}

void UITools::prepareLayout()
{
    /* Setup own layout rules: */
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);

    /* Create main-layout: */
    m_pMainLayout = new QVBoxLayout(this);
    if (m_pMainLayout)
    {
        /* Configure main-layout: */
        m_pMainLayout->setContentsMargins(1, 0, 0, 0);
        m_pMainLayout->setSpacing(0);
    }
}

void UITools::prepareModel()
{
    /* Create Tools-model: */
    m_pToolsModel = new UIToolsModel(this);
}

void UITools::prepareView()
{
    /* Setup Tools-view: */
    m_pToolsView = new UIToolsView(this);
    if (m_pToolsView)
    {
        /* Configure Tools-view. */
        m_pToolsView->setScene(m_pToolsModel->scene());
        m_pToolsView->show();
        setFocusProxy(m_pToolsView);

        /* Add into layout: */
        m_pMainLayout->addWidget(m_pToolsView);
    }
}

void UITools::prepareConnections()
{
    /* Setup Tools-model connections: */
    connect(m_pToolsModel, &UIToolsModel::sigItemMinimumWidthHintChanged,
            m_pToolsView, &UIToolsView::sltMinimumWidthHintChanged);
    connect(m_pToolsModel, &UIToolsModel::sigItemMinimumHeightHintChanged,
            m_pToolsView, &UIToolsView::sltMinimumHeightHintChanged);
    connect(m_pToolsModel, &UIToolsModel::sigFocusChanged,
            m_pToolsView, &UIToolsView::sltFocusChanged);

    /* Setup Tools-view connections: */
    connect(m_pToolsView, &UIToolsView::sigResized,
            m_pToolsModel, &UIToolsModel::sltHandleViewResized);
}

void UITools::loadSettings()
{
    /* Init model: */
    m_pToolsModel->init();
}

void UITools::saveSettings()
{
    /* Deinit model: */
    m_pToolsModel->deinit();
}

void UITools::cleanup()
{
    /* Save settings: */
    saveSettings();
}
