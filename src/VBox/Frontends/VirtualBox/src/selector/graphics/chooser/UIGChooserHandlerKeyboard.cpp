/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooserHandlerKeyboard class implementation
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
#include "UIGChooserHandlerKeyboard.h"
#include "UIGChooserModel.h"
#include "UIGChooserItemGroup.h"

UIGChooserHandlerKeyboard::UIGChooserHandlerKeyboard(UIGChooserModel *pParent)
    : QObject(pParent)
    , m_pModel(pParent)
{
}

bool UIGChooserHandlerKeyboard::handle(QKeyEvent *pEvent, UIKeyboardEventType type) const
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

UIGChooserModel* UIGChooserHandlerKeyboard::model() const
{
    return m_pModel;
}

bool UIGChooserHandlerKeyboard::handleKeyPress(QKeyEvent *pEvent) const
{
    /* Which key it was? */
    switch (pEvent->key())
    {
        /* Key UP? */
        case Qt::Key_Up:
        {
            /* Not during sliding: */
            if (model()->isSlidingInProgress())
                return false;

            /* Determine focus item position: */
            int iPosition = model()->navigationList().indexOf(model()->focusItem());
            /* Determine 'previous' item: */
            UIGChooserItem *pPreviousItem = iPosition > 0 ?
                                             model()->navigationList().at(iPosition - 1) : 0;
            if (pPreviousItem)
            {
                /* Make sure 'previous' item is visible: */
                pPreviousItem->makeSureItsVisible();
                /* Move focus to 'previous' item: */
                model()->setFocusItem(pPreviousItem);
                /* Was 'shift' modifier pressed? */
#ifdef Q_WS_MAC
                if (pEvent->modifiers() & Qt::ShiftModifier &&
                    pEvent->modifiers() & Qt::KeypadModifier)
#else /* Q_WS_MAC */
                if (pEvent->modifiers() == Qt::ShiftModifier)
#endif /* !Q_WS_MAC */
                {
                    /* Calculate positions: */
                    UIGChooserItem *pFirstItem = model()->selectionList().first();
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
#ifdef Q_WS_MAC
                else if (pEvent->modifiers() == Qt::KeypadModifier)
#else /* Q_WS_MAC */
                else if (pEvent->modifiers() == Qt::NoModifier)
#endif /* !Q_WS_MAC */
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
            /* Not during sliding: */
            if (model()->isSlidingInProgress())
                return false;

            /* Determine focus item position: */
            int iPosition = model()->navigationList().indexOf(model()->focusItem());
            /* Determine 'next' item: */
            UIGChooserItem *pNextItem = iPosition < model()->navigationList().size() - 1 ?
                                          model()->navigationList().at(iPosition + 1) : 0;
            if (pNextItem)
            {
                /* Make sure 'next' item is visible: */
                pNextItem->makeSureItsVisible();
                /* Move focus to 'next' item: */
                model()->setFocusItem(pNextItem);
                /* Was shift modifier pressed? */
#ifdef Q_WS_MAC
                if (pEvent->modifiers() & Qt::ShiftModifier &&
                    pEvent->modifiers() & Qt::KeypadModifier)
#else /* Q_WS_MAC */
                if (pEvent->modifiers() == Qt::ShiftModifier)
#endif /* !Q_WS_MAC */
                {
                    /* Calculate positions: */
                    UIGChooserItem *pFirstItem = model()->selectionList().first();
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
#ifdef Q_WS_MAC
                else if (pEvent->modifiers() == Qt::KeypadModifier)
#else /* Q_WS_MAC */
                else if (pEvent->modifiers() == Qt::NoModifier)
#endif /* !Q_WS_MAC */
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
            /* If there is a focus item: */
            if (UIGChooserItem *pFocusItem = model()->focusItem())
            {
                /* Of the known type: */
                switch (pFocusItem->type())
                {
                    case UIGChooserItemType_Group:
                    case UIGChooserItemType_Machine:
                    {
                        /* Unindent root if its NOT main: */
                        if (model()->root() != model()->mainRoot())
                            model()->unindentRoot();
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
            if (UIGChooserItem *pFocusItem = model()->focusItem())
            {
                /* Of the group type: */
                if (pFocusItem->type() == UIGChooserItemType_Group)
                {
                    /* Indent root with this item: */
                    model()->indentRoot(pFocusItem);
                }
            }
            /* Pass that event: */
            return false;
        }
        /* Key F2? */
        case Qt::Key_F2:
        {
            /* If this item is of group type: */
            if (model()->focusItem()->type() == UIGChooserItemType_Group)
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
        case Qt::Key_Space:
        {
            /* If there is a focus item: */
            if (UIGChooserItem *pFocusItem = model()->focusItem())
            {
                /* Of the group type: */
                if (pFocusItem->type() == UIGChooserItemType_Group)
                {
                    /* Toggle that group: */
                    UIGChooserItemGroup *pGroupItem = pFocusItem->toGroupItem();
                    if (pGroupItem->closed())
                        pGroupItem->open();
                    else
                        pGroupItem->close();
                    /* Filter that event out: */
                    return true;
                }
            }
            /* Pass event to other items: */
            return false;
        }
        default:
            break;
    }
    /* Pass all other events: */
    return false;
}

bool UIGChooserHandlerKeyboard::handleKeyRelease(QKeyEvent*) const
{
    /* Pass all events: */
    return false;
}

