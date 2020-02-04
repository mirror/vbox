/* $Id$ */
/** @file
 * VBox Qt GUI - UIKeyboardHandlerScale class declaration.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_runtime_scale_UIKeyboardHandlerScale_h
#define FEQT_INCLUDED_SRC_runtime_scale_UIKeyboardHandlerScale_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIKeyboardHandler.h"

/** UIKeyboardHandler reimplementation
  * providing machine-logic with PopupMenu keyboard handler. */
class UIKeyboardHandlerScale : public UIKeyboardHandler
{
    Q_OBJECT;

protected:

    /** Scale keyboard-handler constructor. */
    UIKeyboardHandlerScale(UIMachineLogic *pMachineLogic);
    /** Scale keyboard-handler destructor. */
    virtual ~UIKeyboardHandlerScale();

private:

#ifndef VBOX_WS_MAC
    /** General event-filter. */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);
#endif /* !VBOX_WS_MAC */

    /* Friend class: */
    friend class UIKeyboardHandler;
};

#endif /* !FEQT_INCLUDED_SRC_runtime_scale_UIKeyboardHandlerScale_h */
