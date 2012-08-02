/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGDetails class implementation
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

/* GUI includes: */
#include "UIGDetails.h"
#include "UIGDetailsModel.h"
#include "UIGDetailsView.h"

UIGDetails::UIGDetails(QWidget *pParent)
    : QWidget(pParent)
    , m_pMainLayout(0)
    , m_pDetailsModel(0)
    , m_pDetailsView(0)
{
    /* Create main-layout: */
    m_pMainLayout = new QVBoxLayout(this);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    m_pMainLayout->setSpacing(0);

    /* Create details-model: */
    m_pDetailsModel = new UIGDetailsModel(this);

    /* Create details-view: */
    m_pDetailsView = new UIGDetailsView(this);
    m_pDetailsView->setFrameShape(QFrame::NoFrame);
    m_pDetailsView->setFrameShadow(QFrame::Plain);
    m_pDetailsView->setScene(m_pDetailsModel->scene());
    m_pDetailsView->show();
    setFocusProxy(m_pDetailsView);

    /* Add tool-bar into layout: */
    m_pMainLayout->addWidget(m_pDetailsView);

    /* Prepare connections: */
    prepareConnections();
}

void UIGDetails::setItems(const QList<UIVMItem*> &items)
{
    m_pDetailsModel->setItems(items);
}

void UIGDetails::prepareConnections()
{
    /* Selector-model connections: */
    connect(m_pDetailsModel, SIGNAL(sigRootItemResized(const QSizeF&, int)),
            m_pDetailsView, SLOT(sltHandleRootItemResized(const QSizeF&, int)));
    connect(m_pDetailsModel, SIGNAL(sigLinkClicked(const QString&, const QString&, const QString&)),
            this, SIGNAL(sigLinkClicked(const QString&, const QString&, const QString&)));
    connect(this, SIGNAL(sigSlidingStarted()),
            m_pDetailsModel, SLOT(sltHandleSlidingStarted()));

    /* Selector-view connections: */
    connect(m_pDetailsView, SIGNAL(sigResized()), m_pDetailsModel, SLOT(sltHandleViewResized()));
}

