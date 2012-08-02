/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGSelectorItem class definition
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
#include <QApplication>
#include <QStyle>
#include <QPainter>
#include <QGraphicsScene>
#include <QStyleOptionFocusRect>
#include <QPropertyAnimation>
#include <QGraphicsSceneMouseEvent>

/* GUI includes: */
#include "UIGSelectorItem.h"
#include "UIGraphicsSelectorModel.h"
#include "UIGSelectorItemGroup.h"
#include "UIGSelectorItemMachine.h"

UIGSelectorItem::UIGSelectorItem(UIGSelectorItem *pParent, int iPosition)
    : QIGraphicsWidget(pParent)
    , m_pParent(pParent)
    , m_iDesiredWidth(0)
    , m_iDesiredHeight(0)
    , m_dragTokenPlace(DragToken_Off)
    , m_pAnimation(0)
    , m_iInitialGradientDarkness(115)
{
    /* Basic item setup: */
    setOwnedByLayout(false);
    setAcceptDrops(true);
    setFocusPolicy(Qt::NoFocus);
    setFlag(QGraphicsItem::ItemIsSelectable, false);

    /* Non-root item? */
    if (parentItem())
    {
        /* Non-root item setup: */
        setAcceptHoverEvents(true);

        /* Prepare animation: */
        m_pAnimation = new QPropertyAnimation(this, "gradient");
        m_pAnimation->setDuration(500);
        m_pAnimation->setStartValue(m_iInitialGradientDarkness - 50);
        m_pAnimation->setEndValue(m_iInitialGradientDarkness);
    }
}

UIGSelectorItem::~UIGSelectorItem()
{
    /* If that item is focused: */
    if (model()->focusItem() == this)
    {
        /* Unset the focus/selection: */
        model()->setFocusItem(0, true);
    }
    /* If that item is NOT focused, but selected: */
    else if (model()->selectionList().contains(this))
    {
        /* Remove item from the selection list: */
        model()->removeFromSelectionList(this);
    }
    /* Remove item from the navigation list: */
    model()->removeFromNavigationList(this);
}

UIGSelectorItemGroup* UIGSelectorItem::toGroupItem()
{
    UIGSelectorItemGroup *pItem = qgraphicsitem_cast<UIGSelectorItemGroup*>(this);
    AssertMsg(pItem, ("Trying to cast invalid item type to UIGSelectorItemGroup!"));
    return pItem;
}

UIGSelectorItemMachine* UIGSelectorItem::toMachineItem()
{
    UIGSelectorItemMachine *pItem = qgraphicsitem_cast<UIGSelectorItemMachine*>(this);
    AssertMsg(pItem, ("Trying to cast invalid item type to UIGSelectorItemMachine!"));
    return pItem;
}

UIGraphicsSelectorModel* UIGSelectorItem::model() const
{
    UIGraphicsSelectorModel *pModel = qobject_cast<UIGraphicsSelectorModel*>(QIGraphicsWidget::scene()->parent());
    AssertMsg(pModel, ("Incorrect graphics scene parent set!"));
    return pModel;
}

UIGSelectorItem* UIGSelectorItem::parentItem() const
{
    return m_pParent;
}

void UIGSelectorItem::setDesiredWidth(int iDesiredWidth)
{
    m_iDesiredWidth = iDesiredWidth;
}

void UIGSelectorItem::setDesiredHeight(int iDesiredHeight)
{
    m_iDesiredHeight = iDesiredHeight;
}

int UIGSelectorItem::desiredWidth() const
{
    return m_iDesiredWidth;
}

int UIGSelectorItem::desiredHeight() const
{
    return m_iDesiredHeight;
}

void UIGSelectorItem::makeSureItsVisible()
{
    /* If item is not visible: */
    if (!isVisible())
    {
        /* Get parrent item, assert if can't: */
        if (UIGSelectorItemGroup *pParentItem = parentItem()->toGroupItem())
        {
            /* We should make parent visible: */
            pParentItem->makeSureItsVisible();
            /* And make sure its opened: */
            if (pParentItem->closed())
                pParentItem->open();
        }
    }
}

void UIGSelectorItem::setDragTokenPlace(DragToken where)
{
    /* Something changed? */
    if (m_dragTokenPlace != where)
    {
        m_dragTokenPlace = where;
        update();
    }
}

DragToken UIGSelectorItem::dragTokenPlace() const
{
    return m_dragTokenPlace;
}

void UIGSelectorItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    if (m_pAnimation && m_pAnimation->state() != QAbstractAnimation::Running)
        m_pAnimation->start();
}

void UIGSelectorItem::mousePressEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* By default, non-moveable and non-selectable items
     * can't grab mouse-press events which is required
     * to grab further mouse-move events which we wants... */
    pEvent->accept();
}

void UIGSelectorItem::mouseMoveEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Make sure item is really dragged: */
    if (QLineF(pEvent->screenPos(),
               pEvent->buttonDownScreenPos(Qt::LeftButton)).length() <
        QApplication::startDragDistance())
        return;

    /* Initialize dragging: */
    QDrag *pDrag = new QDrag(pEvent->widget());
    model()->setCurrentDragObject(pDrag);
    pDrag->setPixmap(toPixmap());
    pDrag->setMimeData(createMimeData());
    pDrag->exec(Qt::MoveAction | Qt::CopyAction, Qt::MoveAction);
}

void UIGSelectorItem::dragMoveEvent(QGraphicsSceneDragDropEvent *pEvent)
{
    /* Make sure we are non-root: */
    if (parentItem())
    {
        /* Allow drag tokens only for the same item type as current: */
        bool fAllowDragToken = false;
        if ((type() == UIGSelectorItemType_Group &&
             pEvent->mimeData()->hasFormat(UIGSelectorItemGroup::className())) ||
            (type() == UIGSelectorItemType_Machine &&
             pEvent->mimeData()->hasFormat(UIGSelectorItemMachine::className())))
            fAllowDragToken = true;
        /* Do we need a drag-token? */
        if (fAllowDragToken)
        {
            QPoint p = pEvent->pos().toPoint();
            if (p.y() < 10)
                setDragTokenPlace(DragToken_Up);
            else if (p.y() > minimumSizeHint().toSize().height() - 10)
                setDragTokenPlace(DragToken_Down);
            else
                setDragTokenPlace(DragToken_Off);
        }
    }
    /* Check if drop is allowed: */
    pEvent->setAccepted(isDropAllowed(pEvent, dragTokenPlace()));
}

void UIGSelectorItem::dragLeaveEvent(QGraphicsSceneDragDropEvent*)
{
    resetDragToken();
}

void UIGSelectorItem::dropEvent(QGraphicsSceneDragDropEvent *pEvent)
{
    /* Do we have token active? */
    switch (dragTokenPlace())
    {
        case DragToken_Off:
        {
            /* Its our drop, processing: */
            processDrop(pEvent);
            break;
        }
        default:
        {
            /* Its parent drop, passing: */
            parentItem()->processDrop(pEvent, this, dragTokenPlace());
            break;
        }
    }
}

/* static */
void UIGSelectorItem::paintBackground(QPainter *pPainter, const QRect &rect,
                                      bool fHasParent, bool fIsSelected,
                                      int iGradientDarkness, DragToken dragTokenWhere)
{
    /* Fill rectangle with brush: */
    QPalette pal = QApplication::palette();
    QColor base = pal.color(QPalette::Active, fIsSelected ? QPalette::Highlight : QPalette::Window);
    pPainter->fillRect(rect, pal.brush(QPalette::Active, QPalette::Base));

    /* Add gradient: */
    QLinearGradient lg(rect.topLeft(), rect.topRight());
    /* For non-root item: */
    if (fHasParent)
    {
        /* Background is animated if drag token missed: */
        lg.setColorAt(0, base.darker(dragTokenWhere == DragToken_Off ? iGradientDarkness : 115));
        lg.setColorAt(1, base.darker(105));
    }
    else
    {
        /* Background is simple: */
        lg.setColorAt(0, base.darker(100));
        lg.setColorAt(1, base.darker(100));
    }
    pPainter->fillRect(rect, lg);

    /* Paint drag token UP? */
    if (dragTokenWhere != DragToken_Off)
    {
        QLinearGradient dragTokenGradient;
        QRect dragTokenRect = rect;
        if (dragTokenWhere == DragToken_Up)
        {
            dragTokenRect.setHeight(10);
            dragTokenGradient.setStart(dragTokenRect.bottomLeft());
            dragTokenGradient.setFinalStop(dragTokenRect.topLeft());
        }
        else if (dragTokenWhere == DragToken_Down)
        {
            dragTokenRect.setTopLeft(dragTokenRect.bottomLeft() - QPoint(0, 10));
            dragTokenGradient.setStart(dragTokenRect.topLeft());
            dragTokenGradient.setFinalStop(dragTokenRect.bottomLeft());
        }
        dragTokenGradient.setColorAt(0, base.darker(115));
        dragTokenGradient.setColorAt(1, base.darker(150));
        pPainter->fillRect(dragTokenRect, dragTokenGradient);
    }
}

/* static */
void UIGSelectorItem::paintPixmap(QPainter *pPainter, const QRect &rect, const QPixmap &pixmap)
{
    pPainter->drawPixmap(rect, pixmap);
}

/* static */
void UIGSelectorItem::paintText(QPainter *pPainter, const QRect &rect, const QFont &font, const QString &strText)
{
    pPainter->setFont(font);
    pPainter->drawText(rect, strText);
}

/* static */
void UIGSelectorItem::paintFocus(QPainter *pPainter, const QRect &rect)
{
    QStyleOptionFocusRect option;
    option.rect = rect;
    QApplication::style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, pPainter);
}

UIGSelectorItemMimeData::UIGSelectorItemMimeData(UIGSelectorItem *pItem)
    : m_pItem(pItem)
{
}

bool UIGSelectorItemMimeData::hasFormat(const QString &strMimeType) const
{
    if (strMimeType == QString(m_pItem->metaObject()->className()))
        return true;
    return false;
}

UIGSelectorItem* UIGSelectorItemMimeData::item() const
{
    return m_pItem;
}

