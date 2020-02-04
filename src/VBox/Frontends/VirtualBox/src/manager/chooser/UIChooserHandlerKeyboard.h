/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserHandlerKeyboard class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserHandlerKeyboard_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserHandlerKeyboard_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>
#include <QMap>

/* Forward declarations: */
class UIChooserModel;
class QKeyEvent;

/* Keyboard event type: */
enum UIKeyboardEventType
{
    UIKeyboardEventType_Press,
    UIKeyboardEventType_Release
};

/* Item shift direction: */
enum UIItemShiftDirection
{
    UIItemShiftDirection_Up,
    UIItemShiftDirection_Down
};

/** Item shift types. */
enum UIItemShiftType
{
    UIItemShiftSize_Item,
    UIItemShiftSize_Full
};

/* Keyboard handler for graphics selector: */
class UIChooserHandlerKeyboard : public QObject
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIChooserHandlerKeyboard(UIChooserModel *pParent);

    /* API: Model keyboard-event handler delegate: */
    bool handle(QKeyEvent *pEvent, UIKeyboardEventType type) const;

private:

    /* API: Model wrapper: */
    UIChooserModel* model() const;

    /* Helpers: Model keyboard-event handler delegates: */
    bool handleKeyPress(QKeyEvent *pEvent) const;
    bool handleKeyRelease(QKeyEvent *pEvent) const;

    /* Helper: Item shift delegate: */
    void shift(UIItemShiftDirection enmDirection, UIItemShiftType enmShiftType) const;

    /* Variables: */
    UIChooserModel *m_pModel;
    QMap<int, UIItemShiftType> m_shiftMap;
};

#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserHandlerKeyboard_h */

