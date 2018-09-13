/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsHandlerKeyboard class declaration.
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

#ifndef ___UIToolsHandlerKeyboard_h___
#define ___UIToolsHandlerKeyboard_h___

/* Qt includes: */
#include <QMap>
#include <QObject>

/* Forward declarations: */
class QKeyEvent;
class UIToolsModel;


/** Keyboard event types. */
enum UIKeyboardEventType
{
    UIKeyboardEventType_Press,
    UIKeyboardEventType_Release
};


/** QObject extension used as keyboard handler for graphics tools selector. */
class UIToolsHandlerKeyboard : public QObject
{
    Q_OBJECT;

public:

    /** Constructs keyboard handler passing @a pParent to the base-class. */
    UIToolsHandlerKeyboard(UIToolsModel *pParent);

    /** Handles keyboard @a pEvent of certain @a enmType. */
    bool handle(QKeyEvent *pEvent, UIKeyboardEventType enmType) const;

private:

    /** Returns the parent model reference. */
    UIToolsModel *model() const;

    /** Handles keyboard press @a pEvent. */
    bool handleKeyPress(QKeyEvent *pEvent) const;
    /** Handles keyboard release @a pEvent. */
    bool handleKeyRelease(QKeyEvent *pEvent) const;

    /** Holds the parent model reference. */
    UIToolsModel *m_pModel;
};


#endif /* !___UIToolsHandlerKeyboard_h___ */
