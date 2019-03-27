/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserHandlerKeyboard class implementation.
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
#include <QKeyEvent>

/* GUI incluedes: */
#include "UIChooserHandlerKeyboard.h"
#include "UIChooserModel.h"
#include "UIChooserItemGroup.h"
#include "UIChooserItemMachine.h"
#include "UIChooserNodeGroup.h"
#include "UIChooserNodeMachine.h"


UIChooserHandlerKeyboard::UIChooserHandlerKeyboard(UIChooserModel *pParent)
    : QObject(pParent)
    , m_pModel(pParent)
{
    /* Setup shift map: */
    m_shiftMap[Qt::Key_Up] = UIItemShiftSize_Item;
    m_shiftMap[Qt::Key_Down] = UIItemShiftSize_Item;
    m_shiftMap[Qt::Key_Home] = UIItemShiftSize_Full;
    m_shiftMap[Qt::Key_End] = UIItemShiftSize_Full;
}

bool UIChooserHandlerKeyboard::handle(QKeyEvent *pEvent, UIKeyboardEventType type) const
{
    /* Process passed event: */
    switch (type)
    {
        case UIKeyboardEventType_Press: return handleKeyPress(pEvent);
        case UIKeyboardEventType_Release: return handleKeyRelease(pEvent);
    }
    /* Pass event if unknown: */
    return false;
}

UIChooserModel* UIChooserHandlerKeyboard::model() const
{
    return m_pModel;
}

bool UIChooserHandlerKeyboard::handleKeyPress(QKeyEvent *pEvent) const
{
    /* Which key it was? */
    switch (pEvent->key())
    {
        /* Key UP? */
        case Qt::Key_Up:
        /* Key HOME? */
        case Qt::Key_Home:
        {
            /* Was control modifier pressed? */
#ifdef VBOX_WS_MAC
            if (pEvent->modifiers() & Qt::ControlModifier &&
                pEvent->modifiers() & Qt::KeypadModifier)
#else /* VBOX_WS_MAC */
            if (pEvent->modifiers() == Qt::ControlModifier)
#endif /* !VBOX_WS_MAC */
            {
                /* Shift item up: */
                shift(UIItemShiftDirection_Up, m_shiftMap[pEvent->key()]);
                return true;
            }

            /* Was shift modifier pressed? */
#ifdef VBOX_WS_MAC
            else if (pEvent->modifiers() & Qt::ShiftModifier &&
                     pEvent->modifiers() & Qt::KeypadModifier)
#else /* VBOX_WS_MAC */
            else if (pEvent->modifiers() == Qt::ShiftModifier)
#endif /* !VBOX_WS_MAC */
            {
                /* Determine focus item position: */
                int iPosition = model()->navigationList().indexOf(model()->focusItem());
                /* Determine 'previous' item: */
                UIChooserItem *pPreviousItem = 0;
                if (iPosition > 0)
                {
                    if (pEvent->key() == Qt::Key_Up)
                        pPreviousItem = model()->navigationList().at(iPosition - 1);
                    else if (pEvent->key() == Qt::Key_Home)
                        pPreviousItem = model()->navigationList().first();
                }
                if (pPreviousItem)
                {
                    /* Make sure 'previous' item is visible: */
                    pPreviousItem->makeSureItsVisible();
                    /* Calculate positions: */
                    UIChooserItem *pFirstItem = model()->currentItem();
                    int iFirstPosition = model()->navigationList().indexOf(pFirstItem);
                    int iPreviousPosition = model()->navigationList().indexOf(pPreviousItem);
                    /* Populate list of items from 'first' to 'previous': */
                    QList<UIChooserItem*> items;
                    if (iFirstPosition <= iPreviousPosition)
                        for (int i = iFirstPosition; i <= iPreviousPosition; ++i)
                            items << model()->navigationList().at(i);
                    else
                        for (int i = iFirstPosition; i >= iPreviousPosition; --i)
                            items << model()->navigationList().at(i);
                    /* Set that list as current: */
                    model()->setCurrentItems(items);
                    /* Move focus to 'previous' item: */
                    model()->setFocusItem(pPreviousItem);
                    /* Filter-out this event: */
                    return true;
                }
            }

            /* There is no modifiers pressed? */
#ifdef VBOX_WS_MAC
            else if (pEvent->modifiers() == Qt::KeypadModifier)
#else /* VBOX_WS_MAC */
            else if (pEvent->modifiers() == Qt::NoModifier)
#endif /* !VBOX_WS_MAC */
            {
                /* Determine focus item position: */
                int iPosition = model()->navigationList().indexOf(model()->focusItem());
                /* Determine 'previous' item: */
                UIChooserItem *pPreviousItem = 0;
                if (iPosition > 0)
                {
                    if (pEvent->key() == Qt::Key_Up)
                        pPreviousItem = model()->navigationList().at(iPosition - 1);
                    else if (pEvent->key() == Qt::Key_Home)
                        pPreviousItem = model()->navigationList().first();
                }
                if (pPreviousItem)
                {
                    /* Make sure 'previous' item is visible: */
                    pPreviousItem->makeSureItsVisible();
                    /* Make 'previous' item the current one: */
                    model()->setCurrentItem(pPreviousItem);
                    /* Filter-out this event: */
                    return true;
                }
            }
            /* Pass this event: */
            return false;
        }
        /* Key DOWN? */
        case Qt::Key_Down:
        /* Key END? */
        case Qt::Key_End:
        {
            /* Was control modifier pressed? */
#ifdef VBOX_WS_MAC
            if (pEvent->modifiers() & Qt::ControlModifier &&
                pEvent->modifiers() & Qt::KeypadModifier)
#else /* VBOX_WS_MAC */
            if (pEvent->modifiers() == Qt::ControlModifier)
#endif /* !VBOX_WS_MAC */
            {
                /* Shift item down: */
                shift(UIItemShiftDirection_Down, m_shiftMap[pEvent->key()]);
                return true;
            }

            /* Was shift modifier pressed? */
#ifdef VBOX_WS_MAC
            else if (pEvent->modifiers() & Qt::ShiftModifier &&
                     pEvent->modifiers() & Qt::KeypadModifier)
#else /* VBOX_WS_MAC */
            else if (pEvent->modifiers() == Qt::ShiftModifier)
#endif /* !VBOX_WS_MAC */
            {
                /* Determine focus item position: */
                int iPosition = model()->navigationList().indexOf(model()->focusItem());
                /* Determine 'next' item: */
                UIChooserItem *pNextItem = 0;
                if (iPosition < model()->navigationList().size() - 1)
                {
                    if (pEvent->key() == Qt::Key_Down)
                        pNextItem = model()->navigationList().at(iPosition + 1);
                    else if (pEvent->key() == Qt::Key_End)
                        pNextItem = model()->navigationList().last();
                }
                if (pNextItem)
                {
                    /* Make sure 'next' item is visible: */
                    pNextItem->makeSureItsVisible();
                    /* Calculate positions: */
                    UIChooserItem *pFirstItem = model()->currentItem();
                    int iFirstPosition = model()->navigationList().indexOf(pFirstItem);
                    int iNextPosition = model()->navigationList().indexOf(pNextItem);
                    /* Populate list of items from 'first' to 'next': */
                    QList<UIChooserItem*> items;
                    if (iFirstPosition <= iNextPosition)
                        for (int i = iFirstPosition; i <= iNextPosition; ++i)
                            items << model()->navigationList().at(i);
                    else
                        for (int i = iFirstPosition; i >= iNextPosition; --i)
                            items << model()->navigationList().at(i);
                    /* Set that list as current: */
                    model()->setCurrentItems(items);
                    /* Move focus to 'next' item: */
                    model()->setFocusItem(pNextItem);
                    /* Filter-out this event: */
                    return true;
                }
            }

            /* There is no modifiers pressed? */
#ifdef VBOX_WS_MAC
            else if (pEvent->modifiers() == Qt::KeypadModifier)
#else /* VBOX_WS_MAC */
            else if (pEvent->modifiers() == Qt::NoModifier)
#endif /* !VBOX_WS_MAC */
            {
                /* Determine focus item position: */
                int iPosition = model()->navigationList().indexOf(model()->focusItem());
                /* Determine 'next' item: */
                UIChooserItem *pNextItem = 0;
                if (iPosition < model()->navigationList().size() - 1)
                {
                    if (pEvent->key() == Qt::Key_Down)
                        pNextItem = model()->navigationList().at(iPosition + 1);
                    else if (pEvent->key() == Qt::Key_End)
                        pNextItem = model()->navigationList().last();
                }
                if (pNextItem)
                {
                    /* Make sure 'next' item is visible: */
                    pNextItem->makeSureItsVisible();
                    /* Make 'next' item the current one: */
                    model()->setCurrentItem(pNextItem);
                    /* Filter-out this event: */
                    return true;
                }
            }
            /* Pass this event: */
            return false;
        }
        /* Key F2? */
        case Qt::Key_F2:
        {
            /* If this item is of group type: */
            if (model()->focusItem()->type() == UIChooserItemType_Group)
            {
                /* Start embedded editing focus item: */
                model()->startEditingGroupItemName();
                /* Filter that event out: */
                return true;
            }
            /* Pass event to other items: */
            return false;
        }
        case Qt::Key_Return:
        case Qt::Key_Enter:
        {
            /* If this item is of group or machine type: */
            if (   model()->focusItem()->type() == UIChooserItemType_Group
                || model()->focusItem()->type() == UIChooserItemType_Machine)
            {
                /* Activate item: */
                model()->activateMachineItem();
                /* And filter out that event: */
                return true;
            }
            /* Pass event to other items: */
            return false;
        }
        case Qt::Key_Space:
        {
            /* If there is a focus item: */
            if (UIChooserItem *pFocusItem = model()->focusItem())
            {
                /* Of the group type: */
                if (pFocusItem->type() == UIChooserItemType_Group)
                {
                    /* Toggle that group: */
                    UIChooserItemGroup *pGroupItem = pFocusItem->toGroupItem();
                    if (pGroupItem->isClosed())
                        pGroupItem->open();
                    else if (pGroupItem->isOpened())
                        pGroupItem->close();
                    /* Filter that event out: */
                    return true;
                }
            }
            /* Pass event to other items: */
            return false;
        }
        case Qt::Key_Escape:
        {
            /* Make sure that vm search widget is hidden: */
            model()->setSearchWidgetVisible(false);
            break;
        }
        default:
        {
            /* Start lookup: */
            const QString strText = pEvent->text();
            if (!strText.isEmpty())
                model()->lookFor(strText);
            break;
        }
    }
    /* Pass all other events: */
    return false;
}

bool UIChooserHandlerKeyboard::handleKeyRelease(QKeyEvent*) const
{
    /* Pass all events: */
    return false;
}

void UIChooserHandlerKeyboard::shift(UIItemShiftDirection enmDirection, UIItemShiftType enmShiftType) const
{
    /* Get focus-node and its parent: */
    UIChooserNode *pFocusNode = model()->focusItem()->node();
    UIChooserNode *pParentNode = pFocusNode->parentNode();
    /* Get focus-node position: */
    const int iFocusNodePosition = pFocusNode->position();

    /* Calculate new position: */
    int iNewFocusNodePosition = -1;
    switch (enmDirection)
    {
        case UIItemShiftDirection_Up:
        {
            if (iFocusNodePosition > 0)
                switch (enmShiftType)
                {
                    case UIItemShiftSize_Item: iNewFocusNodePosition = iFocusNodePosition - 1; break;
                    case UIItemShiftSize_Full: iNewFocusNodePosition = 0; break;
                    default: break;
                }
            break;
        }
        case UIItemShiftDirection_Down:
        {
            if (iFocusNodePosition < pParentNode->nodes(pFocusNode->type()).size() - 1)
                switch (enmShiftType)
                {
                    case UIItemShiftSize_Item: iNewFocusNodePosition = iFocusNodePosition + 2; break;
                    case UIItemShiftSize_Full: iNewFocusNodePosition = pParentNode->nodes(pFocusNode->type()).size();  break;
                    default: break;
                }
            break;
        }
        default:
            break;
    }
    /* Filter out invalid requests: */
    if (iNewFocusNodePosition == -1)
        return;

    /* Create shifted node/item: */
    UIChooserItem *pShiftedItem = 0;
    switch (pFocusNode->type())
    {
        case UIChooserItemType_Group:
        {
            UIChooserNodeGroup *pNewNode = new UIChooserNodeGroup(pParentNode, pFocusNode->toGroupNode(), iNewFocusNodePosition);
            pShiftedItem = new UIChooserItemGroup(pParentNode->item(), pNewNode);
            break;
        }
        case UIChooserItemType_Machine:
        {
            UIChooserNodeMachine *pNewNode = new UIChooserNodeMachine(pParentNode, pFocusNode->toMachineNode(), iNewFocusNodePosition);
            pShiftedItem = new UIChooserItemMachine(pParentNode->item(), pNewNode);
            break;
        }
        default:
            break;
    }

    /* Delete old node/item: */
    delete pFocusNode;

    /* Update model: */
    model()->wipeOutEmptyGroups();
    model()->updateNavigation();
    model()->updateLayout();
    model()->setCurrentItem(pShiftedItem);
    model()->saveGroupSettings();
}
