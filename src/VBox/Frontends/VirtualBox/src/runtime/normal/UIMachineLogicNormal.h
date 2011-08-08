/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicNormal class declaration
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

#ifndef __UIMachineLogicNormal_h__
#define __UIMachineLogicNormal_h__

/* Local includes */
#include "UIMachineLogic.h"

class UIMachineLogicNormal : public UIMachineLogic
{
    Q_OBJECT;

protected:

    /* Normal machine logic constructor/destructor: */
    UIMachineLogicNormal(QObject *pParent,
                         UISession *pSession);
    virtual ~UIMachineLogicNormal();

    void initialize();

private slots:

    /* Windowed mode functionality: */
    void sltPrepareNetworkAdaptersMenu();
    void sltPrepareSharedFoldersMenu();
    void sltPrepareMouseIntegrationMenu();

private:

    /* Prepare helpers: */
    void prepareActionConnections();
    void prepareMachineWindows();

    /* Cleanup helpers: */
    void cleanupMachineWindow();
    //void cleanupActionConnections() {}

    /* Friend classes: */
    friend class UIMachineLogic;
};

#endif // __UIMachineLogicNormal_h__

