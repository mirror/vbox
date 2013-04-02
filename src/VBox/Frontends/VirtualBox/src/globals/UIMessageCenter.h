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
    void setWarningShown(const QString &strWarningName, bool fWarningShown);

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
                        QIMessageBox::Yes | QIMessageBox::Default,
                        QIMessageBox::No | QIMessageBox::Escape,
                        0,
                        strYesButtonText, strNoButtonText, QString()) &
                QIMessageBox::ButtonMask) == QIMessageBox::Yes;
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
                         const QString &strCancelButtonText = QString()) const
    {
        return (message(pParent, type, strMessage, strDetails, pcszAutoConfirmId,
                        QIMessageBox::Ok | QIMessageBox::Default,
                        QIMessageBox::Cancel | QIMessageBox::Escape,
                        0,
                        strOkButtonText, strCancelButtonText, QString()) &
                QIMessageBox::ButtonMask) == QIMessageBox::Ok;
    }

    /* API: Alert providing stuff: Wrapper to the function above,
     * Omits details. Takes button type(s) as "Ok / Cancel": */
    bool messageOkCancel(QWidget *pParent, MessageType type,
                         const QString &strMessage,
                         const char *pcszAutoConfirmId,
                         const QString &strOkButtonText = QString(),
                         const QString &strCancelButtonText = QString()) const
    {
        return messageOkCancel(pParent, type, strMessage, QString(),
                               pcszAutoConfirmId, strOkButtonText, strCancelButtonText);
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
    void warnAboutWrongUSBMounted();
#endif /* RT_OS_LINUX */
    void cannotStartSelector();
    void showBETAWarning();
    void showBEBWarning();

    /* API: COM startup warnings: */
    void cannotInitUserHome(const QString &strUserHome);
    void cannotInitCOM(HRESULT rc);
    void cannotCreateVirtualBox(const CVirtualBox &vbox);

    /* API: Global warnings: */
    void cannotFindLanguage(const QString &strLangId, const QString &strNlsPath);
    void cannotLoadLanguage(const QString &strLangFile);
    void cannotLoadGlobalConfig(const CVirtualBox &vbox, const QString &strError);
    void cannotSaveGlobalConfig(const CVirtualBox &vbox);
    void cannotFindMachineByName(const CVirtualBox &vbox, const QString &strName);
    void cannotFindMachineById(const CVirtualBox &vbox, const QString &strId);
    void cannotOpenSession(const CSession &session);
    void cannotOpenSession(const CMachine &machine, const CProgress &progress = CProgress());
    void cannotGetMediaAccessibility(const UIMedium &medium);
    void cannotOpenURL(const QString &strUrl);

    /* API: Selector warnings: */
    void cannotOpenMachine(const CVirtualBox &vbox, const QString &strMachinePath);
    void cannotReregisterExistingMachine(const QString &strMachinePath, const QString &strMachineName);
    void cannotDeleteMachine(const CMachine &machine);
    void cannotDeleteMachine(const CMachine &machine, const CProgress &progress);
    void cannotDiscardSavedState(const CConsole &console);
    void notifyAboutCollisionOnGroupRemovingCantBeResolved(const QString &strName, const QString &strGroupName);
    int askAboutCollisionOnGroupRemoving(const QString &strName, const QString &strGroupName);
    int confirmMachineItemRemoval(const QStringList &names);
    int confirmMachineRemoval(const QList<CMachine> &machines);
    bool confirmDiscardSavedState(const QString &strNames);
    void cannotSetGroups(const CMachine &machine);
    bool remindAboutInaccessibleMedia();
    bool confirmVMReset(const QString &strNames);
    bool confirmVMACPIShutdown(const QString &strNames);
    bool confirmVMPowerOff(const QString &strNames);

    /* API: Snapshot warnings: */
    int askAboutSnapshotRestoring(const QString &strSnapshotName, bool fAlsoCreateNewSnapshot);
    bool askAboutSnapshotDeleting(const QString &strSnapshotName);
    bool askAboutSnapshotDeletingFreeSpace(const QString &strSnapshotName,
                                           const QString &strTargetImageName,
                                           const QString &strTargetImageMaxSize,
                                           const QString &strTargetFileSystemFree);
    void cannotRestoreSnapshot(const CConsole &console, const QString &strSnapshotName);
    void cannotRestoreSnapshot(const CProgress &progress, const QString &strSnapshotName);
    void cannotDeleteSnapshot(const CConsole &console, const QString &strSnapshotName);
    void cannotDeleteSnapshot(const CProgress &progress, const QString &strSnapshotName);
    void cannotFindSnapshotByName(QWidget *pParent, const CMachine &machine, const QString &strMachine) const;

    /* API: Settings warnings: */
    void cannotSetSystemProperties(const CSystemProperties &properties);
    void cannotAccessUSB(const COMBaseWithEI &object);
    void cannotLoadMachineSettings(const CMachine &machine, bool fStrict = true, QWidget *pParent = 0);
    void cannotSaveMachineSettings(const CMachine &machine, QWidget *pParent = 0);
    void warnAboutStateChange(QWidget *pParent);
    bool confirmSettingsReloading(QWidget *pParent);
    int askAboutHardDiskAttachmentCreation(QWidget *pParent, const QString &strControllerName);
    int askAboutOpticalAttachmentCreation(QWidget *pParent, const QString &strControllerName);
    int askAboutFloppyAttachmentCreation(QWidget *pParent, const QString &strControllerName);
    int confirmRemovingOfLastDVDDevice() const;
    int confirmDeletingHostInterface(const QString &strName, QWidget *pParent = 0);
    void warnAboutIncorrectPort(QWidget *pParent) const;
    bool confirmCancelingPortForwardingDialog(QWidget *pParent) const;

    /* API: Virtual Medium Manager warnings: */
    void cannotChangeMediumType(QWidget *pParent, const CMedium &medium, KMediumType oldMediumType, KMediumType newMediumType);
    bool confirmReleaseMedium(QWidget *pParent, const UIMedium &aMedium,
                              const QString &strUsage);
    bool confirmRemoveMedium(QWidget *pParent, const UIMedium &aMedium);
    int confirmDeleteHardDiskStorage(QWidget *pParent,
                                     const QString &strLocation);
    void cannotDeleteHardDiskStorage(QWidget *pParent, const CMedium &medium,
                                     const CProgress &progress);
    void cannotDetachDevice(QWidget *pParent, const CMachine &machine,
                            UIMediumType type, const QString &strLocation, const StorageSlot &storageSlot);
    int cannotRemountMedium(QWidget *pParent, const CMachine &machine, const UIMedium &aMedium, bool fMount, bool fRetry);
    void cannotOpenMedium(QWidget *pParent, const CVirtualBox &vbox,
                          UIMediumType type, const QString &strLocation);
    void cannotCloseMedium(QWidget *pParent, const UIMedium &aMedium,
                           const COMResult &rc);

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
    void cannotStopMachine(const CConsole &console);
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

    /* Helpers: */
    static QString mediumToAccusative(UIMediumType type, bool fIsHostDrive = false);

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
    QStringList m_warnings;
};

/* Shortcut to the static UIMessageCenter::instance() method, for convenience. */
inline UIMessageCenter &msgCenter() { return UIMessageCenter::instance(); }

#endif // __UIMessageCenter_h__

