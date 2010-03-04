/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicSeamless class declaration
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

#ifndef __UIMachineLogicSeamless_h__
#define __UIMachineLogicSeamless_h__

/* Local includes */
#include "UIMachineLogic.h"

/* Local forwards */
class UIActionsPool;

class UIMachineLogicSeamless : public UIMachineLogic
{
    Q_OBJECT;

protected:

    /* Seamless machine logic constructor/destructor: */
    UIMachineLogicSeamless(QObject *pParent,
                           UISession *pSession,
                           UIActionsPool *pActionsPool);
    virtual ~UIMachineLogicSeamless();

    void initialize();

private:

    /* Prepare helpers: */
    void prepareMachineWindows();

    /* Cleanup helpers: */
    void cleanupMachineWindows();

    /* Friend classes: */
    friend class UIMachineLogic;
};

#endif // __UIMachineLogicSeamless_h__

