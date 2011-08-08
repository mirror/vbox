/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicSeamless class declaration
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMachineLogicSeamless_h__
#define __UIMachineLogicSeamless_h__

/* Local includes */
#include "UIMachineLogic.h"

/* Local forwards */
class UIMultiScreenLayout;

class UIMachineLogicSeamless : public UIMachineLogic
{
    Q_OBJECT;

protected:

    /* Seamless machine logic constructor/destructor: */
    UIMachineLogicSeamless(QObject *pParent,
                           UISession *pSession);
    virtual ~UIMachineLogicSeamless();

    bool checkAvailability();
    void initialize();

    int hostScreenForGuestScreen(int screenId) const;

private:

    /* Prepare helpers: */
    void prepareActionGroups();
    void prepareMachineWindows();

    /* Cleanup helpers: */
    void cleanupMachineWindows();
    void cleanupActionGroups();

    UIMultiScreenLayout *m_pScreenLayout;

    /* Friend classes: */
    friend class UIMachineLogic;
    friend class UIMachineWindowSeamless;
    friend class UIMachineViewSeamless;
};

#endif // __UIMachineLogicSeamless_h__

