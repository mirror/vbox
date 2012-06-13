/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachine class implementation
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QTimer>

/* Local includes: */
#include "VBoxGlobal.h"
#include "UIMachine.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#ifdef Q_WS_MAC
# include <ApplicationServices/ApplicationServices.h>
#endif /* Q_WS_MAC */

/* Visual state interface: */
class UIVisualState : public QObject
{
    Q_OBJECT;

signals:

    /* Signal to change-state: */
    void sigChangeVisualState(UIVisualStateType newVisualStateType);

public:

    /* Constructor: */
    UIVisualState(QObject *pParent, UISession *pSession)
        : QObject(pParent)
        , m_pSession(pSession)
        , m_pMachineLogic(0)
#ifdef Q_WS_MAC
        , m_fadeToken(kCGDisplayFadeReservationInvalidToken)
#endif /* Q_WS_MAC */
    {
        /* Connect state-change handler: */
        connect(this, SIGNAL(sigChangeVisualState(UIVisualStateType)), parent(), SLOT(sltChangeVisualState(UIVisualStateType)));
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
    virtual UIVisualStateType visualStateType() const = 0;

    /* Machine logic getter: */
    UIMachineLogic* machineLogic() const { return m_pMachineLogic; }

    /* Method to prepare change one visual state to another: */
    virtual bool prepareChange(UIVisualStateType previousVisualStateType)
    {
        m_pMachineLogic = UIMachineLogic::create(this, m_pSession, visualStateType());
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

    /* Method to change one visual state to another: */
    virtual void change()
    {
        /* Prepare the logic object: */
        m_pMachineLogic->prepare();
    }

    /* Method to finish change one visual state to another: */
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

protected:

    /* Variables: */
    UISession *m_pSession;
    UIMachineLogic *m_pMachineLogic;
#ifdef Q_WS_MAC
    CGDisplayFadeReservationToken m_fadeToken;
#endif /* Q_WS_MAC */
};

/* Normal visual state implementation: */
class UIVisualStateNormal : public UIVisualState
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIVisualStateNormal(QObject *pParent, UISession *pSession)
        : UIVisualState(pParent, pSession) {}

private slots:

    /* State-change handlers: */
    void sltGoToFullscreenMode() { emit sigChangeVisualState(UIVisualStateType_Fullscreen); }
    void sltGoToSeamlessMode() { emit sigChangeVisualState(UIVisualStateType_Seamless); }
    void sltGoToScaleMode() { emit sigChangeVisualState(UIVisualStateType_Scale); }

private:

    /* Visual state type getter: */
    UIVisualStateType visualStateType() const { return UIVisualStateType_Normal; }

    /* Method to change previous visual state to this one: */
    void change()
    {
        /* Call to base-class: */
        UIVisualState::change();

        /* Connect action handlers: */
        connect(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToFullscreenMode()), Qt::QueuedConnection);
        connect(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToSeamlessMode()), Qt::QueuedConnection);
        connect(gActionPool->action(UIActionIndexRuntime_Toggle_Scale), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToScaleMode()), Qt::QueuedConnection);
    }
};

/* Fullscreen visual state implementation: */
class UIVisualStateFullscreen : public UIVisualState
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIVisualStateFullscreen(QObject *pParent, UISession *pSession)
        : UIVisualState(pParent, pSession)
    {
        /* This visual state should take care of own action: */
        QAction *pActionFullscreen = gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen);
        if (!pActionFullscreen->isChecked())
        {
            pActionFullscreen->blockSignals(true);
            pActionFullscreen->setChecked(true);
            QTimer::singleShot(0, pActionFullscreen, SLOT(sltUpdateAppearance()));
            pActionFullscreen->blockSignals(false);
        }
    }

    /* Destructor: */
    virtual ~UIVisualStateFullscreen()
    {
        /* This visual state should take care of own action: */
        QAction *pActionFullscreen = gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen);
        if (pActionFullscreen->isChecked())
        {
            pActionFullscreen->blockSignals(true);
            pActionFullscreen->setChecked(false);
            QTimer::singleShot(0, pActionFullscreen, SLOT(sltUpdateAppearance()));
            pActionFullscreen->blockSignals(false);
        }
    }

private slots:

    /* State-change handlers: */
    void sltGoToNormalMode() { emit sigChangeVisualState(UIVisualStateType_Normal); }
    void sltGoToSeamlessMode() { emit sigChangeVisualState(UIVisualStateType_Seamless); }
    void sltGoToScaleMode() { emit sigChangeVisualState(UIVisualStateType_Scale); }

private:

    /* Visual state type getter: */
    UIVisualStateType visualStateType() const { return UIVisualStateType_Fullscreen; }

    /* Method to change previous visual state to this one: */
    void change()
    {
        /* Call to base-class: */
        UIVisualState::change();

        /* Connect action handlers: */
        connect(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToNormalMode()), Qt::QueuedConnection);
        connect(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToSeamlessMode()), Qt::QueuedConnection);
        connect(gActionPool->action(UIActionIndexRuntime_Toggle_Scale), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToScaleMode()), Qt::QueuedConnection);
    }
};

/* Seamless visual state implementation: */
class UIVisualStateSeamless : public UIVisualState
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIVisualStateSeamless(QObject *pParent, UISession *pSession)
        : UIVisualState(pParent, pSession)
    {
        /* This visual state should take care of own action: */
        QAction *pActionSeamless = gActionPool->action(UIActionIndexRuntime_Toggle_Seamless);
        if (!pActionSeamless->isChecked())
        {
            pActionSeamless->blockSignals(true);
            pActionSeamless->setChecked(true);
            QTimer::singleShot(0, pActionSeamless, SLOT(sltUpdateAppearance()));
            pActionSeamless->blockSignals(false);
        }
    }

    /* Destructor: */
    virtual ~UIVisualStateSeamless()
    {
        /* This visual state should take care of own action: */
        QAction *pActionSeamless = gActionPool->action(UIActionIndexRuntime_Toggle_Seamless);
        if (pActionSeamless->isChecked())
        {
            pActionSeamless->blockSignals(true);
            pActionSeamless->setChecked(false);
            QTimer::singleShot(0, pActionSeamless, SLOT(sltUpdateAppearance()));
            pActionSeamless->blockSignals(false);
        }
    }

private slots:

    /* State-change handlers: */
    void sltGoToNormalMode() { emit sigChangeVisualState(UIVisualStateType_Normal); }
    void sltGoToFullscreenMode() { emit sigChangeVisualState(UIVisualStateType_Fullscreen); }
    void sltGoToScaleMode() { emit sigChangeVisualState(UIVisualStateType_Scale); }

private:

    /* Visual state type getter: */
    UIVisualStateType visualStateType() const { return UIVisualStateType_Seamless; }

    /* Method to change previous visual state to this one: */
    void change()
    {
        /* Call to base-class: */
        UIVisualState::change();

        /* Connect action handlers: */
        connect(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToNormalMode()), Qt::QueuedConnection);
        connect(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToFullscreenMode()), Qt::QueuedConnection);
        connect(gActionPool->action(UIActionIndexRuntime_Toggle_Scale), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToScaleMode()), Qt::QueuedConnection);
    }
};

/* Scale visual state implementation: */
class UIVisualStateScale : public UIVisualState
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIVisualStateScale(QObject *pParent, UISession *pSession)
        : UIVisualState(pParent, pSession)
    {
        /* This visual state should take care of own action: */
        QAction *pActionScale = gActionPool->action(UIActionIndexRuntime_Toggle_Scale);
        if (!pActionScale->isChecked())
        {
            pActionScale->blockSignals(true);
            pActionScale->setChecked(true);
            QTimer::singleShot(0, pActionScale, SLOT(sltUpdateAppearance()));
            pActionScale->blockSignals(false);
        }
    }

    /* Destructor: */
    virtual ~UIVisualStateScale()
    {
        /* This visual state should take care of own action: */
        QAction *pActionScale = gActionPool->action(UIActionIndexRuntime_Toggle_Scale);
        if (pActionScale->isChecked())
        {
            pActionScale->blockSignals(true);
            pActionScale->setChecked(false);
            QTimer::singleShot(0, pActionScale, SLOT(sltUpdateAppearance()));
            pActionScale->blockSignals(false);
        }
    }

private slots:

    /* State-change handlers: */
    void sltGoToNormalMode() { emit sigChangeVisualState(UIVisualStateType_Normal); }
    void sltGoToFullscreenMode() { emit sigChangeVisualState(UIVisualStateType_Fullscreen); }
    void sltGoToSeamlessMode() { emit sigChangeVisualState(UIVisualStateType_Seamless); }

private:

    /* Visual state type getter: */
    UIVisualStateType visualStateType() const { return UIVisualStateType_Scale; }

    /* Method to change previous visual state to this one: */
    void change()
    {
        /* Call to base-class: */
        UIVisualState::change();

        /* Connect action handlers: */
        connect(gActionPool->action(UIActionIndexRuntime_Toggle_Scale), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToNormalMode()), Qt::QueuedConnection);
        connect(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToFullscreenMode()), Qt::QueuedConnection);
        connect(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless), SIGNAL(triggered(bool)),
                this, SLOT(sltGoToSeamlessMode()), Qt::QueuedConnection);
    }
};

UIMachine::UIMachine(UIMachine **ppSelf, const CSession &session)
    : QObject(0)
    , m_ppThis(ppSelf)
    , initialStateType(UIVisualStateType_Normal)
    , m_session(session)
    , m_pSession(0)
    , m_pVisualState(0)
{
    /* Store self pointer: */
    if (m_ppThis)
        *m_ppThis = this;

    /* Create UI session: */
    m_pSession = new UISession(this, m_session);

    /* Preventing application from closing in case of window(s) closed: */
    qApp->setQuitOnLastWindowClosed(false);

    /* Cache IMedium data: */
    vboxGlobal().startEnumeratingMedia();

    /* Load machine settings: */
    loadMachineSettings();

    /* Enter default (normal) state */
    enterInitialVisualState();
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

    /* Clear self pointer: */
    *m_ppThis = 0;

    /* Quit application: */
    QApplication::quit();
}

QWidget* UIMachine::mainWindow() const
{
    if (machineLogic() && machineLogic()->mainMachineWindow())
        return machineLogic()->mainMachineWindow();
    else
        return 0;
}

void UIMachine::sltChangeVisualState(UIVisualStateType newVisualStateType)
{
    /* Create new state: */
    UIVisualState *pNewVisualState = 0;
    switch (newVisualStateType)
    {
        case UIVisualStateType_Normal:
        {
            /* Create normal visual state: */
            pNewVisualState = new UIVisualStateNormal(this, m_pSession);
            break;
        }
        case UIVisualStateType_Fullscreen:
        {
            /* Create fullscreen visual state: */
            pNewVisualState = new UIVisualStateFullscreen(this, m_pSession);
            break;
        }
        case UIVisualStateType_Seamless:
        {
            /* Create seamless visual state: */
            pNewVisualState = new UIVisualStateSeamless(this, m_pSession);
            break;
        }
        case UIVisualStateType_Scale:
        {
            /* Create scale visual state: */
            pNewVisualState = new UIVisualStateScale(this, m_pSession);
            break;
        }
        default:
            break;
    }

    /* Get previous visual state type: */
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

void UIMachine::sltCloseVirtualMachine()
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
         * at machine start, so we are choosing first of requested modes: */
        bool fIsSomeExtendedModeChosen = false;

        if (!fIsSomeExtendedModeChosen)
        {
            /* Test 'scale' flag: */
            QString strScaleSettings = machine.GetExtraData(GUI_Scale);
            if (strScaleSettings == "on")
            {
                fIsSomeExtendedModeChosen = true;
                /* We can enter scale mode initially: */
                initialStateType = UIVisualStateType_Scale;
            }
        }

        if (!fIsSomeExtendedModeChosen)
        {
            /* Test 'seamless' flag: */
            QString strSeamlessSettings = machine.GetExtraData(GUI_Seamless);
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
            QString strFullscreenSettings = machine.GetExtraData(GUI_Fullscreen);
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
        /* Set 'scale' flag: */
        machine.SetExtraData(GUI_Scale, m_pVisualState &&
                             m_pVisualState->visualStateType() == UIVisualStateType_Scale ? "on" : QString());

        /* Set 'seamless' flag: */
        machine.SetExtraData(GUI_Seamless, m_pVisualState &&
                             m_pVisualState->visualStateType() == UIVisualStateType_Seamless ? "on" : QString());

        /* Set 'fullscreen' flag: */
        machine.SetExtraData(GUI_Fullscreen, m_pVisualState &&
                             m_pVisualState->visualStateType() == UIVisualStateType_Fullscreen ? "on" : QString());
    }
}

#include "UIMachine.moc"

