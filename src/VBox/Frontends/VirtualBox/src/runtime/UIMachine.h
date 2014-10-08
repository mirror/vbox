/** @file
 * VBox Qt GUI - UIMachine class declaration.
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
    /** Static destructor. */
    static void destroy();
    /** Static instance. */
    static UIMachine* instance() { return m_spInstance; }

    /** Returns UI session instance. */
    UISession *uisession() const { return m_pSession; }
    /** Returns machine-logic reference (if possible). */
    UIMachineLogic* machineLogic() const;
    /** Returns active machine-window reference (if possible). */
    QWidget* activeWindow() const;

    /** Returns whether requested visual @a state allowed. */
    bool isVisualStateAllowed(UIVisualStateType state) const { return m_allowedVisualStates & state; }

    /** Requests async visual-state change. */
    void asyncChangeVisualState(UIVisualStateType visualStateType);

private slots:

    /** Visual state-change handler. */
    void sltChangeVisualState(UIVisualStateType visualStateType);

private:

    /** Constructor. */
    UIMachine();
    /** Destructor. */
    ~UIMachine();

    /** Prepare routine. */
    bool prepare();
    /** Prepare routine: Loading stuff. */
    void loadSettings();
    /** Prepare routine: Saving stuff. */
    void saveSettings();
    /** Cleanup routine. */
    void cleanup();

    /** Moves VM to default (normal) state. */
    void enterInitialVisualState();

    /** Static instance. */
    static UIMachine* m_spInstance;

    /** Holds the session instance. */
    CSession m_session;

    /** Holds the session UI instance. */
    UISession *m_pSession;

    /** Holds allowed visual state types. */
    UIVisualStateType m_allowedVisualStates;
    /** Holds initial visual state type. */
    UIVisualStateType m_initialStateType;
    /** Holds current visual state. */
    UIVisualState *m_pVisualState;
};

#define gpMachine UIMachine::instance()

#endif /* !___UIMachine_h___ */
