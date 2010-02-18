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
#include "UIActionsPool.h"
#include "UIMachineLogic.h"

class UIVisualState : public QObject
{
    Q_OBJECT;

public:

    UIVisualState(QObject *pParent) : QObject(pParent), m_pMachineLogic(0)
    {
        /* Connect state-change handler */
        connect(this, SIGNAL(sigChangeVisualState(UIVisualStateType)), parent(), SLOT(sltChangeVisualState(UIVisualStateType)));
    }

signals:

    void sigChangeVisualState(UIVisualStateType visualStateType);

protected:

    UIMachineLogic *m_pMachineLogic;
};

class UIVisualStateNormal : public UIVisualState
{
    Q_OBJECT;

public:

    UIVisualStateNormal(QObject *pParent, const CSession &session, UIActionsPool *pActionsPool)
        : UIVisualState(pParent)
    {
        /* Connect action handlers */
        connect(pActionsPool->action(UIActionIndex_Toggle_Fullscreen), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToFullscreenMode()));
        connect(pActionsPool->action(UIActionIndex_Toggle_Seamless), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToSeamlessMode()));

        /* Create state logic */
        m_pMachineLogic = UIMachineLogic::create(this, session, pActionsPool, UIVisualStateType_Normal);
    }

private slots:

    void sltGoToFullscreenMode()
    {
        emit sigChangeVisualState(UIVisualStateType_Fullscreen);
    }

    void sltGoToSeamlessMode()
    {
        emit sigChangeVisualState(UIVisualStateType_Seamless);
    }
};

class UIVisualStateFullscreen : public UIVisualState
{
    Q_OBJECT;

public:

    UIVisualStateFullscreen(QObject *pParent, const CSession &session, UIActionsPool *pActionsPool)
        : UIVisualState(pParent)
    {
        /* Connect action handlers */
        connect(pActionsPool->action(UIActionIndex_Toggle_Fullscreen), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToNormalMode()));
        connect(pActionsPool->action(UIActionIndex_Toggle_Seamless), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToSeamlessMode()));

        /* Create state logic */
        m_pMachineLogic = UIMachineLogic::create(this, session, pActionsPool, UIVisualStateType_Fullscreen);
    }

private slots:

    void sltGoToNormalMode()
    {
        emit sigChangeVisualState(UIVisualStateType_Normal);
    }

    void sltGoToSeamlessMode()
    {
        emit sigChangeVisualState(UIVisualStateType_Seamless);
    }
};

class UIVisualStateSeamless : public UIVisualState
{
    Q_OBJECT;

public:

    UIVisualStateSeamless(QObject *pParent, const CSession &session, UIActionsPool *pActionsPool)
        : UIVisualState(pParent)
    {
        /* Connect action handlers */
        connect(pActionsPool->action(UIActionIndex_Toggle_Fullscreen), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToFullscreenMode()));
        connect(pActionsPool->action(UIActionIndex_Toggle_Seamless), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToNormalMode()));

        /* Create state logic */
        m_pMachineLogic = UIMachineLogic::create(this, session, pActionsPool, UIVisualStateType_Seamless);
    }

private slots:

    void sltGoToFullscreenMode()
    {
        emit sigChangeVisualState(UIVisualStateType_Fullscreen);
    }

    void sltGoToNormalMode()
    {
        emit sigChangeVisualState(UIVisualStateType_Normal);
    }
};

UIMachine::UIMachine(UIMachine **ppSelf, const CSession &session)
    : QObject(0)
    , m_session(session)
    , m_pActionsPool(new UIActionsPool(this))
    , m_pVisualState(0)
{
    /* Cache IMedium data: */
    vboxGlobal().startEnumeratingMedia();

    /* Check CSession object */
    AssertMsg(!m_session.isNull(), ("CSession is not set!\n"));

    /* Storing self */
    if (ppSelf)
        *ppSelf = this;

    /* Enter default (normal) state */
    enterBaseVisualState();
}

void UIMachine::sltChangeVisualState(UIVisualStateType visualStateType)
{
    /* Delete previous state */
    delete m_pVisualState;
    m_pVisualState = 0;

    /* Create new state */
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:
        {
            m_pVisualState = new UIVisualStateNormal(this, m_session, m_pActionsPool);
            break;
        }
        case UIVisualStateType_Fullscreen:
        {
            m_pVisualState = new UIVisualStateFullscreen(this, m_session, m_pActionsPool);
            break;
        }
        case UIVisualStateType_Seamless:
        {
            m_pVisualState = new UIVisualStateSeamless(this, m_session, m_pActionsPool);
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
