/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserHandlerKeyboard class implementation.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
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
                /* Determine current-item position: */
                int iPosition = model()->navigationItems().indexOf(model()->currentItem());
                /* Determine 'previous' item: */
                UIChooserItem *pPreviousItem = 0;
                if (iPosition > 0)
                {
                    if (pEvent->key() == Qt::Key_Up)
                        pPreviousItem = model()->navigationItems().at(iPosition - 1);
                    else if (pEvent->key() == Qt::Key_Home)
                        pPreviousItem = model()->navigationItems().first();
                }
                if (pPreviousItem)
                {
                    /* Make sure 'previous' item is visible: */
                    pPreviousItem->makeSureItsVisible();
                    /* Calculate positions: */
                    UIChooserItem *pFirstItem = model()->firstSelectedItem();
                    int iFirstPosition = model()->navigationItems().indexOf(pFirstItem);
                    int iPreviousPosition = model()->navigationItems().indexOf(pPreviousItem);
                    /* Populate list of items from 'first' to 'previous': */
                    QList<UIChooserItem*> items;
                    if (iFirstPosition <= iPreviousPosition)
                        for (int i = iFirstPosition; i <= iPreviousPosition; ++i)
                            items << model()->navigationItems().at(i);
                    else
                        for (int i = iFirstPosition; i >= iPreviousPosition; --i)
                            items << model()->navigationItems().at(i);
                    /* Set that list as selected: */
                    model()->setSelectedItems(items);
                    /* Make 'previous' item current one: */
                    model()->setCurrentItem(pPreviousItem);
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
                /* Determine current-item position: */
                int iPosition = model()->navigationItems().indexOf(model()->currentItem());
                /* Determine 'previous' item: */
                UIChooserItem *pPreviousItem = 0;
                if (iPosition > 0)
                {
                    if (pEvent->key() == Qt::Key_Up)
                        pPreviousItem = model()->navigationItems().at(iPosition - 1);
                    else if (pEvent->key() == Qt::Key_Home)
                        pPreviousItem = model()->navigationItems().first();
                }
                if (pPreviousItem)
                {
                    /* Make sure 'previous' item is visible: */
                    pPreviousItem->makeSureItsVisible();
                    /* Make 'previous' item the only selected: */
                    model()->setSelectedItem(pPreviousItem);
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
                /* Determine current-item position: */
                int iPosition = model()->navigationItems().indexOf(model()->currentItem());
                /* Determine 'next' item: */
                UIChooserItem *pNextItem = 0;
                if (iPosition < model()->navigationItems().size() - 1)
                {
                    if (pEvent->key() == Qt::Key_Down)
                        pNextItem = model()->navigationItems().at(iPosition + 1);
                    else if (pEvent->key() == Qt::Key_End)
                        pNextItem = model()->navigationItems().last();
                }
                if (pNextItem)
                {
                    /* Make sure 'next' item is visible: */
                    pNextItem->makeSureItsVisible();
                    /* Calculate positions: */
                    UIChooserItem *pFirstItem = model()->firstSelectedItem();
                    int iFirstPosition = model()->navigationItems().indexOf(pFirstItem);
                    int iNextPosition = model()->navigationItems().indexOf(pNextItem);
                    /* Populate list of items from 'first' to 'next': */
                    QList<UIChooserItem*> items;
                    if (iFirstPosition <= iNextPosition)
                        for (int i = iFirstPosition; i <= iNextPosition; ++i)
                            items << model()->navigationItems().at(i);
                    else
                        for (int i = iFirstPosition; i >= iNextPosition; --i)
                            items << model()->navigationItems().at(i);
                    /* Set that list as selected: */
                    model()->setSelectedItems(items);
                    /* Make 'next' item current one: */
                    model()->setCurrentItem(pNextItem);
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
                /* Determine current-item position: */
                int iPosition = model()->navigationItems().indexOf(model()->currentItem());
                /* Determine 'next' item: */
                UIChooserItem *pNextItem = 0;
                if (iPosition < model()->navigationItems().size() - 1)
                {
                    if (pEvent->key() == Qt::Key_Down)
                        pNextItem = model()->navigationItems().at(iPosition + 1);
                    else if (pEvent->key() == Qt::Key_End)
                        pNextItem = model()->navigationItems().last();
                }
                if (pNextItem)
                {
                    /* Make sure 'next' item is visible: */
                    pNextItem->makeSureItsVisible();
                    /* Make 'next' item the only selected one: */
                    model()->setSelectedItem(pNextItem);
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
            if (model()->currentItem()->type() == UIChooserNodeType_Group)
            {
                /* Start editing selected group item name: */
                model()->startEditingSelectedGroupItemName();
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
            if (   model()->currentItem()->type() == UIChooserNodeType_Group
                || model()->currentItem()->type() == UIChooserNodeType_Machine)
            {
                /* Start or show selected items: */
                model()->startOrShowSelectedItems();
                /* And filter out that event: */
                return true;
            }
            /* Pass event to other items: */
            return false;
        }
        case Qt::Key_Space:
        {
            /* If there is a current-item: */
            if (UIChooserItem *pCurrentItem = model()->currentItem())
            {
                /* Of the group type: */
                if (pCurrentItem->type() == UIChooserNodeType_Group)
                {
                    /* Toggle that group: */
                    UIChooserItemGroup *pGroupItem = pCurrentItem->toGroupItem();
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
            /* Start lookup only for non-empty and alphanumerical strings: */
            const QString strText = pEvent->text();
            if (!checkKey(pEvent->key()) && !strText.isEmpty())
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
    /* Get current-node and its parent: */
    UIChooserNode *pCurrentNode = model()->currentItem()->node();
    UIChooserNode *pParentNode = pCurrentNode->parentNode();
    /* Get current-node position: */
    const int iCurrentNodePosition = pCurrentNode->position();

    /* Calculate new position: */
    int iNewCurrentNodePosition = -1;
    switch (enmDirection)
    {
        case UIItemShiftDirection_Up:
        {
            if (iCurrentNodePosition > 0)
                switch (enmShiftType)
                {
                    case UIItemShiftSize_Item: iNewCurrentNodePosition = iCurrentNodePosition - 1; break;
                    case UIItemShiftSize_Full: iNewCurrentNodePosition = 0; break;
                    default: break;
                }
            break;
        }
        case UIItemShiftDirection_Down:
        {
            if (iCurrentNodePosition < pParentNode->nodes(pCurrentNode->type()).size() - 1)
                switch (enmShiftType)
                {
                    case UIItemShiftSize_Item: iNewCurrentNodePosition = iCurrentNodePosition + 2; break;
                    case UIItemShiftSize_Full: iNewCurrentNodePosition = pParentNode->nodes(pCurrentNode->type()).size();  break;
                    default: break;
                }
            break;
        }
        default:
            break;
    }
    /* Filter out invalid requests: */
    if (iNewCurrentNodePosition == -1)
        return;

    /* Create shifted node/item: */
    UIChooserItem *pShiftedItem = 0;
    switch (pCurrentNode->type())
    {
        case UIChooserNodeType_Group:
        {
            UIChooserNodeGroup *pNewNode = new UIChooserNodeGroup(pParentNode, iNewCurrentNodePosition, pCurrentNode->toGroupNode());
            pShiftedItem = new UIChooserItemGroup(pParentNode->item(), pNewNode);
            break;
        }
        case UIChooserNodeType_Machine:
        {
            UIChooserNodeMachine *pNewNode = new UIChooserNodeMachine(pParentNode, iNewCurrentNodePosition, pCurrentNode->toMachineNode());
            pShiftedItem = new UIChooserItemMachine(pParentNode->item(), pNewNode);
            break;
        }
        default:
            break;
    }

    /* Delete old node/item: */
    delete pCurrentNode;

    /* Update model: */
    model()->wipeOutEmptyGroups();
    model()->updateNavigationItemList();
    model()->updateLayout();
    model()->setSelectedItem(pShiftedItem);
    model()->saveGroups();
}

bool UIChooserHandlerKeyboard::checkKey(int iKey) const
{
    if (iKey == Qt::Key_Tab ||
        iKey == Qt::Key_Backtab ||
        iKey == Qt::Key_Backspace ||
        iKey == Qt::Key_Pause ||
        iKey == Qt::Key_Print ||
        iKey == Qt::Key_SysReq ||
        iKey == Qt::Key_Home ||
        iKey == Qt::Key_End ||
        iKey == Qt::Key_Left ||
        iKey == Qt::Key_Up ||
        iKey == Qt::Key_Right ||
        iKey == Qt::Key_Down ||
        iKey == Qt::Key_PageUp ||
        iKey == Qt::Key_PageDown ||
        iKey == Qt::Key_Shift ||
        iKey == Qt::Key_Alt ||
        iKey == Qt::Key_AltGr ||
        iKey == Qt::Key_CapsLock ||
        iKey == Qt::Key_NumLock ||
        iKey == Qt::Key_ScrollLock ||
        iKey == Qt::Key_Control)
        return true;

    if ((Qt::Key)iKey >= Qt::Key_F1 &&
        (Qt::Key)iKey <= Qt::Key_F35)
        return true;

    return false;
}
