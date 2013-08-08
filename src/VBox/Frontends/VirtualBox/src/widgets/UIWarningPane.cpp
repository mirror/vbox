/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWarningPane class implementation
 */

/*
 * Copyright (C) 2009-2013 Oracle Corporation
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
#include <QHBoxLayout>
#include <QLabel>

/* GUI includes: */
#include "UIWarningPane.h"

UIWarningPane::UIWarningPane(QWidget *pParent)
    : QWidget(pParent)
    , m_pLabelIcon(0)
    , m_pLabelText(0)
{
    /* Prepare: */
    prepare();
}

void UIWarningPane::setWarningPixmap(const QPixmap &pixmap)
{
    m_pLabelIcon->setPixmap(pixmap);
}

void UIWarningPane::setWarningText(const QString &strText)
{
    m_pLabelText->setText(strText);
}

void UIWarningPane::prepare()
{
    /* Prepare content: */
    prepareContent();
}

void UIWarningPane::prepareContent()
{
    /* Create layout: */
    QHBoxLayout *pLayout = new QHBoxLayout(this);
    {
        /* Configure layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);
        /* Create icon label: */
        m_pLabelIcon = new QLabel;
        /* Create text label: */
        m_pLabelText = new QLabel;
        /* Add widgets into layout: */
        pLayout->addWidget(m_pLabelIcon);
        pLayout->addWidget(m_pLabelText);
    }
}

