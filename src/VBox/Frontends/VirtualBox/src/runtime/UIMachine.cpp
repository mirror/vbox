/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachine class implementation
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

/* Local includes */
#include "VBoxGlobal.h"
#include "UIMachine.h"
#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"

class UIVisualState : public QObject
{
    Q_OBJECT;

public:

    /* Visual state holder constructor: */
    UIVisualState(QObject *pParent) : QObject(pParent), m_pMachineLogic(0)
    {
        /* Connect state-change handler: */
        connect(this, SIGNAL(sigChangeVisualState(UIVisualStateType)), parent(), SLOT(sltChangeVisualState(UIVisualStateType)));
    }

    /* Public getters: */
    UIMachineLogic* machineLogic() const  { return m_pMachineLogic; }

signals:

    /* Signal to change-state: */
    void sigChangeVisualState(UIVisualStateType visualStateType);

protected:

    /* Protected members: */
    UIMachineLogic *m_pMachineLogic;
};

class UIVisualStateNormal : public UIVisualState
{
    Q_OBJECT;

public:

    /* Normal visual state holder constructor: */
    UIVisualStateNormal(QObject *pParent, UISession *pSession, UIActionsPool *pActionsPool)
        : UIVisualState(pParent)
    {
        /* Connect action handlers: */
        connect(pActionsPool->action(UIActionIndex_Toggle_Fullscreen), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToFullscreenMode()));
        connect(pActionsPool->action(UIActionIndex_Toggle_Seamless), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToSeamlessMode()));

        /* Create state logic: */
        m_pMachineLogic = UIMachineLogic::create(this, pSession, pActionsPool, UIVisualStateType_Normal);
    }

private slots:

    void sltGoToFullscreenMode()
    {
        /* Change visual state to fullscreen: */
        emit sigChangeVisualState(UIVisualStateType_Fullscreen);
    }

    void sltGoToSeamlessMode()
    {
        /* Change visual state to seamless: */
        emit sigChangeVisualState(UIVisualStateType_Seamless);
    }
};

class UIVisualStateFullscreen : public UIVisualState
{
    Q_OBJECT;

public:

    /* Fullscreen visual state holder constructor: */
    UIVisualStateFullscreen(QObject *pParent, UISession *pSession, UIActionsPool *pActionsPool)
        : UIVisualState(pParent)
    {
        /* Connect action handlers: */
        connect(pActionsPool->action(UIActionIndex_Toggle_Fullscreen), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToNormalMode()));
        connect(pActionsPool->action(UIActionIndex_Toggle_Seamless), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToSeamlessMode()));

        /* Create state logic: */
        m_pMachineLogic = UIMachineLogic::create(this, pSession, pActionsPool, UIVisualStateType_Fullscreen);
    }

private slots:

    void sltGoToNormalMode()
    {
        /* Change visual state to normal: */
        emit sigChangeVisualState(UIVisualStateType_Normal);
    }

    void sltGoToSeamlessMode()
    {
        /* Change visual state to seamless: */
        emit sigChangeVisualState(UIVisualStateType_Seamless);
    }
};

class UIVisualStateSeamless : public UIVisualState
{
    Q_OBJECT;

public:

    /* Seamless visual state holder constructor: */
    UIVisualStateSeamless(QObject *pParent, UISession *pSession, UIActionsPool *pActionsPool)
        : UIVisualState(pParent)
    {
        /* Connect action handlers */
        connect(pActionsPool->action(UIActionIndex_Toggle_Fullscreen), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToFullscreenMode()));
        connect(pActionsPool->action(UIActionIndex_Toggle_Seamless), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToNormalMode()));

        /* Create state logic */
        m_pMachineLogic = UIMachineLogic::create(this, pSession, pActionsPool, UIVisualStateType_Seamless);
    }

private slots:

    void sltGoToFullscreenMode()
    {
        /* Change visual state to fullscreen: */
        emit sigChangeVisualState(UIVisualStateType_Fullscreen);
    }

    void sltGoToNormalMode()
    {
        /* Change visual state to normal: */
        emit sigChangeVisualState(UIVisualStateType_Normal);
    }
};

UIMachine::UIMachine(UIMachine **ppSelf, const CSession &session)
    : QObject(0)
    , m_ppThis(ppSelf)
    , m_session(session)
    , m_pActionsPool(new UIActionsPool(this))
    , m_pSession(new UISession(this, m_session))
    , m_pVisualState(0)
{
    /* Preventing application from closing in case of window(s) closed: */
    qApp->setQuitOnLastWindowClosed(false);

    /* Cache IMedium data: */
    vboxGlobal().startEnumeratingMedia();

    /* Storing self: */
    if (m_ppThis)
        *m_ppThis = this;

    /* Enter default (normal) state */
    enterBaseVisualState();
}

UIMachine::~UIMachine()
{
    /* Erase itself pointer: */
    *m_ppThis = 0;
    /* Delete uisession children in backward direction: */
    delete m_pVisualState;
    m_pVisualState = 0;
    delete m_pSession;
    m_pSession = 0;
    delete m_pActionsPool;
    m_pActionsPool = 0;
    m_session.Close();
    m_session.detach();
    QApplication::quit();
}

QWidget* UIMachine::mainWindow() const
{
    if (machineLogic() &&
        machineLogic()->mainMachineWindow() &&
        machineLogic()->mainMachineWindow()->machineWindow())
        return machineLogic()->mainMachineWindow()->machineWindow();
    else
        return 0;
}

void UIMachine::closeVirtualMachine()
{
    delete this;
}

UIMachineLogic* UIMachine::machineLogic() const
{
    if (m_pVisualState && m_pVisualState->machineLogic())
        return m_pVisualState->machineLogic();
    else
        return 0;
}

void UIMachine::sltChangeVisualState(UIVisualStateType visualStateType)
{
    /* Delete previous state: */
    delete m_pVisualState;
    m_pVisualState = 0;

    /* Create new state: */
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:
        {
            /* Create normal visual state: */
            m_pVisualState = new UIVisualStateNormal(this, m_pSession, m_pActionsPool);
            break;
        }
        case UIVisualStateType_Fullscreen:
        {
            /* Create fullscreen visual state: */
            m_pVisualState = new UIVisualStateFullscreen(this, m_pSession, m_pActionsPool);
            break;
        }
        case UIVisualStateType_Seamless:
        {
            /* Create seamless visual state: */
            m_pVisualState = new UIVisualStateSeamless(this, m_pSession, m_pActionsPool);
            break;
        }
        default:
            break;
    }
}

void UIMachine::enterBaseVisualState()
{
    sltChangeVisualState(UIVisualStateType_Normal);
}

#include "UIMachine.moc"

