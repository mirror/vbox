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
#include <QEvent>

/* GUI includes: */
#include "UIWarningPane.h"

UIWarningPane::UIWarningPane(QWidget *pParent)
    : QWidget(pParent)
    , m_pLabelIcon(0)
    , m_pLabelText(0)
    , m_fHovered(false)
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
    /* Configure self: */
    setMouseTracking(true);
    installEventFilter(this);
    /* Create layout: */
    QHBoxLayout *pLayout = new QHBoxLayout(this);
    {
        /* Configure layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);
        /* Create icon label: */
        m_pLabelIcon = new QLabel;
        {
            /* Configure icon label: */
            m_pLabelIcon->setMouseTracking(true);
            m_pLabelIcon->installEventFilter(this);
        }
        /* Create text label: */
        m_pLabelText = new QLabel;
        {
            /* Configure text label: */
            m_pLabelText->setMouseTracking(true);
            m_pLabelText->installEventFilter(this);
        }
        /* Add widgets into layout: */
        pLayout->addWidget(m_pLabelIcon);
        pLayout->addWidget(m_pLabelText);
    }
}

bool UIWarningPane::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Depending on event-type: */
    switch (pEvent->type())
    {
        /* Anything is hovered: */
        case QEvent::MouseMove:
        {
            /* Hover warning-pane if not yet hovered: */
            if (!m_fHovered)
            {
                m_fHovered = true;
                emit sigHoverEnter();
            }
            break;
        }
        /* Warning-pane is unhovered: */
        case QEvent::Leave:
        {
            /* Unhover warning-pane if hovered: */
            if (pWatched == this && m_fHovered)
            {
                m_fHovered = false;
                emit sigHoverLeave();
            }
            break;
        }
        /* Default case: */
        default: break;
    }
    /* Call to base-class: */
    return QWidget::eventFilter(pWatched, pEvent);
}

