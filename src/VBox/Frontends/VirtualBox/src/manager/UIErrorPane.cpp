/* $Id$ */
/** @file
 * VBox Qt GUI - UIErrorPane class implementation.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
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
#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>

/* GUI includes */
#include "QIWithRetranslateUI.h"
#include "UIErrorPane.h"

/* Other VBox includes: */
#include <iprt/assert.h>


UIErrorPane::UIErrorPane(QAction *pRefreshAction /* = 0 */, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pActionRefresh(pRefreshAction)
    , m_pButtonRefresh(0)
    , m_pBrowserDetails(0)
{
    /* Prepare: */
    prepare();
}

void UIErrorPane::setErrorDetails(const QString &strDetails)
{
    /* Define error details: */
    m_pBrowserDetails->setText(strDetails);
}

void UIErrorPane::retranslateUi()
{
    /* Translate refresh button: */
    if (m_pActionRefresh && m_pButtonRefresh)
    {
        m_pButtonRefresh->setText(m_pActionRefresh->text());
        m_pButtonRefresh->setIcon(m_pActionRefresh->icon());
        m_pButtonRefresh->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
}

void UIErrorPane::prepare()
{
    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Configure layout: */
        pMainLayout->setContentsMargins(0, 0, 0, 0);

        /* Create details browser: */
        m_pBrowserDetails = new QTextBrowser;
        if (m_pBrowserDetails)
        {
            /* Configure browser: */
            m_pBrowserDetails->setFocusPolicy(Qt::StrongFocus);
            m_pBrowserDetails->document()->setDefaultStyleSheet("a { text-decoration: none; }");

            /* Add into layout: */
            pMainLayout->addWidget(m_pBrowserDetails);
        }

        /* If refresh action was set: */
        if (m_pActionRefresh)
        {
            /* Create Refresh button layout: */
            QHBoxLayout *pButtonLayout = new QHBoxLayout;
            if (pButtonLayout)
            {
                /* Add stretch first: */
                pButtonLayout->addStretch();

                /* Create refresh button: */
                m_pButtonRefresh = new QToolButton;
                if (m_pButtonRefresh)
                {
                    /* Configure button: */
                    m_pButtonRefresh->setFocusPolicy(Qt::StrongFocus);
                    connect(m_pButtonRefresh, &QToolButton::clicked,
                            m_pActionRefresh, &QAction::triggered);

                    /* Add into layout: */
                    pButtonLayout->addWidget(m_pButtonRefresh);
                }
            }

            /* Add into layout: */
            pMainLayout->addLayout(pButtonLayout);
        }

        /* Add stretch finally: */
        pMainLayout->addStretch();
    }

    /* Retranslate finally: */
    retranslateUi();
}
