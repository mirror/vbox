/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGraphicsSelectorMouseHandler class declaration
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

#ifndef __UIGraphicsSelectorMouseHandler_h__
#define __UIGraphicsSelectorMouseHandler_h__

/* Qt includes: */
#include <QObject>

/* Forward declarations: */
class UIGraphicsSelectorModel;
class QGraphicsSceneMouseEvent;
class UIGSelectorItem;

/* Mouse event type: */
enum UIMouseEventType
{
    UIMouseEventType_Press,
    UIMouseEventType_Release,
    UIMouseEventType_DoubleClick
};

/* Mouse handler for graphics selector: */
class UIGraphicsSelectorMouseHandler : public QObject
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGraphicsSelectorMouseHandler(UIGraphicsSelectorModel *pParent);

    /* API: Model mouse-event handler delegate: */
    bool handle(QGraphicsSceneMouseEvent *pEvent, UIMouseEventType type) const;

private:

    /* Model wrapper: */
    UIGraphicsSelectorModel* model() const;

    /* Helpers: Model mouse-event handler delegates: */
    bool handleMousePress(QGraphicsSceneMouseEvent *pEvent) const;
    bool handleMouseRelease(QGraphicsSceneMouseEvent *pEvent) const;
    bool handleMouseDoubleClick(QGraphicsSceneMouseEvent *pEvent) const;

    /* Helpers: Others: */
    bool contains(QList<UIGSelectorItem*> list, UIGSelectorItem *pRequiredItem, bool fRecursively = false) const;

    /* Variables: */
    UIGraphicsSelectorModel *m_pModel;
};

#endif /* __UIGraphicsSelectorMouseHandler_h__ */

