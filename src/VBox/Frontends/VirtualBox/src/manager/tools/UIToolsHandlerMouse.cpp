/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsHandlerMouse class implementation.
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
# include <QGraphicsSceneMouseEvent>

/* GUI incluedes: */
# include "UIToolsHandlerMouse.h"
# include "UIToolsModel.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIToolsHandlerMouse::UIToolsHandlerMouse(UIToolsModel *pParent)
    : QObject(pParent)
    , m_pModel(pParent)
{
}

bool UIToolsHandlerMouse::handle(QGraphicsSceneMouseEvent *pEvent, UIMouseEventType enmType) const
{
    /* Process passed event: */
    switch (enmType)
    {
        case UIMouseEventType_Press:   return handleMousePress(pEvent);
        case UIMouseEventType_Release: return handleMouseRelease(pEvent);
    }
    /* Pass event if unknown: */
    return false;
}

UIToolsModel *UIToolsHandlerMouse::model() const
{
    return m_pModel;
}

bool UIToolsHandlerMouse::handleMousePress(QGraphicsSceneMouseEvent *pEvent) const
{
    /* Get item under mouse cursor: */
    QPointF scenePos = pEvent->scenePos();
    if (QGraphicsItem *pItemUnderMouse = model()->itemAt(scenePos))
    {
        /* Which button it was? */
        switch (pEvent->button())
        {
            /* Both buttons: */
            case Qt::LeftButton:
            case Qt::RightButton:
            {
                /* Which item we just clicked? */
                UIToolsItem *pClickedItem = qgraphicsitem_cast<UIToolsItem*>(pItemUnderMouse);
                /* Make clicked item the current one: */
                if (pClickedItem)
                {
                    model()->setCurrentItem(pClickedItem);
                    model()->closeParent();
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

bool UIToolsHandlerMouse::handleMouseRelease(QGraphicsSceneMouseEvent *) const
{
    /* Pass all events: */
    return false;
}
