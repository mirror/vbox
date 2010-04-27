/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineMenuBar class declaration
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

#ifndef __UIMachineMenuBar_h__
#define __UIMachineMenuBar_h__

/* Local includes */
#include "UIMachineDefs.h"

/* Global includes */
#include <QList>

/* Global forwards */
class QMenu;
class QMenuBar;

/* Local forwards */
class UIActionsPool;

class UIMachineMenuBar
{
public:
    UIMachineMenuBar();

    QMenu* createMenu(UIActionsPool *pActionsPool, UIMainMenuType fOptions = UIMainMenuType_All);
    QMenuBar* createMenuBar(UIActionsPool *pActionsPool, UIMainMenuType fOptions = UIMainMenuType_All);

protected:

    QList<QMenu*> prepareSubMenus(UIActionsPool *pActionsPool, UIMainMenuType fOptions = UIMainMenuType_All);
    void prepareMenuMachine(QMenu *pMenu, UIActionsPool *pActionsPool);
    void prepareMenuDevices(QMenu *pMenu, UIActionsPool *pActionsPool);
#ifdef VBOX_WITH_DEBUGGER_GUI
    void prepareMenuDebug(QMenu *pMenu, UIActionsPool *pActionsPool);
#endif
    void prepareMenuHelp(QMenu *pMenu, UIActionsPool *pActionsPool);

    bool m_fIsFirstTime;
};

#endif /* __UIMachineMenuBar_h__ */

