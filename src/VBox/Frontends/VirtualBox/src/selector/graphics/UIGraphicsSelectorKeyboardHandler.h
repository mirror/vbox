/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGraphicsSelectorKeyboardHandler class declaration
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

#ifndef __UIGraphicsSelectorKeyboardHandler_h__
#define __UIGraphicsSelectorKeyboardHandler_h__

/* Qt includes: */
#include <QObject>

/* Forward declarations: */
class UIGraphicsSelectorModel;
class QKeyEvent;

/* Keyboard event type: */
enum UIKeyboardEventType
{
    UIKeyboardEventType_Press,
    UIKeyboardEventType_Release
};

/* Keyboard handler for graphics selector: */
class UIGraphicsSelectorKeyboardHandler : public QObject
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGraphicsSelectorKeyboardHandler(UIGraphicsSelectorModel *pParent);

    /* API: Model keyboard-event handler delegate: */
    bool handle(QKeyEvent *pEvent, UIKeyboardEventType type) const;

private:

    /* Model wrapper: */
    UIGraphicsSelectorModel* model() const;

    /* Helpers: Model keyboard-event handler delegates: */
    bool handleKeyPress(QKeyEvent *pEvent) const;
    bool handleKeyRelease(QKeyEvent *pEvent) const;

    /* Variables: */
    UIGraphicsSelectorModel *m_pModel;
};

#endif /* __UIGraphicsSelectorKeyboardHandler_h__ */

