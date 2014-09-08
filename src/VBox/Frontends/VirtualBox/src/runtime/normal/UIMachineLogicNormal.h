/** @file
 * VBox Qt GUI - UIMachineLogicNormal class declaration.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMachineLogicNormal_h___
#define ___UIMachineLogicNormal_h___

/* Local includes: */
#include "UIMachineLogic.h"

/* Normal machine logic implementation: */
class UIMachineLogicNormal : public UIMachineLogic
{
    Q_OBJECT;

protected:

    /* Constructor: */
    UIMachineLogicNormal(QObject *pParent, UISession *pSession);

    /* Check if this logic is available: */
    bool checkAvailability();

    /** Returns machine-window flags for 'Normal' machine-logic and passed @a uScreenId. */
    virtual Qt::WindowFlags windowFlags(ulong uScreenId) const { Q_UNUSED(uScreenId); return Qt::Window; }

private slots:

    /** Checks if some visual-state type was requested. */
    void sltCheckForRequestedVisualStateType();

    /** Opens menu-bar editor.*/
    void sltOpenMenuBarSettings();
    /** Handles menu-bar editor closing.*/
    void sltMenuBarSettingsClosed();
    /** Toggles menu-bar presence.*/
    void sltToggleMenuBar();

    /** Opens status-bar editor.*/
    void sltOpenStatusBarSettings();
    /** Handles status-bar editor closing.*/
    void sltStatusBarSettingsClosed();
    /** Toggles status-bar presence.*/
    void sltToggleStatusBar();

    /** Handles guest-screen toggle requests. */
    void sltHandleActionTriggerViewScreenToggle(int iIndex, bool fEnabled);
    /** Handles guest-screen resize requests. */
    void sltHandleActionTriggerViewScreenResize(int iIndex, const QSize &size);

private:

    /* Prepare helpers: */
    void prepareActionConnections();
    void prepareMachineWindows();

    /* Cleanup helpers: */
    void cleanupMachineWindows();
    void cleanupActionConnections();

    /* Friend classes: */
    friend class UIMachineLogic;
};

#endif /* !___UIMachineLogicNormal_h___ */

