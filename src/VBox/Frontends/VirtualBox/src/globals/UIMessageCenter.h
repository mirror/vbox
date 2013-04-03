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
    void sigRemindAboutUnsupportedUSB2(const QString &strExtPackName, QWidget *pParent);

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
    bool confirmVMReset(const QString &strNames) const;
    bool confirmVMACPIShutdown(const QString &strNames) const;
    bool confirmVMPowerOff(const QString &strNames) const;
    void cannotDiscardSavedState(const CConsole &console) const;
    void cannotStopMachine(const CConsole &console) const;

    /* API: Snapshot warnings: */
    int confirmSnapshotRestoring(const QString &strSnapshotName, bool fAlsoCreateNewSnapshot) const;
    bool confirmSnapshotRemoval(const QString &strSnapshotName) const;
    bool warnAboutSnapshotRemovalFreeSpace(const QString &strSnapshotName, const QString &strTargetImageName,
                                           const QString &strTargetImageMaxSize, const QString &strTargetFileSystemFree) const;
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
    void cannotChangeMediumType(const CMedium &medium, KMediumType oldMediumType, KMediumType newMediumType, QWidget *pParent = 0);
    bool confirmMediumRelease(const UIMedium &medium, const QString &strUsage, QWidget *pParent = 0);
    bool confirmMediumRemoval(const UIMedium &medium, QWidget *pParent = 0);
    int confirmDeleteHardDiskStorage(const QString &strLocation, QWidget *pParent = 0);
    void cannotDeleteHardDiskStorage(const CMedium &medium, const CProgress &progress, QWidget *pParent = 0);
    void cannotDetachDevice(const CMachine &machine, UIMediumType type, const QString &strLocation, const StorageSlot &storageSlot, QWidget *pParent = 0);
    int cannotRemountMedium(const CMachine &machine, const UIMedium &medium, bool fMount, bool fRetry, QWidget *pParent = 0);
    void cannotOpenMedium(const CVirtualBox &vbox, UIMediumType type, const QString &strLocation, QWidget *pParent = 0);
    void cannotCloseMedium(const UIMedium &medium, const COMResult &rc, QWidget *pParent = 0);

    /* API: Wizards warnings: */
    void cannotCreateMachine(const CVirtualBox &vbox, QWidget *pParent = 0);
    void cannotCreateMachine(const CVirtualBox &vbox, const CMachine &machine, QWidget *pParent = 0);
    void cannotRegisterMachine(const CVirtualBox &vbox, const CMachine &machine, QWidget *pParent);
    void cannotCreateClone(const CMachine &machine, QWidget *pParent = 0);
    void cannotCreateClone(const CMachine &machine, const CProgress &progress, QWidget *pParent = 0);
    void cannotOverwriteHardDiskStorage(QWidget *pParent, const QString &strLocation);
    void cannotCreateHardDiskStorage(QWidget *pParent, const CVirtualBox &vbox, const QString &strLocation,
                                     const CMedium &medium, const CProgress &progress);
    void warnAboutCannotRemoveMachineFolder(QWidget *pParent, const QString &strFolderName);
    void warnAboutCannotRewriteMachineFolder(QWidget *pParent, const QString &strFolderName);
    void warnAboutCannotCreateMachineFolder(QWidget *pParent, const QString &strFolderName);
    bool confirmHardDisklessMachine(QWidget *pParent);
    void cannotImportAppliance(CAppliance *pAppliance, QWidget *pParent = NULL) const;
    void cannotImportAppliance(const CProgress &progress, CAppliance *pAppliance, QWidget *pParent = NULL) const;
    void cannotCheckFiles(const CProgress &progress, QWidget *pParent = NULL) const;
    void cannotRemoveFiles(const CProgress &progress, QWidget *pParent = NULL) const;
    bool confirmExportMachinesInSaveState(const QStringList &strMachineNames, QWidget *pParent = NULL) const;
    void cannotExportAppliance(CAppliance *pAppliance, QWidget *pParent = NULL) const;
    void cannotExportAppliance(const CMachine &machine, CAppliance *pAppliance, QWidget *pParent = NULL) const;
    void cannotExportAppliance(const CProgress &progress, CAppliance *pAppliance, QWidget *pParent = NULL) const;
    void cannotFindSnapshotByName(QWidget *pParent, const CMachine &machine, const QString &strMachine) const;

    /* API: Runtime UI warnings: */
    void showRuntimeError(const CConsole &console, bool fFatal,
                          const QString &strErrorId,
                          const QString &strErrorMsg) const;
    bool warnAboutVirtNotEnabled64BitsGuest(bool fHWVirtExSupported);
    bool warnAboutVirtNotEnabledGuestRequired(bool fHWVirtExSupported);
    bool cannotStartWithoutNetworkIf(const QString &strMachineName, const QString &strIfNames);
    void cannotStartMachine(const CConsole &console);
    void cannotStartMachine(const CProgress &progress);
    void cannotPauseMachine(const CConsole &console);
    void cannotResumeMachine(const CConsole &console);
    void cannotACPIShutdownMachine(const CConsole &console);
    void cannotSaveMachineState(const CConsole &console);
    void cannotSaveMachineState(const CProgress &progress);
    void cannotTakeSnapshot(const CConsole &console);
    void cannotTakeSnapshot(const CProgress &progress);
    void cannotStopMachine(const CProgress &progress);
    void cannotSendACPIToMachine();
    bool confirmInputCapture(bool *pfAutoConfirmed = NULL);
    void remindAboutAutoCapture();
    void remindAboutMouseIntegration(bool fSupportsAbsolute);
    bool remindAboutPausedVMInput();
    void cannotEnterSeamlessMode(ULONG uWidth, ULONG uHeight,
                                 ULONG uBpp, ULONG64 uMinVRAM);
    int cannotEnterFullscreenMode(ULONG uWidth, ULONG uHeight,
                                  ULONG uBpp, ULONG64 uMinVRAM);
    void cannotSwitchScreenInSeamless(quint64 uMinVRAM);
    int cannotSwitchScreenInFullscreen(quint64 uMinVRAM);
    bool confirmGoingFullscreen(const QString &strHotKey);
    bool confirmGoingSeamless(const QString &strHotKey);
    bool confirmGoingScale(const QString &strHotKey);
    void cannotAttachUSBDevice(const CConsole &console, const QString &device);
    void cannotAttachUSBDevice(const CConsole &console, const QString &device,
                               const CVirtualBoxErrorInfo &error);
    void cannotDetachUSBDevice(const CConsole &console, const QString &device);
    void cannotDetachUSBDevice(const CConsole &console, const QString &device,
                               const CVirtualBoxErrorInfo &error);
    void remindAboutGuestAdditionsAreNotActive(QWidget *pParent);
    void cannotMountGuestAdditions(const QString &strMachineName);
    bool remindAboutGuruMeditation(const CConsole &console, const QString &strLogFolder);

    /* API: Network management warnings: */
    bool askAboutCancelAllNetworkRequest(QWidget *pParent);
    void showUpdateSuccess(const QString &strVersion, const QString &strLink);
    void showUpdateNotFound();

    /* API: Downloading warnings: */
    bool cannotFindGuestAdditions();
    bool confirmDownloadAdditions(const QString &strUrl, qulonglong uSize);
    bool confirmMountAdditions(const QString &strUrl, const QString &strSrc);
    void warnAboutAdditionsCantBeSaved(const QString &strTarget);
    bool askAboutUserManualDownload(const QString &strMissedLocation);
    bool confirmUserManualDownload(const QString &strURL, qulonglong uSize);
    void warnAboutUserManualDownloaded(const QString &strURL, const QString &strTarget);
    void warnAboutUserManualCantBeSaved(const QString &strURL, const QString &strTarget);
    bool proposeDownloadExtensionPack(const QString &strExtPackName, const QString &strExtPackVersion);
    bool requestUserDownloadExtensionPack(const QString &strExtPackName, const QString &strExtPackVersion, const QString &strVBoxVersion);
    bool confirmDownloadExtensionPack(const QString &strExtPackName, const QString &strURL, qulonglong uSize);
    bool proposeInstallExtentionPack(const QString &strExtPackName, const QString &strFrom, const QString &strTo);
    void warnAboutExtentionPackCantBeSaved(const QString &strExtPackName, const QString &strFrom, const QString &strTo);
    void cannotUpdateGuestAdditions(const CProgress &progress, QWidget *pParent /* = NULL */) const;
    void cannotOpenExtPack(const QString &strFilename, const CExtPackManager &extPackManager, QWidget *pParent);
    void badExtPackFile(const QString &strFilename, const CExtPackFile &extPackFile, QWidget *pParent);
    void cannotInstallExtPack(const QString &strFilename, const CExtPackFile &extPackFile, const CProgress &progress, QWidget *pParent);
    void cannotUninstallExtPack(const QString &strPackName, const CExtPackManager &extPackManager, const CProgress &progress, QWidget *pParent);
    bool confirmInstallingPackage(const QString &strPackName, const QString &strPackVersion, const QString &strPackDescription, QWidget *pParent);
    bool confirmReplacePackage(const QString &strPackName, const QString &strPackVersionNew, const QString &strPackVersionOld,
                               const QString &strPackDescription, QWidget *pParent);
    bool confirmRemovingPackage(const QString &strPackName, QWidget *pParent);
    void notifyAboutExtPackInstalled(const QString &strPackName, QWidget *pParent);

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
    void sltRemindAboutUnsupportedUSB2(const QString &strExtPackName, QWidget *pParent);

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

