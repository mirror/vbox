/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicFullscreen class declaration
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __UIMachineLogicFullscreen_h__
#define __UIMachineLogicFullscreen_h__

/* Local includes */
#include "UIMachineLogic.h"

/* Local forwards */
class UIActionsPool;
class VBoxChangePresentationModeEvent;

class UIMachineLogicFullscreen : public UIMachineLogic
{
    Q_OBJECT;

protected:

    /* Fullscreen machine logic constructor/destructor: */
    UIMachineLogicFullscreen(QObject *pParent,
                             UISession *pSession,
                             UIActionsPool *pActionsPool);
    virtual ~UIMachineLogicFullscreen();

    bool checkAvailability();
    void initialize();

private slots:

#ifdef Q_WS_MAC
    void sltChangePresentationMode(const VBoxChangePresentationModeEvent &event);
#endif /* Q_WS_MAC */

private:

    /* Prepare helpers: */
#ifdef Q_WS_MAC
    void prepareCommonConnections();
#endif /* Q_WS_MAC */
    void prepareActionGroups();
    void prepareMachineWindows();

    /* Cleanup helpers: */
    void cleanupMachineWindows();
    void cleanupActionGroups();
#ifdef Q_WS_MAC
    //void cleanupCommonConnections() {}
#endif /* Q_WS_MAC */

#ifdef Q_WS_MAC
    void setPresentationModeEnabled(bool fEnabled);
#endif /* Q_WS_MAC */

    /* Friend classes: */
    friend class UIMachineLogic;
};

#endif // __UIMachineLogicFullscreen_h__

