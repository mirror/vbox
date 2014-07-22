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

private slots:

    /** Checks if some visual-state type was requested. */
    void sltCheckForRequestedVisualStateType();

    /* View menu functionality: */
    void sltOpenStatusBarSettings();
    void sltStatusBarSettingsClosed();
    void sltToggleStatusBar();
    void sltPrepareMenuViewPopup();
    void sltPrepareMenuViewVirtualScreen();
    void sltHandleActionTriggerViewVirtualScreen(QAction *pAction);

    /* Devices menu functionality: */
    void sltPrepareHardDisksMenu();
    void sltPrepareSharedFoldersMenu();
    void sltPrepareVideoCaptureMenu();
    void sltPrepareKeyboardMenu();
    void sltPrepareMouseIntegrationMenu();

private:

    /* Prepare helpers: */
    void prepareActionConnections();
    void prepareMachineWindows();

    /* Cleanup helpers: */
    void cleanupMachineWindows();
    void cleanupActionConnections();

    /** Updates the 'View' menu. */
    virtual void updateMenuView();
    /** Updates the 'View' : 'Status Bar' menu. */
    void updateMenuViewStatusBar();

    /* Friend classes: */
    friend class UIMachineLogic;
};

#endif /* !___UIMachineLogicNormal_h___ */

