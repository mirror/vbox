/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachine class implementation.
 */

/*
 * Copyright (C) 2010-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Local includes: */
# include "VBoxGlobal.h"
# include "UIExtraDataManager.h"
# include "UIMachine.h"
# include "UISession.h"
# include "UIActionPoolRuntime.h"
# include "UIMachineLogic.h"
# include "UIMachineWindow.h"
# include "UIMessageCenter.h"

/* COM includes: */
# include "CMachine.h"
# include "CConsole.h"
# include "CSnapshot.h"
# include "CProgress.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/* Visual state interface: */
class UIVisualState : public QObject
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIVisualState(QObject *pParent, UISession *pSession, UIVisualStateType type)
        : QObject(pParent)
        , m_type(type)
        , m_pSession(pSession)
        , m_pMachineLogic(0)
    {
    }

    /* Destructor: */
    ~UIVisualState()
    {
        /* Cleanup/delete machine logic if exists: */
        if (m_pMachineLogic)
        {
            /* Cleanup the logic object: */
            m_pMachineLogic->cleanup();
            /* Destroy the logic object: */
            UIMachineLogic::destroy(m_pMachineLogic);
        }
    }

    /* Visual state type getter: */
    UIVisualStateType visualStateType() const { return m_type; }

    /* Machine logic getter: */
    UIMachineLogic* machineLogic() const { return m_pMachineLogic; }

    /* Method to prepare change one visual state to another: */
    bool prepareChange()
    {
        m_pMachineLogic = UIMachineLogic::create(this, m_pSession, visualStateType());
        return m_pMachineLogic->checkAvailability();
    }

    /* Method to change one visual state to another: */
    void change()
    {
        /* Prepare the logic object: */
        m_pMachineLogic->prepare();
    }

protected:

    /* Variables: */
    UIVisualStateType m_type;
    UISession *m_pSession;
    UIMachineLogic *m_pMachineLogic;
};

/* static */
bool UIMachine::startMachine(const QString &strID)
{
    /* Some restrictions: */
    AssertMsgReturn(vboxGlobal().isValid(), ("VBoxGlobal is invalid.."), false);
    AssertMsgReturn(!vboxGlobal().virtualMachine(), ("Machine already started.."), false);

    /* Restore current snapshot if requested: */
    if (vboxGlobal().shouldRestoreCurrentSnapshot())
    {
        /* Create temporary session: */
        CSession session = vboxGlobal().openSession(strID, KLockType_VM);
        if (session.isNull())
            return false;

        /* Which VM we operate on? */
        CMachine machine  = session.GetMachine();
        /* Which snapshot we are restoring? */
        CSnapshot snapshot = machine.GetCurrentSnapshot();

        /* Open corresponding console: */
        CConsole console  = session.GetConsole();
        /* Prepare restore-snapshot progress: */
        CProgress progress = console.RestoreSnapshot(snapshot);
        if (!console.isOk())
            return msgCenter().cannotRestoreSnapshot(console, snapshot.GetName(), machine.GetName());

        /* Show the snapshot-discarding progress: */
        msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_snapshot_discard_90px.png");
        if (progress.GetResultCode() != 0)
            return msgCenter().cannotRestoreSnapshot(progress, snapshot.GetName(), machine.GetName());

        /* Unlock session finally: */
        session.UnlockMachine();

        /* Clear snapshot-restoring request: */
        vboxGlobal().setShouldRestoreCurrentSnapshot(false);
    }

    /* Create machine UI: */
    UIMachine *pMachine = new UIMachine;

    /* Prepare machine UI: */
    return pMachine->prepare();
}

UIMachine::UIMachine()
    : QObject(0)
    , initialStateType(UIVisualStateType_Normal)
    , m_pSession(0)
    , m_pVisualState(0)
    , m_allowedVisualStates(UIVisualStateType_Invalid)
{
    /* Make sure VBoxGlobal is aware of VM creation: */
    vboxGlobal().setVirtualMachine(this);
}

bool UIMachine::prepare()
{
    KLockType lockType;
    if (!vboxGlobal().isSeparateProcess())
    {
        lockType = KLockType_VM;
    }
    else
    {
        /* Try to find corresponding machine: */
        CVirtualBox virtualBox = vboxGlobal().virtualBox();
        CMachine machine = virtualBox.FindMachine(vboxGlobal().managedVMUuid());
        if (machine.isNull())
        {
            msgCenter().cannotFindMachineById(virtualBox, vboxGlobal().managedVMUuid());
            return false;
        }

        /* Try to launch corresponding machine: */
        if (!vboxGlobal().launchMachine(machine, VBoxGlobal::LaunchMode_Separate))
            return false;

        lockType = KLockType_Shared;
    }

    /* Create VM session: */
    m_session = vboxGlobal().openSession(vboxGlobal().managedVMUuid(), lockType);
    if (m_session.isNull())
        return false;

    /* Create UI session: */
    m_pSession = new UISession(this, m_session);

    /* Preventing application from closing in case of window(s) closed: */
    qApp->setQuitOnLastWindowClosed(false);

    /* Cache medium data only if really necessary: */
    vboxGlobal().startMediumEnumeration(false /* force start */);

    /* Load machine settings: */
    loadMachineSettings();

    /* Prepare async visual-state change handler: */
    qRegisterMetaType<UIVisualStateType>();
    connect(this, SIGNAL(sigRequestAsyncVisualStateChange(UIVisualStateType)),
            this, SLOT(sltChangeVisualState(UIVisualStateType)),
            Qt::QueuedConnection);

    /* Enter default (normal) state */
    enterInitialVisualState();

    /* Now power up the machine.
     * Actually powerUp does more that just a power up,
     * so call it regardless of isSeparateProcess setting. */
    uisession()->powerUp();

    /* Initialization of MachineLogic internals after the powerUp.
     * This is a hack, maybe more generic approach can be used. */
    machineLogic()->initializePostPowerUp();

    return true;
}

UIMachine::~UIMachine()
{
    /* Save machine settings: */
    saveMachineSettings();

    /* Delete visual state: */
    delete m_pVisualState;
    m_pVisualState = 0;

    /* Delete UI session: */
    delete m_pSession;
    m_pSession = 0;

    /* Free session finally: */
    m_session.UnlockMachine();
    m_session.detach();

    /* Make sure VBoxGlobal is aware of VM destruction: */
    vboxGlobal().setVirtualMachine(0);

    /* Quit application: */
    QApplication::quit();
}

QWidget* UIMachine::activeWindow() const
{
    /* Null if machine-logic not yet created: */
    if (!machineLogic())
        return 0;
    /* Active machine-window otherwise: */
    return machineLogic()->activeMachineWindow();
}

void UIMachine::asyncChangeVisualState(UIVisualStateType visualStateType)
{
    emit sigRequestAsyncVisualStateChange(visualStateType);
}

void UIMachine::sltChangeVisualState(UIVisualStateType newVisualStateType)
{
    /* Create new state: */
    UIVisualState *pNewVisualState = new UIVisualState(this, m_pSession, newVisualStateType);

    /* First we have to check if the selected mode is available at all.
     * Only then we delete the old mode and switch to the new mode. */
    if (pNewVisualState->prepareChange())
    {
        /* Delete previous state: */
        delete m_pVisualState;

        /* Set the new mode as current mode: */
        m_pVisualState = pNewVisualState;
        m_pVisualState->change();
    }
    else
    {
        /* Discard the temporary created new state: */
        delete pNewVisualState;

        /* If there is no state currently created => we have to exit: */
        if (!m_pVisualState)
            deleteLater();
    }
}

void UIMachine::enterInitialVisualState()
{
    sltChangeVisualState(initialStateType);
}

UIMachineLogic* UIMachine::machineLogic() const
{
    if (m_pVisualState && m_pVisualState->machineLogic())
        return m_pVisualState->machineLogic();
    return 0;
}

void UIMachine::loadMachineSettings()
{
    /* Load 'visual state' option: */
    {
        /* Load restricted visual states: */
        UIVisualStateType restrictedVisualStates = gEDataManager->restrictedVisualStates(vboxGlobal().managedVMUuid());
        /* Acquire allowed visual states: */
        m_allowedVisualStates = static_cast<UIVisualStateType>(UIVisualStateType_All ^ restrictedVisualStates);

        /* Load requested visual state: */
        UIVisualStateType requestedVisualState = gEDataManager->requestedVisualState(vboxGlobal().managedVMUuid());
        /* Check if requested visual state type allowed: */
        if (isVisualStateAllowed(requestedVisualState))
        {
            switch (requestedVisualState)
            {
                /* Direct transition to scale/fullscreen mode allowed: */
                case UIVisualStateType_Scale: initialStateType = UIVisualStateType_Scale; break;
                case UIVisualStateType_Fullscreen: initialStateType = UIVisualStateType_Fullscreen; break;
                /* While to seamless is not, so we have to request transition on GA capability-change event: */
                case UIVisualStateType_Seamless: uisession()->setRequestedVisualState(UIVisualStateType_Seamless); break;
                default: break;
            }
        }
    }
}

void UIMachine::saveMachineSettings()
{
    /* Save 'visual state' option: */
    {
        /* Get requested visual state: */
        UIVisualStateType requestedVisualState = uisession()->requestedVisualState();

        /* If requested state is invalid: */
        if (requestedVisualState == UIVisualStateType_Invalid)
        {
            /* Get current if still exists or normal otherwise: */
            requestedVisualState = m_pVisualState ? m_pVisualState->visualStateType() : UIVisualStateType_Normal;
        }

        /* Save requested visual state: */
        gEDataManager->setRequestedVisualState(requestedVisualState, vboxGlobal().managedVMUuid());
    }
}

#include "UIMachine.moc"

