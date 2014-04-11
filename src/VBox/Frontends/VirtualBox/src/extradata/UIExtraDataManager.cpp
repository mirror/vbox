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
#include "UIConverter.h"

/* COM includes: */
#include "COMEnums.h"
#include "CEventSource.h"
#include "CVirtualBox.h"
#include "CMachine.h"


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

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
bool UIExtraDataManager::shouldWeAllowApplicationUpdate() const
{
    /* Allow unless 'forbidden' flag is set: */
    return !isFeatureAllowed(GUI_PreventApplicationUpdate);
}
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

bool UIExtraDataManager::shouldWeShowMachine(const QString &strID) const
{
    /* Show unless 'forbidden' flag is set: */
    return !isFeatureAllowed(GUI_HideFromManager, strID);
}

bool UIExtraDataManager::shouldWeShowMachineDetails(const QString &strID) const
{
    /* Show unless 'forbidden' flag is set: */
    return !isFeatureAllowed(GUI_HideDetails, strID);
}

bool UIExtraDataManager::shouldWeAllowMachineReconfiguration(const QString &strID) const
{
    /* Show unless 'forbidden' flag is set: */
    return !isFeatureAllowed(GUI_PreventReconfiguration, strID);
}

bool UIExtraDataManager::shouldWeAllowMachineSnapshotOperations(const QString &strID) const
{
    /* Show unless 'forbidden' flag is set: */
    return !isFeatureAllowed(GUI_PreventSnapshotOperations, strID);
}

bool UIExtraDataManager::shouldWeAutoMountGuestScreens(const QString &strID) const
{
    /* Show only if 'allowed' flag is set: */
    return isFeatureAllowed(GUI_AutomountGuestScreens, strID);
}

RuntimeMenuType UIExtraDataManager::restrictedRuntimeMenuTypes(const QString &strID) const
{
    /* Prepare result: */
    RuntimeMenuType result = RuntimeMenuType_Invalid;
    /* Load restricted runtime-menu-types: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedRuntimeMenus, strID))
    {
        RuntimeMenuType value = gpConverter->fromInternalString<RuntimeMenuType>(strValue);
        if (value != RuntimeMenuType_Invalid)
            result = static_cast<RuntimeMenuType>(result | value);
    }
    /* Return result: */
    return result;
}

#ifdef Q_WS_MAC
RuntimeMenuApplicationActionType UIExtraDataManager::restrictedRuntimeMenuApplicationActionTypes(const QString &strID) const
{
    /* Prepare result: */
    RuntimeMenuApplicationActionType result = RuntimeMenuApplicationActionType_Invalid;
    /* Load restricted runtime-application-menu action-types: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedRuntimeApplicationMenuActions, strID))
    {
        RuntimeMenuApplicationActionType value = gpConverter->fromInternalString<RuntimeMenuApplicationActionType>(strValue);
        if (value != RuntimeMenuApplicationActionType_Invalid)
            result = static_cast<RuntimeMenuApplicationActionType>(result | value);
    }
    /* Return result: */
    return result;
}
#endif /* Q_WS_MAC */

RuntimeMenuMachineActionType UIExtraDataManager::restrictedRuntimeMenuMachineActionTypes(const QString &strID) const
{
    /* Prepare result: */
    RuntimeMenuMachineActionType result = RuntimeMenuMachineActionType_Invalid;
    /* Load restricted runtime-machine-menu action-types: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedRuntimeMachineMenuActions, strID))
    {
        RuntimeMenuMachineActionType value = gpConverter->fromInternalString<RuntimeMenuMachineActionType>(strValue);
        if (value != RuntimeMenuMachineActionType_Invalid)
            result = static_cast<RuntimeMenuMachineActionType>(result | value);
    }
    /* Return result: */
    return result;
}

RuntimeMenuViewActionType UIExtraDataManager::restrictedRuntimeMenuViewActionTypes(const QString &strID) const
{
    /* Prepare result: */
    RuntimeMenuViewActionType result = RuntimeMenuViewActionType_Invalid;
    /* Load restricted runtime-view-menu action-types: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedRuntimeViewMenuActions, strID))
    {
        RuntimeMenuViewActionType value = gpConverter->fromInternalString<RuntimeMenuViewActionType>(strValue);
        if (value != RuntimeMenuViewActionType_Invalid)
            result = static_cast<RuntimeMenuViewActionType>(result | value);
    }
    /* Return result: */
    return result;
}

RuntimeMenuDevicesActionType UIExtraDataManager::restrictedRuntimeMenuDevicesActionTypes(const QString &strID) const
{
    /* Prepare result: */
    RuntimeMenuDevicesActionType result = RuntimeMenuDevicesActionType_Invalid;
    /* Load restricted runtime-devices-menu action-types: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedRuntimeDevicesMenuActions, strID))
    {
        RuntimeMenuDevicesActionType value = gpConverter->fromInternalString<RuntimeMenuDevicesActionType>(strValue);
        if (value != RuntimeMenuDevicesActionType_Invalid)
            result = static_cast<RuntimeMenuDevicesActionType>(result | value);
    }
    /* Return result: */
    return result;
}

#ifdef VBOX_WITH_DEBUGGER_GUI
RuntimeMenuDebuggerActionType UIExtraDataManager::restrictedRuntimeMenuDebuggerActionTypes(const QString &strID) const
{
    /* Prepare result: */
    RuntimeMenuDebuggerActionType result = RuntimeMenuDebuggerActionType_Invalid;
    /* Load restricted runtime-debugger-menu action-types: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedRuntimeDebuggerMenuActions, strID))
    {
        RuntimeMenuDebuggerActionType value = gpConverter->fromInternalString<RuntimeMenuDebuggerActionType>(strValue);
        if (value != RuntimeMenuDebuggerActionType_Invalid)
            result = static_cast<RuntimeMenuDebuggerActionType>(result | value);
    }
    /* Return result: */
    return result;
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

RuntimeMenuHelpActionType UIExtraDataManager::restrictedRuntimeMenuHelpActionTypes(const QString &strID) const
{
    /* Prepare result: */
    RuntimeMenuHelpActionType result = RuntimeMenuHelpActionType_Invalid;
    /* Load restricted runtime-help-menu action-types: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedRuntimeHelpMenuActions, strID))
    {
        RuntimeMenuHelpActionType value = gpConverter->fromInternalString<RuntimeMenuHelpActionType>(strValue);
        if (value != RuntimeMenuHelpActionType_Invalid)
            result = static_cast<RuntimeMenuHelpActionType>(result | value);
    }
    /* Return result: */
    return result;
}

UIVisualStateType UIExtraDataManager::restrictedVisualStateTypes(const QString &strID) const
{
    /* Prepare result: */
    UIVisualStateType result = UIVisualStateType_Invalid;
    /* Load restricted visual-state-types: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedVisualStates, strID))
    {
        UIVisualStateType value = gpConverter->fromInternalString<UIVisualStateType>(strValue);
        if (value != UIVisualStateType_Invalid)
            result = static_cast<UIVisualStateType>(result | value);
    }
    /* Return result: */
    return result;
}

MachineCloseAction UIExtraDataManager::defaultMachineCloseAction(const QString &strID) const
{
    return gpConverter->fromInternalString<MachineCloseAction>(extraDataString(GUI_DefaultCloseAction, strID));
}

MachineCloseAction UIExtraDataManager::restrictedMachineCloseActions(const QString &strID) const
{
    /* Prepare result: */
    MachineCloseAction result = MachineCloseAction_Invalid;
    /* Load restricted machine-close-actions: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedCloseActions, strID))
    {
        MachineCloseAction value = gpConverter->fromInternalString<MachineCloseAction>(strValue);
        if (value != MachineCloseAction_Invalid)
            result = static_cast<MachineCloseAction>(result | value);
    }
    /* Return result: */
    return result;
}

QList<IndicatorType> UIExtraDataManager::restrictedStatusBarIndicators(const QString &strID) const
{
    /* Prepare result: */
    QList<IndicatorType> result;
    /* Load restricted status-bar-indicators: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedStatusBarIndicators, strID))
    {
        IndicatorType value = gpConverter->fromInternalString<IndicatorType>(strValue);
        if (value != IndicatorType_Invalid)
            result << value;
    }
    /* Return result: */
    return result;
}

QList<GlobalSettingsPageType> UIExtraDataManager::restrictedGlobalSettingsPages() const
{
    /* Prepare result: */
    QList<GlobalSettingsPageType> result;
    /* Load restricted global-settings-pages: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedGlobalSettingsPages))
    {
        GlobalSettingsPageType value = gpConverter->fromInternalString<GlobalSettingsPageType>(strValue);
        if (value != GlobalSettingsPageType_Invalid)
            result << value;
    }
    /* Return result: */
    return result;
}

QList<MachineSettingsPageType> UIExtraDataManager::restrictedMachineSettingsPages(const QString &strID) const
{
    /* Prepare result: */
    QList<MachineSettingsPageType> result;
    /* Load restricted machine-settings-pages: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedMachineSettingsPages, strID))
    {
        MachineSettingsPageType value = gpConverter->fromInternalString<MachineSettingsPageType>(strValue);
        if (value != MachineSettingsPageType_Invalid)
            result << value;
    }
    /* Return result: */
    return result;
}

#ifndef Q_WS_MAC
QStringList UIExtraDataManager::machineWindowIconNames(const QString &strID) const
{
    return extraDataStringList(GUI_MachineWindowIcons, strID);
}
#endif /* !Q_WS_MAC */

GuruMeditationHandlerType UIExtraDataManager::guruMeditationHandlerType(const QString &strID) const
{
    return gpConverter->fromInternalString<GuruMeditationHandlerType>(extraDataString(GUI_GuruMeditationHandler, strID));
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
    /* Prepare global extra-data map: */
    prepareGlobalExtraDataMap();
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

void UIExtraDataManager::prepareGlobalExtraDataMap()
{
    /* Get CVirtualBox: */
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* Load global extra-data map: */
    foreach (const QString &strKey, vbox.GetExtraDataKeys())
        m_data[QString()][strKey] = vbox.GetExtraData(strKey);
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

void UIExtraDataManager::hotloadMachineExtraDataMap(const QString &strID) const
{
    /* Make sure it is not yet loaded: */
    AssertReturnVoid(!strID.isNull() && !m_data.contains(strID));

    /* Search for corresponding machine: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CMachine machine = vbox.FindMachine(strID);
    AssertReturnVoid(vbox.isOk() && !machine.isNull());

    /* Make sure at least empty map is created: */
    m_data[strID] = ExtraDataMap();

    /* Do not handle inaccessible machine: */
    if (!machine.GetAccessible())
        return;

    /* Load machine extra-data map: */
    foreach (const QString &strKey, machine.GetExtraDataKeys())
        m_data[strID][strKey] = machine.GetExtraData(strKey);
}

bool UIExtraDataManager::isFeatureAllowed(const QString &strKey, const QString &strID /* = QString() */) const
{
    /* Hot-load machine extra-data map if necessary: */
    if (!strID.isNull() && !m_data.contains(strID))
        hotloadMachineExtraDataMap(strID);

    /* Access corresponding map: */
    ExtraDataMap &data = m_data[strID];

    /* 'false' if value was not set: */
    if (!data.contains(strKey))
        return false;

    /* Check corresponding value: */
    const QString &strValue = data[strKey];
    return    strValue.compare("true", Qt::CaseInsensitive) == 0
           || strValue.compare("yes", Qt::CaseInsensitive) == 0
           || strValue.compare("on", Qt::CaseInsensitive) == 0
           || strValue == "1";
}

QString UIExtraDataManager::extraDataString(const QString &strKey, const QString &strID /* = QString() */) const
{
    /* Hot-load machine extra-data map if necessary: */
    if (!strID.isNull() && !m_data.contains(strID))
        hotloadMachineExtraDataMap(strID);

    /* Access corresponding map: */
    ExtraDataMap &data = m_data[strID];

    /* QString() if value was not set: */
    if (!data.contains(strKey))
        return QString();

    /* Returns corresponding value: */
    return data[strKey];
}

QStringList UIExtraDataManager::extraDataStringList(const QString &strKey, const QString &strID /* = QString() */) const
{
    /* Hot-load machine extra-data map if necessary: */
    if (!strID.isNull() && !m_data.contains(strID))
        hotloadMachineExtraDataMap(strID);

    /* Access corresponding map: */
    ExtraDataMap &data = m_data[strID];

    /* QStringList() if machine value was not set: */
    if (!data.contains(strKey))
        return QStringList();

    /* Returns corresponding value: */
    return data[strKey].split(',', QString::SkipEmptyParts);
}

#include "UIExtraDataManager.moc"

