/* $Id$ */
/** @file
 * VBox Qt GUI - UIGraphicsScrollArea class implementation.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
#include <QGraphicsScene>

/* GUI includes: */
#include "UIGraphicsScrollArea.h"
#include "UIGraphicsScrollBar.h"


UIGraphicsScrollArea::UIGraphicsScrollArea(Qt::Orientation enmOrientation, QGraphicsScene *pScene /* = 0 */)
    : m_enmOrientation(enmOrientation)
    , m_pScrollBar(0)
    , m_pViewport(0)
{
    pScene->addItem(this);
    prepare();
}

UIGraphicsScrollArea::UIGraphicsScrollArea(Qt::Orientation enmOrientation, QIGraphicsWidget *pParent /* = 0 */)
    : QIGraphicsWidget(pParent)
    , m_enmOrientation(enmOrientation)
    , m_pScrollBar(0)
    , m_pViewport(0)
{
    prepare();
}

QSizeF UIGraphicsScrollArea::minimumSizeHint() const
{
    /* Minimum size-hint of scroll-bar by default: */
    QSizeF msh = m_pScrollBar->minimumSizeHint();
    if (m_pViewport)
    {
        switch (m_enmOrientation)
        {
            case Qt::Horizontal:
            {
                /* Expand it with viewport height: */
                const int iWidgetHeight = m_pViewport->size().height();
                if (msh.height() < iWidgetHeight)
                    msh.setHeight(iWidgetHeight);
                break;
            }
            case Qt::Vertical:
            {
                /* Expand it with viewport width: */
                const int iWidgetWidth = m_pViewport->size().width();
                if (msh.width() < iWidgetWidth)
                    msh.setWidth(iWidgetWidth);
                break;
            }
        }
    }
    return msh;
}

void UIGraphicsScrollArea::setViewport(QIGraphicsWidget *pViewport)
{
    /* Forget previous widget: */
    if (m_pViewport)
    {
        m_pViewport->removeEventFilter(this);
        m_pViewport->setParentItem(0);
        m_pViewport = 0;
    }

    /* Remember passed widget: */
    if (pViewport)
    {
        m_pViewport = pViewport;
        m_pViewport->setParentItem(this);
        m_pViewport->installEventFilter(this);
    }

    /* Layout widgets: */
    layoutWidgets();
}

QIGraphicsWidget *UIGraphicsScrollArea::viewport() const
{
    return m_pViewport;
}

void UIGraphicsScrollArea::makeSureRectIsVisible(const QRectF &rect)
{
    /* Make sure rect size is bound by the scroll-area size: */
    QRectF actualRect = rect;
    QSizeF actualRectSize = actualRect.size();
    actualRectSize = actualRectSize.boundedTo(size());
    actualRect.setSize(actualRectSize);

    switch (m_enmOrientation)
    {
        /* Scroll viewport horizontally: */
        case Qt::Horizontal:
        {
            /* If rectangle is at least partially right of visible area: */
            if (actualRect.x() + actualRect.width() > size().width())
                m_pScrollBar->setValue(m_pScrollBar->value() + actualRect.x() + actualRect.width() - size().width());
            /* If rectangle is at least partially left of visible area: */
            else if (actualRect.x() < 0)
                m_pScrollBar->setValue(m_pScrollBar->value() + actualRect.x());
            break;
        }
        /* Scroll viewport vertically: */
        case Qt::Vertical:
        {
            /* If rectangle is at least partially under visible area: */
            if (actualRect.y() + actualRect.height() > size().height())
                m_pScrollBar->setValue(m_pScrollBar->value() + actualRect.y() + actualRect.height() - size().height());
            /* If rectangle is at least partially above visible area: */
            else if (actualRect.y() < 0)
                m_pScrollBar->setValue(m_pScrollBar->value() + actualRect.y());
            break;
        }
    }
}

bool UIGraphicsScrollArea::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Handle layout requests for m_pViewport if set: */
    if (   m_pViewport
        && pObject == m_pViewport
        && pEvent->type() == QEvent::LayoutRequest)
        layoutWidgets();

    /* Handle redirected wheel events: */
    if (pEvent->type() == QEvent::Wheel)
    {
        QWheelEvent *pWheelEvent = static_cast<QWheelEvent*>(pEvent);
        const QPoint angleDelta = pWheelEvent->angleDelta();
        switch (m_enmOrientation)
        {
            /* Scroll viewport horizontally: */
            case Qt::Horizontal:
            {
                if (angleDelta.x() > 0)
                    m_pScrollBar->setValue(m_pScrollBar->value() - m_pScrollBar->step());
                else
                    m_pScrollBar->setValue(m_pScrollBar->value() + m_pScrollBar->step());
                break;
            }
            /* Scroll viewport vertically: */
            case Qt::Vertical:
            {
                if (angleDelta.y() > 0)
                    m_pScrollBar->setValue(m_pScrollBar->value() - m_pScrollBar->step());
                else
                    m_pScrollBar->setValue(m_pScrollBar->value() + m_pScrollBar->step());
                break;
            }
        }
    }

    /* Call to base-class: */
    return QIGraphicsWidget::eventFilter(pObject, pEvent);
}

void UIGraphicsScrollArea::resizeEvent(QGraphicsSceneResizeEvent *pEvent)
{
    /* Call to base-class: */
    QIGraphicsWidget::resizeEvent(pEvent);

    /* Layout widgets: */
    layoutWidgets();
}

void UIGraphicsScrollArea::sltHandleScrollBarValueChange(int iValue)
{
    switch (m_enmOrientation)
    {
        /* Shift viewport horizontally: */
        case Qt::Horizontal: m_pViewport->setPos(-iValue, 0); break;
        /* Shift viewport vertically: */
        case Qt::Vertical:   m_pViewport->setPos(0, -iValue); break;
    }
}

void UIGraphicsScrollArea::prepare()
{
    /* Prepare/layout widgets: */
    prepareWidgets();
    layoutWidgets();
}

void UIGraphicsScrollArea::prepareWidgets()
{
    /* Create scroll-bar: */
    m_pScrollBar = new UIGraphicsScrollBar(m_enmOrientation, this);
    if (m_pScrollBar)
    {
        m_pScrollBar->setStep(10);
        m_pScrollBar->setZValue(1);
        connect(m_pScrollBar, &UIGraphicsScrollBar::sigValueChanged,
                this, &UIGraphicsScrollArea::sltHandleScrollBarValueChange);
    }
}

void UIGraphicsScrollArea::layoutWidgets()
{
    layoutViewport();
    layoutScrollBar();
}

void UIGraphicsScrollArea::layoutViewport()
{
    if (m_pViewport)
    {
        switch (m_enmOrientation)
        {
            case Qt::Horizontal:
            {
                /* Align viewport and shift it horizontally: */
                m_pViewport->resize(m_pViewport->minimumSizeHint().width(), size().height());
                m_pViewport->setPos(-m_pScrollBar->value(), 0);
                break;
            }
            case Qt::Vertical:
            {
                /* Align viewport and shift it vertically: */
                m_pViewport->resize(size().width(), m_pViewport->minimumSizeHint().height());
                m_pViewport->setPos(0, -m_pScrollBar->value());
                break;
            }
        }
    }
}

void UIGraphicsScrollArea::layoutScrollBar()
{
    switch (m_enmOrientation)
    {
        case Qt::Horizontal:
        {
            /* Align scroll-bar horizontally: */
            m_pScrollBar->resize(size().width(), m_pScrollBar->minimumSizeHint().height());
            m_pScrollBar->setPos(0, size().height() - m_pScrollBar->size().height());
            if (m_pViewport)
            {
                /* Adjust scroll-bar maximum value according to viewport width: */
                const int iWidth = size().width();
                const int iWidgetWidth = m_pViewport->size().width();
                if (iWidgetWidth > iWidth)
                    m_pScrollBar->setMaximum(iWidgetWidth - iWidth);
                else
                    m_pScrollBar->setMaximum(0);
            }
            break;
        }
        case Qt::Vertical:
        {
            /* Align scroll-bar vertically: */
            m_pScrollBar->resize(m_pScrollBar->minimumSizeHint().width(), size().height());
            m_pScrollBar->setPos(size().width() - m_pScrollBar->size().width(), 0);
            if (m_pViewport)
            {
                /* Adjust scroll-bar maximum value according to viewport height: */
                const int iHeight = size().height();
                const int iWidgetHeight = m_pViewport->size().height();
                if (iWidgetHeight > iHeight)
                    m_pScrollBar->setMaximum(iWidgetHeight - iHeight);
                else
                    m_pScrollBar->setMaximum(0);
            }
            break;
        }
    }

    /* Make scroll-bar visible only when there is viewport and maximum less than minimum: */
    m_pScrollBar->setVisible(m_pViewport && m_pScrollBar->maximum() > m_pScrollBar->minimum());
}
