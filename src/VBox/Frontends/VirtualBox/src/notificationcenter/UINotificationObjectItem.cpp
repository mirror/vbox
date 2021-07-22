/* $Id$ */
/** @file
 * VBox Qt GUI - UINotificationObjectItem class implementation.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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
#include <QApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QProgressBar>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UINotificationObject.h"
#include "UINotificationObjectItem.h"


/*********************************************************************************************************************************
*   Class UINotificationObjectItem implementation.                                                                               *
*********************************************************************************************************************************/

UINotificationObjectItem::UINotificationObjectItem(QWidget *pParent, UINotificationObject *pObject /* = 0 */)
    : QWidget(pParent)
    , m_pObject(pObject)
    , m_pLayoutMain(0)
    , m_pLayoutUpper(0)
    , m_pLabelName(0)
    , m_pButtonClose(0)
    , m_pLabelDetails(0)
    , m_fHovered(false)
    , m_fToggled(false)
{
    /* Make sure item is opaque. */
    setAutoFillBackground(true);

    /* Prepare main layout: */
    m_pLayoutMain = new QVBoxLayout(this);
    if (m_pLayoutMain)
    {
        /* Prepare upper layout: */
        m_pLayoutUpper = new QHBoxLayout;
        if (m_pLayoutUpper)
        {
            /* Prepare name label: */
            m_pLabelName = new QLabel(this);
            if (m_pLabelName)
                m_pLayoutUpper->addWidget(m_pLabelName);

            /* Prepare close button: */
            m_pButtonClose = new QIToolButton(this);
            if (m_pButtonClose)
            {
                m_pButtonClose->setIcon(UIIconPool::iconSet(":/close_16px.png"));
                m_pButtonClose->setIconSize(QSize(10, 10));
                connect(m_pButtonClose, &QIToolButton::clicked,
                        m_pObject, &UINotificationObject::close);

                m_pLayoutUpper->addWidget(m_pButtonClose);
            }

            /* Add to layout: */
            m_pLayoutMain->addLayout(m_pLayoutUpper);
        }

        /* Prepare details label: */
        m_pLabelDetails = new QIRichTextLabel(this);
        if (m_pLabelDetails)
        {
            QFont myFont = m_pLabelDetails->font();
            myFont.setPointSize(myFont.pointSize() - 1);
            m_pLabelDetails->setBrowserFont(myFont);
            m_pLabelDetails->setVisible(false);

            m_pLayoutMain->addWidget(m_pLabelDetails);
        }
    }
}

bool UINotificationObjectItem::event(QEvent *pEvent)
{
    /* Handle required event types: */
    switch (pEvent->type())
    {
        case QEvent::Enter:
        case QEvent::MouseMove:
        {
            m_fHovered = true;
            update();
            break;
        }
        case QEvent::Leave:
        {
            m_fHovered = false;
            update();
            break;
        }
        case QEvent::MouseButtonRelease:
        {
            m_fToggled = !m_fToggled;
            m_pLabelDetails->setVisible(m_fToggled);
            break;
        }
        default:
            break;
    }

    /* Call to base-class: */
    return QWidget::event(pEvent);
}

void UINotificationObjectItem::paintEvent(QPaintEvent *pPaintEvent)
{
    /* Prepare painter: */
    QPainter painter(this);
    painter.setClipRect(pPaintEvent->rect());
    /* Acquire palette: */
    const bool fActive = isActiveWindow();
    QPalette pal = QApplication::palette();

    /* Gather suitable colors: */
    QColor color = pal.color(fActive ? QPalette::Active : QPalette::Inactive, QPalette::Window);
    QColor color1;
    QColor color2;
    if (color.black() > 128)
    {
        color1 = color.lighter(110);
        color2 = color.lighter(105);
    }
    else
    {
        color1 = color.darker(105);
        color2 = color.darker(110);
    }
    /* Prepare background gradient: */
    QLinearGradient grad(QPointF(0, 0), QPointF(width(), height()));
    {
        grad.setColorAt(0, color1);
        grad.setColorAt(1, color2);
    }
    /* Fill background: */
    painter.fillRect(rect(), grad);

    /* If item is hovered: */
    if (m_fHovered)
    {
        /* Gather suitable color: */
        QColor color3 = pal.color(fActive ? QPalette::Active : QPalette::Inactive, QPalette::Highlight);
        /* Override painter pen: */
        painter.setPen(color3);
        /* Draw frame: */
        painter.drawRect(rect());
    }
}


/*********************************************************************************************************************************
*   Class UINotificationProgressItem implementation.                                                                             *
*********************************************************************************************************************************/

UINotificationProgressItem::UINotificationProgressItem(QWidget *pParent, UINotificationProgress *pProgress /* = 0 */)
    : UINotificationObjectItem(pParent, pProgress)
    , m_pProgressBar(0)
{
    /* Main layout was prepared in base-class: */
    if (m_pLayoutMain)
    {
        /* Name label was prepared in base-class: */
        if (m_pLabelName)
            m_pLabelName->setText(progress()->name());
        /* Details label was prepared in base-class: */
        if (m_pLabelDetails)
        {
            m_pLabelDetails->setText(progress()->details());
            int iHint = m_pLabelName->minimumSizeHint().width()
                      + m_pLayoutUpper->spacing()
                      + m_pButtonClose->minimumSizeHint().width();
            m_pLabelDetails->setMinimumTextWidth(iHint);
        }

        /* Prepare progress-bar: */
        m_pProgressBar = new QProgressBar(this);
        if (m_pProgressBar)
        {
            m_pProgressBar->setMinimum(0);
            m_pProgressBar->setMaximum(100);
            m_pProgressBar->setValue(progress()->percent());

            m_pLayoutMain->addWidget(m_pProgressBar);
        }
    }

    /* Prepare progress connections: */
    connect(progress(), &UINotificationProgress::sigProgressStarted,
            this, &UINotificationProgressItem::sltHandleProgressStarted);
    connect(progress(), &UINotificationProgress::sigProgressChange,
            this, &UINotificationProgressItem::sltHandleProgressChange);
    connect(progress(), &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressItem::sltHandleProgressFinished);
}

void UINotificationProgressItem::sltHandleProgressStarted()
{
    /* Init close-button and progress-bar states: */
    if (m_pButtonClose)
        m_pButtonClose->setEnabled(progress()->isCancelable());
    if (m_pProgressBar)
        m_pProgressBar->setValue(0);
}

void UINotificationProgressItem::sltHandleProgressChange(ulong uPercent)
{
    /* Update close-button and progress-bar states: */
    if (m_pButtonClose)
        m_pButtonClose->setEnabled(progress()->isCancelable());
    if (m_pProgressBar)
        m_pProgressBar->setValue(uPercent);
}

void UINotificationProgressItem::sltHandleProgressFinished()
{
    /* Finalize close-button and progress-bar states: */
    if (m_pButtonClose)
        m_pButtonClose->setEnabled(true);
    if (m_pProgressBar)
        m_pProgressBar->setValue(100);
    /* Update details with error text if any: */
    if (m_pLabelDetails)
    {
        const QString strDetails = progress()->details();
        const QString strError = progress()->error();
        const QString strFullDetails = strError.isNull()
                                     ? strDetails
                                     : QString("%1<br>%2").arg(strDetails, strError);
        m_pLabelDetails->setText(strFullDetails);
    }
}

UINotificationProgress *UINotificationProgressItem::progress() const
{
    return qobject_cast<UINotificationProgress*>(m_pObject);
}


/*********************************************************************************************************************************
*   Namespace UINotificationProgressItem implementation.                                                                         *
*********************************************************************************************************************************/

UINotificationObjectItem *UINotificationItem::create(QWidget *pParent, UINotificationObject *pObject)
{
    /* Handle known types: */
    if (pObject->inherits("UINotificationProgress"))
        return new UINotificationProgressItem(pParent, static_cast<UINotificationProgress*>(pObject));
    /* Handle defaults: */
    return new UINotificationObjectItem(pParent, pObject);
}
