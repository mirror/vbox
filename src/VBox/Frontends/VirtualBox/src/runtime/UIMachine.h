/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachine class declaration
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

#ifndef __UIMachine_h__
#define __UIMachine_h__

/* Local includes */
#include "COMDefs.h"
#include "UIMachineDefs.h"

/* Local forwards */
class UIActionsPool;
class UIVisualState;

class UIMachine : public QObject
{
    Q_OBJECT;

public:

    UIMachine(UIMachine **ppSelf, const CSession &session);

private slots:

    void sltChangeVisualState(UIVisualStateType visualStateType);

private:

    void enterBaseVisualState();

    CSession m_session;
    UIActionsPool *m_pActionsPool;
    UIVisualState *m_pVisualState;
};

#endif // __UIMachine_h__
