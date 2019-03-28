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
                        && (   model()->firstSelectedItem() == pGlobalItem
                            || pGlobalItem->isHovered()))
                    {
                        model()->handleToolButtonClick(pGlobalItem);
                        if (model()->firstSelectedItem() != pGlobalItem)
                            pClickedItem = pGlobalItem;
                    }
                    else
                    if (   pGlobalItem->isPinButtonArea(itemCursorPos)
                        && (   model()->firstSelectedItem() == pGlobalItem
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
                        && (   model()->firstSelectedItem() == pMachineItem
                            || pMachineItem->isHovered()))
                    {
                        model()->handleToolButtonClick(pMachineItem);
                        if (model()->firstSelectedItem() != pMachineItem)
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
                        UIChooserItem *pFirstItem = model()->firstSelectedItem();
                        int iFirstPosition = model()->navigationItems().indexOf(pFirstItem);
                        int iClickedPosition = model()->navigationItems().indexOf(pClickedItem);
                        /* Populate list of items from 'first' to 'clicked': */
                        QList<UIChooserItem*> items;
                        if (iFirstPosition <= iClickedPosition)
                            for (int i = iFirstPosition; i <= iClickedPosition; ++i)
                                items << model()->navigationItems().at(i);
                        else
                            for (int i = iFirstPosition; i >= iClickedPosition; --i)
                                items << model()->navigationItems().at(i);
                        /* Make that list selected: */
                        model()->setSelectedItems(items);
                        /* Make clicked item current one: */
                        model()->setCurrentItem(pClickedItem);
                    }
                    /* Was 'control' modifier pressed? */
                    else if (pEvent->modifiers() == Qt::ControlModifier)
                    {
                        /* Invert selection state for clicked item: */
                        if (model()->selectedItems().contains(pClickedItem))
                            model()->removeFromSelectedItems(pClickedItem);
                        else
                            model()->addToSelectedItems(pClickedItem);
                        /* Make clicked item current one: */
                        model()->setCurrentItem(pClickedItem);
                        model()->makeSureSomeItemIsSelected();
                    }
                    /* Was no modifiers pressed? */
                    else if (pEvent->modifiers() == Qt::NoModifier)
                    {
                        /* Make clicked item the only selected one: */
                        model()->setSelectedItem(pClickedItem);
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
                    if (!model()->selectedItems().contains(pClickedItem))
                        model()->setSelectedItem(pClickedItem);
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
                    /* If it was not root: */
                    if (!pGroupItem->isRoot())
                    {
                        /* Toggle it: */
                        if (pGroupItem->isClosed())
                            pGroupItem->open();
                        else if (pGroupItem->isOpened())
                            pGroupItem->close();
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

