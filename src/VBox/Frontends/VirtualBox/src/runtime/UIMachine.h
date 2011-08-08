/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachine class declaration
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

#ifndef __UIMachine_h__
#define __UIMachine_h__

/* Global includes */
#include <QObject>

/* Local includes */
#include "COMDefs.h"
#include "UIMachineDefs.h"

/* Global forwards: */
class QWidget;

/* Local forwards */
class UISession;
class UIVisualState;
class UIMachineLogic;

class UIMachine : public QObject
{
    Q_OBJECT;

public:

    /* Virtual Machine constructor/destructor: */
    UIMachine(UIMachine **ppSelf, const CSession &session);
    virtual ~UIMachine();

    /* Public getters: */
    QWidget* mainWindow() const;
    UISession *uisession() const { return m_pSession; }

private slots:

    /* Visual state-change handler: */
    void sltChangeVisualState(UIVisualStateType visualStateType);

    /* Close VM slot: */
    void sltCloseVirtualMachine();

private:

    /* Move VM to default (normal) state: */
    void enterInitialVisualState();

    /* Private getters: */
    UIMachineLogic* machineLogic() const;

    /* Prepare helpers: */
    void loadMachineSettings();

    /* Cleanup helpers: */
    void saveMachineSettings();

    /* Private variables: */
    UIMachine **m_ppThis;
    UIVisualStateType initialStateType;
    CSession m_session;
    UISession *m_pSession;
    UIVisualState *m_pVisualState;

    /* Friend classes: */
    friend class UISession;
};

#endif // __UIMachine_h__

