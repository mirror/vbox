/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxProblemReporter class declaration
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxProblemReporter_h__
#define __VBoxProblemReporter_h__

#include "COMDefs.h"
#include "QIMessageBox.h"

/* Qt icludes */
#include <QObject>

class VBoxMedium;

// VBoxProblemReporter class
////////////////////////////////////////////////////////////////////////////////

/**
 * The VBoxProblemReporter class is a central place to handle all problem/error
 * situations that happen during application runtime and require the user's
 * attention.
 *
 * The role of this class is to describe the problem and/or the cause of the
 * error to the user and give him the opportunity to select an action (when
 * appropriate).
 *
 * Every problem sutiation has its own (correspondingly named) method in this
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

    static VBoxProblemReporter &instance();

    bool isValid() const;

    // helpers

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
                                aAutoConfirmId, aOkText, aCancelText); }

    bool showModalProgressDialog (CProgress &aProgress, const QString &aTitle,
                                  QWidget *aParent, int aMinDuration = 2000);

    QWidget *mainWindowShown() const;

    /* Generic problem handlers */
    bool askForOverridingFileIfExists (const QString& path, QWidget *aParent = NULL) const;
    bool askForOverridingFilesIfExists (const QStringList& aPaths, QWidget *aParent = NULL) const;

    void cannotDeleteFile (const QString& path, QWidget *aParent = NULL) const;

    /* Special problem handlers */
    void showBETAWarning();

#ifdef Q_WS_X11
    void cannotFindLicenseFiles (const QString &aPath);
    void cannotOpenLicenseFile (QWidget *aParent, const QString &aPath);
#endif

    void cannotOpenURL (const QString &aURL);
    void cannotCopyFile (const QString &aSrc, const QString &aDst, int aVRC);

    void cannotFindLanguage (const QString &aLangID, const QString &aNlsPath);
    void cannotLoadLanguage (const QString &aLangFile);

    void cannotInitCOM (HRESULT rc);
    void cannotCreateVirtualBox (const CVirtualBox &vbox);

    void cannotSaveGlobalSettings (const CVirtualBox &vbox,
                                   QWidget *parent = 0);

    void cannotLoadGlobalConfig (const CVirtualBox &vbox, const QString &error);
    void cannotSaveGlobalConfig (const CVirtualBox &vbox);
    void cannotSetSystemProperties (const CSystemProperties &props);
    void cannotAccessUSB (const COMBaseWithEI &aObj);

    void cannotCreateMachine (const CVirtualBox &vbox,
                              QWidget *parent = 0);
    void cannotCreateMachine (const CVirtualBox &vbox, const CMachine &machine,
                              QWidget *parent = 0);
    void cannotApplyMachineSettings (const CMachine &machine, const COMResult &res);
    void cannotSaveMachineSettings (const CMachine &machine,
                                    QWidget *parent = 0);
    void cannotLoadMachineSettings (const CMachine &machine,
                                    bool strict = true,
                                    QWidget *parent = 0);

    void cannotStartMachine (const CConsole &console);
    void cannotStartMachine (const CProgress &progress);
    void cannotPauseMachine (const CConsole &console);
    void cannotResumeMachine (const CConsole &console);
    void cannotACPIShutdownMachine (const CConsole &console);
    void cannotSaveMachineState (const CConsole &console);
    void cannotSaveMachineState (const CProgress &progress);
    void cannotTakeSnapshot (const CConsole &console);
    void cannotTakeSnapshot (const CProgress &progress);
    void cannotStopMachine (const CConsole &console);
    void cannotStopMachine (const CProgress &progress);
    void cannotDeleteMachine (const CVirtualBox &vbox, const CMachine &machine);
    void cannotDiscardSavedState (const CConsole &console);

    void cannotSendACPIToMachine();
    bool warnAboutVirtNotEnabled64BitsGuest();
    bool warnAboutVirtNotEnabledGuestRequired();

    void cannotSetSnapshotFolder (const CMachine &aMachine, const QString &aPath);
    bool askAboutSnapshotAndStateDiscarding();
    void cannotDiscardSnapshot (const CConsole &aConsole,
                                const QString &aSnapshotName);
    void cannotDiscardSnapshot (const CProgress &aProgress,
                                const QString &aSnapshotName);
    void cannotDiscardCurrentState (const CConsole &console);
    void cannotDiscardCurrentState (const CProgress &progress);
    void cannotDiscardCurrentSnapshotAndState (const CConsole &console);
    void cannotDiscardCurrentSnapshotAndState (const CProgress &progress);

    void cannotFindMachineByName (const CVirtualBox &vbox, const QString &name);

    void cannotEnterSeamlessMode (ULONG aWidth, ULONG aHeight,
                                  ULONG aBpp, ULONG64 aMinVRAM);
    int cannotEnterFullscreenMode (ULONG aWidth, ULONG aHeight,
                                   ULONG aBpp, ULONG64 aMinVRAM);

    bool confirmMachineDeletion (const CMachine &machine);
    bool confirmDiscardSavedState (const CMachine &machine);

    bool confirmReleaseMedium (QWidget *aParent, const VBoxMedium &aMedium,
                               const QString &aUsage);

    bool confirmRemoveMedium (QWidget *aParent, const VBoxMedium &aMedium);

    void sayCannotOverwriteHardDiskStorage (QWidget *aParent,
                                            const QString &aLocation);
    int confirmDeleteHardDiskStorage (QWidget *aParent,
                                      const QString &aLocation);
    void cannotDeleteHardDiskStorage (QWidget *aParent, const CHardDisk &aHD,
                                      const CProgress &aProgress);

    int confirmDetachAddControllerSlots (QWidget *aParent) const;
    int confirmChangeAddControllerSlots (QWidget *aParent) const;
    int confirmRunNewHDWzdOrVDM (QWidget* aParent);

    void cannotCreateHardDiskStorage (QWidget *aParent, const CVirtualBox &aVBox,
                                      const QString &aLocaiton,
                                      const CHardDisk &aHD,
                                      const CProgress &aProgress);
    void cannotAttachHardDisk (QWidget *aParent, const CMachine &aMachine,
                               const QString &aLocation, KStorageBus aBus,
                               LONG aChannel, LONG aDevice);
    void cannotDetachHardDisk (QWidget *aParent, const CMachine &aMachine,
                               const QString &aLocation, KStorageBus aBus,
                               LONG aChannel, LONG aDevice);

    void cannotMountMedium (QWidget *aParent, const CMachine &aMachine,
                            const VBoxMedium &aMedium, const COMResult &aResult);
    void cannotUnmountMedium (QWidget *aParent, const CMachine &aMachine,
                            const VBoxMedium &aMedium, const COMResult &aResult);
    void cannotOpenMedium (QWidget *aParent, const CVirtualBox &aVBox,
                           VBoxDefs::MediaType aType, const QString &aLocation);
    void cannotCloseMedium (QWidget *aParent, const VBoxMedium &aMedium,
                            const COMResult &aResult);

    void cannotOpenSession (const CSession &session);
    void cannotOpenSession (const CVirtualBox &vbox, const CMachine &machine,
                            const CProgress &progress = CProgress());

    void cannotGetMediaAccessibility (const VBoxMedium &aMedium);

    int confirmDeletingHostInterface (const QString &aName, QWidget *aParent = 0);
    void cannotCreateHostInterface (const CHost &aHost, QWidget *aParent = 0);
    void cannotCreateHostInterface (const CProgress &aProgress, QWidget *aParent = 0);
    void cannotRemoveHostInterface (const CHost &aHost,
                                    const CHostNetworkInterface &aIface,
                                    QWidget *aParent = 0);
    void cannotRemoveHostInterface (const CProgress &aProgress,
                                    const CHostNetworkInterface &aIface,
                                    QWidget *aParent = 0);

    void cannotAttachUSBDevice (const CConsole &console, const QString &device);
    void cannotAttachUSBDevice (const CConsole &console, const QString &device,
                                const CVirtualBoxErrorInfo &error);
    void cannotDetachUSBDevice (const CConsole &console, const QString &device);
    void cannotDetachUSBDevice (const CConsole &console, const QString &device,
                                const CVirtualBoxErrorInfo &error);

    void cannotCreateSharedFolder (QWidget *, const CMachine &,
                                   const QString &, const QString &);
    void cannotRemoveSharedFolder (QWidget *, const CMachine &,
                                   const QString &, const QString &);
    void cannotCreateSharedFolder (QWidget *, const CConsole &,
                                   const QString &, const QString &);
    void cannotRemoveSharedFolder (QWidget *, const CConsole &,
                                   const QString &, const QString &);

    int cannotFindGuestAdditions (const QString &aSrc1, const QString &aSrc2);
    void cannotDownloadGuestAdditions (const QString &aURL,
                                       const QString &aReason);
    bool confirmDownloadAdditions (const QString &aURL, ulong aSize);
    bool confirmMountAdditions (const QString &aURL, const QString &aSrc);
    void warnAboutTooOldAdditions (QWidget *, const QString &, const QString &);
    void warnAboutOldAdditions (QWidget *, const QString &, const QString &);
    void warnAboutNewAdditions (QWidget *, const QString &, const QString &);

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

    int warnAboutAutoConvertedSettings (const QString &aFormatVersion,
                                        const QString &aFileList,
                                        bool aAfterRefresh);

    bool remindAboutInaccessibleMedia();

    bool confirmGoingFullscreen (const QString &aHotKey);
    bool confirmGoingSeamless (const QString &aHotKey);

    void remindAboutWrongColorDepth (ulong aRealBPP, ulong aWantedBPP);

    bool remindAboutGuruMeditation (const CConsole &aConsole,
                                    const QString &aLogFolder);

    bool confirmVMReset (QWidget *aParent);

    bool confirmHardDisklessMachine (QWidget *aParent);

    void cannotRunInSelectorMode();

    void cannotImportAppliance (CAppliance *aAppliance, QWidget *aParent = NULL) const;
    void cannotImportAppliance (const CProgress &aProgress, CAppliance *aAppliance, QWidget *aParent = NULL) const;

    void cannotExportAppliance (CAppliance *aAppliance, QWidget *aParent = NULL) const;
    void cannotExportAppliance (const CMachine &aMachine, CAppliance *aAppliance, QWidget *aParent = NULL) const;
    void cannotExportAppliance (const CProgress &aProgress, CAppliance *aAppliance, QWidget *aParent = NULL) const;

    void showRuntimeError (const CConsole &console, bool fatal,
                           const QString &errorID,
                           const QString &errorMsg) const;

    static QString toAccusative (VBoxDefs::MediaType aType);

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

public slots:

    void showHelpWebDialog();
    void showHelpAboutDialog();
    void showHelpHelpDialog();
    void resetSuppressedMessages();

private:

    friend VBoxProblemReporter &vboxProblem();

    static QString doFormatErrorInfo (const COMErrorInfo &aInfo,
                                      HRESULT aWrapperRC = S_OK);
};

/**
 * Shortcut to the static VBoxProblemReporter::instance() method, for
 * convenience.
 */
inline VBoxProblemReporter &vboxProblem() { return VBoxProblemReporter::instance(); }

#endif // __VBoxProblemReporter_h__
