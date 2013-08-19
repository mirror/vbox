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
#include "QIWidgetValidator.h"

/* Other VBox includes: */
#include <VBox/sup.h>

UIWarningPane::UIWarningPane(QWidget *pParent)
    : QWidget(pParent)
    , m_pIconLayout(0)
    , m_pTextLabel(0)
{
    /* Prepare: */
    prepare();
}

void UIWarningPane::setWarningLabel(const QString &strWarningLabel)
{
    /* Assign passed text directly to warning-label: */
    m_pTextLabel->setText(strWarningLabel);
}

void UIWarningPane::registerValidator(UIPageValidator *pValidator)
{
    /* Make sure validator exists: */
    AssertPtrReturnVoid(pValidator);

    /* Make sure validator is not registered yet: */
    if (m_validators.contains(pValidator))
    {
        AssertMsgFailed(("Validator is registered already!\n"));
        return;
    }

    /* Register validator: */
    m_validators << pValidator;

    /* Create icon-label for newly registered validator: */
    QLabel *pIconLabel = new QLabel;
    {
        /* Add icon-label into list: */
        m_icons << pIconLabel;
        /* Add icon-label into layout: */
        m_pIconLayout->addWidget(pIconLabel);
        /* Configure icon-label: */
        pIconLabel->setMouseTracking(true);
        pIconLabel->installEventFilter(this);
        pIconLabel->setPixmap(pValidator->warningPixmap());
        connect(pValidator, SIGNAL(sigShowWarningIcon()), pIconLabel, SLOT(show()));
        connect(pValidator, SIGNAL(sigHideWarningIcon()), pIconLabel, SLOT(hide()));
    }

    /* Mark icon as 'unhovered': */
    m_hovered << false;
}

void UIWarningPane::prepare()
{
    /* Prepare content: */
    prepareContent();
}

void UIWarningPane::prepareContent()
{
    /* Create main-layout: */
    QHBoxLayout *pMainLayout = new QHBoxLayout(this);
    {
        /* Configure layout: */
        pMainLayout->setContentsMargins(0, 0, 0, 0);
        /* Add left stretch: */
        pMainLayout->addStretch();
        /* Create text-label: */
        m_pTextLabel = new QLabel;
        {
            /* Add into main-layout: */
            pMainLayout->addWidget(m_pTextLabel);
        }
        /* Create layout: */
        m_pIconLayout = new QHBoxLayout;
        {
            /* Configure layout: */
            m_pIconLayout->setContentsMargins(0, 0, 0, 0);
            /* Add into main-layout: */
            pMainLayout->addLayout(m_pIconLayout);
        }
        /* Add right stretch: */
        pMainLayout->addStretch();
    }
}

bool UIWarningPane::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Depending on event-type: */
    switch (pEvent->type())
    {
        /* One of icons hovered: */
        case QEvent::MouseMove:
        {
            /* Cast object to label: */
            if (QLabel *pIconLabel = qobject_cast<QLabel*>(pWatched))
            {
                /* Search for the corresponding icon: */
                if (m_icons.contains(pIconLabel))
                {
                    /* Mark icon-label hovered if not yet: */
                    int iIconLabelPosition = m_icons.indexOf(pIconLabel);
                    if (!m_hovered[iIconLabelPosition])
                    {
                        m_hovered[iIconLabelPosition] = true;
                        emit sigHoverEnter(m_validators[iIconLabelPosition]);
                    }
                }
            }
            break;
        }
        /* One of icons unhovered: */
        case QEvent::Leave:
        {
            /* Cast object to label: */
            if (QLabel *pIconLabel = qobject_cast<QLabel*>(pWatched))
            {
                /* Search for the corresponding icon: */
                if (m_icons.contains(pIconLabel))
                {
                    /* Mark icon-label unhovered if not yet: */
                    int iIconLabelPosition = m_icons.indexOf(pIconLabel);
                    if (m_hovered[iIconLabelPosition])
                    {
                        m_hovered[iIconLabelPosition] = false;
                        emit sigHoverLeave(m_validators[iIconLabelPosition]);
                    }
                }
            }
            break;
        }
        /* Default case: */
        default: break;
    }
    /* Call to base-class: */
    return QWidget::eventFilter(pWatched, pEvent);
}

