/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsHandlerKeyboard class implementation.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QKeyEvent>

/* GUI incluedes: */
# include "UIToolsHandlerKeyboard.h"
# include "UIToolsModel.h"
# include "UIToolsItem.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIToolsHandlerKeyboard::UIToolsHandlerKeyboard(UIToolsModel *pParent)
    : QObject(pParent)
    , m_pModel(pParent)
{
}

bool UIToolsHandlerKeyboard::handle(QKeyEvent *pEvent, UIKeyboardEventType enmType) const
{
    /* Process passed event: */
    switch (enmType)
    {
        case UIKeyboardEventType_Press:   return handleKeyPress(pEvent);
        case UIKeyboardEventType_Release: return handleKeyRelease(pEvent);
    }
    /* Pass event if unknown: */
    return false;
}

UIToolsModel *UIToolsHandlerKeyboard::model() const
{
    return m_pModel;
}

bool UIToolsHandlerKeyboard::handleKeyPress(QKeyEvent *pEvent) const
{
    /* Which key it was? */
    switch (pEvent->key())
    {
        /* Key UP? */
        case Qt::Key_Up:
        /* Key HOME? */
        case Qt::Key_Home:
        {
            /* Determine focus item position: */
            const int iPosition = model()->navigationList().indexOf(model()->focusItem());
            /* Determine 'previous' item: */
            UIToolsItem *pPreviousItem = 0;
            if (iPosition > 0)
            {
                if (pEvent->key() == Qt::Key_Up)
                    pPreviousItem = model()->navigationList().at(iPosition - 1);
                else if (pEvent->key() == Qt::Key_Home)
                    pPreviousItem = model()->navigationList().first();
            }
            if (pPreviousItem)
            {
                /* Make 'previous' item the current one: */
                model()->setCurrentItem(pPreviousItem);
                /* Filter-out this event: */
                return true;
            }
            /* Pass this event: */
            return false;
        }
        /* Key DOWN? */
        case Qt::Key_Down:
        /* Key END? */
        case Qt::Key_End:
        {
            /* Determine focus item position: */
            int iPosition = model()->navigationList().indexOf(model()->focusItem());
            /* Determine 'next' item: */
            UIToolsItem *pNextItem = 0;
            if (iPosition < model()->navigationList().size() - 1)
            {
                if (pEvent->key() == Qt::Key_Down)
                    pNextItem = model()->navigationList().at(iPosition + 1);
                else if (pEvent->key() == Qt::Key_End)
                    pNextItem = model()->navigationList().last();
            }
            if (pNextItem)
            {
                /* Make 'next' item the current one: */
                model()->setCurrentItem(pNextItem);
                /* Filter-out this event: */
                return true;
            }
            /* Pass this event: */
            return false;
        }
        default:
            break;
    }
    /* Pass all other events: */
    return false;
}

bool UIToolsHandlerKeyboard::handleKeyRelease(QKeyEvent *) const
{
    /* Pass all events: */
    return false;
}
