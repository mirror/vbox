/** @file
 * VBox Qt GUI - UIMachineLogicFullscreen class declaration.
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

#ifndef ___UIMachineLogicFullscreen_h___
#define ___UIMachineLogicFullscreen_h___

/* Local includes: */
#include "UIMachineLogic.h"

/* Forward declarations: */
class UIMultiScreenLayout;

/* Fullscreen machine logic implementation: */
class UIMachineLogicFullscreen : public UIMachineLogic
{
    Q_OBJECT;

protected:

    /* Constructor/destructor: */
    UIMachineLogicFullscreen(QObject *pParent, UISession *pSession);
    ~UIMachineLogicFullscreen();

    /* Check if this logic is available: */
    bool checkAvailability();

    /* Helpers: Multi-screen stuff: */
    void maybeAdjustGuestScreenSize();
    int hostScreenForGuestScreen(int iScreenId) const;
    bool hasHostScreenForGuestScreen(int iScreenId) const;

private slots:

    /* Handler: Console callback stuff: */
    void sltMachineStateChanged();

#ifdef Q_WS_MAC
    void sltChangePresentationMode(bool fEnabled);
    void sltScreenLayoutChanged();
#endif /* Q_WS_MAC */
    void sltGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo);
    void sltHostScreenCountChanged();

private:

    /* Prepare helpers: */
    void prepareActionGroups();
    void prepareActionConnections();
#ifdef Q_WS_MAC
    void prepareOtherConnections();
#endif /* Q_WS_MAC */
    void prepareMachineWindows();
    void prepareMenu();

    /* Cleanup helpers: */
    //void cleanupMenu() {}
    void cleanupMachineWindows();
#ifdef Q_WS_MAC
    //void cleanupOtherConnections() {}
#endif /* Q_WS_MAC */
    void cleanupActionConnections();
    void cleanupActionGroups();

#ifdef Q_WS_MAC
    void setPresentationModeEnabled(bool fEnabled);
#endif /* Q_WS_MAC */

    /* Variables: */
    UIMultiScreenLayout *m_pScreenLayout;

    /* Friend classes: */
    friend class UIMachineLogic;
    friend class UIMachineWindowFullscreen;
    friend class UIMachineViewFullscreen;
};

#endif /* !___UIMachineLogicFullscreen_h___ */

