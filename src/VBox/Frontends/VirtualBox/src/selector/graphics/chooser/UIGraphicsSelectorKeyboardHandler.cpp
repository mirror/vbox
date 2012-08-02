/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGraphicsSelectorKeyboardHandler class implementation
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
#include <QKeyEvent>

/* GUI incluedes: */
#include "UIGraphicsSelectorKeyboardHandler.h"
#include "UIGraphicsSelectorModel.h"
#include "UIGSelectorItemGroup.h"

UIGraphicsSelectorKeyboardHandler::UIGraphicsSelectorKeyboardHandler(UIGraphicsSelectorModel *pParent)
    : QObject(pParent)
    , m_pModel(pParent)
{
}

bool UIGraphicsSelectorKeyboardHandler::handle(QKeyEvent *pEvent, UIKeyboardEventType type) const
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

UIGraphicsSelectorModel* UIGraphicsSelectorKeyboardHandler::model() const
{
    return m_pModel;
}

bool UIGraphicsSelectorKeyboardHandler::handleKeyPress(QKeyEvent *pEvent) const
{
    /* Which key it was? */
    switch (pEvent->key())
    {
        /* Key UP? */
        case Qt::Key_Up:
        {
            /* Determine focus item position: */
            int iPosition = model()->navigationList().indexOf(model()->focusItem());
            /* Determine 'previous' item: */
            UIGSelectorItem *pPreviousItem = iPosition > 0 ?
                                             model()->navigationList().at(iPosition - 1) : 0;
            if (pPreviousItem)
            {
                /* Make sure 'previous' item is visible: */
                pPreviousItem->makeSureItsVisible();
                /* Move focus to 'previous' item: */
                model()->setFocusItem(pPreviousItem);
                /* Was 'shift' modifier pressed? */
                if (pEvent->modifiers() == Qt::ShiftModifier)
                {
                    /* Calculate positions: */
                    UIGSelectorItem *pFirstItem = model()->selectionList().first();
                    int iFirstPosition = model()->navigationList().indexOf(pFirstItem);
                    int iPreviousPosition = model()->navigationList().indexOf(pPreviousItem);
                    /* Clear selection: */
                    model()->clearSelectionList();
                    /* Select all the items from 'first' to 'previous': */
                    if (iFirstPosition <= iPreviousPosition)
                        for (int i = iFirstPosition; i <= iPreviousPosition; ++i)
                            model()->addToSelectionList(model()->navigationList().at(i));
                    else
                        for (int i = iFirstPosition; i >= iPreviousPosition; --i)
                            model()->addToSelectionList(model()->navigationList().at(i));
                }
                /* There is no modifiers pressed? */
                else if (pEvent->modifiers() == Qt::NoModifier)
                {
                    /* Move selection to 'previous' item: */
                    model()->clearSelectionList();
                    model()->addToSelectionList(pPreviousItem);
                }
                /* Notify selection changed: */
                model()->notifySelectionChanged();
                /* Filter-out this event: */
                return true;
            }
            /* Pass this event: */
            return false;
        }
        /* Key DOWN? */
        case Qt::Key_Down:
        {
            /* Determine focus item position: */
            int iPosition = model()->navigationList().indexOf(model()->focusItem());
            /* Determine 'next' item: */
            UIGSelectorItem *pNextItem = iPosition < model()->navigationList().size() - 1 ?
                                          model()->navigationList().at(iPosition + 1) : 0;
            if (pNextItem)
            {
                /* Make sure 'next' item is visible: */
                pNextItem->makeSureItsVisible();
                /* Move focus to 'next' item: */
                model()->setFocusItem(pNextItem);
                /* Was shift modifier pressed? */
                if (pEvent->modifiers() == Qt::ShiftModifier)
                {
                    /* Calculate positions: */
                    UIGSelectorItem *pFirstItem = model()->selectionList().first();
                    int iFirstPosition = model()->navigationList().indexOf(pFirstItem);
                    int iNextPosition = model()->navigationList().indexOf(pNextItem);
                    /* Clear selection: */
                    model()->clearSelectionList();
                    /* Select all the items from 'first' to 'next': */
                    if (iFirstPosition <= iNextPosition)
                        for (int i = iFirstPosition; i <= iNextPosition; ++i)
                            model()->addToSelectionList(model()->navigationList().at(i));
                    else
                        for (int i = iFirstPosition; i >= iNextPosition; --i)
                            model()->addToSelectionList(model()->navigationList().at(i));
                }
                /* There is no modifiers pressed? */
                else if (pEvent->modifiers() == Qt::NoModifier)
                {
                    /* Move selection to 'next' item: */
                    model()->clearSelectionList();
                    model()->addToSelectionList(pNextItem);
                }
                /* Notify selection changed: */
                model()->notifySelectionChanged();
                /* Filter-out this event: */
                return true;
            }
            /* Pass this event: */
            return false;
        }
        /* Key LEFT? */
        case Qt::Key_Left:
        {
            /* If there is focus item: */
            if (UIGSelectorItem *pFocusItem = model()->focusItem())
            {
                /* Known item type? */
                switch (pFocusItem->type())
                {
                    /* Group one? */
                    case UIGSelectorItemType_Group:
                    {
                        /* Get focus group: */
                        UIGSelectorItemGroup *pFocusGroup = pFocusItem->toGroupItem();
                        /* If focus group is opened: */
                        if (pFocusGroup->opened())
                        {
                            /* Close it: */
                            pFocusGroup->close();
                            /* Filter that event out: */
                            return true;
                        }
                        /* If focus group is closed and not from root-group: */
                        else if (pFocusItem->parentItem() && pFocusItem->parentItem()->parentItem())
                        {
                            /* Move focus to parent item: */
                            model()->setFocusItem(pFocusItem->parentItem(), true);
                            /* Filter that event out: */
                            return true;
                        }
                        break;
                    }
                    /* Machine one? */
                    case UIGSelectorItemType_Machine:
                    {
                        /* If focus machine is not from root-group: */
                        if (pFocusItem->parentItem() && pFocusItem->parentItem()->parentItem())
                        {
                            /* Move focus to parent item: */
                            model()->setFocusItem(pFocusItem->parentItem(), true);
                            /* Filter that event out: */
                            return true;
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
            /* Pass that event: */
            return false;
        }
        /* Key RIGHT? */
        case Qt::Key_Right:
        {
            /* If there is focus item: */
            if (UIGSelectorItem *pFocusItem = model()->focusItem())
            {
                /* And focus item is of 'group' type and opened: */
                if (pFocusItem->type() == UIGSelectorItemType_Group &&
                    pFocusItem->toGroupItem()->closed())
                {
                    /* Open it: */
                    pFocusItem->toGroupItem()->open();
                    /* And filter out that event: */
                    return true;
                }
            }
            /* Pass that event: */
            return false;
        }
        /* Key F2? */
        case Qt::Key_F2:
        {
            /* If this item is of group type: */
            if (model()->focusItem()->type() == UIGSelectorItemType_Group)
            {
                /* Start embedded editing focus item: */
                model()->focusItem()->startEditing();
                /* Filter that event out: */
                return true;
            }
            /* Pass event to other items: */
            return false;
        }
        case Qt::Key_Return:
        case Qt::Key_Enter:
        {
            /* Activate item: */
            model()->activate();
            /* And filter out that event: */
            return true;
        }
        default:
            break;
    }
    /* Pass all other events: */
    return false;
}

bool UIGraphicsSelectorKeyboardHandler::handleKeyRelease(QKeyEvent*) const
{
    /* Pass all events: */
    return false;
}

