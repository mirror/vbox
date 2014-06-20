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
#include <QDesktopWidget>

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

/* Namespaces: */
using namespace UIExtraDataDefs;


/** QObject extension
  * notifying UIExtraDataManager whenever any of extra-data values changed. */
class UIExtraDataEventHandler : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about 'extra-data change' event: */
    void sigExtraDataChange(QString strMachineID, QString strKey, QString strValue);

public:

    /** Extra-data event-handler constructor. */
    UIExtraDataEventHandler(QObject *pParent);

public slots:

    /** Preprocess 'extra-data can change' event: */
    void sltPreprocessExtraDataCanChange(QString strMachineID, QString strKey, QString strValue, bool &fVeto, QString &strVetoReason);
    /** Preprocess 'extra-data change' event: */
    void sltPreprocessExtraDataChange(QString strMachineID, QString strKey, QString strValue);

private:

    /** Protects sltPreprocessExtraDataChange. */
    QMutex m_mutex;
};

UIExtraDataEventHandler::UIExtraDataEventHandler(QObject *pParent)
    : QObject(pParent)
{
}

void UIExtraDataEventHandler::sltPreprocessExtraDataCanChange(QString strMachineID, QString strKey, QString strValue, bool &fVeto, QString &strVetoReason)
{
    /* Preprocess global 'extra-data can change' event: */
    if (QUuid(strMachineID).isNull())
    {
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
            }
        }
    }
}

void UIExtraDataEventHandler::sltPreprocessExtraDataChange(QString strMachineID, QString strKey, QString strValue)
{
    /* Preprocess global 'extra-data change' event: */
    if (QUuid(strMachineID).isNull())
    {
        if (strKey.startsWith("GUI/"))
        {
            /* Apply global property: */
            m_mutex.lock();
            vboxGlobal().settings().setPublicProperty(strKey, strValue);
            m_mutex.unlock();
            AssertMsgReturnVoid(!!vboxGlobal().settings(), ("Failed to apply global property.\n"));
        }
    }

    /* Motify listener about 'extra-data change' event: */
    emit sigExtraDataChange(strMachineID, strKey, strValue);
}


/* static */
UIExtraDataManager *UIExtraDataManager::m_pInstance = 0;
QString UIExtraDataManager::m_sstrGlobalID = QUuid().toString().remove(QRegExp("[{}]"));

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
    : m_pHandler(0)
{
    /* Connect to static instance: */
    m_pInstance = this;
}

UIExtraDataManager::~UIExtraDataManager()
{
    /* Disconnect from static instance: */
    m_pInstance = 0;
}

QStringList UIExtraDataManager::suppressedMessages() const
{
    return extraDataStringList(GUI_SuppressMessages);
}

void UIExtraDataManager::setSuppressedMessages(const QStringList &list)
{
    setExtraDataStringList(GUI_SuppressMessages, list);
}

QStringList UIExtraDataManager::messagesWithInvertedOption() const
{
    return extraDataStringList(GUI_InvertMessageOption);
}

#if !defined(VBOX_BLEEDING_EDGE) && !defined(DEBUG)
QString UIExtraDataManager::preventBetaBuildWarningForVersion() const
{
    return extraDataString(GUI_PreventBetaWarning);
}
#endif /* !defined(VBOX_BLEEDING_EDGE) && !defined(DEBUG) */

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
bool UIExtraDataManager::applicationUpdateEnabled() const
{
    /* 'True' unless 'restriction' feature allowed: */
    return !isFeatureAllowed(GUI_PreventApplicationUpdate);
}

QString UIExtraDataManager::applicationUpdateData() const
{
    return extraDataString(GUI_UpdateDate);
}

void UIExtraDataManager::setApplicationUpdateData(const QString &strValue)
{
    setExtraDataString(GUI_UpdateDate, strValue);
}

qulonglong UIExtraDataManager::applicationUpdateCheckCounter() const
{
    /* Read subsequent update check counter value: */
    qulonglong uResult = 1;
    const QString strCheckCount = extraDataString(GUI_UpdateCheckCount);
    if (!strCheckCount.isEmpty())
    {
        bool ok = false;
        qulonglong uCheckCount = strCheckCount.toULongLong(&ok);
        if (ok) uResult = uCheckCount;
    }
    /* Return update check counter value: */
    return uResult;
}

void UIExtraDataManager::incrementApplicationUpdateCheckCounter()
{
    /* Increment update check counter value: */
    setExtraDataString(GUI_UpdateCheckCount, QString::number(applicationUpdateCheckCounter() + 1));
}
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

QList<GlobalSettingsPageType> UIExtraDataManager::restrictedGlobalSettingsPages() const
{
    /* Prepare result: */
    QList<GlobalSettingsPageType> result;
    /* Get restricted global-settings-pages: */
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
    /* Get restricted machine-settings-pages: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedMachineSettingsPages, strID))
    {
        MachineSettingsPageType value = gpConverter->fromInternalString<MachineSettingsPageType>(strValue);
        if (value != MachineSettingsPageType_Invalid)
            result << value;
    }
    /* Return result: */
    return result;
}

QStringList UIExtraDataManager::shortcutOverrides(const QString &strPoolExtraDataID) const
{
    if (strPoolExtraDataID == GUI_Input_SelectorShortcuts)
        return extraDataStringList(GUI_Input_SelectorShortcuts);
    if (strPoolExtraDataID == GUI_Input_MachineShortcuts)
        return extraDataStringList(GUI_Input_MachineShortcuts);
    return QStringList();
}

QString UIExtraDataManager::recentFolderForHardDrives() const
{
    return extraDataString(GUI_RecentFolderHD);
}

QString UIExtraDataManager::recentFolderForOpticalDisks() const
{
    return extraDataString(GUI_RecentFolderCD);
}

QString UIExtraDataManager::recentFolderForFloppyDisks() const
{
    return extraDataString(GUI_RecentFolderFD);
}

void UIExtraDataManager::setRecentFolderForHardDrives(const QString &strValue)
{
    setExtraDataString(GUI_RecentFolderHD, strValue);
}

void UIExtraDataManager::setRecentFolderForOpticalDisks(const QString &strValue)
{
    setExtraDataString(GUI_RecentFolderCD, strValue);
}

void UIExtraDataManager::setRecentFolderForFloppyDisks(const QString &strValue)
{
    setExtraDataString(GUI_RecentFolderFD, strValue);
}

QStringList UIExtraDataManager::recentListOfHardDrives() const
{
    return extraDataStringList(GUI_RecentListHD);
}

QStringList UIExtraDataManager::recentListOfOpticalDisks() const
{
    return extraDataStringList(GUI_RecentListCD);
}

QStringList UIExtraDataManager::recentListOfFloppyDisks() const
{
    return extraDataStringList(GUI_RecentListFD);
}

void UIExtraDataManager::setRecentListOfHardDrives(const QStringList &value)
{
    setExtraDataStringList(GUI_RecentListHD, value);
}

void UIExtraDataManager::setRecentListOfOpticalDisks(const QStringList &value)
{
    setExtraDataStringList(GUI_RecentListCD, value);
}

void UIExtraDataManager::setRecentListOfFloppyDisks(const QStringList &value)
{
    setExtraDataStringList(GUI_RecentListFD, value);
}

QRect UIExtraDataManager::selectorWindowGeometry(QWidget *pWidget) const
{
    /* Get corresponding extra-data: */
    const QStringList data = extraDataStringList(GUI_LastSelectorWindowPosition);

    /* Parse loaded data: */
    int iX = 0, iY = 0, iW = 0, iH = 0;
    bool fOk = data.size() >= 4;
    do
    {
        if (!fOk) break;
        iX = data[0].toInt(&fOk);
        if (!fOk) break;
        iY = data[1].toInt(&fOk);
        if (!fOk) break;
        iW = data[2].toInt(&fOk);
        if (!fOk) break;
        iH = data[3].toInt(&fOk);
    }
    while (0);

    /* Use geometry (loaded or default): */
    QRect geometry = fOk ? QRect(iX, iY, iW, iH) : QRect(0, 0, 770, 550);

    /* Take widget into account: */
    if (pWidget)
        geometry.setSize(geometry.size().expandedTo(pWidget->minimumSizeHint()));

    /* Get screen-geometry [of screen with point (iX, iY) if possible]: */
    const QRect screenGeometry = fOk ? QApplication::desktop()->availableGeometry(QPoint(iX, iY)) :
                                       QApplication::desktop()->availableGeometry();

    /* Make sure resulting geometry is within current bounds: */
    geometry = geometry.intersected(screenGeometry);

    /* Move default-geometry to screen-geometry' center: */
    if (!fOk)
        geometry.moveCenter(screenGeometry.center());

    /* Return result: */
    return geometry;
}

bool UIExtraDataManager::selectorWindowShouldBeMaximized() const
{
    /* Get corresponding extra-data: */
    const QStringList data = extraDataStringList(GUI_LastSelectorWindowPosition);

    /* Make sure 5th item has required value: */
    return data.size() == 5 && data[4] == GUI_Geometry_State_Max;
}

void UIExtraDataManager::setSelectorWindowGeometry(const QRect &geometry, bool fMaximized)
{
    /* Serialize passed values: */
    QStringList data;
    data << QString::number(geometry.x());
    data << QString::number(geometry.y());
    data << QString::number(geometry.width());
    data << QString::number(geometry.height());
    if (fMaximized)
        data << GUI_Geometry_State_Max;

    /* Re-cache corresponding extra-data: */
    setExtraDataStringList(GUI_LastSelectorWindowPosition, data);
}

QList<int> UIExtraDataManager::selectorWindowSplitterHints() const
{
    /* Get corresponding extra-data: */
    const QStringList data = extraDataStringList(GUI_SplitterSizes);

    /* Parse loaded data: */
    QList<int> hints;
    hints << (data.size() > 0 ? data[0].toInt() : 0);
    hints << (data.size() > 1 ? data[1].toInt() : 0);

    /* Return hints: */
    return hints;
}

void UIExtraDataManager::setSelectorWindowSplitterHints(const QList<int> &hints)
{
    /* Parse passed hints: */
    QStringList data;
    data << (hints.size() > 0 ? QString::number(hints[0]) : QString());
    data << (hints.size() > 1 ? QString::number(hints[1]) : QString());

    /* Re-cache corresponding extra-data: */
    setExtraDataStringList(GUI_SplitterSizes, data);
}

bool UIExtraDataManager::selectorWindowToolBarVisible() const
{
    /* 'True' unless feature restricted: */
    return !isFeatureRestricted(GUI_Toolbar);
}

void UIExtraDataManager::setSelectorWindowToolBarVisible(bool fVisible)
{
    /* 'False' if feature restricted, null-string otherwise: */
    setExtraDataString(GUI_Toolbar, toFeatureRestricted(!fVisible));
}

bool UIExtraDataManager::selectorWindowStatusBarVisible() const
{
    /* 'True' unless feature restricted: */
    return !isFeatureRestricted(GUI_Statusbar);
}

void UIExtraDataManager::setSelectorWindowStatusBarVisible(bool fVisible)
{
    /* 'False' if feature restricted, null-string otherwise: */
    setExtraDataString(GUI_Statusbar, toFeatureRestricted(!fVisible));
}

void UIExtraDataManager::clearSelectorWindowGroupsDefinitions()
{
    /* Read-only access global extra-data map: */
    const ExtraDataMap &data = m_data[m_sstrGlobalID];
    /* Wipe-out each the group definition record: */
    foreach (const QString &strKey, data.keys())
        if (strKey.startsWith(GUI_GroupDefinitions))
            setExtraDataString(strKey, QString());
}

QStringList UIExtraDataManager::selectorWindowGroupsDefinitions(const QString &strGroupID) const
{
    return extraDataStringList(GUI_GroupDefinitions + strGroupID);
}

void UIExtraDataManager::setSelectorWindowGroupsDefinitions(const QString &strGroupID, const QStringList &definitions)
{
    setExtraDataStringList(GUI_GroupDefinitions + strGroupID, definitions);
}

QString UIExtraDataManager::selectorWindowLastItemChosen() const
{
    return extraDataString(GUI_LastItemSelected);
}

void UIExtraDataManager::setSelectorWindowLastItemChosen(const QString &strItemID)
{
    setExtraDataString(GUI_LastItemSelected, strItemID);
}

QMap<DetailsElementType, bool> UIExtraDataManager::selectorWindowDetailsElements()
{
    /* Get corresponding extra-data: */
    const QStringList data = extraDataStringList(GUI_DetailsPageBoxes);

    /* Desearialize passed elements: */
    QMap<DetailsElementType, bool> elements;
    foreach (QString strItem, data)
    {
        bool fOpened = true;
        if (strItem.endsWith("Closed", Qt::CaseInsensitive))
        {
            fOpened = false;
            strItem.remove("Closed");
        }
        DetailsElementType type = gpConverter->fromInternalString<DetailsElementType>(strItem);
        if (type != DetailsElementType_Invalid)
            elements[type] = fOpened;
    }

    /* Return elements: */
    return elements;
}

void UIExtraDataManager::setSelectorWindowDetailsElements(const QMap<DetailsElementType, bool> &elements)
{
    /* Prepare corresponding extra-data: */
    QStringList data;

    /* Searialize passed elements: */
    foreach (DetailsElementType type, elements.keys())
    {
        QString strValue = gpConverter->toInternalString(type);
        if (!elements[type])
            strValue += "Closed";
        data << strValue;
    }

    /* Re-cache corresponding extra-data: */
    setExtraDataStringList(GUI_DetailsPageBoxes, data);
}

PreviewUpdateIntervalType UIExtraDataManager::selectorWindowPreviewUpdateInterval() const
{
    return gpConverter->fromInternalString<PreviewUpdateIntervalType>(extraDataString(GUI_PreviewUpdate));
}

void UIExtraDataManager::setSelectorWindowPreviewUpdateInterval(PreviewUpdateIntervalType interval)
{
    setExtraDataString(GUI_PreviewUpdate, gpConverter->toInternalString(interval));
}

WizardMode UIExtraDataManager::modeForWizardType(WizardType type)
{
    /* Some wizard use only 'basic' mode: */
    if (type == WizardType_FirstRun)
        return WizardMode_Basic;
    /* Otherwise get mode from cached extra-data: */
    return extraDataStringList(GUI_HideDescriptionForWizards).contains(gpConverter->toInternalString(type))
           ? WizardMode_Expert : WizardMode_Basic;
}

void UIExtraDataManager::setModeForWizardType(WizardType type, WizardMode mode)
{
    /* Get wizard name: */
    const QString strWizardName = gpConverter->toInternalString(type);
    /* Get current value: */
    const QStringList oldValue = extraDataStringList(GUI_HideDescriptionForWizards);
    QStringList newValue = oldValue;
    /* Include wizard-name into expert-mode wizard list if necessary: */
    if (mode == WizardMode_Expert && !newValue.contains(strWizardName))
        newValue << strWizardName;
    /* Exclude wizard-name from expert-mode wizard list if necessary: */
    else if (mode == WizardMode_Basic && newValue.contains(strWizardName))
        newValue.removeAll(strWizardName);
    /* Update extra-data if necessary: */
    if (newValue != oldValue)
        setExtraDataStringList(GUI_HideDescriptionForWizards, newValue);
}

bool UIExtraDataManager::showMachineInSelectorChooser(const QString &strID) const
{
    /* 'True' unless 'restriction' feature allowed: */
    return !isFeatureAllowed(GUI_HideFromManager, strID);
}

bool UIExtraDataManager::showMachineInSelectorDetails(const QString &strID) const
{
    /* 'True' unless 'restriction' feature allowed: */
    return !isFeatureAllowed(GUI_HideDetails, strID);
}

bool UIExtraDataManager::machineReconfigurationEnabled(const QString &strID) const
{
    /* 'True' unless 'restriction' feature allowed: */
    return !isFeatureAllowed(GUI_PreventReconfiguration, strID);
}

bool UIExtraDataManager::machineSnapshotOperationsEnabled(const QString &strID) const
{
    /* 'True' unless 'restriction' feature allowed: */
    return !isFeatureAllowed(GUI_PreventSnapshotOperations, strID);
}

bool UIExtraDataManager::machineFirstTimeStarted(const QString &strID) const
{
    /* 'True' only if feature is allowed: */
    return isFeatureAllowed(GUI_FirstRun, strID);
}

void UIExtraDataManager::setMachineFirstTimeStarted(bool fFirstTimeStarted, const QString &strID)
{
    /* 'True' if feature allowed, null-string otherwise: */
    setExtraDataString(GUI_FirstRun, toFeatureAllowed(fFirstTimeStarted), strID);
}

#ifndef Q_WS_MAC
QStringList UIExtraDataManager::machineWindowIconNames(const QString &strID) const
{
    return extraDataStringList(GUI_MachineWindowIcons, strID);
}

QString UIExtraDataManager::machineWindowNamePostfix(const QString &strID) const
{
    return extraDataString(GUI_MachineWindowNamePostfix, strID);
}
#endif /* !Q_WS_MAC */

QRect UIExtraDataManager::machineWindowGeometry(UIVisualStateType visualStateType, ulong uScreenIndex, const QString &strID) const
{
    /* Choose corresponding key: */
    QString strKey;
    switch (visualStateType)
    {
        case UIVisualStateType_Normal: strKey = extraDataKeyPerScreen(GUI_LastNormalWindowPosition, uScreenIndex); break;
        case UIVisualStateType_Scale:  strKey = extraDataKeyPerScreen(GUI_LastScaleWindowPosition, uScreenIndex); break;
        default: AssertFailedReturn(QRect());
    }

    /* Get corresponding extra-data: */
    const QStringList data = extraDataStringList(strKey, strID);

    /* Parse loaded data: */
    int iX = 0, iY = 0, iW = 0, iH = 0;
    bool fOk = data.size() >= 4;
    do
    {
        if (!fOk) break;
        iX = data[0].toInt(&fOk);
        if (!fOk) break;
        iY = data[1].toInt(&fOk);
        if (!fOk) break;
        iW = data[2].toInt(&fOk);
        if (!fOk) break;
        iH = data[3].toInt(&fOk);
    }
    while (0);

    /* Return geometry (loaded or null): */
    return fOk ? QRect(iX, iY, iW, iH) : QRect();
}

bool UIExtraDataManager::machineWindowShouldBeMaximized(UIVisualStateType visualStateType, ulong uScreenIndex, const QString &strID) const
{
    /* Choose corresponding key: */
    QString strKey;
    switch (visualStateType)
    {
        case UIVisualStateType_Normal: strKey = extraDataKeyPerScreen(GUI_LastNormalWindowPosition, uScreenIndex); break;
        case UIVisualStateType_Scale:  strKey = extraDataKeyPerScreen(GUI_LastScaleWindowPosition, uScreenIndex); break;
        default: AssertFailedReturn(false);
    }

    /* Get corresponding extra-data: */
    const QStringList data = extraDataStringList(strKey, strID);

    /* Make sure 5th item has required value: */
    return data.size() == 5 && data[4] == GUI_Geometry_State_Max;
}

void UIExtraDataManager::setMachineWindowGeometry(UIVisualStateType visualStateType, ulong uScreenIndex, const QRect &geometry, bool fMaximized, const QString &strID)
{
    /* Choose corresponding key: */
    QString strKey;
    switch (visualStateType)
    {
        case UIVisualStateType_Normal: strKey = extraDataKeyPerScreen(GUI_LastNormalWindowPosition, uScreenIndex); break;
        case UIVisualStateType_Scale:  strKey = extraDataKeyPerScreen(GUI_LastScaleWindowPosition, uScreenIndex); break;
        default: AssertFailedReturnVoid();
    }

    /* Serialize passed values: */
    QStringList data;
    data << QString::number(geometry.x());
    data << QString::number(geometry.y());
    data << QString::number(geometry.width());
    data << QString::number(geometry.height());
    if (fMaximized)
        data << GUI_Geometry_State_Max;

    /* Re-cache corresponding extra-data: */
    setExtraDataStringList(strKey, data, strID);
}

RuntimeMenuType UIExtraDataManager::restrictedRuntimeMenuTypes(const QString &strID) const
{
    /* Prepare result: */
    RuntimeMenuType result = RuntimeMenuType_Invalid;
    /* Get restricted runtime-menu-types: */
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
    /* Get restricted runtime-application-menu action-types: */
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
    /* Get restricted runtime-machine-menu action-types: */
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
    /* Get restricted runtime-view-menu action-types: */
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
    /* Get restricted runtime-devices-menu action-types: */
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
    /* Get restricted runtime-debugger-menu action-types: */
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
    /* Get restricted runtime-help-menu action-types: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedRuntimeHelpMenuActions, strID))
    {
        RuntimeMenuHelpActionType value = gpConverter->fromInternalString<RuntimeMenuHelpActionType>(strValue);
        if (value != RuntimeMenuHelpActionType_Invalid)
            result = static_cast<RuntimeMenuHelpActionType>(result | value);
    }
    /* Return result: */
    return result;
}

UIVisualStateType UIExtraDataManager::restrictedVisualStates(const QString &strID) const
{
    /* Prepare result: */
    UIVisualStateType result = UIVisualStateType_Invalid;
    /* Get restricted visual-state-types: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedVisualStates, strID))
    {
        UIVisualStateType value = gpConverter->fromInternalString<UIVisualStateType>(strValue);
        if (value != UIVisualStateType_Invalid)
            result = static_cast<UIVisualStateType>(result | value);
    }
    /* Return result: */
    return result;
}

UIVisualStateType UIExtraDataManager::requestedVisualState(const QString &strID) const
{
    if (isFeatureAllowed(GUI_Fullscreen, strID)) return UIVisualStateType_Fullscreen;
    if (isFeatureAllowed(GUI_Seamless, strID)) return UIVisualStateType_Seamless;
    if (isFeatureAllowed(GUI_Scale, strID)) return UIVisualStateType_Scale;
    return UIVisualStateType_Normal;
}

void UIExtraDataManager::setRequestedVisualState(UIVisualStateType visualState, const QString &strID)
{
    setExtraDataString(GUI_Fullscreen, toFeatureAllowed(visualState == UIVisualStateType_Fullscreen), strID);
    setExtraDataString(GUI_Seamless, toFeatureAllowed(visualState == UIVisualStateType_Seamless), strID);
    setExtraDataString(GUI_Scale, toFeatureAllowed(visualState == UIVisualStateType_Scale), strID);
}

bool UIExtraDataManager::guestScreenAutoResizeEnabled(const QString &strID) const
{
    /* 'True' unless feature restricted: */
    return !isFeatureRestricted(GUI_AutoresizeGuest, strID);
}

void UIExtraDataManager::setGuestScreenAutoResizeEnabled(bool fEnabled, const QString &strID)
{
    /* 'False' if feature restricted, null-string otherwise: */
    setExtraDataString(GUI_AutoresizeGuest, toFeatureRestricted(!fEnabled), strID);
}

QSize UIExtraDataManager::lastGuestSizeHint(ulong uScreenIndex, const QString &strID) const
{
    /* Choose corresponding key: */
    const QString strKey = extraDataKeyPerScreen(GUI_LastGuestSizeHint, uScreenIndex);

    /* Get corresponding extra-data: */
    const QStringList data = extraDataStringList(strKey, strID);

    /* Parse loaded data: */
    int iW = 0, iH = 0;
    bool fOk = data.size() == 2;
    do
    {
        if (!fOk) break;
        iW = data[0].toInt(&fOk);
        if (!fOk) break;
        iH = data[1].toInt(&fOk);
    }
    while (0);

    /* Return size (loaded or invalid): */
    return fOk ? QSize(iW, iH) : QSize();
}

void UIExtraDataManager::setLastGuestSizeHint(ulong uScreenIndex, const QSize &sizeHint, const QString &strID)
{
    /* Choose corresponding key: */
    const QString strKey = extraDataKeyPerScreen(GUI_LastGuestSizeHint, uScreenIndex);

    /* Serialize passed values: */
    QStringList data;
    data << QString::number(sizeHint.width());
    data << QString::number(sizeHint.height());

    /* Re-cache corresponding extra-data: */
    setExtraDataStringList(strKey, data, strID);
}

bool UIExtraDataManager::wasLastGuestSizeHintForFullScreen(ulong uScreenIndex, const QString &strID) const
{
    /* Choose corresponding key: */
    const QString strKey = extraDataKeyPerScreen(GUI_LastGuestSizeHintWasFullscreen, uScreenIndex);

    /* 'True' only if feature is allowed: */
    return isFeatureAllowed(strKey, strID);
}

void UIExtraDataManager::markLastGuestSizeHintAsFullScreen(ulong uScreenIndex, bool fWas, const QString &strID)
{
    /* Choose corresponding key: */
    const QString strKey = extraDataKeyPerScreen(GUI_LastGuestSizeHintWasFullscreen, uScreenIndex);

    /* 'True' if feature allowed, null-string otherwise: */
    return setExtraDataString(strKey, toFeatureAllowed(fWas), strID);
}

int UIExtraDataManager::hostScreenForPassedGuestScreen(int iGuestScreenIndex, const QString &strID)
{
    /* Choose corresponding key: */
    const QString strKey = extraDataKeyPerScreen(GUI_VirtualScreenToHostScreen, iGuestScreenIndex, true);

    /* Get value and convert it to index: */
    const QString strValue = extraDataString(strKey, strID);
    bool fOk = false;
    const int iHostScreenIndex = strValue.toULong(&fOk);

    /* Return corresponding index: */
    return fOk ? iHostScreenIndex : -1;
}

void UIExtraDataManager::setHostScreenForPassedGuestScreen(int iGuestScreenIndex, int iHostScreenIndex, const QString &strID)
{
    /* Choose corresponding key: */
    const QString strKey = extraDataKeyPerScreen(GUI_VirtualScreenToHostScreen, iGuestScreenIndex, true);

    /* Save passed index under corresponding value: */
    setExtraDataString(strKey, iHostScreenIndex != -1 ? QString::number(iHostScreenIndex) : QString(), strID);
}

bool UIExtraDataManager::autoMountGuestScreensEnabled(const QString &strID) const
{
    /* Show only if 'allowed' flag is set: */
    return isFeatureAllowed(GUI_AutomountGuestScreens, strID);
}

#ifdef VBOX_WITH_VIDEOHWACCEL
bool UIExtraDataManager::useLinearStretch(const QString &strID) const
{
    /* 'True' unless feature restricted: */
    return !isFeatureRestricted(GUI_Accelerate2D_StretchLinear, strID);
}

bool UIExtraDataManager::usePixelFormatYV12(const QString &strID) const
{
    /* 'True' unless feature restricted: */
    return !isFeatureRestricted(GUI_Accelerate2D_PixformatYV12, strID);
}

bool UIExtraDataManager::usePixelFormatUYVY(const QString &strID) const
{
    /* 'True' unless feature restricted: */
    return !isFeatureRestricted(GUI_Accelerate2D_PixformatUYVY, strID);
}

bool UIExtraDataManager::usePixelFormatYUY2(const QString &strID) const
{
    /* 'True' unless feature restricted: */
    return !isFeatureRestricted(GUI_Accelerate2D_PixformatYUY2, strID);
}

bool UIExtraDataManager::usePixelFormatAYUV(const QString &strID) const
{
    /* 'True' unless feature restricted: */
    return !isFeatureRestricted(GUI_Accelerate2D_PixformatAYUV, strID);
}
#endif /* VBOX_WITH_VIDEOHWACCEL */

HiDPIOptimizationType UIExtraDataManager::hiDPIOptimizationType(const QString &strID) const
{
    return gpConverter->fromInternalString<HiDPIOptimizationType>(extraDataString(GUI_HiDPI_Optimization, strID));
}

bool UIExtraDataManager::miniToolbarEnabled(const QString &strID) const
{
    /* 'True' unless feature restricted: */
    return !isFeatureRestricted(GUI_ShowMiniToolBar, strID);
}

void UIExtraDataManager::setMiniToolbarEnabled(bool fEnabled, const QString &strID)
{
    /* 'False' if feature restricted, null-string otherwise: */
    setExtraDataString(GUI_ShowMiniToolBar, toFeatureRestricted(!fEnabled), strID);
}

bool UIExtraDataManager::autoHideMiniToolbar(const QString &strID) const
{
    /* 'True' unless feature restricted: */
    return !isFeatureRestricted(GUI_MiniToolBarAutoHide, strID);
}

void UIExtraDataManager::setAutoHideMiniToolbar(bool fAutoHide, const QString &strID)
{
    /* 'False' if feature restricted, null-string otherwise: */
    setExtraDataString(GUI_MiniToolBarAutoHide, toFeatureRestricted(!fAutoHide), strID);
}

Qt::AlignmentFlag UIExtraDataManager::miniToolbarAlignment(const QString &strID) const
{
    /* Return Qt::AlignBottom unless MiniToolbarAlignment_Top specified separately: */
    switch (gpConverter->fromInternalString<MiniToolbarAlignment>(extraDataString(GUI_MiniToolBarAlignment, strID)))
    {
        case MiniToolbarAlignment_Top: return Qt::AlignTop;
        default: break;
    }
    return Qt::AlignBottom;
}

void UIExtraDataManager::setMiniToolbarAlignment(Qt::AlignmentFlag alignment, const QString &strID)
{
    /* Remove record unless Qt::AlignTop specified separately: */
    switch (alignment)
    {
        case Qt::AlignTop: setExtraDataString(GUI_MiniToolBarAlignment, gpConverter->toInternalString(MiniToolbarAlignment_Top), strID); return;
        default: break;
    }
    setExtraDataString(GUI_MiniToolBarAlignment, QString(), strID);
}

QList<IndicatorType> UIExtraDataManager::restrictedStatusBarIndicators(const QString &strID) const
{
    /* Prepare result: */
    QList<IndicatorType> result;
    /* Get restricted status-bar-indicators: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedStatusBarIndicators, strID))
    {
        IndicatorType value = gpConverter->fromInternalString<IndicatorType>(strValue);
        if (value != IndicatorType_Invalid)
            result << value;
    }
    /* Return result: */
    return result;
}

#ifdef Q_WS_MAC
bool UIExtraDataManager::presentationModeEnabled(const QString &strID) const
{
    /* 'False' unless feature allowed: */
    return isFeatureAllowed(GUI_PresentationModeEnabled, strID);
}

bool UIExtraDataManager::realtimeDockIconUpdateEnabled(const QString &strID) const
{
    /* 'True' unless feature restricted: */
    return !isFeatureRestricted(GUI_RealtimeDockIconUpdateEnabled, strID);
}

void UIExtraDataManager::setRealtimeDockIconUpdateEnabled(bool fEnabled, const QString &strID)
{
    /* 'False' if feature restricted, null-string otherwise: */
    setExtraDataString(GUI_RealtimeDockIconUpdateEnabled, toFeatureRestricted(!fEnabled), strID);
}

int UIExtraDataManager::realtimeDockIconUpdateMonitor(const QString &strID) const
{
    return extraDataString(GUI_RealtimeDockIconUpdateMonitor, strID).toInt();
}

void UIExtraDataManager::setRealtimeDockIconUpdateMonitor(int iIndex, const QString &strID)
{
    setExtraDataString(GUI_RealtimeDockIconUpdateMonitor, iIndex ? QString::number(iIndex) : QString(), strID);
}
#endif /* Q_WS_MAC */

bool UIExtraDataManager::passCADtoGuest(const QString &strID) const
{
    /* 'False' unless feature allowed: */
    return isFeatureAllowed(GUI_PassCAD, strID);
}

GuruMeditationHandlerType UIExtraDataManager::guruMeditationHandlerType(const QString &strID) const
{
    return gpConverter->fromInternalString<GuruMeditationHandlerType>(extraDataString(GUI_GuruMeditationHandler, strID));
}

bool UIExtraDataManager::hidLedsSyncState(const QString &strID) const
{
    /* 'True' unless feature restricted: */
    return !isFeatureRestricted(GUI_HidLedsSync, strID);
}

QRect UIExtraDataManager::informationWindowGeometry(QWidget *pWidget, QWidget *pParentWidget, const QString &strID) const
{
    /* Get corresponding extra-data: */
    const QStringList data = extraDataStringList(GUI_InformationWindowGeometry, strID);

    /* Parse loaded data: */
    int iX = 0, iY = 0, iW = 0, iH = 0;
    bool fOk = data.size() >= 4;
    do
    {
        if (!fOk) break;
        iX = data[0].toInt(&fOk);
        if (!fOk) break;
        iY = data[1].toInt(&fOk);
        if (!fOk) break;
        iW = data[2].toInt(&fOk);
        if (!fOk) break;
        iH = data[3].toInt(&fOk);
    }
    while (0);

    /* Use geometry (loaded or default): */
    QRect geometry = fOk ? QRect(iX, iY, iW, iH) : QRect(0, 0, 600, 450);

    /* Take hint-widget into account: */
    if (pWidget)
        geometry.setSize(geometry.size().expandedTo(pWidget->minimumSizeHint()));

    /* Get screen-geometry [of screen with point (iX, iY) if possible]: */
    const QRect screenGeometry = fOk ? QApplication::desktop()->availableGeometry(QPoint(iX, iY)) :
                                       QApplication::desktop()->availableGeometry();

    /* Make sure resulting geometry is within current bounds: */
    geometry = geometry.intersected(screenGeometry);

    /* Move default-geometry to pParentWidget' geometry center: */
    if (!fOk && pParentWidget)
        geometry.moveCenter(pParentWidget->geometry().center());

    /* Return result: */
    return geometry;
}

bool UIExtraDataManager::informationWindowShouldBeMaximized(const QString &strID) const
{
    /* Get corresponding extra-data: */
    const QStringList data = extraDataStringList(GUI_InformationWindowGeometry, strID);

    /* Make sure 5th item has required value: */
    return data.size() == 5 && data[4] == GUI_Geometry_State_Max;
}

void UIExtraDataManager::setInformationWindowGeometry(const QRect &geometry, bool fMaximized, const QString &strID)
{
    /* Serialize passed values: */
    QStringList data;
    data << QString::number(geometry.x());
    data << QString::number(geometry.y());
    data << QString::number(geometry.width());
    data << QString::number(geometry.height());
    if (fMaximized)
        data << GUI_Geometry_State_Max;

    /* Re-cache corresponding extra-data: */
    setExtraDataStringList(GUI_InformationWindowGeometry, data, strID);
}

MachineCloseAction UIExtraDataManager::defaultMachineCloseAction(const QString &strID) const
{
    return gpConverter->fromInternalString<MachineCloseAction>(extraDataString(GUI_DefaultCloseAction, strID));
}

MachineCloseAction UIExtraDataManager::restrictedMachineCloseActions(const QString &strID) const
{
    /* Prepare result: */
    MachineCloseAction result = MachineCloseAction_Invalid;
    /* Get restricted machine-close-actions: */
    foreach (const QString &strValue, extraDataStringList(GUI_RestrictedCloseActions, strID))
    {
        MachineCloseAction value = gpConverter->fromInternalString<MachineCloseAction>(strValue);
        if (value != MachineCloseAction_Invalid)
            result = static_cast<MachineCloseAction>(result | value);
    }
    /* Return result: */
    return result;
}

MachineCloseAction UIExtraDataManager::lastMachineCloseAction(const QString &strID) const
{
    return gpConverter->fromInternalString<MachineCloseAction>(extraDataString(GUI_LastCloseAction, strID));
}

void UIExtraDataManager::setLastMachineCloseAction(MachineCloseAction machineCloseAction, const QString &strID)
{
    setExtraDataString(GUI_LastCloseAction, gpConverter->toInternalString(machineCloseAction), strID);
}

QString UIExtraDataManager::machineCloseHookScript(const QString &strID) const
{
    return extraDataString(GUI_CloseActionHook, strID);
}

#ifdef VBOX_WITH_DEBUGGER_GUI
QString UIExtraDataManager::debugFlagValue(const QString &strDebugFlagKey) const
{
    return extraDataString(strDebugFlagKey).toLower().trimmed();
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

void UIExtraDataManager::sltExtraDataChange(QString strMachineID, QString strKey, QString strValue)
{
    /* Re-cache value only if strMachineID known already: */
    if (m_data.contains(strMachineID))
        m_data[strMachineID][strKey] = strValue;

    /* Global extra-data 'change' event: */
    if (strMachineID == m_sstrGlobalID)
    {
        if (strKey.startsWith("GUI/"))
        {
            /* Language changed? */
            if (strKey == GUI_LanguageId)
                emit sigLanguageChange(extraDataString(strKey));
            /* Selector UI shortcut changed? */
            else if (strKey == GUI_Input_SelectorShortcuts && gActionPool->type() == UIActionPoolType_Selector)
                emit sigSelectorUIShortcutChange();
            /* Runtime UI shortcut changed? */
            else if (strKey == GUI_Input_MachineShortcuts && gActionPool->type() == UIActionPoolType_Runtime)
                emit sigRuntimeUIShortcutChange();
#ifdef Q_WS_MAC
            /* 'Presentation mode' status changed (allowed if not restricted)? */
            else if (strKey == GUI_PresentationModeEnabled)
                emit sigPresentationModeChange(!isFeatureRestricted(strKey));
#endif /* Q_WS_MAC */
        }
    }
    /* Make sure event came for the currently running VM: */
    else if (   vboxGlobal().isVMConsoleProcess()
             && strMachineID == vboxGlobal().managedVMUuid())
    {
        /* HID LEDs sync state changed (allowed if not restricted)? */
        if (strKey == GUI_HidLedsSync)
            emit sigHidLedsSyncStateChange(!isFeatureRestricted(strKey, strMachineID));
#ifdef Q_WS_MAC
        /* 'Dock icon' appearance changed (allowed if not restricted)? */
        else if (   strKey == GUI_RealtimeDockIconUpdateEnabled
                 || strKey == GUI_RealtimeDockIconUpdateMonitor)
            emit sigDockIconAppearanceChange(!isFeatureRestricted(strKey, strMachineID));
#endif /* Q_WS_MAC */
    }
}

void UIExtraDataManager::prepare()
{
    /* Prepare global extra-data map: */
    prepareGlobalExtraDataMap();
    /* Prepare extra-data event-handler: */
    prepareExtraDataEventHandler();
}

void UIExtraDataManager::prepareGlobalExtraDataMap()
{
    /* Get CVirtualBox: */
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* Make sure at least empty map is created: */
    m_data[m_sstrGlobalID] = ExtraDataMap();

    /* Load global extra-data map: */
    foreach (const QString &strKey, vbox.GetExtraDataKeys())
        m_data[m_sstrGlobalID][strKey] = vbox.GetExtraData(strKey);
}

void UIExtraDataManager::prepareExtraDataEventHandler()
{
    /* Create extra-data event-handler: */
    m_pHandler = new UIExtraDataEventHandler(this);
    /* Configure extra-data event-handler: */
    AssertPtrReturnVoid(m_pHandler);
    {
        /* Extra-data change signal: */
        connect(m_pHandler, SIGNAL(sigExtraDataChange(QString, QString, QString)),
                this, SLOT(sltExtraDataChange(QString, QString, QString)),
                Qt::QueuedConnection);

        /* Prepare Main event-listener: */
        prepareMainEventListener();
    }
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
            m_pHandler, SLOT(sltPreprocessExtraDataCanChange(QString, QString, QString, bool&, QString&)),
            Qt::DirectConnection);
    /* Use a direct connection to the helper class: */
    connect(pListener->getWrapped(), SIGNAL(sigExtraDataChange(QString, QString, QString)),
            m_pHandler, SLOT(sltPreprocessExtraDataChange(QString, QString, QString)),
            Qt::DirectConnection);
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
    /* Make sure it is valid ID: */
    AssertMsgReturnVoid(!strID.isNull() && strID != m_sstrGlobalID,
                        ("Invalid VM ID = {%s}\n", strID.toAscii().constData()));
    /* Which is not loaded yet: */
    AssertReturnVoid(!m_data.contains(strID));

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

bool UIExtraDataManager::isFeatureAllowed(const QString &strKey, const QString &strID /* = m_sstrGlobalID */) const
{
    /* Hot-load machine extra-data map if necessary: */
    if (strID != m_sstrGlobalID && !m_data.contains(strID))
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

bool UIExtraDataManager::isFeatureRestricted(const QString &strKey, const QString &strID /* = m_sstrGlobalID */) const
{
    /* Hot-load machine extra-data map if necessary: */
    if (strID != m_sstrGlobalID && !m_data.contains(strID))
        hotloadMachineExtraDataMap(strID);

    /* Access corresponding map: */
    ExtraDataMap &data = m_data[strID];

    /* 'false' if value was not set: */
    if (!data.contains(strKey))
        return false;

    /* Check corresponding value: */
    const QString &strValue = data[strKey];
    return    strValue.compare("false", Qt::CaseInsensitive) == 0
           || strValue.compare("no", Qt::CaseInsensitive) == 0
           || strValue.compare("off", Qt::CaseInsensitive) == 0
           || strValue == "0";
}

QString UIExtraDataManager::toFeatureAllowed(bool fAllowed)
{
    return fAllowed ? QString("true") : QString();
}

QString UIExtraDataManager::toFeatureRestricted(bool fRestricted)
{
    return fRestricted ? QString("false") : QString();
}

QString UIExtraDataManager::extraDataString(const QString &strKey, const QString &strID /* = m_sstrGlobalID */) const
{
    /* Hot-load machine extra-data map if necessary: */
    if (strID != m_sstrGlobalID && !m_data.contains(strID))
        hotloadMachineExtraDataMap(strID);

    /* Access corresponding map: */
    ExtraDataMap &data = m_data[strID];

    /* QString() if value was not set: */
    if (!data.contains(strKey))
        return QString();

    /* Returns corresponding value: */
    return data[strKey];
}

void UIExtraDataManager::setExtraDataString(const QString &strKey, const QString &strValue, const QString &strID /* = m_sstrGlobalID */)
{
    /* Hot-load machine extra-data map if necessary: */
    if (strID != m_sstrGlobalID && !m_data.contains(strID))
        hotloadMachineExtraDataMap(strID);

    /* Access corresponding map: */
    ExtraDataMap &data = m_data[strID];

    /* [Re]cache passed value: */
    data[strKey] = strValue;

    /* Global extra-data: */
    if (strID == m_sstrGlobalID)
    {
        /* Get global object: */
        CVirtualBox vbox = vboxGlobal().virtualBox();
        /* Update global extra-data: */
        vbox.SetExtraData(strKey, strValue);
    }
    /* Machine extra-data: */
    else
    {
        /* Search for corresponding machine: */
        CVirtualBox vbox = vboxGlobal().virtualBox();
        CMachine machine = vbox.FindMachine(strID);
        AssertReturnVoid(vbox.isOk() && !machine.isNull());
        /* Update machine extra-data: */
        machine.SetExtraData(strKey, strValue);
    }
}

QStringList UIExtraDataManager::extraDataStringList(const QString &strKey, const QString &strID /* = m_sstrGlobalID */) const
{
    /* Hot-load machine extra-data map if necessary: */
    if (strID != m_sstrGlobalID && !m_data.contains(strID))
        hotloadMachineExtraDataMap(strID);

    /* Access corresponding map: */
    ExtraDataMap &data = m_data[strID];

    /* QStringList() if machine value was not set: */
    if (!data.contains(strKey))
        return QStringList();

    /* Few old extra-data string-lists were separated with 'semicolon' symbol.
     * All new separated by 'comma'. We have to take that into account. */
    return data[strKey].split(QRegExp("[;,]"), QString::SkipEmptyParts);
}

void UIExtraDataManager::setExtraDataStringList(const QString &strKey, const QStringList &strValue, const QString &strID /* = m_sstrGlobalID */)
{
    /* Hot-load machine extra-data map if necessary: */
    if (strID != m_sstrGlobalID && !m_data.contains(strID))
        hotloadMachineExtraDataMap(strID);

    /* Access corresponding map: */
    ExtraDataMap &data = m_data[strID];

    /* [Re]cache passed value: */
    data[strKey] = strValue.join(",");

    /* Global extra-data: */
    if (strID == m_sstrGlobalID)
    {
        /* Get global object: */
        CVirtualBox vbox = vboxGlobal().virtualBox();
        /* Update global extra-data: */
        vbox.SetExtraDataStringList(strKey, strValue);
    }
    /* Machine extra-data: */
    else
    {
        /* Search for corresponding machine: */
        CVirtualBox vbox = vboxGlobal().virtualBox();
        CMachine machine = vbox.FindMachine(strID);
        AssertReturnVoid(vbox.isOk() && !machine.isNull());
        /* Update machine extra-data: */
        machine.SetExtraDataStringList(strKey, strValue);
    }
}

/* static */
QString UIExtraDataManager::extraDataKeyPerScreen(const QString &strBase, ulong uScreenIndex, bool fSameRuleForPrimary /* = false */)
{
    return fSameRuleForPrimary || uScreenIndex ? strBase + QString::number(uScreenIndex) : strBase;
}

#include "UIExtraDataManager.moc"

