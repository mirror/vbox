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

signals:

    void sigChangeVisualState(UIVisualStateType visualStateType);

protected:

    /* Protected members: */
    UIMachineLogic *m_pMachineLogic;

    /* Friend classes: */
    friend class UIMachine;
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
    , m_pSession(new UISession(this, session))
    , m_pActionsPool(new UIActionsPool(this))
    , m_pVisualState(0)
{
    /* Cache IMedium data: */
    vboxGlobal().startEnumeratingMedia();

    /* Storing self */
    if (ppSelf)
        *ppSelf = this;

    /* Enter default (normal) state */
    enterBaseVisualState();
}

UIMachineLogic* UIMachine::machineLogic() const
{
    return m_pVisualState->m_pMachineLogic;
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
