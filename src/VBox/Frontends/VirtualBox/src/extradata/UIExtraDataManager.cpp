/* $Id$ */
/** @file
 * VBox Qt GUI - UIExtraDataManager class implementation.
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

/* Qt includes: */
#include <QMutex>

/* GUI includes: */
#include "UIExtraDataManager.h"
#include "UIMainEventListener.h"
#include "VBoxGlobal.h"
#include "VBoxGlobalSettings.h"
#include "UIActionPool.h"

/* COM includes: */
#include "COMEnums.h"
#include "CEventSource.h"


/** QObject extension
  * notifying UIExtraDataManager whenever any of extra-data values changed. */
class UIExtraDataEventHandler : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about GUI language change. */
    void sigLanguageChange(QString strLanguage);

    /** Notifies about Selector UI keyboard shortcut change. */
    void sigSelectorUIShortcutChange();
    /** Notifies about Runtime UI keyboard shortcut change. */
    void sigRuntimeUIShortcutChange();

    /** Notifies about HID LED sync state change. */
    void sigHIDLedsSyncStateChange(bool fEnabled);

#ifdef RT_OS_DARWIN
    /** Mac OS X: Notifies about 'presentation mode' status change. */
    void sigPresentationModeChange(bool fEnabled);
    /** Mac OS X: Notifies about 'dock icon' appearance change. */
    void sigDockIconAppearanceChange(bool fEnabled);
#endif /* RT_OS_DARWIN */

public:

    /** Extra-data event-handler constructor. */
    UIExtraDataEventHandler(QObject *pParent);

public slots:

    /** Handles extra-data 'can change' event: */
    void sltExtraDataCanChange(QString strMachineID, QString strKey, QString strValue, bool &fVeto, QString &strVetoReason);
    /** Handles extra-data 'change' event: */
    void sltExtraDataChange(QString strMachineID, QString strKey, QString strValue);

private:

    /** Protects sltExtraDataChange. */
    QMutex m_mutex;
};

UIExtraDataEventHandler::UIExtraDataEventHandler(QObject *pParent)
    : QObject(pParent)
{}

void UIExtraDataEventHandler::sltExtraDataCanChange(QString strMachineID, QString strKey, QString strValue, bool &fVeto, QString &strVetoReason)
{
    /* Global extra-data 'can change' event: */
    if (QUuid(strMachineID).isNull())
    {
        /* It's a global extra-data key someone wants to change: */
        if (strKey.startsWith("GUI/"))
        {
            /* Try to set the global setting to check its syntax: */
            VBoxGlobalSettings gs(false /* non-null */);
            /* Known GUI property key? */
            if (gs.setPublicProperty(strKey, strValue))
            {
                /* But invalid GUI property value? */
                if (!gs)
                {
                    /* Remember veto reason: */
                    strVetoReason = gs.lastError();
                    /* And disallow that change: */
                    fVeto = true;
                }
                return;
            }
        }
    }
}

void UIExtraDataEventHandler::sltExtraDataChange(QString strMachineID, QString strKey, QString strValue)
{
    /* Global extra-data 'change' event: */
    if (QUuid(strMachineID).isNull())
    {
        if (strKey.startsWith("GUI/"))
        {
            /* Language changed? */
            if (strKey == GUI_LanguageId)
                emit sigLanguageChange(strValue);
            /* Selector UI shortcut changed? */
            else if (strKey == GUI_Input_SelectorShortcuts && gActionPool->type() == UIActionPoolType_Selector)
                emit sigSelectorUIShortcutChange();
            /* Runtime UI shortcut changed? */
            else if (strKey == GUI_Input_MachineShortcuts && gActionPool->type() == UIActionPoolType_Runtime)
                emit sigRuntimeUIShortcutChange();
#ifdef Q_WS_MAC
            /* 'Presentation mode' status changed? */
            else if (strKey == GUI_PresentationModeEnabled)
            {
                // TODO: Make it global..
                /* Allowed what is not restricted: */
                bool fEnabled = strValue.isEmpty() || strValue.toLower() != "false";
                emit sigPresentationModeChange(fEnabled);
            }
#endif /* Q_WS_MAC */

            /* Apply global property: */
            m_mutex.lock();
            vboxGlobal().settings().setPublicProperty(strKey, strValue);
            m_mutex.unlock();
            AssertMsgReturnVoid(!!vboxGlobal().settings(), ("Failed to apply global property.\n"));
        }
    }
    /* Make sure event came for the currently running VM: */
    else if (   vboxGlobal().isVMConsoleProcess()
             && strMachineID == vboxGlobal().managedVMUuid())
    {
        /* HID LEDs sync state changed? */
        if (strKey == GUI_HidLedsSync)
        {
            /* Allowed what is not restricted: */
            // TODO: Make it global..
            bool fEnabled = strValue.isEmpty() || strValue != "0";
            emit sigHIDLedsSyncStateChange(fEnabled);
        }
#ifdef Q_WS_MAC
        /* 'Dock icon' appearance changed? */
        else if (   strKey == GUI_RealtimeDockIconUpdateEnabled
                 || strKey == GUI_RealtimeDockIconUpdateMonitor)
        {
            /* Allowed what is not restricted: */
            // TODO: Make it global..
            bool fEnabled = strValue.isEmpty() || strValue.toLower() != "false";
            emit sigDockIconAppearanceChange(fEnabled);
        }
#endif /* Q_WS_MAC */
    }
}


/* static */
UIExtraDataManager *UIExtraDataManager::m_pInstance = 0;

/* static */
UIExtraDataManager* UIExtraDataManager::instance()
{
    /* Create/prepare instance if not yet exists: */
    if (!m_pInstance)
    {
        new UIExtraDataManager;
        m_pInstance->prepare();
    }
    /* Return instance: */
    return m_pInstance;
}

/* static */
void UIExtraDataManager::destroy()
{
    /* Destroy/cleanup instance if still exists: */
    if (m_pInstance)
    {
        m_pInstance->cleanup();
        delete m_pInstance;
    }
}

UIExtraDataManager::UIExtraDataManager()
    : m_pHandler(new UIExtraDataEventHandler(this))
{
    /* Connect to static instance: */
    m_pInstance = this;
}

UIExtraDataManager::~UIExtraDataManager()
{
    /* Disconnect from static instance: */
    m_pInstance = 0;
}

void UIExtraDataManager::prepare()
{
    /* Prepare Main event-listener: */
    prepareMainEventListener();
    /* Prepare extra-data event-handler: */
    prepareExtraDataEventHandler();
}

void UIExtraDataManager::prepareMainEventListener()
{
    /* Register Main event-listener:  */
    const CVirtualBox &vbox = vboxGlobal().virtualBox();
    ComObjPtr<UIMainEventListenerImpl> pListener;
    pListener.createObject();
    pListener->init(new UIMainEventListener, this);
    m_listener = CEventListener(pListener);
    QVector<KVBoxEventType> events;
    events
        << KVBoxEventType_OnExtraDataCanChange
        << KVBoxEventType_OnExtraDataChanged;
    vbox.GetEventSource().RegisterListener(m_listener, events, TRUE);
    AssertWrapperOk(vbox);

    /* This is a vetoable event, so we have to respond to the event and have to use a direct connection therefor: */
    connect(pListener->getWrapped(), SIGNAL(sigExtraDataCanChange(QString, QString, QString, bool&, QString&)),
            m_pHandler, SLOT(sltExtraDataCanChange(QString, QString, QString, bool&, QString&)),
            Qt::DirectConnection);
    /* Use a direct connection to the helper class: */
    connect(pListener->getWrapped(), SIGNAL(sigExtraDataChange(QString, QString, QString)),
            m_pHandler, SLOT(sltExtraDataChange(QString, QString, QString)),
            Qt::DirectConnection);
}

void UIExtraDataManager::prepareExtraDataEventHandler()
{
    /* Language change signal: */
    connect(m_pHandler, SIGNAL(sigLanguageChange(QString)),
            this, SIGNAL(sigLanguageChange(QString)),
            Qt::QueuedConnection);

    /* Selector/Runtime UI shortcut change signals: */
    connect(m_pHandler, SIGNAL(sigSelectorUIShortcutChange()),
            this, SIGNAL(sigSelectorUIShortcutChange()),
            Qt::QueuedConnection);
    connect(m_pHandler, SIGNAL(sigRuntimeUIShortcutChange()),
            this, SIGNAL(sigRuntimeUIShortcutChange()),
            Qt::QueuedConnection);

    /* HID LED sync state change signal: */
    connect(m_pHandler, SIGNAL(sigHIDLedsSyncStateChange(bool)),
            this, SIGNAL(sigHIDLedsSyncStateChange(bool)),
            Qt::QueuedConnection);

#ifdef Q_WS_MAC
    /* 'Presentation mode' status change signal: */
    connect(m_pHandler, SIGNAL(sigPresentationModeChange(bool)),
            this, SIGNAL(sigPresentationModeChange(bool)),
            Qt::QueuedConnection);

    /* 'Dock icon' appearance change signal: */
    connect(m_pHandler, SIGNAL(sigDockIconAppearanceChange(bool)),
            this, SIGNAL(sigDockIconAppearanceChange(bool)),
            Qt::QueuedConnection);
#endif /* Q_WS_MAC */
}

void UIExtraDataManager::cleanup()
{
    /* Cleanup Main event-listener: */
    cleanupMainEventListener();
}

void UIExtraDataManager::cleanupMainEventListener()
{
    /* Unregister Main event-listener:  */
    const CVirtualBox &vbox = vboxGlobal().virtualBox();
    vbox.GetEventSource().UnregisterListener(m_listener);
}

#include "UIExtraDataManager.moc"

