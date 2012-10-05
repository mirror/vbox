/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooserHandlerMouse class implementation
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
#include "UIGChooserHandlerMouse.h"
#include "UIGChooserModel.h"
#include "UIGChooserItemGroup.h"
#include "UIGChooserItemMachine.h"

UIGChooserHandlerMouse::UIGChooserHandlerMouse(UIGChooserModel *pParent)
    : QObject(pParent)
    , m_pModel(pParent)
{
}

bool UIGChooserHandlerMouse::handle(QGraphicsSceneMouseEvent *pEvent, UIMouseEventType type) const
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

UIGChooserModel* UIGChooserHandlerMouse::model() const
{
    return m_pModel;
}

bool UIGChooserHandlerMouse::handleMousePress(QGraphicsSceneMouseEvent *pEvent) const
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
                UIGChooserItem *pClickedItem = 0;
                /* Was that a group item? */
                if (UIGChooserItemGroup *pGroupItem = qgraphicsitem_cast<UIGChooserItemGroup*>(pItemUnderMouse))
                    pClickedItem = pGroupItem;
                /* Or a machine one? */
                else if (UIGChooserItemMachine *pMachineItem = qgraphicsitem_cast<UIGChooserItemMachine*>(pItemUnderMouse))
                    pClickedItem = pMachineItem;
                /* If we had clicked one of required item types: */
                if (pClickedItem && !pClickedItem->isRoot())
                {
                    /* Old selection list: */
                    QList<UIGChooserItem*> oldSelectionList = model()->currentItems();
                    /* Move focus to clicked item: */
                    model()->setFocusItem(pClickedItem);
                    /* Was 'shift' modifier pressed? */
                    if (pEvent->modifiers() == Qt::ShiftModifier)
                    {
                        /* Calculate positions: */
                        UIGChooserItem *pFirstItem = model()->currentItem();
                        int iFirstPosition = model()->navigationList().indexOf(pFirstItem);
                        int iClickedPosition = model()->navigationList().indexOf(pClickedItem);
                        /* Clear selection: */
                        model()->clearSelectionList();
                        /* Select all the items from 'first' to 'clicked': */
                        if (iFirstPosition <= iClickedPosition)
                            for (int i = iFirstPosition; i <= iClickedPosition; ++i)
                                model()->addToCurrentItems(model()->navigationList().at(i));
                        else
                            for (int i = iFirstPosition; i >= iClickedPosition; --i)
                                model()->addToCurrentItems(model()->navigationList().at(i));
                    }
                    /* Was 'control' modifier pressed? */
                    else if (pEvent->modifiers() == Qt::ControlModifier)
                    {
                        /* Select clicked item, inverting if necessary: */
                        if (model()->currentItems().contains(pClickedItem))
                            model()->removeFromCurrentItems(pClickedItem);
                        else
                            model()->addToCurrentItems(pClickedItem);
                    }
                    /* Was no modifiers pressed? */
                    else if (pEvent->modifiers() == Qt::NoModifier)
                    {
                        /* Move selection to clicked item: */
                        model()->clearSelectionList();
                        model()->addToCurrentItems(pClickedItem);
                    }
                    /* Selection list changed?: */
                    if (oldSelectionList != model()->currentItems())
                        model()->notifyCurrentItemChanged();
                }
                break;
            }
            /* Right one? */
            case Qt::RightButton:
            {
                /* Which item we just clicked? */
                UIGChooserItem *pClickedItem = 0;
                /* Was that a group item? */
                if (UIGChooserItemGroup *pGroupItem = qgraphicsitem_cast<UIGChooserItemGroup*>(pItemUnderMouse))
                    pClickedItem = pGroupItem;
                /* Or a machine one? */
                else if (UIGChooserItemMachine *pMachineItem = qgraphicsitem_cast<UIGChooserItemMachine*>(pItemUnderMouse))
                    pClickedItem = pMachineItem;
                /* If we had clicked one of required item types: */
                if (pClickedItem)
                {
                    /* For non-root items: */
                    if (!pClickedItem->isRoot())
                    {
                        /* Move focus to clicked item (with selection if not selected yet): */
                        model()->setCurrentItem(pClickedItem);
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

bool UIGChooserHandlerMouse::handleMouseRelease(QGraphicsSceneMouseEvent*) const
{
    /* Pass all events: */
    return false;
}

bool UIGChooserHandlerMouse::handleMouseDoubleClick(QGraphicsSceneMouseEvent *pEvent) const
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
                if (UIGChooserItemGroup *pGroupItem = qgraphicsitem_cast<UIGChooserItemGroup*>(pItemUnderMouse))
                {
                    /* Prepare variables: */
                    int iGroupItemWidth = pGroupItem->geometry().toRect().width();
                    int iMouseDoubleClickX = pEvent->scenePos().toPoint().x();
                    /* If it was a root: */
                    if (pGroupItem->isRoot())
                    {
                        /* Do not allow for unhovered root: */
                        if (!pGroupItem->isHovered())
                            return false;
                        /* Unindent root if possible: */
                        if (model()->root() != model()->mainRoot())
                        {
                            pGroupItem->setHovered(false);
                            model()->unindentRoot();
                        }
                    }
                    /* If it was a simple group item: */
                    else
                    {
                        /* If click was at left part: */
                        if (iMouseDoubleClickX < iGroupItemWidth / 2)
                        {
                            /* Toggle it: */
                            if (pGroupItem->opened())
                                pGroupItem->close();
                            else if (pGroupItem->closed())
                                pGroupItem->open();
                        }
                        /* If click was at right part: */
                        else
                        {
                            /* Indent root with group item: */
                            pGroupItem->setHovered(false);
                            model()->indentRoot(pGroupItem);
                        }
                    }
                    /* Filter that event out: */
                    return true;
                }
                /* Or a machine one? */
                else if (pItemUnderMouse->type() == UIGChooserItemType_Machine)
                {
                    /* Activate machine item: */
                    model()->activate();
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

