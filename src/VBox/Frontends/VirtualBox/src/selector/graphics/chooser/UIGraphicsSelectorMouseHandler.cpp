/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGraphicsSelectorMouseHandler class implementation
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
#include <QGraphicsSceneMouseEvent>

/* GUI incluedes: */
#include "UIGraphicsSelectorMouseHandler.h"
#include "UIGraphicsSelectorModel.h"
#include "UIGSelectorItemGroup.h"
#include "UIGSelectorItemMachine.h"

UIGraphicsSelectorMouseHandler::UIGraphicsSelectorMouseHandler(UIGraphicsSelectorModel *pParent)
    : QObject(pParent)
    , m_pModel(pParent)
{
}

bool UIGraphicsSelectorMouseHandler::handle(QGraphicsSceneMouseEvent *pEvent, UIMouseEventType type) const
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

UIGraphicsSelectorModel* UIGraphicsSelectorMouseHandler::model() const
{
    return m_pModel;
}

bool UIGraphicsSelectorMouseHandler::handleMousePress(QGraphicsSceneMouseEvent *pEvent) const
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
                UIGSelectorItem *pClickedItem = 0;
                /* Was that a group item? */
                if (UIGSelectorItemGroup *pGroupItem = qgraphicsitem_cast<UIGSelectorItemGroup*>(pItemUnderMouse))
                    pClickedItem = pGroupItem;
                /* Or a machine one? */
                else if (UIGSelectorItemMachine *pMachineItem = qgraphicsitem_cast<UIGSelectorItemMachine*>(pItemUnderMouse))
                    pClickedItem = pMachineItem;
                /* If we had clicked one of the required item types: */
                if (pClickedItem)
                {
                    /* For non-root items: */
                    if (pClickedItem->parentItem())
                    {
                        /* Move focus to clicked item: */
                        model()->setFocusItem(pClickedItem);
                        /* Was 'shift' modifier pressed? */
                        if (pEvent->modifiers() == Qt::ShiftModifier)
                        {
                            /* Calculate positions: */
                            UIGSelectorItem *pFirstItem = model()->selectionList().first();
                            int iFirstPosition = model()->navigationList().indexOf(pFirstItem);
                            int iClickedPosition = model()->navigationList().indexOf(pClickedItem);
                            /* Clear selection: */
                            model()->clearSelectionList();
                            /* Select all the items from 'first' to 'clicked': */
                            if (iFirstPosition <= iClickedPosition)
                                for (int i = iFirstPosition; i <= iClickedPosition; ++i)
                                    model()->addToSelectionList(model()->navigationList().at(i));
                            else
                                for (int i = iFirstPosition; i >= iClickedPosition; --i)
                                    model()->addToSelectionList(model()->navigationList().at(i));
                        }
                        /* Was 'control' modifier pressed? */
                        else if (pEvent->modifiers() == Qt::ControlModifier)
                        {
                            /* Select clicked item, inverting if necessary: */
                            if (model()->selectionList().contains(pClickedItem))
                                model()->removeFromSelectionList(pClickedItem);
                            else
                                model()->addToSelectionList(pClickedItem);
                        }
                        /* Was no modifiers pressed? */
                        else if (pEvent->modifiers() == Qt::NoModifier)
                        {
                            /* Move selection to clicked item: */
                            model()->clearSelectionList();
                            model()->addToSelectionList(pClickedItem);
                        }
                        /* Notify selection changed: */
                        model()->notifySelectionChanged();
                    }
                }
                break;
            }
            /* Right one? */
            case Qt::RightButton:
            {
                /* Which item we just clicked? */
                UIGSelectorItem *pClickedItem = 0;
                /* Was that a group item? */
                if (UIGSelectorItemGroup *pGroupItem = qgraphicsitem_cast<UIGSelectorItemGroup*>(pItemUnderMouse))
                    pClickedItem = pGroupItem;
                /* Or a machine one? */
                else if (UIGSelectorItemMachine *pMachineItem = qgraphicsitem_cast<UIGSelectorItemMachine*>(pItemUnderMouse))
                    pClickedItem = pMachineItem;
                /* If we had clicked one of the required item types: */
                if (pClickedItem)
                {
                    /* For non-root items: */
                    if (pClickedItem->parentItem())
                    {
                        /* Is clicked item in selection list: */
                        bool fIsClickedItemInSelectionList = contains(model()->selectionList(), pClickedItem);
                        /* Move focus to clicked item (with selection if not selected yet): */
                        model()->setFocusItem(pClickedItem, !fIsClickedItemInSelectionList);
                    }
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

bool UIGraphicsSelectorMouseHandler::handleMouseRelease(QGraphicsSceneMouseEvent*) const
{
    /* Pass all events: */
    return false;
}

bool UIGraphicsSelectorMouseHandler::handleMouseDoubleClick(QGraphicsSceneMouseEvent *pEvent) const
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
                if (UIGSelectorItemGroup *pGroupItem = qgraphicsitem_cast<UIGSelectorItemGroup*>(pItemUnderMouse))
                {
                    /* Toggle group item: */
                    if (pGroupItem->opened())
                        pGroupItem->close();
                    else if (pGroupItem->closed())
                        pGroupItem->open();
                    /* Filter that event out: */
                    return true;
                }
                /* Or a machine one? */
                else if (qgraphicsitem_cast<UIGSelectorItemMachine*>(pItemUnderMouse))
                {
                    /* Activate machine item: */
                    model()->activate();
                    /* Filter that event out: */
                    return true;
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

bool UIGraphicsSelectorMouseHandler::contains(QList<UIGSelectorItem*> list,
                                              UIGSelectorItem *pRequiredItem,
                                              bool fRecursively /* = false */) const
{
    /* Search throught the all passed list items: */
    foreach (UIGSelectorItem *pItem, list)
    {
        /* Check item first: */
        if (pItem == pRequiredItem)
            return true;
        /* Check if this item supports children: */
        if (fRecursively && pItem->type() == UIGSelectorItemType_Group)
        {
            /* Check machine items recursively: */
            if (contains(pItem->items(UIGSelectorItemType_Machine), pRequiredItem))
                return true;
            /* Check group items recursively: */
            if (contains(pItem->items(UIGSelectorItemType_Group), pRequiredItem))
                return true;
        }
    }
    return false;
}

