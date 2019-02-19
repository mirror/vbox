/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserHandlerMouse class implementation.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
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
#include <QGraphicsSceneMouseEvent>

/* GUI incluedes: */
#include "UIChooserHandlerMouse.h"
#include "UIChooserModel.h"
#include "UIChooserItemGroup.h"
#include "UIChooserItemGlobal.h"
#include "UIChooserItemMachine.h"


UIChooserHandlerMouse::UIChooserHandlerMouse(UIChooserModel *pParent)
    : QObject(pParent)
    , m_pModel(pParent)
{
}

bool UIChooserHandlerMouse::handle(QGraphicsSceneMouseEvent *pEvent, UIMouseEventType type) const
{
    /* Process passed event: */
    switch (type)
    {
        case UIMouseEventType_Press: return handleMousePress(pEvent);
        case UIMouseEventType_Release: return handleMouseRelease(pEvent);
        case UIMouseEventType_DoubleClick: return handleMouseDoubleClick(pEvent);
    }
    /* Pass event if unknown: */
    return false;
}

UIChooserModel* UIChooserHandlerMouse::model() const
{
    return m_pModel;
}

bool UIChooserHandlerMouse::handleMousePress(QGraphicsSceneMouseEvent *pEvent) const
{
    /* Get item under mouse cursor: */
    QPointF scenePos = pEvent->scenePos();
    if (QGraphicsItem *pItemUnderMouse = model()->itemAt(scenePos))
    {
        /* Which button it was? */
        switch (pEvent->button())
        {
            /* Left one? */
            case Qt::LeftButton:
            {
                /* Which item we just clicked? */
                UIChooserItem *pClickedItem = 0;
                /* Was that a group item? */
                if (UIChooserItemGroup *pGroupItem = qgraphicsitem_cast<UIChooserItemGroup*>(pItemUnderMouse))
                    pClickedItem = pGroupItem;
                /* Or a global one? */
                else if (UIChooserItemGlobal *pGlobalItem = qgraphicsitem_cast<UIChooserItemGlobal*>(pItemUnderMouse))
                {
                    const QPoint itemCursorPos = pGlobalItem->mapFromScene(scenePos).toPoint();
                    if (   pGlobalItem->isToolButtonArea(itemCursorPos)
                        && (   model()->currentItem() == pGlobalItem
                            || pGlobalItem->isHovered()))
                    {
                        model()->handleToolButtonClick(pGlobalItem);
                        if (model()->currentItem() != pGlobalItem)
                            pClickedItem = pGlobalItem;
                    }
                    else
                    if (   pGlobalItem->isPinButtonArea(itemCursorPos)
                        && (   model()->currentItem() == pGlobalItem
                            || pGlobalItem->isHovered()))
                        model()->handlePinButtonClick(pGlobalItem);
                    else
                        pClickedItem = pGlobalItem;
                }
                /* Or a machine one? */
                else if (UIChooserItemMachine *pMachineItem = qgraphicsitem_cast<UIChooserItemMachine*>(pItemUnderMouse))
                {
                    const QPoint itemCursorPos = pMachineItem->mapFromScene(scenePos).toPoint();
                    if (   pMachineItem->isToolButtonArea(itemCursorPos)
                        && (   model()->currentItem() == pMachineItem
                            || pMachineItem->isHovered()))
                    {
                        model()->handleToolButtonClick(pMachineItem);
                        if (model()->currentItem() != pMachineItem)
                            pClickedItem = pMachineItem;
                    }
                    else
                        pClickedItem = pMachineItem;
                }
                /* If we had clicked one of required item types: */
                if (pClickedItem && !pClickedItem->isRoot())
                {
                    /* Was 'shift' modifier pressed? */
                    if (pEvent->modifiers() == Qt::ShiftModifier)
                    {
                        /* Calculate positions: */
                        UIChooserItem *pFirstItem = model()->currentItem();
                        int iFirstPosition = model()->navigationList().indexOf(pFirstItem);
                        int iClickedPosition = model()->navigationList().indexOf(pClickedItem);
                        /* Populate list of items from 'first' to 'clicked': */
                        QList<UIChooserItem*> items;
                        if (iFirstPosition <= iClickedPosition)
                            for (int i = iFirstPosition; i <= iClickedPosition; ++i)
                                items << model()->navigationList().at(i);
                        else
                            for (int i = iFirstPosition; i >= iClickedPosition; --i)
                                items << model()->navigationList().at(i);
                        /* Set that list as current: */
                        model()->setCurrentItems(items);
                        /* Move focus to clicked item: */
                        model()->setFocusItem(pClickedItem);
                    }
                    /* Was 'control' modifier pressed? */
                    else if (pEvent->modifiers() == Qt::ControlModifier)
                    {
                        /* Invert selection state for clicked item: */
                        if (model()->currentItems().contains(pClickedItem))
                            model()->removeFromCurrentItems(pClickedItem);
                        else
                            model()->addToCurrentItems(pClickedItem);
                        /* Move focus to clicked item: */
                        model()->setFocusItem(pClickedItem);
                        model()->makeSureSomeItemIsSelected();
                    }
                    /* Was no modifiers pressed? */
                    else if (pEvent->modifiers() == Qt::NoModifier)
                    {
                        /* Make clicked item the current one: */
                        model()->setCurrentItem(pClickedItem);
                    }
                }
                break;
            }
            /* Right one? */
            case Qt::RightButton:
            {
                /* Which item we just clicked? */
                UIChooserItem *pClickedItem = 0;
                /* Was that a group item? */
                if (UIChooserItemGroup *pGroupItem = qgraphicsitem_cast<UIChooserItemGroup*>(pItemUnderMouse))
                    pClickedItem = pGroupItem;
                /* Or a global one? */
                else if (UIChooserItemGlobal *pGlobalItem = qgraphicsitem_cast<UIChooserItemGlobal*>(pItemUnderMouse))
                    pClickedItem = pGlobalItem;
                /* Or a machine one? */
                else if (UIChooserItemMachine *pMachineItem = qgraphicsitem_cast<UIChooserItemMachine*>(pItemUnderMouse))
                    pClickedItem = pMachineItem;
                /* If we had clicked one of required item types: */
                if (pClickedItem && !pClickedItem->isRoot())
                {
                    /* Select clicked item if not selected yet: */
                    if (!model()->currentItems().contains(pClickedItem))
                        model()->setCurrentItem(pClickedItem);
                }
                break;
            }
            default:
                break;
        }
    }
    /* Pass all other events: */
    return false;
}

bool UIChooserHandlerMouse::handleMouseRelease(QGraphicsSceneMouseEvent*) const
{
    /* Pass all events: */
    return false;
}

bool UIChooserHandlerMouse::handleMouseDoubleClick(QGraphicsSceneMouseEvent *pEvent) const
{
    /* Get item under mouse cursor: */
    QPointF scenePos = pEvent->scenePos();
    if (QGraphicsItem *pItemUnderMouse = model()->itemAt(scenePos))
    {
        /* Which button it was? */
        switch (pEvent->button())
        {
            /* Left one? */
            case Qt::LeftButton:
            {
                /* Was that a group item? */
                if (UIChooserItemGroup *pGroupItem = qgraphicsitem_cast<UIChooserItemGroup*>(pItemUnderMouse))
                {
                    /* Prepare variables: */
                    int iGroupItemWidth = pGroupItem->geometry().toRect().width();
                    int iMouseDoubleClickX = pEvent->scenePos().toPoint().x();
                    /* If it was a root: */
                    if (pGroupItem->isRoot())
                    {
#if 0
                        /* Do not allow for unhovered root: */
                        if (!pGroupItem->isHovered())
                            return false;
                        /* Unindent root if possible: */
                        if (model()->root() != model()->mainRoot())
                        {
                            pGroupItem->setHovered(false);
                            model()->unindentRoot();
                        }
#endif
                    }
                    /* If it was a simple group item: */
                    else
                    {
                        /* If click was at left part: */
                        if (iMouseDoubleClickX < iGroupItemWidth / 2)
                        {
                            /* Toggle it: */
                            if (pGroupItem->isClosed())
                                pGroupItem->open();
                            else if (pGroupItem->isOpened())
                                pGroupItem->close();
                        }
#if 0
                        /* If click was at right part: */
                        else
                        {
                            /* Indent root with group item: */
                            pGroupItem->setHovered(false);
                            model()->indentRoot(pGroupItem);
                        }
#endif
                    }
                    /* Filter that event out: */
                    return true;
                }
                /* Or a machine one? */
                else if (pItemUnderMouse->type() == UIChooserItemType_Machine)
                {
                    /* Activate machine-item: */
                    model()->activateMachineItem();
                }
                break;
            }
            default:
                break;
        }
    }
    /* Pass all other events: */
    return false;
}

