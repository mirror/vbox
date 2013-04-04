/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMessageCenter class declaration
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMessageCenter_h__
#define __UIMessageCenter_h__

/* Qt includes: */
#include <QObject>
#include <QPointer>

/* GUI includes: */
#include "QIMessageBox.h"
#include "UIMediumDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CProgress.h"

/* Forward declarations: */
class UIMedium;
struct StorageSlot;
#ifdef VBOX_WITH_DRAG_AND_DROP
class CGuest;
#endif /* VBOX_WITH_DRAG_AND_DROP */

/* Possible message types: */
enum MessageType
{
    MessageType_Info = 1,
    MessageType_Question,
    MessageType_Warning,
    MessageType_Error,
    MessageType_Critical,
    MessageType_GuruMeditation
};
Q_DECLARE_METATYPE(MessageType);

/* Global message-center object: */
class UIMessageCenter: public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Interthreading stuff: */
    void sigToShowMessageBox(QWidget *pParent, MessageType type,
                             const QString &strMessage, const QString &strDetails,
                             int iButton1, int iButton2, int iButton3,
                             const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3,
                             const QString &strAutoConfirmId) const;

    /* Notifiers: Synchronization stuff: */
    void sigRemindAboutWrongColorDepth(ulong uRealBPP, ulong uWantedBPP);

public:

    enum
    {
        AutoConfirmed = 0x8000
    };

    /* API: Warning registration stuff: */
    bool warningShown(const QString &strWarningName) const;
    void setWarningShown(const QString &strWarningName, bool fWarningShown) const;

    /* API: Alert providing stuff: Main function: */
    int message(QWidget *pParent, MessageType type,
                const QString &strMessage,
                const QString &strDetails = QString(),
                const char *pcszAutoConfirmId = 0,
                int iButton1 = 0, int iButton2 = 0, int iButton3 = 0,
                const QString &strButtonText1 = QString(),
                const QString &strButtonText2 = QString(),
                const QString &strButtonText3 = QString()) const;

    /* API: Alert providing stuff: Wrapper to the main function,
     * Omits details: */
    int message(QWidget *pParent, MessageType type,
                const QString &strMessage,
                const char *pcszAutoConfirmId,
                int iButton1 = 0, int iButton2 = 0, int iButton3 = 0,
                const QString &strButtonText1 = QString(),
                const QString &strButtonText2 = QString(),
                const QString &strButtonText3 = QString()) const
    {
        return message(pParent, type, strMessage, QString(), pcszAutoConfirmId,
                       iButton1, iButton2, iButton3, strButtonText1, strButtonText2, strButtonText3);
    }

    /* API: Alert providing stuff: Wrapper to the main function,
     * Takes button type(s) as "Yes / No": */
    bool messageYesNo(QWidget *pParent, MessageType type,
                      const QString &strMessage,
                      const QString &strDetails = QString(),
                      const char *pcszAutoConfirmId = 0,
                      const QString &strYesButtonText = QString(),
                      const QString &strNoButtonText = QString()) const
    {
        return (message(pParent, type, strMessage, strDetails, pcszAutoConfirmId,
                        AlertButton_Yes | AlertButtonOption_Default,
                        AlertButton_No | AlertButtonOption_Escape,
                        0,
                        strYesButtonText, strNoButtonText, QString()) &
                AlertButtonMask) == AlertButton_Yes;
    }

    /* API: Alert providing stuff: Wrapper to the function above,
     * Omits details. Takes button type(s) as "Yes / No": */
    bool messageYesNo(QWidget *pParent, MessageType type,
                      const QString &strMessage,
                      const char *pcszAutoConfirmId,
                      const QString &strYesButtonText = QString(),
                      const QString &strNoButtonText = QString()) const
    {
        return messageYesNo(pParent, type, strMessage, QString(),
                            pcszAutoConfirmId, strYesButtonText, strNoButtonText);
    }

    /* API: Alert providing stuff: Wrapper to the main function,
     * Takes button type(s) as "Ok / Cancel": */
    bool messageOkCancel(QWidget *pParent, MessageType type,
                         const QString &strMessage,
                         const QString &strDetails = QString(),
                         const char *pcszAutoConfirmId = 0,
                         const QString &strOkButtonText = QString(),
                         const QString &strCancelButtonText = QString(),
                         bool fOkByDefault = true) const
    {
        int iOkButton = fOkByDefault ? AlertButton_Ok | AlertButtonOption_Default :
                                       AlertButton_Ok;
        int iCancelButton = fOkByDefault ? AlertButton_Cancel | AlertButtonOption_Escape :
                                           AlertButton_Cancel | AlertButtonOption_Escape | AlertButtonOption_Default;
        return (message(pParent, type, strMessage, strDetails, pcszAutoConfirmId,
                        iOkButton, iCancelButton, 0, strOkButtonText, strCancelButtonText, QString()) &
                AlertButtonMask) == AlertButton_Ok;
    }

    /* API: Alert providing stuff: Wrapper to the function above,
     * Omits details. Takes button type(s) as "Ok / Cancel": */
    bool messageOkCancel(QWidget *pParent, MessageType type,
                         const QString &strMessage,
                         const char *pcszAutoConfirmId,
                         const QString &strOkButtonText = QString(),
                         const QString &strCancelButtonText = QString(),
                         bool fOkByDefault = true) const
    {
        return messageOkCancel(pParent, type, strMessage, QString(), pcszAutoConfirmId,
                               strOkButtonText, strCancelButtonText, fOkByDefault);
    }

    /* API: Alert providing stuff: One more main function: */
    int messageWithOption(QWidget *pParent, MessageType type,
                          const QString &strMessage,
                          const QString &strOptionText,
                          bool fDefaultOptionValue = true,
                          const QString &strDetails = QString(),
                          int iButton1 = 0, int iButton2 = 0, int iButton3 = 0,
                          const QString &strButtonText1 = QString(),
                          const QString &strButtonText2 = QString(),
                          const QString &strButtonText3 = QString()) const;

    /* API: Progress-dialog stuff: */
    bool showModalProgressDialog(CProgress &progress, const QString &strTitle,
                                 const QString &strImage = "", QWidget *pParent = 0,
                                 int cMinDuration = 2000);

    /* API: Main window stuff: */
    QWidget* mainWindowShown() const;
    QWidget* mainMachineWindowShown() const;
    QWidget* networkManagerOrMainWindowShown() const;
    QWidget* networkManagerOrMainMachineWindowShown() const;

    /* API: Main (startup) warnings: */
#ifdef RT_OS_LINUX
    void warnAboutWrongUSBMounted() const;
#endif /* RT_OS_LINUX */
    void cannotStartSelector() const;
    void showBETAWarning() const;
    void showBEBWarning() const;

    /* API: COM startup warnings: */
    void cannotInitUserHome(const QString &strUserHome) const;
    void cannotInitCOM(HRESULT rc) const;
    void cannotCreateVirtualBox(const CVirtualBox &vbox) const;

    /* API: Global warnings: */
    void cannotFindLanguage(const QString &strLangId, const QString &strNlsPath) const;
    void cannotLoadLanguage(const QString &strLangFile) const;
    void cannotLoadGlobalConfig(const CVirtualBox &vbox, const QString &strError) const;
    void cannotSaveGlobalConfig(const CVirtualBox &vbox) const;
    void cannotFindMachineByName(const CVirtualBox &vbox, const QString &strName) const;
    void cannotFindMachineById(const CVirtualBox &vbox, const QString &strId) const;
    void cannotOpenSession(const CSession &session) const;
    void cannotOpenSession(const CMachine &machine) const;
    void cannotOpenSession(const CProgress &progress, const QString &strMachineName) const;
    void cannotGetMediaAccessibility(const UIMedium &medium) const;
    void cannotOpenURL(const QString &strUrl) const;

    /* API: Selector warnings: */
    void cannotOpenMachine(const CVirtualBox &vbox, const QString &strMachinePath) const;
    void cannotReregisterExistingMachine(const QString &strMachinePath, const QString &strMachineName) const;
    void cannotResolveCollisionAutomatically(const QString &strName, const QString &strGroupName) const;
    bool confirmAutomaticCollisionResolve(const QString &strName, const QString &strGroupName) const;
    void cannotSetGroups(const CMachine &machine) const;
    bool confirmMachineItemRemoval(const QStringList &names) const;
    int confirmMachineRemoval(const QList<CMachine> &machines) const;
    void cannotRemoveMachine(const CMachine &machine) const;
    void cannotRemoveMachine(const CMachine &machine, const CProgress &progress) const;
    bool remindAboutInaccessibleMedia() const;
    bool confirmDiscardSavedState(const QString &strNames) const;
    bool confirmResetMachine(const QString &strNames) const;
    bool confirmACPIShutdownMachine(const QString &strNames) const;
    bool confirmPowerOffMachine(const QString &strNames) const;
    void cannotPauseMachine(const CConsole &console) const;
    void cannotResumeMachine(const CConsole &console) const;
    void cannotDiscardSavedState(const CConsole &console) const;
    void cannotSaveMachineState(const CConsole &console);
    void cannotSaveMachineState(const CProgress &progress, const QString &strName);
    void cannotACPIShutdownMachine(const CConsole &console) const;
    void cannotPowerDownMachine(const CConsole &console) const;
    void cannotPowerDownMachine(const CProgress &progress, const QString &strName) const;

    /* API: Snapshot warnings: */
    int confirmSnapshotRestoring(const QString &strSnapshotName, bool fAlsoCreateNewSnapshot) const;
    bool confirmSnapshotRemoval(const QString &strSnapshotName) const;
    bool warnAboutSnapshotRemovalFreeSpace(const QString &strSnapshotName, const QString &strTargetImageName,
                                           const QString &strTargetImageMaxSize, const QString &strTargetFileSystemFree) const;
    void cannotTakeSnapshot(const CConsole &console, const QString &strMachineName, QWidget *pParent = 0) const;
    void cannotTakeSnapshot(const CProgress &progress, const QString &strMachineName, QWidget *pParent = 0) const;
    void cannotRestoreSnapshot(const CConsole &console, const QString &strSnapshotName, const QString &strMachineName, QWidget *pParent = 0) const;
    void cannotRestoreSnapshot(const CProgress &progress, const QString &strSnapshotName, const QString &strMachineName, QWidget *pParent = 0) const;
    void cannotRemoveSnapshot(const CConsole &console, const QString &strSnapshotName, const QString &strMachineName) const;
    void cannotRemoveSnapshot(const CProgress &progress, const QString &strSnapshotName, const QString &strMachineName) const;

    /* API: Settings warnings: */
    void cannotAccessUSB(const COMBaseWithEI &object) const;
    void cannotSetSystemProperties(const CSystemProperties &properties) const;
    void cannotSaveMachineSettings(const CMachine &machine, QWidget *pParent = 0) const;
    void warnAboutStateChange(QWidget *pParent = 0) const;
    bool confirmSettingsReloading(QWidget *pParent = 0) const;
    bool confirmDeletingHostInterface(const QString &strName, QWidget *pParent = 0) const;
    int askAboutHardDiskAttachmentCreation(const QString &strControllerName, QWidget *pParent = 0) const;
    int askAboutOpticalAttachmentCreation(const QString &strControllerName, QWidget *pParent = 0) const;
    int askAboutFloppyAttachmentCreation(const QString &strControllerName, QWidget *pParent = 0) const;
    int confirmRemovingOfLastDVDDevice(QWidget *pParent = 0) const;
    void warnAboutIncorrectPort(QWidget *pParent = 0) const;
    bool confirmCancelingPortForwardingDialog(QWidget *pParent = 0) const;

    /* API: Virtual Medium Manager warnings: */
    void cannotChangeMediumType(const CMedium &medium, KMediumType oldMediumType, KMediumType newMediumType, QWidget *pParent = 0) const;
    bool confirmMediumRelease(const UIMedium &medium, const QString &strUsage, QWidget *pParent = 0) const;
    bool confirmMediumRemoval(const UIMedium &medium, QWidget *pParent = 0) const;
    int confirmDeleteHardDiskStorage(const QString &strLocation, QWidget *pParent = 0) const;
    void cannotDeleteHardDiskStorage(const CMedium &medium, const QString &strLocation, QWidget *pParent = 0) const;
    void cannotDeleteHardDiskStorage(const CProgress &progress, const QString &strLocation, QWidget *pParent = 0) const;
    void cannotDetachDevice(const CMachine &machine, UIMediumType type, const QString &strLocation, const StorageSlot &storageSlot, QWidget *pParent = 0) const;
    int cannotRemountMedium(const CMachine &machine, const UIMedium &medium, bool fMount, bool fRetry, QWidget *pParent = 0) const;
    void cannotOpenMedium(const CVirtualBox &vbox, UIMediumType type, const QString &strLocation, QWidget *pParent = 0) const;
    void cannotCloseMedium(const UIMedium &medium, const COMResult &rc, QWidget *pParent = 0) const;

    /* API: Wizards warnings: */
    bool confirmHardDisklessMachine(QWidget *pParent = 0) const;
    void cannotCreateMachine(const CVirtualBox &vbox, QWidget *pParent = 0) const;
    void cannotRegisterMachine(const CVirtualBox &vbox, const QString &strMachineName, QWidget *pParent = 0) const;
    void cannotCreateClone(const CMachine &machine, QWidget *pParent = 0) const;
    void cannotCreateClone(const CProgress &progress, const QString &strMachineName, QWidget *pParent = 0) const;
    void cannotOverwriteHardDiskStorage(const QString &strLocation, QWidget *pParent = 0) const;
    void cannotCreateHardDiskStorage(const CVirtualBox &vbox, const QString &strLocation,QWidget *pParent = 0) const;
    void cannotCreateHardDiskStorage(const CMedium &medium, const QString &strLocation, QWidget *pParent = 0) const;
    void cannotCreateHardDiskStorage(const CProgress &progress, const QString &strLocation, QWidget *pParent = 0) const;
    void cannotRemoveMachineFolder(const QString &strFolderName, QWidget *pParent = 0) const;
    void cannotRewriteMachineFolder(const QString &strFolderName, QWidget *pParent = 0) const;
    void cannotCreateMachineFolder(const QString &strFolderName, QWidget *pParent = 0) const;
    void cannotImportAppliance(CAppliance &appliance, QWidget *pParent = 0) const;
    void cannotImportAppliance(const CProgress &progress, const QString &strPath, QWidget *pParent = 0) const;
    void cannotCheckFiles(const CProgress &progress, QWidget *pParent = 0) const;
    void cannotRemoveFiles(const CProgress &progress, QWidget *pParent = 0) const;
    bool confirmExportMachinesInSaveState(const QStringList &machineNames, QWidget *pParent = 0) const;
    void cannotExportAppliance(const CAppliance &appliance, QWidget *pParent = 0) const;
    void cannotExportAppliance(const CMachine &machine, const QString &strPath, QWidget *pParent = 0) const;
    void cannotExportAppliance(const CProgress &progress, const QString &strPath, QWidget *pParent = 0) const;
    void cannotFindSnapshotByName(const CMachine &machine, const QString &strMachine, QWidget *pParent = 0) const;

    /* API: Runtime UI warnings: */
    void showRuntimeError(const CConsole &console, bool fFatal, const QString &strErrorId, const QString &strErrorMsg) const;
    bool remindAboutGuruMeditation(const QString &strLogFolder);
    bool warnAboutVirtNotEnabled64BitsGuest(bool fHWVirtExSupported) const;
    bool warnAboutVirtNotEnabledGuestRequired(bool fHWVirtExSupported) const;
    bool cannotStartWithoutNetworkIf(const QString &strMachineName, const QString &strIfNames) const;
    void cannotStartMachine(const CConsole &console, const QString &strName) const;
    void cannotStartMachine(const CProgress &progress, const QString &strName) const;
    void cannotSendACPIToMachine() const;
    bool confirmInputCapture(bool &fAutoConfirmed) const;
    void remindAboutAutoCapture() const;
    void remindAboutMouseIntegration(bool fSupportsAbsolute) const;
    bool remindAboutPausedVMInput() const;
    bool confirmGoingFullscreen(const QString &strHotKey) const;
    bool confirmGoingSeamless(const QString &strHotKey) const;
    bool confirmGoingScale(const QString &strHotKey) const;
    bool cannotEnterFullscreenMode(ULONG uWidth, ULONG uHeight, ULONG uBpp, ULONG64 uMinVRAM) const;
    void cannotEnterSeamlessMode(ULONG uWidth, ULONG uHeight, ULONG uBpp, ULONG64 uMinVRAM) const;
    bool cannotSwitchScreenInFullscreen(quint64 uMinVRAM) const;
    void cannotSwitchScreenInSeamless(quint64 uMinVRAM) const;
    void cannotAttachUSBDevice(const CConsole &console, const QString &strDevice);
    void cannotAttachUSBDevice(const CVirtualBoxErrorInfo &error, const QString &strDevice, const QString &strMachineName);
    void cannotDetachUSBDevice(const CConsole &console, const QString &strDevice);
    void cannotDetachUSBDevice(const CVirtualBoxErrorInfo &error, const QString &strDevice, const QString &strMachineName);
    void remindAboutGuestAdditionsAreNotActive(QWidget *pParent = 0);

    /* API: Network management warnings: */
    bool confirmCancelingAllNetworkRequests();
    void showUpdateSuccess(const QString &strVersion, const QString &strLink);
    void showUpdateNotFound();
    bool askUserToDownloadExtensionPack(const QString &strExtPackName, const QString &strExtPackVersion, const QString &strVBoxVersion);

    /* API: Downloading warnings: */
    bool cannotFindGuestAdditions() const;
    bool confirmDownloadGuestAdditions(const QString &strUrl, qulonglong uSize) const;
    void cannotSaveGuestAdditions(const QString &strURL, const QString &strTarget) const;
    bool proposeMountGuestAdditions(const QString &strUrl, const QString &strSrc) const;
    void cannotMountGuestAdditions(const QString &strMachineName) const;
    void cannotUpdateGuestAdditions(const CProgress &progress) const;
    bool cannotFindUserManual(const QString &strMissedLocation) const;
    bool confirmDownloadUserManual(const QString &strURL, qulonglong uSize) const;
    void cannotSaveUserManual(const QString &strURL, const QString &strTarget) const;
    void warnAboutUserManualDownloaded(const QString &strURL, const QString &strTarget) const;
    bool warAboutOutdatedExtensionPack(const QString &strExtPackName, const QString &strExtPackVersion) const;
    bool confirmDownloadExtensionPack(const QString &strExtPackName, const QString &strURL, qulonglong uSize) const;
    void cannotSaveExtensionPack(const QString &strExtPackName, const QString &strFrom, const QString &strTo) const;
    bool proposeInstallExtentionPack(const QString &strExtPackName, const QString &strFrom, const QString &strTo) const;
    bool confirmInstallExtensionPack(const QString &strPackName, const QString &strPackVersion, const QString &strPackDescription, QWidget *pParent = 0) const;
    bool confirmReplaceExtensionPack(const QString &strPackName, const QString &strPackVersionNew, const QString &strPackVersionOld,
                                     const QString &strPackDescription, QWidget *pParent = 0) const;
    bool confirmRemoveExtensionPack(const QString &strPackName, QWidget *pParent = 0) const;
    void cannotOpenExtPack(const QString &strFilename, const CExtPackManager &extPackManager, QWidget *pParent = 0) const;
    void warnAboutBadExtPackFile(const QString &strFilename, const CExtPackFile &extPackFile, QWidget *pParent = 0) const;
    void cannotInstallExtPack(const CExtPackFile &extPackFile, const QString &strFilename, QWidget *pParent = 0) const;
    void cannotInstallExtPack(const CProgress &progress, const QString &strFilename, QWidget *pParent = 0) const;
    void cannotUninstallExtPack(const CExtPackManager &extPackManager, const QString &strPackName, QWidget *pParent = 0) const;
    void cannotUninstallExtPack(const CProgress &progress, const QString &strPackName, QWidget *pParent = 0) const;
    void warnAboutExtPackInstalled(const QString &strPackName, QWidget *pParent = 0) const;

    /* API: License-viewer warnings: */
    void cannotOpenLicenseFile(QWidget *pParent, const QString &strPath);

    /* API: File-dialog warnings: */
    bool askForOverridingFile(const QString &strPath, QWidget *pParent = 0);
    bool askForOverridingFiles(const QVector<QString> &strPaths, QWidget *pParent = 0);
    bool askForOverridingFileIfExists(const QString &strPath, QWidget *pParent = 0);
    bool askForOverridingFilesIfExists(const QVector<QString> &strPaths, QWidget *pParent = 0);

    static QString formatRC(HRESULT rc);
    static QString formatErrorInfo(const COMErrorInfo &info, HRESULT wrapperRC = S_OK);
    static QString formatErrorInfo(const CVirtualBoxErrorInfo &info);
    static QString formatErrorInfo(const COMBaseWithEI &wrapper);
    static QString formatErrorInfo(const COMResult &rc);

    /* Stuff supporting interthreading: */
    void cannotCreateHostInterface(const CHost &host, QWidget *pParent = 0);
    void cannotCreateHostInterface(const CProgress &progress, QWidget *pParent = 0);
    void cannotRemoveHostInterface(const CHost &host, const CHostNetworkInterface &iface, QWidget *pParent = 0);
    void cannotRemoveHostInterface(const CProgress &progress, const CHostNetworkInterface &iface, QWidget *pParent = 0);
    void cannotAttachDevice(const CMachine &machine, UIMediumType type,
                            const QString &strLocation, const StorageSlot &storageSlot, QWidget *pParent = 0);
    void cannotCreateSharedFolder(const CMachine &machine, const QString &strName,
                                  const QString &strPath, QWidget *pParent = 0);
    void cannotRemoveSharedFolder(const CMachine &machine, const QString &strName,
                                  const QString &strPath, QWidget *pParent = 0);
    void cannotCreateSharedFolder(const CConsole &console, const QString &strName,
                                  const QString &strPath, QWidget *pParent = 0);
    void cannotRemoveSharedFolder(const CConsole &console, const QString &strName,
                                  const QString &strPath, QWidget *pParent = 0);
#ifdef VBOX_WITH_DRAG_AND_DROP
    void cannotDropData(const CGuest &guest, QWidget *pParent = 0) const;
    void cannotDropData(const CProgress &progress, QWidget *pParent = 0) const;
#endif /* VBOX_WITH_DRAG_AND_DROP */
    void remindAboutWrongColorDepth(ulong uRealBPP, ulong uWantedBPP);
    void remindAboutUnsupportedUSB2(const QString &strExtPackName, QWidget *pParent = 0);

public slots:

    void sltShowHelpWebDialog();
    void sltShowHelpAboutDialog();
    void sltShowHelpHelpDialog();
    void sltResetSuppressedMessages();
    void sltShowUserManual(const QString &strLocation);

private slots:

    /* Handler: Interthreading stuff: */
    void sltShowMessageBox(QWidget *pParent, MessageType type,
                           const QString &strMessage, const QString &strDetails,
                           int iButton1, int iButton2, int iButton3,
                           const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3,
                           const QString &strAutoConfirmId) const;

    /* Handlers: Synchronization stuff: */
    void sltRemindAboutWrongColorDepth(ulong uRealBPP, ulong uWantedBPP);

private:

    /* Constructor: */
    UIMessageCenter();

    /* Instance stuff: */
    static UIMessageCenter &instance();
    friend UIMessageCenter &msgCenter();

    /* Helper: */
    static QString errorInfoToString(const COMErrorInfo &info, HRESULT wrapperRC = S_OK);

    /* Helper: Message-box stuff: */
    int showMessageBox(QWidget *pParent, MessageType type,
                       const QString &strMessage, const QString &strDetails,
                       int iButton1, int iButton2, int iButton3,
                       const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3,
                       const QString &strAutoConfirmId) const;

    /* Variables: */
    mutable QStringList m_warnings;
};

/* Shortcut to the static UIMessageCenter::instance() method, for convenience. */
inline UIMessageCenter &msgCenter() { return UIMessageCenter::instance(); }

#endif // __UIMessageCenter_h__

