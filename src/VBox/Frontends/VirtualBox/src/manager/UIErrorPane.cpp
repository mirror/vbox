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
#include <QTextBrowser>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIErrorPane.h"


UIErrorPane::UIErrorPane(QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pBrowserDetails(0)
{
    prepare();
}

void UIErrorPane::setErrorDetails(const QString &strDetails)
{
    /* Redirect to details browser: */
    m_pBrowserDetails->setText(strDetails);
}

void UIErrorPane::prepare()
{
    /* Prepare main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        pMainLayout->setContentsMargins(0, 0, 0, 0);

        /* Prepare details browser: */
        m_pBrowserDetails = new QTextBrowser;
        if (m_pBrowserDetails)
        {
            m_pBrowserDetails->setFocusPolicy(Qt::StrongFocus);
            m_pBrowserDetails->document()->setDefaultStyleSheet("a { text-decoration: none; }");

            /* Add into layout: */
            pMainLayout->addWidget(m_pBrowserDetails);
        }
    }
}
