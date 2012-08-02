/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGraphicsButton class definition
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
#include <QPainter>

/* GUI includes: */
#include "UIGraphicsButton.h"

UIGraphicsButton::UIGraphicsButton(QIGraphicsWidget *pParent)
    : QIGraphicsWidget(pParent)
{
    /* Refresh finally: */
    refresh();
}

void UIGraphicsButton::setIcon(const QIcon &icon)
{
    /* Remember icon: */
    m_icon = icon;
    /* Relayout/redraw button: */
    refresh();
}

QVariant UIGraphicsButton::data(int iKey) const
{
    switch (iKey)
    {
        case GraphicsButton_Margin: return 3;
        case GraphicsButton_IconSize: return m_icon.isNull() ? QSize(16, 16) : m_icon.availableSizes().first();
        case GraphicsButton_Icon: return m_icon;
        default: break;
    }
    return QVariant();
}

QSizeF UIGraphicsButton::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* Calculations for minimum size: */
    if (which == Qt::MinimumSize)
    {
        /* Variables: */
        int iMargin = data(GraphicsButton_Margin).toInt();
        QSize iconSize = data(GraphicsButton_IconSize).toSize();
        /* Calculations: */
        int iWidth = 2 * iMargin + iconSize.width();
        int iHeight = 2 * iMargin + iconSize.height();
        return QSize(iWidth, iHeight);
    }
    /* Call to base-class: */
    return QIGraphicsWidget::sizeHint(which, constraint);
}

void UIGraphicsButton::paint(QPainter *pPainter, const QStyleOptionGraphicsItem* /* pOption */, QWidget* /* pWidget = 0 */)
{
    /* Variables: */
    int iMargin = data(GraphicsButton_Margin).toInt();
    QSize iconSize = data(GraphicsButton_IconSize).toSize();
    QIcon icon = data(GraphicsButton_Icon).value<QIcon>();

    /* Draw pixmap: */
    pPainter->drawPixmap(/* Pixmap rectangle: */
                         QRect(QPoint(iMargin, iMargin), iconSize),
                         /* Pixmap size: */
                         icon.pixmap(iconSize));
}

void UIGraphicsButton::mousePressEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Notify listeners about button click: */
    emit sigButtonClicked();
    /* Propagate to parent: */
    QIGraphicsWidget::mousePressEvent(pEvent);
}

void UIGraphicsButton::refresh()
{
    /* Refresh geometry: */
    updateGeometry();
    /* Resize to minimum size: */
    resize(minimumSizeHint());
}

