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
        /* Key HOME? */
        case Qt::Key_Home:
        {
            /* Not during sliding: */
            if (model()->isSlidingInProgress())
                return false;

            /* Was control modifier pressed? */
#ifdef Q_WS_MAC
            if (pEvent->modifiers() & Qt::ControlModifier &&
                pEvent->modifiers() & Qt::KeypadModifier)
#else /* Q_WS_MAC */
            if (pEvent->modifiers() == Qt::ControlModifier)
#endif /* !Q_WS_MAC */
            {
                /* Get focus and his parent: */
                UIGChooserItem *pFocusItem = model()->focusItem();
                UIGChooserItem *pParentItem = pFocusItem->parentItem();
                UIGChooserItemType type = (UIGChooserItemType)pFocusItem->type();
                QList<UIGChooserItem*> items = pParentItem->items(type);
                int iFocusPosition = items.indexOf(pFocusItem);
                if (iFocusPosition > 0)
                {
                    if (pEvent->key() == Qt::Key_Up)
                        items.move(iFocusPosition, iFocusPosition - 1);
                    else if (pEvent->key() == Qt::Key_Home)
                        items.move(iFocusPosition, 0);
                    pParentItem->setItems(items, type);
                    model()->updateNavigation();
                    model()->updateLayout();
                }
                /* Filter-out this event: */
                return true;
            }
            /* Was shift modifier pressed? */
#ifdef Q_WS_MAC
            else if (pEvent->modifiers() & Qt::ShiftModifier &&
                     pEvent->modifiers() & Qt::KeypadModifier)
#else /* Q_WS_MAC */
            else if (pEvent->modifiers() == Qt::ShiftModifier)
#endif /* !Q_WS_MAC */
            {
                /* Determine focus item position: */
                int iPosition = model()->navigationList().indexOf(model()->focusItem());
                /* Determine 'previous' item: */
                UIGChooserItem *pPreviousItem = 0;
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
                    UIGChooserItem *pFirstItem = model()->currentItem();
                    int iFirstPosition = model()->navigationList().indexOf(pFirstItem);
                    int iPreviousPosition = model()->navigationList().indexOf(pPreviousItem);
                    /* Populate list of items from 'first' to 'previous': */
                    QList<UIGChooserItem*> items;
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
#ifdef Q_WS_MAC
            else if (pEvent->modifiers() == Qt::KeypadModifier)
#else /* Q_WS_MAC */
            else if (pEvent->modifiers() == Qt::NoModifier)
#endif /* !Q_WS_MAC */
            {
                /* Determine focus item position: */
                int iPosition = model()->navigationList().indexOf(model()->focusItem());
                /* Determine 'previous' item: */
                UIGChooserItem *pPreviousItem = 0;
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
            /* Not during sliding: */
            if (model()->isSlidingInProgress())
                return false;

            /* Was control modifier pressed? */
#ifdef Q_WS_MAC
            if (pEvent->modifiers() & Qt::ControlModifier &&
                pEvent->modifiers() & Qt::KeypadModifier)
#else /* Q_WS_MAC */
            if (pEvent->modifiers() == Qt::ControlModifier)
#endif /* !Q_WS_MAC */
            {
                /* Get focus and his parent: */
                UIGChooserItem *pFocusItem = model()->focusItem();
                UIGChooserItem *pParentItem = pFocusItem->parentItem();
                UIGChooserItemType type = (UIGChooserItemType)pFocusItem->type();
                QList<UIGChooserItem*> items = pParentItem->items(type);
                int iFocusPosition = items.indexOf(pFocusItem);
                if (iFocusPosition < items.size() - 1)
                {
                    if (pEvent->key() == Qt::Key_Down)
                        items.move(iFocusPosition, iFocusPosition + 1);
                    else if (pEvent->key() == Qt::Key_End)
                        items.move(iFocusPosition, items.size() - 1);
                    pParentItem->setItems(items, type);
                    model()->updateNavigation();
                    model()->updateLayout();
                }
                /* Filter-out this event: */
                return true;
            }
            /* Was shift modifier pressed? */
#ifdef Q_WS_MAC
            else if (pEvent->modifiers() & Qt::ShiftModifier &&
                     pEvent->modifiers() & Qt::KeypadModifier)
#else /* Q_WS_MAC */
            else if (pEvent->modifiers() == Qt::ShiftModifier)
#endif /* !Q_WS_MAC */
            {
                /* Determine focus item position: */
                int iPosition = model()->navigationList().indexOf(model()->focusItem());
                /* Determine 'next' item: */
                UIGChooserItem *pNextItem = 0;
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
                    UIGChooserItem *pFirstItem = model()->currentItem();
                    int iFirstPosition = model()->navigationList().indexOf(pFirstItem);
                    int iNextPosition = model()->navigationList().indexOf(pNextItem);
                    /* Populate list of items from 'first' to 'next': */
                    QList<UIGChooserItem*> items;
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
#ifdef Q_WS_MAC
            else if (pEvent->modifiers() == Qt::KeypadModifier)
#else /* Q_WS_MAC */
            else if (pEvent->modifiers() == Qt::NoModifier)
#endif /* !Q_WS_MAC */
            {
                /* Determine focus item position: */
                int iPosition = model()->navigationList().indexOf(model()->focusItem());
                /* Determine 'next' item: */
                UIGChooserItem *pNextItem = 0;
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
            /* Activate item: */
            model()->activateMachineItem();
            /* And filter out that event: */
            return true;
        }
        case Qt::Key_Space:
        {
            /* If model is performing lookup: */
            if (model()->isPerformingLookup())
            {
                /* Continue lookup: */
                QString strText = pEvent->text();
                if (!strText.isEmpty())
                    model()->lookFor(strText);
            }
            /* If there is a focus item: */
            else if (UIGChooserItem *pFocusItem = model()->focusItem())
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
        {
            /* Start lookup: */
            QString strText = pEvent->text();
            if (!strText.isEmpty())
                model()->lookFor(strText);
            break;
        }
    }
    /* Pass all other events: */
    return false;
}

bool UIGChooserHandlerKeyboard::handleKeyRelease(QKeyEvent*) const
{
    /* Pass all events: */
    return false;
}

