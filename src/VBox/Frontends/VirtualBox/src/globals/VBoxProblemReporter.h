/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxProblemReporter class declaration
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxProblemReporter_h__
#define __VBoxProblemReporter_h__

/* Global includes */
#include <QObject>
#include <QPointer>

/* Local includes */
#include "COMDefs.h"
#include "QIMessageBox.h"

/* Forward declarations */
class VBoxMedium;
struct StorageSlot;

/**
 * The VBoxProblemReporter class is a central place to handle all problem/error
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
class VBoxProblemReporter : public QObject
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

    int message (QWidget *aParent, Type aType, const QString &aMessage,
                 const QString &aDetails = QString::null,
                 const char *aAutoConfirmId = 0,
                 int aButton1 = 0, int aButton2 = 0, int aButton3 = 0,
                 const QString &aText1 = QString::null,
                 const QString &aText2 = QString::null,
                 const QString &aText3 = QString::null) const;

    int message (QWidget *aParent, Type aType, const QString &aMessage,
                 const char *aAutoConfirmId,
                 int aButton1 = 0, int aButton2 = 0, int aButton3 = 0,
                 const QString &aText1 = QString::null,
                 const QString &aText2 = QString::null,
                 const QString &aText3 = QString::null) const
    {
        return message (aParent, aType, aMessage, QString::null, aAutoConfirmId,
                        aButton1, aButton2, aButton3, aText1, aText2, aText3);
    }

    bool messageYesNo (QWidget *aParent, Type aType, const QString &aMessage,
                       const QString &aDetails = QString::null,
                       const char *aAutoConfirmId = 0,
                       const QString &aYesText = QString::null,
                       const QString &aNoText = QString::null) const
    {
        return (message (aParent, aType, aMessage, aDetails, aAutoConfirmId,
                         QIMessageBox::Yes | QIMessageBox::Default,
                         QIMessageBox::No | QIMessageBox::Escape,
                         0,
                         aYesText, aNoText, QString::null) &
                QIMessageBox::ButtonMask) == QIMessageBox::Yes;
    }

    bool messageYesNo (QWidget *aParent, Type aType, const QString &aMessage,
                       const char *aAutoConfirmId,
                       const QString &aYesText = QString::null,
                       const QString &aNoText = QString::null) const
    {
        return messageYesNo (aParent, aType, aMessage, QString::null,
                             aAutoConfirmId, aYesText, aNoText);
    }

    bool messageOkCancel (QWidget *aParent, Type aType, const QString &aMessage,
                          const QString &aDetails = QString::null,
                          const char *aAutoConfirmId = 0,
                          const QString &aOkText = QString::null,
                          const QString &aCancelText = QString::null) const
    {
        return (message (aParent, aType, aMessage, aDetails, aAutoConfirmId,
                         QIMessageBox::Ok | QIMessageBox::Default,
                         QIMessageBox::Cancel | QIMessageBox::Escape,
                         0,
                         aOkText, aCancelText, QString::null) &
                QIMessageBox::ButtonMask) == QIMessageBox::Ok;
    }

    bool messageOkCancel (QWidget *aParent, Type aType, const QString &aMessage,
                          const char *aAutoConfirmId,
                          const QString &aOkText = QString::null,
                          const QString &aCancelText = QString::null) const
    {
        return messageOkCancel (aParent, aType, aMessage, QString::null,
                                aAutoConfirmId, aOkText, aCancelText);
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

    bool askForOverridingFile (const QString& aPath, QWidget *aParent  = NULL) const;
    bool askForOverridingFiles (const QVector<QString>& aPaths, QWidget *aParent = NULL) const;
    bool askForOverridingFileIfExists (const QString& path, QWidget *aParent = NULL) const;
    bool askForOverridingFilesIfExists (const QVector<QString>& aPaths, QWidget *aParent = NULL) const;

    void checkForMountedWrongUSB() const;

    void showBETAWarning();
    void showBEBWarning();

#ifdef Q_WS_X11
    void cannotFindLicenseFiles (const QString &aPath);
#endif
    void cannotOpenLicenseFile (QWidget *aParent, const QString &aPath);

    void cannotOpenURL (const QString &aURL);

    void cannotFindLanguage (const QString &aLangID, const QString &aNlsPath);
    void cannotLoadLanguage (const QString &aLangFile);

    void cannotInitCOM (HRESULT rc);
    void cannotCreateVirtualBox (const CVirtualBox &vbox);

    void cannotLoadGlobalConfig (const CVirtualBox &vbox, const QString &error);
    void cannotSaveGlobalConfig (const CVirtualBox &vbox);
    void cannotSetSystemProperties (const CSystemProperties &props);

    void cannotAccessUSB (const COMBaseWithEI &aObj);

    void cannotCreateMachine (const CVirtualBox &vbox,
                              QWidget *parent = 0);
    void cannotCreateMachine (const CVirtualBox &vbox, const CMachine &machine,
                              QWidget *parent = 0);

    void cannotOpenMachine(QWidget *pParent, const QString &strMachinePath, const CVirtualBox &vbox);
    void cannotRegisterMachine(const CVirtualBox &vbox, const CMachine &machine, QWidget *pParent);
    void cannotReregisterMachine(QWidget *pParent, const QString &strMachinePath, const QString &strMachineName);
    void cannotApplyMachineSettings (const CMachine &machine, const COMResult &res);
    void cannotSaveMachineSettings (const CMachine &machine,
                                    QWidget *parent = 0);
    void cannotLoadMachineSettings (const CMachine &machine,
                                    bool strict = true,
                                    QWidget *parent = 0);

    bool confirmedSettingsReloading(QWidget *pParent);
    void warnAboutStateChange(QWidget *pParent);

    void cannotStartMachine (const CConsole &console);
    void cannotStartMachine (const CProgress &progress);
    void cannotPauseMachine (const CConsole &console);
    void cannotResumeMachine (const CConsole &console);
    void cannotACPIShutdownMachine (const CConsole &console);
    void cannotSaveMachineState (const CConsole &console);
    void cannotSaveMachineState (const CProgress &progress);
    void cannotCreateClone(const CMachine &machine, QWidget *pParent = 0);
    void cannotCreateClone(const CMachine &machine, const CProgress &progress, QWidget *pParent = 0);
    void cannotTakeSnapshot (const CConsole &console);
    void cannotTakeSnapshot (const CProgress &progress);
    void cannotStopMachine (const CConsole &console);
    void cannotStopMachine (const CProgress &progress);
    void cannotDeleteMachine (const CMachine &machine);
    void cannotDeleteMachine(const CMachine &machine, const CProgress &progress);
    void cannotDiscardSavedState (const CConsole &console);

    void cannotSendACPIToMachine();

    bool warnAboutVirtNotEnabled64BitsGuest(bool fHWVirtExSupported);
    bool warnAboutVirtNotEnabledGuestRequired(bool fHWVirtExSupported);

    int askAboutSnapshotRestoring (const QString &aSnapshotName, bool fAlsoCreateNewSnapshot);
    bool askAboutSnapshotDeleting (const QString &aSnapshotName);
    bool askAboutSnapshotDeletingFreeSpace (const QString &aSnapshotName,
                                            const QString &aTargetImageName,
                                            const QString &aTargetImageMaxSize,
                                            const QString &aTargetFilesystemFree);
    void cannotRestoreSnapshot (const CConsole &aConsole, const QString &aSnapshotName);
    void cannotRestoreSnapshot (const CProgress &aProgress, const QString &aSnapshotName);
    void cannotDeleteSnapshot (const CConsole &aConsole, const QString &aSnapshotName);
    void cannotDeleteSnapshot (const CProgress &aProgress, const QString &aSnapshotName);

    void cannotFindMachineByName (const CVirtualBox &vbox, const QString &name);

    void cannotEnterSeamlessMode (ULONG aWidth, ULONG aHeight,
                                  ULONG aBpp, ULONG64 aMinVRAM);
    int cannotEnterFullscreenMode (ULONG aWidth, ULONG aHeight,
                                   ULONG aBpp, ULONG64 aMinVRAM);
    void cannotSwitchScreenInSeamless(quint64 minVRAM);
    int cannotSwitchScreenInFullscreen(quint64 minVRAM);
    int cannotEnterFullscreenMode();
    int cannotEnterSeamlessMode();

    int confirmMachineDeletion(const CMachine &machine);
    bool confirmDiscardSavedState (const CMachine &machine);

    void cannotChangeMediumType(QWidget *pParent, const CMedium &medium, KMediumType oldMediumType, KMediumType newMediumType);

    bool confirmReleaseMedium (QWidget *aParent, const VBoxMedium &aMedium,
                               const QString &aUsage);

    bool confirmRemoveMedium (QWidget *aParent, const VBoxMedium &aMedium);

    void sayCannotOverwriteHardDiskStorage (QWidget *aParent,
                                            const QString &aLocation);
    int confirmDeleteHardDiskStorage (QWidget *aParent,
                                      const QString &aLocation);
    void cannotDeleteHardDiskStorage (QWidget *aParent, const CMedium &aHD,
                                      const CProgress &aProgress);

    int askAboutHardDiskAttachmentCreation(QWidget *pParent, const QString &strControllerName);
    int askAboutOpticalAttachmentCreation(QWidget *pParent, const QString &strControllerName);
    int askAboutFloppyAttachmentCreation(QWidget *pParent, const QString &strControllerName);

    int confirmRemovingOfLastDVDDevice() const;

    void cannotCreateHardDiskStorage (QWidget *aParent, const CVirtualBox &aVBox,
                                      const QString &aLocaiton,
                                      const CMedium &aHD,
                                      const CProgress &aProgress);
    void cannotDetachDevice(QWidget *pParent, const CMachine &machine,
                            VBoxDefs::MediumType type, const QString &strLocation, const StorageSlot &storageSlot);

    int cannotRemountMedium (QWidget *aParent, const CMachine &aMachine, const VBoxMedium &aMedium, bool aMount, bool aRetry);
    void cannotOpenMedium (QWidget *aParent, const CVirtualBox &aVBox,
                           VBoxDefs::MediumType aType, const QString &aLocation);
    void cannotCloseMedium (QWidget *aParent, const VBoxMedium &aMedium,
                            const COMResult &aResult);

    void cannotOpenSession (const CSession &session);
    void cannotOpenSession (const CVirtualBox &vbox, const CMachine &machine,
                            const CProgress &progress = CProgress());

    void cannotGetMediaAccessibility (const VBoxMedium &aMedium);

    int confirmDeletingHostInterface (const QString &aName, QWidget *aParent = 0);

    void cannotAttachUSBDevice (const CConsole &console, const QString &device);
    void cannotAttachUSBDevice (const CConsole &console, const QString &device,
                                const CVirtualBoxErrorInfo &error);
    void cannotDetachUSBDevice (const CConsole &console, const QString &device);
    void cannotDetachUSBDevice (const CConsole &console, const QString &device,
                                const CVirtualBoxErrorInfo &error);

    void remindAboutGuestAdditionsAreNotActive(QWidget *pParent);
    int cannotFindGuestAdditions (const QString &aSrc1, const QString &aSrc2);
    void cannotDownloadGuestAdditions (const QString &aURL, const QString &aReason);
    void cannotMountGuestAdditions (const QString &aMachineName);
    bool confirmDownloadAdditions (const QString &aURL, ulong aSize);
    bool confirmMountAdditions (const QString &aURL, const QString &aSrc);

    bool askAboutUserManualDownload(const QString &strMissedLocation);
    bool confirmUserManualDownload(const QString &strURL, ulong uSize);
    void warnAboutUserManualCantBeDownloaded(const QString &strURL, const QString &strReason);
    void warnAboutUserManualDownloaded(const QString &strURL, const QString &strTarget);
    void warnAboutUserManualCantBeSaved(const QString &strURL, const QString &strTarget);

    void cannotConnectRegister (QWidget *aParent,
                                const QString &aURL,
                                const QString &aReason);
    void showRegisterResult (QWidget *aParent,
                             const QString &aResult);

    void showUpdateSuccess (QWidget *aParent,
                            const QString &aVersion,
                            const QString &aLink);
    void showUpdateFailure (QWidget *aParent,
                            const QString &aReason);
    void showUpdateNotFound (QWidget *aParent);

    bool confirmInputCapture (bool *aAutoConfirmed = NULL);
    void remindAboutAutoCapture();
    void remindAboutMouseIntegration (bool aSupportsAbsolute);
    bool remindAboutPausedVMInput();

    int warnAboutSettingsAutoConversion (const QString &aFileList, bool aAfterRefresh);

    bool remindAboutInaccessibleMedia();

    bool confirmGoingFullscreen (const QString &aHotKey);
    bool confirmGoingSeamless (const QString &aHotKey);
    bool confirmGoingScale (const QString &aHotKey);

    bool remindAboutGuruMeditation (const CConsole &aConsole,
                                    const QString &aLogFolder);

    bool confirmVMReset (QWidget *aParent);

    void warnAboutCannotCreateMachineFolder(QWidget *pParent, const QString &strFolderName);
    bool confirmHardDisklessMachine (QWidget *aParent);

    void cannotRunInSelectorMode();

    void cannotImportAppliance (CAppliance *aAppliance, QWidget *aParent = NULL) const;
    void cannotImportAppliance (const CProgress &aProgress, CAppliance *aAppliance, QWidget *aParent = NULL) const;

    void cannotCheckFiles (const CProgress &aProgress, QWidget *aParent = NULL) const;
    void cannotRemoveFiles (const CProgress &aProgress, QWidget *aParent = NULL) const;

    bool confirmExportMachinesInSaveState(const QStringList &machineNames, QWidget *aParent = NULL) const;
    void cannotExportAppliance (CAppliance *aAppliance, QWidget *aParent = NULL) const;
    void cannotExportAppliance (const CMachine &aMachine, CAppliance *aAppliance, QWidget *aParent = NULL) const;
    void cannotExportAppliance (const CProgress &aProgress, CAppliance *aAppliance, QWidget *aParent = NULL) const;

    void cannotUpdateGuestAdditions (const CProgress &aProgress, QWidget *aParent /* = NULL */) const;

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

    void showRuntimeError (const CConsole &console, bool fatal,
                           const QString &errorID,
                           const QString &errorMsg) const;

    static QString mediumToAccusative (VBoxDefs::MediumType aType, bool aIsHostDrive = false);

    static QString formatRC (HRESULT aRC);

    static QString formatErrorInfo (const COMErrorInfo &aInfo,
                                    HRESULT aWrapperRC = S_OK);

    static QString formatErrorInfo (const CVirtualBoxErrorInfo &aInfo)
    {
        return formatErrorInfo (COMErrorInfo (aInfo));
    }

    static QString formatErrorInfo (const COMBaseWithEI &aWrapper)
    {
        Assert (aWrapper.lastRC() != S_OK);
        return formatErrorInfo (aWrapper.errorInfo(), aWrapper.lastRC());
    }

    static QString formatErrorInfo (const COMResult &aRC)
    {
        Assert (aRC.rc() != S_OK);
        return formatErrorInfo (aRC.errorInfo(), aRC.rc());
    }

    void showGenericError(COMBaseWithEI *object, QWidget *pParent = 0);

    /* Stuff supporting interthreading: */
    void cannotCreateHostInterface(const CHost &host, QWidget *pParent = 0);
    void cannotCreateHostInterface(const CProgress &progress, QWidget *pParent = 0);
    void cannotRemoveHostInterface(const CHost &host, const CHostNetworkInterface &iface, QWidget *pParent = 0);
    void cannotRemoveHostInterface(const CProgress &progress, const CHostNetworkInterface &iface, QWidget *pParent = 0);
    void cannotAttachDevice(const CMachine &machine, VBoxDefs::MediumType type,
                            const QString &strLocation, const StorageSlot &storageSlot, QWidget *pParent = 0);
    void cannotCreateSharedFolder(const CMachine &machine, const QString &strName,
                                  const QString &strPath, QWidget *pParent = 0);
    void cannotRemoveSharedFolder(const CMachine &machine, const QString &strName,
                                  const QString &strPath, QWidget *pParent = 0);
    void cannotCreateSharedFolder(const CConsole &console, const QString &strName,
                                  const QString &strPath, QWidget *pParent = 0);
    void cannotRemoveSharedFolder(const CConsole &console, const QString &strName,
                                  const QString &strPath, QWidget *pParent = 0);
    void remindAboutWrongColorDepth(ulong uRealBPP, ulong uWantedBPP);
    void remindAboutUnsupportedUSB2(const QString &strExtPackName, QWidget *pParent = 0);

signals:

    void sigDownloaderUserManualCreated();
    void sigToCloseAllWarnings();

    /* Stuff supporting interthreading: */
    void sigCannotCreateHostInterface(const CHost &host, QWidget *pParent);
    void sigCannotCreateHostInterface(const CProgress &progress, QWidget *pParent);
    void sigCannotRemoveHostInterface(const CHost &host, const CHostNetworkInterface &iface, QWidget *pParent);
    void sigCannotRemoveHostInterface(const CProgress &progress, const CHostNetworkInterface &iface, QWidget *pParent);
    void sigCannotAttachDevice(const CMachine &machine, VBoxDefs::MediumType type,
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

    void showHelpWebDialog();
    void showHelpAboutDialog();
    void showHelpHelpDialog();
    void resetSuppressedMessages();
    void sltShowUserManual(const QString &strLocation);

private slots:

    /* Stuff supporting interthreading: */
    void sltCannotCreateHostInterface(const CHost &host, QWidget *pParent);
    void sltCannotCreateHostInterface(const CProgress &progress, QWidget *pParent);
    void sltCannotRemoveHostInterface(const CHost &host, const CHostNetworkInterface &iface, QWidget *pParent);
    void sltCannotRemoveHostInterface(const CProgress &progress, const CHostNetworkInterface &iface, QWidget *pParent);
    void sltCannotAttachDevice(const CMachine &machine, VBoxDefs::MediumType type,
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

    VBoxProblemReporter();

    static VBoxProblemReporter &instance();

    friend VBoxProblemReporter &vboxProblem();

    static QString doFormatErrorInfo (const COMErrorInfo &aInfo,
                                      HRESULT aWrapperRC = S_OK);

    QStringList m_shownWarnings;
    mutable QList<QPointer<QIMessageBox> > m_warnings;
};

/* Shortcut to the static VBoxProblemReporter::instance() method, for convenience. */
inline VBoxProblemReporter &vboxProblem() { return VBoxProblemReporter::instance(); }

#endif // __VBoxProblemReporter_h__

