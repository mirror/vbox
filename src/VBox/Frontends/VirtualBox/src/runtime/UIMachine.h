/** @file
 * VBox Qt GUI - UIMachine class declaration.
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

#ifndef ___UIMachine_h___
#define ___UIMachine_h___

/* Qt includes: */
#include <QObject>

/* GUI includes: */
#include "UIExtraDataDefs.h"
#include "UIMachineDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CSession.h"

/* Forward declarations: */
class QWidget;
class UISession;
class UIVisualState;
class UIMachineLogic;

/** Singleton QObject extension
  * used as virtual machine (VM) singleton instance. */
class UIMachine : public QObject
{
    Q_OBJECT;

signals:

    /** Requests async visual-state change. */
    void sigRequestAsyncVisualStateChange(UIVisualStateType visualStateType);

public:

    /** Static factory to start machine with passed @a strID.
      * @return true if machine was started, false otherwise. */
    static bool startMachine(const QString &strID);

    /** Constructor. */
    UIMachine();
    /** Destructor. */
    ~UIMachine();

    /** Returns active machine-window reference (if possible). */
    QWidget* activeWindow() const;
    /** Returns UI session instance. */
    UISession *uisession() const { return m_pSession; }

    /** Returns whether requested visual @a state allowed. */
    bool isVisualStateAllowed(UIVisualStateType state) const { return m_allowedVisualStates & state; }

    /** Requests async visual-state change. */
    void asyncChangeVisualState(UIVisualStateType visualStateType);

private slots:

    /** Visual state-change handler. */
    void sltChangeVisualState(UIVisualStateType visualStateType);

private:

    /** Prepare routine. */
    bool prepare();

    /** Moves VM to default (normal) state. */
    void enterInitialVisualState();

    /** Returns machine-logic reference (if possible). */
    UIMachineLogic* machineLogic() const;

    /** Prepare routine: Loading stuff. */
    void loadMachineSettings();

    /** Prepare routine: Saving stuff. */
    void saveMachineSettings();

    /* Private variables: */
    UIVisualStateType initialStateType;
    CSession m_session;
    UISession *m_pSession;
    UIVisualState *m_pVisualState;
    UIVisualStateType m_allowedVisualStates;

    /* Friend classes: */
    friend class UISession;
};

#endif /* !___UIMachine_h___ */
