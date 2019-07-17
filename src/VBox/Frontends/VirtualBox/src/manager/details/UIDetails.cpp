/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetails class implementation.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
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
#include "UIDetails.h"
#include "UIDetailsModel.h"
#include "UIDetailsView.h"
#include "UIExtraDataManager.h"


UIDetails::UIDetails(QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pDetailsModel(0)
    , m_pDetailsView(0)
{
    /* Prepare: */
    prepare();
}

void UIDetails::setItems(const QList<UIVirtualMachineItem*> &items)
{
    /* Propagate to details-model: */
    m_pDetailsModel->setItems(items);
}

void UIDetails::prepare()
{
    /* Create main-layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        pMainLayout->setContentsMargins(0, 0, 0, 0);
        pMainLayout->setSpacing(0);

        /* Create details-model: */
        m_pDetailsModel = new UIDetailsModel(this);
        if (m_pDetailsModel)
        {
            /* Create details-view: */
            m_pDetailsView = new UIDetailsView(this);
            if (m_pDetailsView)
            {
                m_pDetailsView->setScene(m_pDetailsModel->scene());
                m_pDetailsView->show();
                setFocusProxy(m_pDetailsView);

                /* Add into layout: */
                pMainLayout->addWidget(m_pDetailsView);
            }

            /* Init model: */
            m_pDetailsModel->init();
        }
    }

    /* Extra-data events connections: */
    connect(gEDataManager, &UIExtraDataManager::sigDetailsCategoriesChange,
            m_pDetailsModel, &UIDetailsModel::sltHandleExtraDataCategoriesChange);
    connect(gEDataManager, &UIExtraDataManager::sigDetailsOptionsChange,
            m_pDetailsModel, &UIDetailsModel::sltHandleExtraDataOptionsChange);

    /* Setup details-model connections: */
    connect(m_pDetailsModel, &UIDetailsModel::sigRootItemMinimumWidthHintChanged,
            m_pDetailsView, &UIDetailsView::sltMinimumWidthHintChanged);
    connect(m_pDetailsModel, &UIDetailsModel::sigLinkClicked,
            this, &UIDetails::sigLinkClicked);
    connect(this, &UIDetails::sigToggleStarted,
            m_pDetailsModel, &UIDetailsModel::sltHandleToggleStarted);
    connect(this, &UIDetails::sigToggleFinished,
            m_pDetailsModel, &UIDetailsModel::sltHandleToggleFinished);

    /* Setup details-view connections: */
    connect(m_pDetailsView, &UIDetailsView::sigResized,
            m_pDetailsModel, &UIDetailsModel::sltHandleViewResize);
}
