/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachine class implementation
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

/* Global includes */
#include <QTimer>

/* Local includes */
#include "VBoxGlobal.h"
#include "UIMachine.h"
#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"

#ifdef Q_WS_MAC
# include <ApplicationServices/ApplicationServices.h>
#endif /* Q_WS_MAC */

class UIVisualState : public QObject
{
    Q_OBJECT;

public:

    /* Visual state holder constructor: */
    UIVisualState(QObject *pParent, UISession *pSession, UIActionsPool *pActionsPool)
        : QObject(pParent)
        , m_pSession(pSession)
        , m_pActionsPool(pActionsPool)
        , m_pMachineLogic(0)
#ifdef Q_WS_MAC
        , m_fadeToken(kCGDisplayFadeReservationInvalidToken)
#endif /* Q_WS_MAC */
    {
        /* Connect state-change handler: */
        connect(this, SIGNAL(sigChangeVisualState(UIVisualStateType)), parent(), SLOT(sltChangeVisualState(UIVisualStateType)));
    }

    /* Public getters: */
    UIMachineLogic* machineLogic() const  { return m_pMachineLogic; }
    virtual UIVisualStateType visualStateType() const = 0;

    virtual bool prepareChange(UIVisualStateType previousVisualStateType)
    {
        m_pMachineLogic = UIMachineLogic::create(this, m_pSession, m_pActionsPool, visualStateType());
        bool fResult = m_pMachineLogic->checkAvailability();
#ifdef Q_WS_MAC
        /* If the new is or the old type was fullscreen we add the blending
         * transition between the mode switches.
         * TODO: make this more general. */
        if (   fResult
            && (   visualStateType() == UIVisualStateType_Fullscreen
                || previousVisualStateType == UIVisualStateType_Fullscreen))
        {
            /* Fade to black */
            CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &m_fadeToken);
            CGDisplayFade(m_fadeToken, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, true);
        }
#else /* Q_WS_MAC */
        Q_UNUSED(previousVisualStateType);
#endif /* !Q_WS_MAC */
        return fResult;
    }

    virtual void change() = 0;

    virtual void finishChange()
    {
#ifdef Q_WS_MAC
        /* If there is a valid fade token, fade back to normal color in any
         * case. */
        if (m_fadeToken != kCGDisplayFadeReservationInvalidToken)
        {
            /* Fade back to the normal gamma */
            CGDisplayFade(m_fadeToken, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, false);
            CGReleaseDisplayFadeReservation(m_fadeToken);
            m_fadeToken = kCGDisplayFadeReservationInvalidToken;
        }
#endif /* Q_WS_MAC */
    }


signals:

    /* Signal to change-state: */
    void sigChangeVisualState(UIVisualStateType visualStateType);

protected:

    /* Protected members: */
    UISession *m_pSession;
    UIActionsPool *m_pActionsPool;
    UIMachineLogic *m_pMachineLogic;
#ifdef Q_WS_MAC
    CGDisplayFadeReservationToken m_fadeToken;
#endif /* Q_WS_MAC */
};

class UIVisualStateNormal : public UIVisualState
{
    Q_OBJECT;

public:

    /* Normal visual state holder constructor: */
    UIVisualStateNormal(QObject *pParent, UISession *pSession, UIActionsPool *pActionsPool)
        : UIVisualState(pParent, pSession, pActionsPool) {}

    UIVisualStateType visualStateType() const { return UIVisualStateType_Normal; }

    void change()
    {
        /* Connect action handlers: */
        connect(m_pActionsPool->action(UIActionIndex_Toggle_Fullscreen), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToFullscreenMode()), Qt::QueuedConnection);
        connect(m_pActionsPool->action(UIActionIndex_Toggle_Seamless), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToSeamlessMode()), Qt::QueuedConnection);

        /* Initialize the logic object: */
        m_pMachineLogic->initialize();
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
        : UIVisualState(pParent, pSession, pActionsPool)
    {
        /* This visual state should take care of own action: */
        QAction *pActionFullscreen = m_pActionsPool->action(UIActionIndex_Toggle_Fullscreen);
        if (!pActionFullscreen->isChecked())
        {
            pActionFullscreen->blockSignals(true);
            pActionFullscreen->setChecked(true);
            QTimer::singleShot(0, pActionFullscreen, SLOT(sltUpdateAppearance()));
            pActionFullscreen->blockSignals(false);
        }
    }

    /* Fullscreen visual state holder destructor: */
    virtual ~UIVisualStateFullscreen()
    {
        /* This visual state should take care of own action: */
        QAction *pActionFullscreen = m_pActionsPool->action(UIActionIndex_Toggle_Fullscreen);
        if (pActionFullscreen->isChecked())
        {
            pActionFullscreen->blockSignals(true);
            pActionFullscreen->setChecked(false);
            QTimer::singleShot(0, pActionFullscreen, SLOT(sltUpdateAppearance()));
            pActionFullscreen->blockSignals(false);
        }
    }

    UIVisualStateType visualStateType() const { return UIVisualStateType_Fullscreen; }

    void change()
    {
        /* Connect action handlers: */
        connect(m_pActionsPool->action(UIActionIndex_Toggle_Fullscreen), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToNormalMode()), Qt::QueuedConnection);
        connect(m_pActionsPool->action(UIActionIndex_Toggle_Seamless), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToSeamlessMode()), Qt::QueuedConnection);

        /* Initialize the logic object: */
        m_pMachineLogic->initialize();
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
        : UIVisualState(pParent, pSession, pActionsPool)
    {
        /* This visual state should take care of own action: */
        QAction *pActionSeamless = m_pActionsPool->action(UIActionIndex_Toggle_Seamless);
        if (pActionSeamless->isChecked())
        {
            pActionSeamless->blockSignals(true);
            pActionSeamless->setChecked(true);
            QTimer::singleShot(0, pActionSeamless, SLOT(sltUpdateAppearance()));
            pActionSeamless->blockSignals(false);
        }
    }

    /* Seamless visual state holder destructor: */
    virtual ~UIVisualStateSeamless()
    {
        /* This visual state should take care of own action: */
        QAction *pActionSeamless = m_pActionsPool->action(UIActionIndex_Toggle_Seamless);
        if (pActionSeamless->isChecked())
        {
            pActionSeamless->blockSignals(true);
            pActionSeamless->setChecked(false);
            QTimer::singleShot(0, pActionSeamless, SLOT(sltUpdateAppearance()));
            pActionSeamless->blockSignals(false);
        }
    }

    UIVisualStateType visualStateType() const { return UIVisualStateType_Seamless; }

    void change()
    {
        /* Connect action handlers: */
        connect(m_pActionsPool->action(UIActionIndex_Toggle_Fullscreen), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToFullscreenMode()), Qt::QueuedConnection);
        connect(m_pActionsPool->action(UIActionIndex_Toggle_Seamless), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToNormalMode()), Qt::QueuedConnection);

        /* Initialize the logic object: */
        m_pMachineLogic->initialize();
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
    , initialStateType(UIVisualStateType_Normal)
    , m_session(session)
    , m_pActionsPool(new UIActionsPool(this))
    , m_pSession(new UISession(this, m_session))
    , m_pVisualState(0)
{
#ifdef DEBUG_poetzsch
    printf("New code path\n");
#endif /* DEBUG_poetzsch */
    /* Preventing application from closing in case of window(s) closed: */
    qApp->setQuitOnLastWindowClosed(false);

    /* Cache IMedium data: */
    vboxGlobal().startEnumeratingMedia();

    /* Storing self: */
    if (m_ppThis)
        *m_ppThis = this;

    /* Load machine settings: */
    loadMachineSettings();

    /* Enter default (normal) state */
    enterInitialVisualState();
}

UIMachine::~UIMachine()
{
    /* Save machine settings: */
    saveMachineSettings();
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

void UIMachine::sltChangeVisualState(UIVisualStateType visualStateType)
{
    /* Create new state: */
    UIVisualState *pNewVisualState = 0;
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:
        {
            /* Create normal visual state: */
            pNewVisualState = new UIVisualStateNormal(this, m_pSession, m_pActionsPool);
            break;
        }
        case UIVisualStateType_Fullscreen:
        {
            /* Create fullscreen visual state: */
            pNewVisualState = new UIVisualStateFullscreen(this, m_pSession, m_pActionsPool);
            break;
        }
        case UIVisualStateType_Seamless:
        {
            /* Create seamless visual state: */
            pNewVisualState = new UIVisualStateSeamless(this, m_pSession, m_pActionsPool);
            break;
        }
        default:
            break;
    }

    UIVisualStateType previousVisualStateType = UIVisualStateType_Normal;
    if (m_pVisualState)
        previousVisualStateType = m_pVisualState->visualStateType();

    /* First we have to check if the selected mode is available at all.
     * Only then we delete the old mode and switch to the new mode. */
    if (pNewVisualState->prepareChange(previousVisualStateType))
    {
        /* Delete previous state: */
        delete m_pVisualState;

        /* Set the new mode as current mode: */
        m_pVisualState = pNewVisualState;
        m_pVisualState->change();

        /* Finish any setup: */
        m_pVisualState->finishChange();
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

void UIMachine::closeVirtualMachine()
{
    delete this;
}

void UIMachine::enterInitialVisualState()
{
    sltChangeVisualState(initialStateType);
}

UIMachineLogic* UIMachine::machineLogic() const
{
    if (m_pVisualState && m_pVisualState->machineLogic())
        return m_pVisualState->machineLogic();
    else
        return 0;
}

void UIMachine::loadMachineSettings()
{
    /* Load machine settings: */
    CMachine machine = uisession()->session().GetMachine();

    /* Load extra-data settings: */
    {
        /* Machine while saving own settings will save "yes" only for current
         * visual representation mode if its differs from normal mode of course.
         * But user can alter extra data manually in machine xml file and set there
         * more than one visual representation mode flags. Shame on such user!
         * There is no reason to enter in more than one visual representation mode
         * at machine start, so we are chosing first of requested modes: */
        bool fIsSomeExtendedModeChosen = false;

        if (!fIsSomeExtendedModeChosen)
        {
            /* Test 'seamless' flag: */
            QString strSeamlessSettings = machine.GetExtraData(VBoxDefs::GUI_Seamless);
            if (strSeamlessSettings == "on")
            {
                fIsSomeExtendedModeChosen = true;
                /* We can't enter seamless mode initially,
                 * so we should ask ui-session for that: */
                uisession()->setSeamlessModeRequested(true);
            }
        }

        if (!fIsSomeExtendedModeChosen)
        {
            /* Test 'fullscreen' flag: */
            QString strFullscreenSettings = machine.GetExtraData(VBoxDefs::GUI_Fullscreen);
            if (strFullscreenSettings == "on")
            {
                fIsSomeExtendedModeChosen = true;
                /* We can enter fullscreen mode initially: */
                initialStateType = UIVisualStateType_Fullscreen;
            }
        }
    }
}

void UIMachine::saveMachineSettings()
{
    /* Save machine settings: */
    CMachine machine = uisession()->session().GetMachine();

    /* Save extra-data settings: */
    {
        /* Set 'seamless' flag: */
        machine.SetExtraData(VBoxDefs::GUI_Seamless, m_pVisualState &&
                             m_pVisualState->visualStateType() == UIVisualStateType_Seamless ? "on" : QString());

        /* Set 'fullscreen' flag: */
        machine.SetExtraData(VBoxDefs::GUI_Fullscreen, m_pVisualState &&
                             m_pVisualState->visualStateType() == UIVisualStateType_Fullscreen ? "on" : QString());
    }
}

#include "UIMachine.moc"

