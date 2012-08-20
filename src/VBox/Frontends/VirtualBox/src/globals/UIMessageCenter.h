/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMessageCenter class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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

/**
 * The UIMessageCenter class is a central place to handle all problem/error
 * situations that happen during application runtime and require the user's
 * attention.
 *
 * The role of this class is to describe the problem and/or the cause of the
 * error to the user and give him the opportunity to select an action (when
 * appropriate).
 *
 * Every problem situation has its own (correspondingly named) method in this
 * class that takes a list of arguments necessary to describe the situation and
 * to provide the appropriate actions. The method then returns the choice to the
 * caller.
 */
class UIMessageCenter: public QObject
{
    Q_OBJECT;

public:

    enum Type
    {
        Info = 1,
        Question,
        Warning,
        Error,
        Critical,
        GuruMeditation
    };

    enum
    {
        AutoConfirmed = 0x8000
    };

    bool isAnyWarningShown();
    bool isAlreadyShown(const QString &strGuardBlockName) const;
    void setShownStatus(const QString &strGuardBlockName);
    void clearShownStatus(const QString &strGuardBlockName);
    void closeAllWarnings();

    int message(QWidget *pParent, Type type, const QString &strMessage,
                const QString &strDetails = QString::null,
                const char *pcszAutoConfirmId = 0,
                int button1 = 0, int button2 = 0, int button3 = 0,
                const QString &strText1 = QString::null,
                const QString &strText2 = QString::null,
                const QString &strText3 = QString::null) const;

    int message(QWidget *pParent, Type type, const QString &strMessage,
                const char *pcszAutoConfirmId,
                int button1 = 0, int button2 = 0, int button3 = 0,
                const QString &strText1 = QString::null,
                const QString &strText2 = QString::null,
                const QString &strText3 = QString::null) const
    {
        return message(pParent, type, strMessage, QString::null, pcszAutoConfirmId,
                       button1, button2, button3, strText1, strText2, strText3);
    }

    bool messageYesNo(QWidget *pParent, Type type, const QString &strMessage,
                      const QString &strDetails = QString::null,
                      const char *pcszAutoConfirmId = 0,
                      const QString &strYesText = QString::null,
                      const QString &strNoText = QString::null) const
    {
        return(message(pParent, type, strMessage, strDetails, pcszAutoConfirmId,
                       QIMessageBox::Yes | QIMessageBox::Default,
                       QIMessageBox::No | QIMessageBox::Escape,
                       0,
                       strYesText, strNoText, QString::null) &
               QIMessageBox::ButtonMask) == QIMessageBox::Yes;
    }

    bool messageYesNo(QWidget *pParent, Type type, const QString &strMessage,
                      const char *pcszAutoConfirmId,
                      const QString &strYesText = QString::null,
                      const QString &strNoText = QString::null) const
    {
        return messageYesNo(pParent, type, strMessage, QString::null,
                            pcszAutoConfirmId, strYesText, strNoText);
    }

    bool messageOkCancel(QWidget *pParent, Type type, const QString &strMessage,
                         const QString &strDetails = QString::null,
                         const char *pcszAutoConfirmId = 0,
                         const QString &strOkText = QString::null,
                         const QString &strCancelText = QString::null) const
    {
        return(message(pParent, type, strMessage, strDetails, pcszAutoConfirmId,
                       QIMessageBox::Ok | QIMessageBox::Default,
                       QIMessageBox::Cancel | QIMessageBox::Escape,
                       0,
                       strOkText, strCancelText, QString::null) &
               QIMessageBox::ButtonMask) == QIMessageBox::Ok;
    }

    bool messageOkCancel(QWidget *pParent, Type type, const QString &strMessage,
                         const char *pcszAutoConfirmId,
                         const QString &strOkText = QString::null,
                         const QString &strCancelText = QString::null) const
    {
        return messageOkCancel(pParent, type, strMessage, QString::null,
                               pcszAutoConfirmId, strOkText, strCancelText);
    }

    int messageWithOption(QWidget *pParent,
                          Type type,
                          const QString &strMessage,
                          const QString &strOptionText,
                          bool fDefaultOptionValue = true,
                          const QString &strDetails = QString::null,
                          int iButton1 = 0,
                          int iButton2 = 0,
                          int iButton3 = 0,
                          const QString &strButtonName1 = QString::null,
                          const QString &strButtonName2 = QString::null,
                          const QString &strButtonName3 = QString::null) const;

    bool showModalProgressDialog(CProgress &progress, const QString &strTitle,
                                 const QString &strImage = "", QWidget *pParent = 0,
                                 bool fSheetOnDarwin = false, int cMinDuration = 2000);

    QWidget* mainWindowShown() const;
    QWidget* mainMachineWindowShown() const;
    QWidget* networkManagerOrMainWindowShown() const;
    QWidget* networkManagerOrMainMachineWindowShown() const;

    bool askForOverridingFile(const QString& strPath, QWidget *pParent  = NULL);
    bool askForOverridingFiles(const QVector<QString>& strPaths, QWidget *pParent = NULL);
    bool askForOverridingFileIfExists(const QString& strPath, QWidget *pParent = NULL);
    bool askForOverridingFilesIfExists(const QVector<QString>& strPaths, QWidget *pParent = NULL);

    void checkForMountedWrongUSB();

    void showBETAWarning();
    void showBEBWarning();

#ifdef Q_WS_X11
    void cannotFindLicenseFiles(const QString &strPath);
#endif
    void cannotOpenLicenseFile(QWidget *pParent, const QString &strPath);

    void cannotOpenURL(const QString &strUrl);

    void cannotFindLanguage(const QString &strLangId, const QString &strNlsPath);
    void cannotLoadLanguage(const QString &strLangFile);

    void cannotInitCOM(HRESULT rc);
    void cannotInitUserHome(const QString &strUserHome);
    void cannotCreateVirtualBox(const CVirtualBox &vbox);

    void cannotLoadGlobalConfig(const CVirtualBox &vbox, const QString &strError);
    void cannotSaveGlobalConfig(const CVirtualBox &vbox);
    void cannotSetSystemProperties(const CSystemProperties &props);

    void cannotAccessUSB(const COMBaseWithEI &object);

    void cannotCreateMachine(const CVirtualBox &vbox, QWidget *pParent = 0);
    void cannotCreateMachine(const CVirtualBox &vbox, const CMachine &machine, QWidget *pParent = 0);

    void cannotOpenMachine(QWidget *pParent, const QString &strMachinePath, const CVirtualBox &vbox);
    void cannotRegisterMachine(const CVirtualBox &vbox, const CMachine &machine, QWidget *pParent);
    void cannotReregisterMachine(QWidget *pParent, const QString &strMachinePath, const QString &strMachineName);
    void cannotApplyMachineSettings(const CMachine &machine, const COMResult &res);
    void cannotSaveMachineSettings(const CMachine &machine, QWidget *pParent = 0);
    void cannotLoadMachineSettings(const CMachine &machine, bool fStrict = true, QWidget *pParent = 0);

    bool confirmedSettingsReloading(QWidget *pParent);
    void warnAboutStateChange(QWidget *pParent);

    void cannotStartMachine(const CConsole &console);
    void cannotStartMachine(const CProgress &progress);
    void cannotPauseMachine(const CConsole &console);
    void cannotResumeMachine(const CConsole &console);
    void cannotACPIShutdownMachine(const CConsole &console);
    void cannotSaveMachineState(const CConsole &console);
    void cannotSaveMachineState(const CProgress &progress);
    void cannotCreateClone(const CMachine &machine, QWidget *pParent = 0);
    void cannotCreateClone(const CMachine &machine, const CProgress &progress, QWidget *pParent = 0);
    void cannotTakeSnapshot(const CConsole &console);
    void cannotTakeSnapshot(const CProgress &progress);
    void cannotStopMachine(const CConsole &console);
    void cannotStopMachine(const CProgress &progress);
    void cannotDeleteMachine(const CMachine &machine);
    void cannotDeleteMachine(const CMachine &machine, const CProgress &progress);
    void cannotDiscardSavedState(const CConsole &console);

    void cannotSendACPIToMachine();

    bool warnAboutVirtNotEnabled64BitsGuest(bool fHWVirtExSupported);
    bool warnAboutVirtNotEnabledGuestRequired(bool fHWVirtExSupported);

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

    void cannotFindMachineByName(const CVirtualBox &vbox, const QString &name);

    void cannotEnterSeamlessMode(ULONG uWidth, ULONG uHeight,
                                 ULONG uBpp, ULONG64 uMinVRAM);
    int cannotEnterFullscreenMode(ULONG uWidth, ULONG uHeight,
                                  ULONG uBpp, ULONG64 uMinVRAM);
    void cannotSwitchScreenInSeamless(quint64 uMinVRAM);
    int cannotSwitchScreenInFullscreen(quint64 uMinVRAM);
    int cannotEnterFullscreenMode();
    int cannotEnterSeamlessMode();

    void notifyAboutCollisionOnGroupRemovingCantBeResolved(const QString &strName, const QString &strGroupName);
    int askAboutCollisionOnGroupRemoving(const QString &strName, const QString &strGroupName);
    int confirmMachineItemRemoval(const QStringList &names);
    int confirmMachineDeletion(const QList<CMachine> &machines);
    bool confirmDiscardSavedState(const QString &strNames);

    void cannotChangeMediumType(QWidget *pParent, const CMedium &medium, KMediumType oldMediumType, KMediumType newMediumType);

    bool confirmReleaseMedium(QWidget *pParent, const UIMedium &aMedium,
                              const QString &strUsage);

    bool confirmRemoveMedium(QWidget *pParent, const UIMedium &aMedium);

    void sayCannotOverwriteHardDiskStorage(QWidget *pParent,
                                           const QString &strLocation);
    int confirmDeleteHardDiskStorage(QWidget *pParent,
                                     const QString &strLocation);
    void cannotDeleteHardDiskStorage(QWidget *pParent, const CMedium &medium,
                                     const CProgress &progress);

    int askAboutHardDiskAttachmentCreation(QWidget *pParent, const QString &strControllerName);
    int askAboutOpticalAttachmentCreation(QWidget *pParent, const QString &strControllerName);
    int askAboutFloppyAttachmentCreation(QWidget *pParent, const QString &strControllerName);

    int confirmRemovingOfLastDVDDevice() const;

    void cannotCreateHardDiskStorage(QWidget *pParent, const CVirtualBox &vbox,
                                     const QString &strLocation,
                                     const CMedium &medium,
                                     const CProgress &progress);
    void cannotDetachDevice(QWidget *pParent, const CMachine &machine,
                            UIMediumType type, const QString &strLocation, const StorageSlot &storageSlot);

    int cannotRemountMedium(QWidget *pParent, const CMachine &machine, const UIMedium &aMedium, bool fMount, bool fRetry);
    void cannotOpenMedium(QWidget *pParent, const CVirtualBox &vbox,
                          UIMediumType type, const QString &strLocation);
    void cannotCloseMedium(QWidget *pParent, const UIMedium &aMedium,
                           const COMResult &rc);

    void cannotOpenSession(const CSession &session);
    void cannotOpenSession(const CVirtualBox &vbox, const CMachine &machine,
                           const CProgress &progress = CProgress());

    void cannotGetMediaAccessibility(const UIMedium &aMedium);

    int confirmDeletingHostInterface(const QString &strName, QWidget *pParent = 0);

    void cannotAttachUSBDevice(const CConsole &console, const QString &device);
    void cannotAttachUSBDevice(const CConsole &console, const QString &device,
                               const CVirtualBoxErrorInfo &error);
    void cannotDetachUSBDevice(const CConsole &console, const QString &device);
    void cannotDetachUSBDevice(const CConsole &console, const QString &device,
                               const CVirtualBoxErrorInfo &error);

    void remindAboutGuestAdditionsAreNotActive(QWidget *pParent);
    bool cannotFindGuestAdditions();
    void cannotMountGuestAdditions(const QString &strMachineName);
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

    void cannotConnectRegister(QWidget *pParent,
                               const QString &strUrl,
                               const QString &strReason);
    void showRegisterResult(QWidget *pParent,
                            const QString &strResult);

    void showUpdateSuccess(const QString &strVersion, const QString &strLink);
    void showUpdateNotFound();

    bool askAboutCancelAllNetworkRequest(QWidget *pParent);

    bool confirmInputCapture(bool *pfAutoConfirmed = NULL);
    void remindAboutAutoCapture();
    void remindAboutMouseIntegration(bool fSupportsAbsolute);
    bool remindAboutPausedVMInput();

    int warnAboutSettingsAutoConversion(const QString &strFileList, bool fAfterRefresh);

    bool remindAboutInaccessibleMedia();

    bool confirmGoingFullscreen(const QString &strHotKey);
    bool confirmGoingSeamless(const QString &strHotKey);
    bool confirmGoingScale(const QString &strHotKey);

    bool remindAboutGuruMeditation(const CConsole &console,
                                   const QString &strLogFolder);

    bool confirmVMReset(const QString &strNames);
    bool confirmVMACPIShutdown(const QString &strNames);
    bool confirmVMPowerOff(const QString &strNames);

    void warnAboutCannotRemoveMachineFolder(QWidget *pParent, const QString &strFolderName);
    void warnAboutCannotRewriteMachineFolder(QWidget *pParent, const QString &strFolderName);
    void warnAboutCannotCreateMachineFolder(QWidget *pParent, const QString &strFolderName);
    bool confirmHardDisklessMachine(QWidget *pParent);

    void cannotRunInSelectorMode();

    void cannotImportAppliance(CAppliance *pAppliance, QWidget *pParent = NULL) const;
    void cannotImportAppliance(const CProgress &progress, CAppliance *pAppliance, QWidget *pParent = NULL) const;

    void cannotCheckFiles(const CProgress &progress, QWidget *pParent = NULL) const;
    void cannotRemoveFiles(const CProgress &progress, QWidget *pParent = NULL) const;

    bool confirmExportMachinesInSaveState(const QStringList &strMachineNames, QWidget *pParent = NULL) const;
    void cannotExportAppliance(CAppliance *pAppliance, QWidget *pParent = NULL) const;
    void cannotExportAppliance(const CMachine &machine, CAppliance *pAppliance, QWidget *pParent = NULL) const;
    void cannotExportAppliance(const CProgress &progress, CAppliance *pAppliance, QWidget *pParent = NULL) const;

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

    void warnAboutIncorrectPort(QWidget *pParent) const;
    bool confirmCancelingPortForwardingDialog(QWidget *pParent) const;

    void showRuntimeError(const CConsole &console, bool fFatal,
                          const QString &strErrorId,
                          const QString &strErrorMsg) const;

    static QString mediumToAccusative(UIMediumType type, bool fIsHostDrive = false);

    static QString formatRC(HRESULT rc);

    static QString formatErrorInfo(const COMErrorInfo &info,
                                   HRESULT wrapperRC = S_OK);

    static QString formatErrorInfo(const CVirtualBoxErrorInfo &info)
    {
        return formatErrorInfo(COMErrorInfo(info));
    }

    static QString formatErrorInfo(const COMBaseWithEI &wrapper)
    {
        Assert(wrapper.lastRC() != S_OK);
        return formatErrorInfo(wrapper.errorInfo(), wrapper.lastRC());
    }

    static QString formatErrorInfo(const COMResult &rc)
    {
        Assert(rc.rc() != S_OK);
        return formatErrorInfo(rc.errorInfo(), rc.rc());
    }

    void showGenericError(COMBaseWithEI *object, QWidget *pParent = 0);

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

signals:

    void sigToCloseAllWarnings();

    /* Stuff supporting interthreading: */
    void sigCannotCreateHostInterface(const CHost &host, QWidget *pParent);
    void sigCannotCreateHostInterface(const CProgress &progress, QWidget *pParent);
    void sigCannotRemoveHostInterface(const CHost &host, const CHostNetworkInterface &iface, QWidget *pParent);
    void sigCannotRemoveHostInterface(const CProgress &progress, const CHostNetworkInterface &iface, QWidget *pParent);
    void sigCannotAttachDevice(const CMachine &machine, UIMediumType type,
                               const QString &strLocation, const StorageSlot &storageSlot, QWidget *pParent);
    void sigCannotCreateSharedFolder(const CMachine &machine, const QString &strName,
                                     const QString &strPath, QWidget *pParent);
    void sigCannotRemoveSharedFolder(const CMachine &machine, const QString &strName,
                                     const QString &strPath, QWidget *pParent);
    void sigCannotCreateSharedFolder(const CConsole &console, const QString &strName,
                                     const QString &strPath, QWidget *pParent);
    void sigCannotRemoveSharedFolder(const CConsole &console, const QString &strName,
                                     const QString &strPath, QWidget *pParent);
    void sigRemindAboutWrongColorDepth(ulong uRealBPP, ulong uWantedBPP);
    void sigRemindAboutUnsupportedUSB2(const QString &strExtPackName, QWidget *pParent);

public slots:

    void sltShowHelpWebDialog();
    void sltShowHelpAboutDialog();
    void sltShowHelpHelpDialog();
    void sltResetSuppressedMessages();
    void sltShowUserManual(const QString &strLocation);

private slots:

    /* Stuff supporting interthreading: */
    void sltCannotCreateHostInterface(const CHost &host, QWidget *pParent);
    void sltCannotCreateHostInterface(const CProgress &progress, QWidget *pParent);
    void sltCannotRemoveHostInterface(const CHost &host, const CHostNetworkInterface &iface, QWidget *pParent);
    void sltCannotRemoveHostInterface(const CProgress &progress, const CHostNetworkInterface &iface, QWidget *pParent);
    void sltCannotAttachDevice(const CMachine &machine, UIMediumType type,
                               const QString &strLocation, const StorageSlot &storageSlot, QWidget *pParent);
    void sltCannotCreateSharedFolder(const CMachine &machine, const QString &strName,
                                     const QString &strPath, QWidget *pParent);
    void sltCannotRemoveSharedFolder(const CMachine &machine, const QString &strName,
                                     const QString &strPath, QWidget *pParent);
    void sltCannotCreateSharedFolder(const CConsole &console, const QString &strName,
                                     const QString &strPath, QWidget *pParent);
    void sltCannotRemoveSharedFolder(const CConsole &console, const QString &strName,
                                     const QString &strPath, QWidget *pParent);
    void sltRemindAboutWrongColorDepth(ulong uRealBPP, ulong uWantedBPP);
    void sltRemindAboutUnsupportedUSB2(const QString &strExtPackName, QWidget *pParent);

private:

    UIMessageCenter();

    static UIMessageCenter &instance();

    friend UIMessageCenter &msgCenter();

    static QString errorInfoToString(const COMErrorInfo &info,
                                     HRESULT wrapperRC = S_OK);

    QStringList m_strShownWarnings;
    mutable QList<QPointer<QIMessageBox> > m_warnings;
};

/* Shortcut to the static UIMessageCenter::instance() method, for convenience. */
inline UIMessageCenter &msgCenter() { return UIMessageCenter::instance(); }

#endif // __UIMessageCenter_h__

