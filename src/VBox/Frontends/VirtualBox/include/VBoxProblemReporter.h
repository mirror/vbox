/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxProblemReporter class declaration
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __VBoxProblemReporter_h__
#define __VBoxProblemReporter_h__

#include "COMDefs.h"
#include "QIMessageBox.h"

#include <qobject.h>

class QProcess;

class VBoxProblemReporter : public QObject
{
    Q_OBJECT

public:

    enum Type {
        Info = 1,
        Question,
        Warning,
        Error,
        Critical
    };
    enum {
        AutoConfirmed = 0x8000
    };

    static VBoxProblemReporter &instance();

    bool isValid();

    // helpers

    int message (
        QWidget *parent, Type type, const QString &msg,
        const QString &details = QString::null,
        const char *autoConfirmId = 0,
        int b1 = 0, int b2 = 0, int b3 = 0,
        const char *name = 0
    );

    int message (
        QWidget *parent, Type type, const QString &msg,
        const char *autoConfirmId,
        int b1 = 0, int b2 = 0, int b3 = 0,
        const char *name = 0
    ) {
        return message (parent, type, msg, QString::null, autoConfirmId,
                        b1, b2, b3, name);
    }

    bool messageYesNo (
        QWidget *parent, Type type, const QString &msg,
        const QString &details = QString::null,
        const char *autoConfirmId = 0,
        const char *name = 0
    );

    bool messageYesNo (
        QWidget *parent, Type type, const QString &msg,
        const char *autoConfirmId,
        const char *name = 0
    ) {
        return messageYesNo (parent, type, msg, QString::null, autoConfirmId, name);
    }

    bool showModalProgressDialog (
        CProgress &aProgress, const QString &aTitle, QWidget *aParent,
        int aMinDuration = 2000
    );

    QWidget *mainWindowShown();

    // problem handlers

    void cannotInitCOM (HRESULT rc);
    void cannotCreateVirtualBox (const CVirtualBox &vbox);

    void cannotLoadGlobalConfig (const CVirtualBox &vbox, const QString &error);
    void cannotSaveGlobalConfig (const CVirtualBox &vbox);
    void cannotSetSystemProperties (const CSystemProperties &props);

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
    void cannotGetUSBController (const CMachine &machine);

    void cannotStartMachine (const CConsole &console);
    void cannotStartMachine (const CProgress &progress);
    void cannotPauseMachine (const CConsole &console);
    void cannotResumeMachine (const CConsole &console);
    void cannotSaveMachineState (const CConsole &console);
    void cannotSaveMachineState (const CProgress &progress);
    void cannotTakeSnapshot (const CConsole &console);
    void cannotTakeSnapshot (const CProgress &progress);
    void cannotStopMachine (const CConsole &console);
    void cannotDeleteMachine (const CVirtualBox &vbox, const CMachine &machine);
    void cannotDiscardSavedState (const CConsole &console);

    void cannotDiscardSnapshot (const CConsole &console, const CSnapshot &snapshot);
    void cannotDiscardSnapshot (const CProgress &progress, const CSnapshot &snapshot);
    void cannotDiscardCurrentState (const CConsole &console);
    void cannotDiscardCurrentState (const CProgress &progress);
    void cannotDiscardCurrentSnapshotAndState (const CConsole &console);
    void cannotDiscardCurrentSnapshotAndState (const CProgress &progress);

    void cannotFindMachineByName (const CVirtualBox &vbox, const QString &name);

    bool confirmMachineDeletion (const CMachine &machine);
    bool confirmDiscardSavedState (const CMachine &machine);

    void sayCannotOverwriteHardDiskImage (QWidget *parent, const QString &src);
    int confirmHardDiskImageDeletion (QWidget *parent, const QString &src);
    void cannotDeleteHardDiskImage (QWidget *parent, const CVirtualDiskImage &vdi);

    int confirmHardDiskUnregister (QWidget *parent, const QString &src);
    
    void cannotCreateHardDiskImage (
        QWidget *parent, const CVirtualBox &vbox, const QString &src,
        const CVirtualDiskImage &vdi, const CProgress &progress);
    void cannotAttachHardDisk (QWidget *parent, const CMachine &m, const QUuid &id,
                               CEnums::DiskControllerType ctl, LONG dev);
    void cannotDetachHardDisk (QWidget *parent, const CMachine &m,
                               CEnums::DiskControllerType ctl, LONG dev);
    void cannotRegisterMedia (QWidget *parent, const CVirtualBox &vbox,
                              VBoxDefs::DiskType type, const QString &src);
    void cannotUnregisterMedia (QWidget *parent, const CVirtualBox &vbox,
                                VBoxDefs::DiskType type, const QString &src);

    void cannotOpenSession (const CSession &session);
    void cannotOpenSession (const CVirtualBox &vbox, const QUuid &id);
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
    void cannotDetachUSBDevice (const CConsole &console, const QString &device);

    bool confirmReleaseImage (QWidget*, QString);

    bool remindAboutInputCapture();
    bool remindAboutAutoCapture();
    bool remindAboutMouseIntegration (bool supportsAbsolute);
    bool remindAboutPausedVMInput();

    bool remindAboutInaccessibleMedia();

    void remindAboutGoingFullscreen (const QString &hotKey,
                                     const QString &hostKey);

    void showRuntimeError (const CConsole &console, bool fatal,
                           const QString &errorID,
                           const QString &errorMsg);

    static QString formatErrorInfo (const COMErrorInfo &info,
                                    HRESULT wrapperRC = S_OK);
    static QString formatErrorInfo (const CVirtualBoxErrorInfo &info) {
        return formatErrorInfo (COMErrorInfo (info));
    }
    static QString formatErrorInfo (const COMBase &wrapper) {
        Assert (FAILED (wrapper.lastRC()));
        return formatErrorInfo (wrapper.errorInfo(), wrapper.lastRC());
    }
    static QString formatErrorInfo (const COMResult &rc) {
        Assert (FAILED (rc.rc()));
        return formatErrorInfo (rc.errorInfo(), rc.rc());
    }

public slots:

    void showHelpWebDialog();
    void showHelpAboutDialog();
    void resetSuppressedMessages();

private:

    friend VBoxProblemReporter &vboxProblem();
};

inline VBoxProblemReporter &vboxProblem() { return VBoxProblemReporter::instance(); }

#endif // __VBoxProblemReporter_h__
