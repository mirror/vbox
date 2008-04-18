/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxProblemReporter class declaration
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

class VBoxProblemReporter : public QObject
{
    Q_OBJECT

public:

    enum Type {
        Info = 1,
        Question,
        Warning,
        Error,
        Critical,
        GuruMeditation
    };
    enum {
        AutoConfirmed = 0x8000
    };

    static VBoxProblemReporter &instance();

    bool isValid();

    // helpers

    int message (QWidget *aParent, Type aType, const QString &aMessage,
                 const QString &aDetails = QString::null,
                 const char *aAutoConfirmId = 0,
                 int aButton1 = 0, int aButton2 = 0, int aButton3 = 0,
                 const QString &aText1 = QString::null,
                 const QString &aText2 = QString::null,
                 const QString &aText3 = QString::null);

    int message (QWidget *aParent, Type aType, const QString &aMessage,
                 const char *aAutoConfirmId,
                 int aButton1 = 0, int aButton2 = 0, int aButton3 = 0,
                 const QString &aText1 = QString::null,
                 const QString &aText2 = QString::null,
                 const QString &aText3 = QString::null)
    {
        return message (aParent, aType, aMessage, QString::null, aAutoConfirmId,
                        aButton1, aButton2, aButton3, aText1, aText2, aText3);
    }

    bool messageYesNo (QWidget *aParent, Type aType, const QString &aMessage,
                       const QString &aDetails = QString::null,
                       const char *aAutoConfirmId = 0,
                       const QString &aYesText = QString::null,
                       const QString &aNoText = QString::null)
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
                       const QString &aNoText = QString::null)
    {
        return messageYesNo (aParent, aType, aMessage, QString::null,
                             aAutoConfirmId, aYesText, aNoText);
    }

    bool messageOkCancel (QWidget *aParent, Type aType, const QString &aMessage,
                          const QString &aDetails = QString::null,
                          const char *aAutoConfirmId = 0,
                          const QString &aOkText = QString::null,
                          const QString &aCancelText = QString::null)
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
                          const QString &aCancelText = QString::null)
    {
        return messageOkCancel (aParent, aType, aMessage, QString::null,
                                aAutoConfirmId, aOkText, aCancelText);
    }

    bool showModalProgressDialog (CProgress &aProgress, const QString &aTitle,
                                  QWidget *aParent, int aMinDuration = 2000);

    QWidget *mainWindowShown();

    // problem handlers

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
    void cannotAccessUSB (const COMBase &obj);

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
    void cannotDeleteMachine (const CVirtualBox &vbox, const CMachine &machine);
    void cannotDiscardSavedState (const CConsole &console);

    void cannotSetSnapshotFolder (const CMachine &aMachine, const QString &aPath);
    void cannotDiscardSnapshot (const CConsole &console, const CSnapshot &snapshot);
    void cannotDiscardSnapshot (const CProgress &progress, const CSnapshot &snapshot);
    void cannotDiscardCurrentState (const CConsole &console);
    void cannotDiscardCurrentState (const CProgress &progress);
    void cannotDiscardCurrentSnapshotAndState (const CConsole &console);
    void cannotDiscardCurrentSnapshotAndState (const CProgress &progress);

    void cannotFindMachineByName (const CVirtualBox &vbox, const QString &name);

    void cannotEnterSeamlessMode (ULONG aWidth, ULONG aHeight, ULONG aBpp);

    bool confirmMachineDeletion (const CMachine &machine);
    bool confirmDiscardSavedState (const CMachine &machine);

    bool confirmReleaseImage (QWidget *parent, const QString &usage);

    void sayCannotOverwriteHardDiskImage (QWidget *parent, const QString &src);
    int confirmHardDiskImageDeletion (QWidget *parent, const QString &src);
    void cannotDeleteHardDiskImage (QWidget *parent, const CVirtualDiskImage &vdi);

    bool confirmHardDiskUnregister (QWidget *parent, const QString &src);

    void cannotCreateHardDiskImage (
        QWidget *parent, const CVirtualBox &vbox, const QString &src,
        const CVirtualDiskImage &vdi, const CProgress &progress);
    void cannotAttachHardDisk (QWidget *parent, const CMachine &m, const QUuid &id,
                               KStorageBus bus, LONG channel, LONG dev);
    void cannotDetachHardDisk (QWidget *parent, const CMachine &m,
                               KStorageBus bus, LONG channel, LONG dev);
    void cannotRegisterMedia (QWidget *parent, const CVirtualBox &vbox,
                              VBoxDefs::DiskType type, const QString &src);
    void cannotUnregisterMedia (QWidget *parent, const CVirtualBox &vbox,
                                VBoxDefs::DiskType type, const QString &src);

    void cannotOpenSession (const CSession &session);
    void cannotOpenSession (const CVirtualBox &vbox, const CMachine &machine,
                            const CProgress &progress = CProgress());

    void cannotGetMediaAccessibility (const CUnknown &unk);

/// @todo (r=dmik) later
//    void cannotMountMedia (const CUnknown &unk);
//    void cannotUnmountMedia (const CUnknown &unk);

#if defined Q_WS_WIN
    void cannotCreateHostInterface (const CHost &host, const QString &name,
                                    QWidget *parent = 0);
    void cannotCreateHostInterface (const CProgress &progress, const QString &name,
                                    QWidget *parent = 0);
    void cannotRemoveHostInterface (const CHost &host,
                                    const CHostNetworkInterface &iface,
                                    QWidget *parent = 0);
    void cannotRemoveHostInterface (const CProgress &progress,
                                    const CHostNetworkInterface &iface,
                                    QWidget *parent = 0);
#endif

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

    bool confirmInputCapture (bool *aAutoConfirmed = NULL);
    void remindAboutAutoCapture();
    void remindAboutMouseIntegration (bool aSupportsAbsolute);
    bool remindAboutPausedVMInput();

    int warnAboutAutoConvertedSettings (const QString &aFormatVersion,
                                        const QString &aFileList);

    bool remindAboutInaccessibleMedia();

    bool confirmGoingFullscreen (const QString &aHotKey);
    bool confirmGoingSeamless (const QString &aHotKey);

    void remindAboutWrongColorDepth (ulong aRealBPP, ulong aWantedBPP);

    bool remindAboutGuruMeditation (const CConsole &aConsole,
                                    const QString &aLogFolder);

    bool confirmVMReset (QWidget *aParent);

    bool confirmHardDisklessMachine (QWidget *aParent);

    void cannotRunInSelectorMode();

    void showRuntimeError (const CConsole &console, bool fatal,
                           const QString &errorID,
                           const QString &errorMsg);

    static QString formatErrorInfo (const COMErrorInfo &aInfo,
                                    HRESULT aWrapperRC = S_OK);

    static QString formatErrorInfo (const CVirtualBoxErrorInfo &aInfo)
    {
        return formatErrorInfo (COMErrorInfo (aInfo));
    }

    static QString formatErrorInfo (const COMBase &aWrapper)
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

inline VBoxProblemReporter &vboxProblem() { return VBoxProblemReporter::instance(); }

#endif // __VBoxProblemReporter_h__
