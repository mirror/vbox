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
#include <QGraphicsSceneMouseEvent>
#include <QStateMachine>
#include <QPropertyAnimation>
#include <QSignalTransition>

/* GUI includes: */
#include "UIGSelectorItem.h"
#include "UIGraphicsSelectorModel.h"
#include "UIGSelectorItemGroup.h"
#include "UIGSelectorItemMachine.h"

UIGSelectorItem::UIGSelectorItem(UIGSelectorItem *pParent)
    : QIGraphicsWidget(pParent)
    , m_pParent(pParent)
    , m_dragTokenPlace(DragToken_Off)
    , m_pHighlightMachine(0)
    , m_pForwardAnimation(0)
    , m_pBackwardAnimation(0)
    , m_iAnimationDuration(300)
    , m_iDefaultDarkness(115)
    , m_iHighlightDarkness(90)
    , m_iGradient(m_iDefaultDarkness)
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

        /* Create state machine: */
        m_pHighlightMachine = new QStateMachine(this);
        /* Create 'default' state: */
        QState *pStateDefault = new QState(m_pHighlightMachine);
        /* Create 'highlighted' state: */
        QState *pStateHighlighted = new QState(m_pHighlightMachine);

        /* Forward animation: */
        m_pForwardAnimation = new QPropertyAnimation(this, "gradient", this);
        m_pForwardAnimation->setDuration(m_iAnimationDuration);
        m_pForwardAnimation->setStartValue(m_iDefaultDarkness);
        m_pForwardAnimation->setEndValue(m_iHighlightDarkness);

        /* Backward animation: */
        m_pBackwardAnimation = new QPropertyAnimation(this, "gradient", this);
        m_pBackwardAnimation->setDuration(m_iAnimationDuration);
        m_pBackwardAnimation->setStartValue(m_iHighlightDarkness);
        m_pBackwardAnimation->setEndValue(m_iDefaultDarkness);

        /* Add state transitions: */
        QSignalTransition *pDefaultToHighlighted = pStateDefault->addTransition(this, SIGNAL(sigHoverEnter()), pStateHighlighted);
        pDefaultToHighlighted->addAnimation(m_pForwardAnimation);
        QSignalTransition *pHighlightedToDefault = pStateHighlighted->addTransition(this, SIGNAL(sigHoverLeave()), pStateDefault);
        pHighlightedToDefault->addAnimation(m_pBackwardAnimation);

        /* Setup connections: */
        connect(this, SIGNAL(sigHoverEnter()), this, SLOT(sltUnhoverParent()));

        /* Initial state is 'default': */
        m_pHighlightMachine->setInitialState(pStateDefault);
        /* Start state-machine: */
        m_pHighlightMachine->start();
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

void UIGSelectorItem::releaseHover()
{
    emit sigHoverLeave();
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
    emit sigHoverEnter();
}

void UIGSelectorItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    emit sigHoverLeave();
}

void UIGSelectorItem::mousePressEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* By default, non-moveable and non-selectable items
     * can't grab mouse-press events which is required
     * to grab further mouse-move events which we wants... */
    if (parentItem())
        pEvent->accept();
    else
        pEvent->ignore();
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
void UIGSelectorItem::configurePainterShape(QPainter *pPainter,
                                            const QStyleOptionGraphicsItem *pOption,
                                            int iRadius)
{
    /* Rounded corners? */
    if (iRadius)
    {
        /* Setup clipping: */
        QPainterPath roundedPath;
        roundedPath.addRoundedRect(pOption->rect, iRadius, iRadius);
        pPainter->setRenderHint(QPainter::Antialiasing);
        pPainter->setClipPath(roundedPath);
    }
}

/* static */
void UIGSelectorItem::paintBackground(QPainter *pPainter, const QRect &rect,
                                      bool fHasParent, bool fIsSelected,
                                      int iGradientDarkness, DragToken dragTokenWhere,
                                      int iRadius)
{
    /* Save painter: */
    pPainter->save();

    /* Fill rectangle with brush/color: */
    QPalette pal = QApplication::palette();
    QBrush brush = pal.brush(QPalette::Active, QPalette::Base);
    pPainter->fillRect(rect, brush);
    pPainter->fillRect(rect, Qt::white);

    /* Prepare color: */
    QColor base = pal.color(QPalette::Active, fIsSelected ? QPalette::Highlight : QPalette::Window);

    /* Non-root item: */
    if (fHasParent)
    {
        /* Make even less rectangle: */
        QRect backGroundRect = rect;
        backGroundRect.setTopLeft(backGroundRect.topLeft() + QPoint(2, 2));
        backGroundRect.setBottomRight(backGroundRect.bottomRight() - QPoint(2, 2));
        /* Add even more clipping: */
        QPainterPath roundedPath;
        roundedPath.addRoundedRect(backGroundRect, iRadius, iRadius);
        pPainter->setClipPath(roundedPath);

        /* Calculate top rectangle: */
        QRect tRect = backGroundRect;
        tRect.setBottom(tRect.top() + tRect.height() / 3);
        /* Calculate bottom rectangle: */
        QRect bRect = backGroundRect;
        bRect.setTop(bRect.bottom() - bRect.height() / 3);
        /* Calculate middle rectangle: */
        QRect midRect = QRect(tRect.bottomLeft(), bRect.topRight());

        /* Prepare top gradient: */
        QLinearGradient tGradient(tRect.bottomLeft(), tRect.topLeft());
        tGradient.setColorAt(0, base.darker(104));
        tGradient.setColorAt(1, base.darker(iGradientDarkness));
        /* Prepare bottom gradient: */
        QLinearGradient bGradient(bRect.topLeft(), bRect.bottomLeft());
        bGradient.setColorAt(0, base.darker(104));
        bGradient.setColorAt(1, base.darker(iGradientDarkness));

        /* Paint all the stuff: */
        pPainter->fillRect(midRect, base.darker(104));
        pPainter->fillRect(tRect, tGradient);
        pPainter->fillRect(bRect, bGradient);

        /* Paint drag token UP? */
        if (dragTokenWhere != DragToken_Off)
        {
            QLinearGradient dragTokenGradient;
            QRect dragTokenRect = backGroundRect;
            if (dragTokenWhere == DragToken_Up)
            {
                dragTokenRect.setHeight(5);
                dragTokenGradient.setStart(dragTokenRect.bottomLeft());
                dragTokenGradient.setFinalStop(dragTokenRect.topLeft());
            }
            else if (dragTokenWhere == DragToken_Down)
            {
                dragTokenRect.setTopLeft(dragTokenRect.bottomLeft() - QPoint(0, 5));
                dragTokenGradient.setStart(dragTokenRect.topLeft());
                dragTokenGradient.setFinalStop(dragTokenRect.bottomLeft());
            }
            dragTokenGradient.setColorAt(0, base.darker(110));
            dragTokenGradient.setColorAt(1, base.darker(150));
            pPainter->fillRect(dragTokenRect, dragTokenGradient);
        }
    }
    /* Root item: */
    else
    {
        /* Simple and clear: */
        pPainter->fillRect(rect, base.darker(100));
    }

    /* Restore painter: */
    pPainter->restore();
}

/* static */
void UIGSelectorItem::paintFrameRect(QPainter *pPainter, const QRect &rect, bool fIsSelected, int iRadius)
{
    pPainter->save();
    QPalette pal = QApplication::palette();
    QColor base = pal.color(QPalette::Active, fIsSelected ? QPalette::Highlight : QPalette::Window);
    pPainter->setPen(base.darker(160));
    if (iRadius)
        pPainter->drawRoundedRect(rect, iRadius, iRadius);
    else
        pPainter->drawRect(rect);
    pPainter->restore();
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

void UIGSelectorItem::sltUnhoverParent()
{
    AssertMsg(parentItem(), ("There should be a parent!"));
    parentItem()->releaseHover();
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

