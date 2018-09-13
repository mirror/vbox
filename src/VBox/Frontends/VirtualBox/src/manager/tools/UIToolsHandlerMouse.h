/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsHandlerMouse class declaration.
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

#ifndef ___UIToolsHandlerMouse_h___
#define ___UIToolsHandlerMouse_h___

/* Qt includes: */
#include <QObject>

/* Forward declarations: */
class QGraphicsSceneMouseEvent;
class UIToolsModel;
class UIToolsItem;


/** Mouse event type. */
enum UIMouseEventType
{
    UIMouseEventType_Press,
    UIMouseEventType_Release
};


/** QObject extension used as mouse handler for graphics tools selector. */
class UIToolsHandlerMouse : public QObject
{
    Q_OBJECT;

public:

    /** Constructs mouse handler passing @a pParent to the base-class. */
    UIToolsHandlerMouse(UIToolsModel *pParent);

    /** Handles mouse @a pEvent of certain @a enmType. */
    bool handle(QGraphicsSceneMouseEvent *pEvent, UIMouseEventType enmType) const;

private:

    /** Returns the parent model reference. */
    UIToolsModel *model() const;

    /** Handles mouse press @a pEvent. */
    bool handleMousePress(QGraphicsSceneMouseEvent *pEvent) const;
    /** Handles mouse release @a pEvent. */
    bool handleMouseRelease(QGraphicsSceneMouseEvent *pEvent) const;

    /** Holds the parent model reference. */
    UIToolsModel *m_pModel;
};


#endif /* !___UIToolsHandlerMouse_h___ */
