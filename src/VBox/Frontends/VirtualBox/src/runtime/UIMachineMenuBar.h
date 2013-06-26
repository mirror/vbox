/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineMenuBar class declaration
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
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
#include "UIDefs.h"

/* Global includes */
#include <QList>

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* Global forwards */
class QMenu;
class QMenuBar;

class UIMachineMenuBar
{
public:

    UIMachineMenuBar(const CMachine &machine);

    QMenu* createMenu(RuntimeMenuType fOptions = RuntimeMenuType_All);
    QMenuBar* createMenuBar(RuntimeMenuType fOptions = RuntimeMenuType_All);

protected:

    QList<QMenu*> prepareSubMenus(RuntimeMenuType fOptions = RuntimeMenuType_All);
    void prepareMenuMachine(QMenu *pMenu);
    void prepareMenuView(QMenu *pMenu);
    void prepareMenuDevices(QMenu *pMenu);
#ifdef VBOX_WITH_DEBUGGER_GUI
    void prepareMenuDebug(QMenu *pMenu);
#endif
    void prepareMenuHelp(QMenu *pMenu);

    bool m_fIsFirstTime;
    CMachine m_machine;
};

#endif /* __UIMachineMenuBar_h__ */

