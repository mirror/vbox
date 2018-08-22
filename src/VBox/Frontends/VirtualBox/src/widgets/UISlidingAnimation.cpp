/* $Id$ */
/** @file
 * VBox Qt GUI - UISlidingAnimation class implementation.
 */

/*
 * Copyright (C) 2017-2018 Oracle Corporation
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
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIAnimationFramework.h"
#include "UISlidingAnimation.h"


UISlidingAnimation::UISlidingAnimation(Qt::Orientation enmOrientation, QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_enmOrientation(enmOrientation)
    , m_pAnimation(0)
    , m_pWidget(0)
    , m_pLabel1(0)
    , m_pLabel2(0)
    , m_pWidget1(0)
    , m_pWidget2(0)
{
    /* Prepare: */
    prepare();
}

void UISlidingAnimation::setWidgets(QWidget *pWidget1, QWidget *pWidget2)
{
    /* Remember rendered widgets: */
    m_pWidget1 = pWidget1;
    m_pWidget2 = pWidget2;
}

void UISlidingAnimation::animate(SlidingDirection enmDirection)
{
    /* Acquire parent size: */
    const QSize parentSize = parentWidget()->size();

    /* Update animation boundaries based on parent size: */
    switch (m_enmOrientation)
    {
        case Qt::Horizontal:
        {
            m_startWidgetGeometry = QRect(  0,           0,
                                            2 * parentSize.width(), parentSize.height());
            m_finalWidgetGeometry = QRect(- parentSize.width(),     0,
                                            2 * parentSize.width(), parentSize.height());
            break;
        }
        case Qt::Vertical:
        {
            m_startWidgetGeometry = QRect(0,         0,
                                          parentSize.width(),   2 * parentSize.height());
            m_finalWidgetGeometry = QRect(0,       - parentSize.height(),
                                          parentSize.width(),   2 * parentSize.height());
            break;
        }
    }
    if (m_pAnimation)
        m_pAnimation->update();

    /* Update label content: */
    QPixmap pixmap1(parentSize);
    QPixmap pixmap2(parentSize);
    m_pWidget1->render(&pixmap1);
    m_pWidget2->render(&pixmap2);
    m_pLabel1->setPixmap(pixmap1);
    m_pLabel2->setPixmap(pixmap2);

    /* Update initial widget geometry: */
    switch (enmDirection)
    {
        case SlidingDirection_Forward:
        {
            setWidgetGeometry(m_startWidgetGeometry);
            emit sigForward();
            break;
        }
        case SlidingDirection_Reverse:
        {
            setWidgetGeometry(m_finalWidgetGeometry);
            emit sigReverse();
            break;
        }
    }
}

void UISlidingAnimation::prepare()
{
    /* Create animation: */
    m_pAnimation = UIAnimation::installPropertyAnimation(this,
                                                         "widgetGeometry",
                                                         "startWidgetGeometry", "finalWidgetGeometry",
                                                         SIGNAL(sigForward()), SIGNAL(sigReverse()));
    connect(m_pAnimation, &UIAnimation::sigStateEnteredStart, this, &UISlidingAnimation::sltHandleStateEnteredStart);
    connect(m_pAnimation, &UIAnimation::sigStateEnteredFinal, this, &UISlidingAnimation::sltHandleStateEnteredFinal);

    /* Create private sliding widget: */
    m_pWidget = new QWidget(this);
    if (m_pWidget)
    {
        /* Create layout: */
        QBoxLayout *pLayout = 0;
        switch (m_enmOrientation)
        {
            case Qt::Horizontal: pLayout = new QHBoxLayout(m_pWidget); break;
            case Qt::Vertical:   pLayout = new QVBoxLayout(m_pWidget); break;
        }
        if (pLayout)
        {
            /* Configure layout: */
            pLayout->setSpacing(0);
            pLayout->setContentsMargins(0, 0, 0, 0);

            /* Create 1st label: */
            m_pLabel1 = new QLabel;
            if (m_pLabel1)
                pLayout->addWidget(m_pLabel1);
            /* Create 2nd label: */
            m_pLabel2 = new QLabel;
            if (m_pLabel2)
                pLayout->addWidget(m_pLabel2);
        }
    }

    /* Assign initial widget geometry: */
    m_pWidget->setGeometry(0, 0, width(), height());
}

void UISlidingAnimation::setWidgetGeometry(const QRect &rect)
{
    /* Define widget geometry: */
    if (m_pWidget)
        m_pWidget->setGeometry(rect);
}

QRect UISlidingAnimation::widgetGeometry() const
{
    /* Return widget geometry: */
    return m_pWidget ? m_pWidget->geometry() : QRect();
}
