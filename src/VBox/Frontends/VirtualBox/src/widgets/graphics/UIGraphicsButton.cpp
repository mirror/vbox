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
#include <QGraphicsSceneMouseEvent>
#include <QPropertyAnimation>

/* GUI includes: */
#include "UIGraphicsButton.h"

UIGraphicsButton::UIGraphicsButton(QIGraphicsWidget *pParent, const QIcon &icon)
    : QIGraphicsWidget(pParent)
    , m_icon(icon)
    , m_buttonType(UIGraphicsButtonType_Iconified)
    , m_pAnimation(0)
    , m_fParentSelected(false)
    , m_fHovered(false)
    , m_iColor(-1)
{
    /* Refresh finally: */
    refresh();
}

UIGraphicsButton::UIGraphicsButton(QIGraphicsWidget *pParent, UIGraphicsButtonType buttonType)
    : QIGraphicsWidget(pParent)
    , m_buttonType(buttonType)
    , m_pAnimation(0)
    , m_fParentSelected(false)
    , m_fHovered(false)
    , m_iColor(-1)
{
    /* Refresh finally: */
    refresh();

    /* Prepare animation: */
    setAcceptHoverEvents(true);
    m_pAnimation = new QPropertyAnimation(this, "color", this);
    m_pAnimation->setDuration(1000);
    m_pAnimation->setLoopCount(-1);
    reconfigureAnimation();
}

void UIGraphicsButton::setParentSelected(bool fParentSelected)
{
    if (m_fParentSelected == fParentSelected)
        return;
    m_fParentSelected = fParentSelected;
    reconfigureAnimation();
}

QVariant UIGraphicsButton::data(int iKey) const
{
    switch (iKey)
    {
        case GraphicsButton_Margin: return 0;
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
    /* Prepare variables: */
    int iMargin = data(GraphicsButton_Margin).toInt();
    QIcon icon = data(GraphicsButton_Icon).value<QIcon>();
    QSize iconSize = data(GraphicsButton_IconSize).toSize();

    /* Which type button has: */
    switch (m_buttonType)
    {
        case UIGraphicsButtonType_Iconified:
        {
            /* Just draw the pixmap: */
            pPainter->drawPixmap(QRect(QPoint(iMargin, iMargin), iconSize), icon.pixmap(iconSize));
            break;
        }
        case UIGraphicsButtonType_DirectArrow:
        {
            /* Prepare variables: */
            QPalette pal = palette();
            QColor windowColor = pal.color(QPalette::Window);

            /* Setup: */
            pPainter->setRenderHint(QPainter::Antialiasing);
            QPen pen = pPainter->pen();
            pen.setColor(windowColor.darker(color()));
            pen.setWidth(2);
            pen.setCapStyle(Qt::RoundCap);

            /* Draw path: */
            QPainterPath circlePath;
            circlePath.moveTo(iMargin, iMargin);
            circlePath.lineTo(iMargin + iconSize.width() / 2, iMargin);
            circlePath.arcTo(QRectF(circlePath.currentPosition(), iconSize).translated(-iconSize.width() / 2, 0), 90, -180);
            circlePath.lineTo(iMargin, iMargin + iconSize.height());
            circlePath.closeSubpath();
            pPainter->strokePath(circlePath, pen);

            /* Draw triangle: */
            QPainterPath linePath;
            linePath.moveTo(iMargin + 5, iMargin + 5);
            linePath.lineTo(iMargin + iconSize.height() - 5, iMargin + iconSize.width() / 2);
            linePath.lineTo(iMargin + 5, iMargin + iconSize.width() - 5);
            pPainter->strokePath(linePath, pen);
            break;
        }
        case UIGraphicsButtonType_RoundArrow:
        {
            /* Prepare variables: */
            QPalette pal = palette();
            QColor windowColor = pal.color(QPalette::Window);

            /* Setup: */
            pPainter->setRenderHint(QPainter::Antialiasing);
            QPen pen = pPainter->pen();
            pen.setColor(windowColor.darker(color()));
            pen.setWidth(2);
            pen.setCapStyle(Qt::RoundCap);

            /* Draw circle: */
            QPainterPath circlePath;
            circlePath.moveTo(iMargin, iMargin);
            circlePath.addEllipse(QRectF(circlePath.currentPosition(), iconSize));
            pPainter->strokePath(circlePath, pen);

            /* Draw triangle: */
            QPainterPath linePath;
            linePath.moveTo(iMargin + 5, iMargin + 5);
            linePath.lineTo(iMargin + iconSize.height() - 5, iMargin + iconSize.width() / 2);
            linePath.lineTo(iMargin + 5, iMargin + iconSize.width() - 5);
            pPainter->strokePath(linePath, pen);
            break;
        }
    }
}

void UIGraphicsButton::hideEvent(QHideEvent*)
{
    setHovered(false);
}

void UIGraphicsButton::mousePressEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Accepting this event allows to get release-event: */
    pEvent->accept();
}

void UIGraphicsButton::mouseReleaseEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Call to base-class: */
    QIGraphicsWidget::mouseReleaseEvent(pEvent);
    /* Notify listeners about button click: */
    emit sigButtonClicked();
}

void UIGraphicsButton::hoverMoveEvent(QGraphicsSceneHoverEvent*)
{
    setHovered(true);
}

void UIGraphicsButton::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    setHovered(false);
}

void UIGraphicsButton::refresh()
{
    /* Refresh geometry: */
    updateGeometry();
    /* Resize to minimum size: */
    resize(minimumSizeHint());
}

void UIGraphicsButton::reconfigureAnimation()
{
    setColor(m_fParentSelected ? 105 : 140);
    m_pAnimation->setStartValue(m_fParentSelected ? 105 : 140);
    m_pAnimation->setEndValue(m_fParentSelected ? 105 : 140);
    m_pAnimation->setKeyValueAt(0.5, m_fParentSelected ? 130 : 115);
}

bool UIGraphicsButton::hovered() const
{
    return m_fHovered;
}

void UIGraphicsButton::setHovered(bool fHovered)
{
    if (m_fHovered == fHovered)
        return;

    m_fHovered = fHovered;
    if (m_fHovered)
    {
        m_pAnimation->start();
    }
    else
    {
        m_pAnimation->stop();
        setColor(m_fParentSelected ? 105 : 140);
    }
}

int UIGraphicsButton::color() const
{
    return m_iColor;
}

void UIGraphicsButton::setColor(int iColor)
{
    m_iColor = iColor;
    update();
}

