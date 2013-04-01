/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMessageCenter class implementation
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

/* Qt includes: */
#include <QDir>
#include <QDesktopWidget>
#include <QFileInfo>
#include <QLocale>
#include <QThread>
#include <QProcess>
#ifdef Q_WS_MAC
# include <QPushButton>
#endif /* Q_WS_MAC */

/* GUI includes: */
#include "UIMessageCenter.h"
#include "VBoxGlobal.h"
#include "UISelectorWindow.h"
#include "UIProgressDialog.h"
#include "UINetworkManager.h"
#include "UINetworkManagerDialog.h"
#include "UIConverter.h"
#include "UIModalWindowManager.h"
#ifdef VBOX_OSE
# include "UIDownloaderUserManual.h"
#endif /* VBOX_OSE */
#include "UIMachine.h"
#include "VBoxAboutDlg.h"
#include "UIHostComboEditor.h"
#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
#endif /* Q_WS_MAC */
#ifdef Q_WS_WIN
# include <Htmlhelp.h>
#endif /* Q_WS_WIN */

/* COM includes: */
#include "CConsole.h"
#include "CMachine.h"
#include "CSystemProperties.h"
#include "CVirtualBoxErrorInfo.h"
#include "CMediumAttachment.h"
#include "CMediumFormat.h"
#include "CAppliance.h"
#include "CExtPackManager.h"
#include "CExtPackFile.h"
#include "CHostNetworkInterface.h"
#ifdef VBOX_WITH_DRAG_AND_DROP
# include "CGuest.h"
#endif /* VBOX_WITH_DRAG_AND_DROP */

/* Other VBox includes: */
#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/path.h>

bool UIMessageCenter::warningShown(const QString &strWarningName) const
{
    return m_warnings.contains(strWarningName);
}

void UIMessageCenter::setWarningShown(const QString &strWarningName, bool fWarningShown)
{
    if (fWarningShown && !m_warnings.contains(strWarningName))
        m_warnings.append(strWarningName);
    else if (!fWarningShown && m_warnings.contains(strWarningName))
        m_warnings.removeAll(strWarningName);
}

int UIMessageCenter::message(QWidget *pParent, MessageType type,
                             const QString &strMessage,
                             const QString &strDetails /* = QString() */,
                             const char *pcszAutoConfirmId /* = 0 */,
                             int iButton1 /* = 0 */,
                             int iButton2 /* = 0 */,
                             int iButton3 /* = 0 */,
                             const QString &strButtonText1 /* = QString() */,
                             const QString &strButtonText2 /* = QString() */,
                             const QString &strButtonText3 /* = QString() */) const
{
    /* If this is NOT a GUI thread: */
    if (thread() != QThread::currentThread())
    {
        /* We have to throw a blocking signal
         * to show a message-box in the GUI thread: */
        emit sigToShowMessageBox(pParent, type,
                                 strMessage, strDetails,
                                 iButton1, iButton2, iButton3,
                                 strButtonText1, strButtonText2, strButtonText3,
                                 QString(pcszAutoConfirmId));
        /* Inter-thread communications are not yet implemented: */
        return 0;
    }
    /* In usual case we can chow a message-box directly: */
    return showMessageBox(pParent, type,
                          strMessage, strDetails,
                          iButton1, iButton2, iButton3,
                          strButtonText1, strButtonText2, strButtonText3,
                          QString(pcszAutoConfirmId));
}

int UIMessageCenter::messageWithOption(QWidget *pParent, MessageType type,
                                       const QString &strMessage,
                                       const QString &strOptionText,
                                       bool fDefaultOptionValue /* = true */,
                                       const QString &strDetails /* = QString() */,
                                       int iButton1 /* = 0 */,
                                       int iButton2 /* = 0 */,
                                       int iButton3 /* = 0 */,
                                       const QString &strButtonName1 /* = QString() */,
                                       const QString &strButtonName2 /* = QString() */,
                                       const QString &strButtonName3 /* = QString() */) const
{
    /* If no buttons are set, using single 'OK' button: */
    if (iButton1 == 0 && iButton2 == 0 && iButton3 == 0)
        iButton1 = QIMessageBox::Ok | QIMessageBox::Default;

    /* Assign corresponding title and icon: */
    QString strTitle;
    QIMessageBox::Icon icon;
    switch (type)
    {
        default:
        case MessageType_Info:
            strTitle = tr("VirtualBox - Information", "msg box title");
            icon = QIMessageBox::Information;
            break;
        case MessageType_Question:
            strTitle = tr("VirtualBox - Question", "msg box title");
            icon = QIMessageBox::Question;
            break;
        case MessageType_Warning:
            strTitle = tr("VirtualBox - Warning", "msg box title");
            icon = QIMessageBox::Warning;
            break;
        case MessageType_Error:
            strTitle = tr("VirtualBox - Error", "msg box title");
            icon = QIMessageBox::Critical;
            break;
        case MessageType_Critical:
            strTitle = tr("VirtualBox - Critical Error", "msg box title");
            icon = QIMessageBox::Critical;
            break;
        case MessageType_GuruMeditation:
            strTitle = "VirtualBox - Guru Meditation"; /* don't translate this */
            icon = QIMessageBox::GuruMeditation;
            break;
    }

    /* Create message-box: */
    QWidget *pBoxParent = mwManager().realParentWindow(pParent);
    QPointer<QIMessageBox> pBox = new QIMessageBox(strTitle, strMessage, icon,
                                                   iButton1, iButton2, iButton3, pBoxParent);
    mwManager().registerNewParent(pBox, pBoxParent);

    /* Load option: */
    if (!strOptionText.isNull())
    {
        pBox->setFlagText(strOptionText);
        pBox->setFlagChecked(fDefaultOptionValue);
    }

    /* Configure button-text: */
    if (!strButtonName1.isNull())
        pBox->setButtonText(0, strButtonName1);
    if (!strButtonName2.isNull())
        pBox->setButtonText(1, strButtonName2);
    if (!strButtonName3.isNull())
        pBox->setButtonText(2, strButtonName3);
    if (!strDetails.isEmpty())
        pBox->setDetailsText(strDetails);

    /* Show box: */
    int rc = pBox->exec();

    /* Make sure box still valid: */
    if (!pBox)
        return rc;

    /* Save option: */
    if (pBox->isFlagChecked())
        rc |= QIMessageBox::OptionChosen;

    /* Delete message-box: */
    if (pBox)
        delete pBox;

    return rc;
}

bool UIMessageCenter::showModalProgressDialog(CProgress &progress,
                                              const QString &strTitle,
                                              const QString &strImage /* = "" */,
                                              QWidget *pParent /* = 0 */,
                                              int cMinDuration /* = 2000 */)
{
    /* Prepare pixmap: */
    QPixmap *pPixmap = 0;
    if (!strImage.isEmpty())
        pPixmap = new QPixmap(strImage);

    /* Create progress-dialog: */
    QWidget *pDlgParent = mwManager().realParentWindow(pParent);
    QPointer<UIProgressDialog> pProgressDlg = new UIProgressDialog(progress, strTitle, pPixmap, cMinDuration, pDlgParent);
    mwManager().registerNewParent(pProgressDlg, pDlgParent);

    /* Run the dialog with the 350 ms refresh interval. */
    pProgressDlg->run(350);

    /* Make sure progress-dialog still valid: */
    if (!pProgressDlg)
        return false;

    /* Delete progress-dialog: */
    delete pProgressDlg;

    /* Cleanup pixmap: */
    if (pPixmap)
        delete pPixmap;

    return true;
}

QWidget* UIMessageCenter::mainWindowShown() const
{
    /* It may happen that this method is called during VBoxGlobal
     * initialization or even after it failed (for example, to show some
     * error message). Return no main window in this case: */
    if (!vboxGlobal().isValid())
        return 0;

    if (vboxGlobal().isVMConsoleProcess())
    {
        if (vboxGlobal().vmWindow() && vboxGlobal().vmWindow()->isVisible()) /* VM window is visible */
            return vboxGlobal().vmWindow(); /* return that window */
    }
    else
    {
        if (vboxGlobal().selectorWnd().isVisible()) /* VM selector is visible */
            return &vboxGlobal().selectorWnd(); /* return that window */
    }

    return 0;
}

QWidget* UIMessageCenter::mainMachineWindowShown() const
{
    /* It may happen that this method is called during VBoxGlobal
     * initialization or even after it failed (for example, to show some
     * error message). Return no main window in this case: */
    if (!vboxGlobal().isValid())
        return 0;

    if (vboxGlobal().vmWindow() && vboxGlobal().vmWindow()->isVisible()) /* VM window is visible */
        return vboxGlobal().vmWindow(); /* return that window */

    return 0;
}

QWidget* UIMessageCenter::networkManagerOrMainWindowShown() const
{
    return gNetworkManager->window()->isVisible() ? gNetworkManager->window() : mainWindowShown();
}

QWidget* UIMessageCenter::networkManagerOrMainMachineWindowShown() const
{
    return gNetworkManager->window()->isVisible() ? gNetworkManager->window() : mainMachineWindowShown();
}

#ifdef RT_OS_LINUX
void UIMessageCenter::warnAboutWrongUSBMounted()
{
    message(mainWindowShown(), MessageType_Warning,
            tr("You seem to have the USBFS filesystem mounted at /sys/bus/usb/drivers. "
               "We strongly recommend that you change this, as it is a severe mis-configuration of "
               "your system which could cause USB devices to fail in unexpected ways."),
            "checkForMountedWrongUSB");
}
#endif /* RT_OS_LINUX */

void UIMessageCenter::cannotStartSelector()
{
    message(0, MessageType_Critical,
            tr("<p>Cannot start VirtualBox Manager due to local restrictions.</p>"
               "<p>The application will now terminate.</p>"));
}

void UIMessageCenter::showBETAWarning()
{
    message(0, MessageType_Warning,
            tr("You are running a prerelease version of VirtualBox. "
               "This version is not suitable for production use."));
}

void UIMessageCenter::showBEBWarning()
{
    message(0, MessageType_Warning,
            tr("You are running an EXPERIMENTAL build of VirtualBox. "
               "This version is not suitable for production use."));
}

void UIMessageCenter::cannotInitUserHome(const QString &strUserHome)
{
    message(0, MessageType_Critical,
            tr("<p>Failed to initialize COM because the VirtualBox global "
               "configuration directory <b><nobr>%1</nobr></b> is not accessible. "
               "Please check the permissions of this directory and of its parent directory.</p>"
               "<p>The application will now terminate.</p>").arg(strUserHome),
            formatErrorInfo(COMErrorInfo()));
}

void UIMessageCenter::cannotInitCOM(HRESULT rc)
{
    message(0, MessageType_Critical,
            tr("<p>Failed to initialize COM or to find the VirtualBox COM server. "
               "Most likely, the VirtualBox server is not running or failed to start.</p>"
               "<p>The application will now terminate.</p>"),
            formatErrorInfo(COMErrorInfo(), rc));
}

void UIMessageCenter::cannotCreateVirtualBox(const CVirtualBox &vbox)
{
    message(0, MessageType_Critical,
            tr("<p>Failed to create the VirtualBox COM object.</p>"
               "<p>The application will now terminate.</p>"),
            formatErrorInfo(vbox));
}

void UIMessageCenter::cannotFindLanguage(const QString &strLangId, const QString &strNlsPath)
{
    message(0, MessageType_Error,
            tr("<p>Could not find a language file for the language <b>%1</b> in the directory <b><nobr>%2</nobr></b>.</p>"
               "<p>The language will be temporarily reset to the system default language. "
               "Please go to the <b>Preferences</b> dialog which you can open from the <b>File</b> menu "
               "of the main VirtualBox window, and select one of the existing languages on the <b>Language</b> page.</p>")
               .arg(strLangId).arg(strNlsPath));
}

void UIMessageCenter::cannotLoadLanguage(const QString &strLangFile)
{
    message(0, MessageType_Error,
            tr("<p>Could not load the language file <b><nobr>%1</nobr></b>. "
               "<p>The language will be temporarily reset to English (built-in). "
               "Please go to the <b>Preferences</b> dialog which you can open from the <b>File</b> menu "
               "of the main VirtualBox window, and select one of the existing languages on the <b>Language</b> page.</p>")
               .arg(strLangFile));
}

void UIMessageCenter::cannotLoadGlobalConfig(const CVirtualBox &vbox, const QString &strError)
{
    /* Preserve error-info: */
    COMResult res(vbox);
    /* Show the message: */
    message(0, MessageType_Critical,
            tr("<p>Failed to load the global GUI configuration from <b><nobr>%1</nobr></b>.</p>"
               "<p>The application will now terminate.</p>").arg(vbox.GetSettingsFilePath()),
            !res.isOk() ? formatErrorInfo(res)
                        : QString("<!--EOM--><p>%1</p>").arg(vboxGlobal().emphasize(strError)));
}

void UIMessageCenter::cannotSaveGlobalConfig(const CVirtualBox &vbox)
{
    /* Preserve error-info: */
    COMResult res(vbox);
    /* Show the message: */
    message(0, MessageType_Critical,
            tr("<p>Failed to save the global GUI configuration to <b><nobr>%1</nobr></b>.</p>"
               "<p>The application will now terminate.</p>").arg(vbox.GetSettingsFilePath()),
            formatErrorInfo(res));
}

void UIMessageCenter::cannotFindMachineByName(const CVirtualBox &vbox, const QString &strName)
{
    message(0, MessageType_Error,
            tr("There is no virtual machine named <b>%1</b>.").arg(strName),
            formatErrorInfo(vbox));
}

void UIMessageCenter::cannotFindMachineById(const CVirtualBox &vbox, const QString &strId)
{
    message(0, MessageType_Error,
            tr("There is no virtual machine with id <b>%1</b>.").arg(strId),
            formatErrorInfo(vbox));
}

void UIMessageCenter::cannotOpenSession(const CSession &session)
{
    Assert(session.isNull());
    /* Show the message: */
    message(mainWindowShown(), MessageType_Error,
            tr("Failed to create a new session."),
            formatErrorInfo(session));
}

void UIMessageCenter::cannotOpenSession(const CVirtualBox &vbox, const CMachine &machine,
                                        const CProgress &progress /* = CProgress() */)
{
    Assert(!vbox.isOk() || progress.isOk());
    /* Compose machine name: */
    QString strName = machine.GetName();
    if (strName.isEmpty())
        strName = QFileInfo(machine.GetSettingsFilePath()).baseName();
    /* Show the message: */
    message(mainWindowShown(), MessageType_Error,
            tr("Failed to open a session for the virtual machine <b>%1</b>.").arg(strName),
            !vbox.isOk() ? formatErrorInfo(vbox) : formatErrorInfo(progress.GetErrorInfo()));
}

void UIMessageCenter::cannotOpenSession(const CMachine &machine)
{
    /* Preserve error-info: */
    COMResult res(machine);
    /* Compose machine name: */
    QString strName = machine.GetName();
    if (strName.isEmpty())
        strName = QFileInfo(machine.GetSettingsFilePath()).baseName();
    /* Show the message: */
    message(mainWindowShown(), MessageType_Error,
            tr("Failed to open a session for the virtual machine <b>%1</b>.").arg(strName),
            formatErrorInfo(res));
}

void UIMessageCenter::cannotGetMediaAccessibility(const UIMedium &medium)
{
    message(mainWindowShown(), MessageType_Error,
            tr("Failed to determine the accessibility state of the medium <nobr><b>%1</b></nobr>.")
               .arg(medium.location()),
            formatErrorInfo(medium.result()));
}

void UIMessageCenter::cannotOpenURL(const QString &strUrl)
{
    message(mainWindowShown(), MessageType_Error,
            tr("Failed to open <tt>%1</tt>. "
               "Make sure your desktop environment can properly handle URLs of this type.")
               .arg(strUrl));
}

void UIMessageCenter::cannotOpenMachine(QWidget *pParent, const QString &strMachinePath, const CVirtualBox &vbox)
{
    message(pParent ? pParent : mainWindowShown(),
            MessageType_Error,
            tr("Failed to open virtual machine located in %1.")
               .arg(strMachinePath),
            formatErrorInfo(vbox));
}

void UIMessageCenter::cannotReregisterMachine(QWidget *pParent, const QString &strMachinePath, const QString &strMachineName)
{
    message(pParent ? pParent : mainWindowShown(),
            MessageType_Error,
            tr("Failed to add virtual machine <b>%1</b> located in <i>%2</i> because its already present.")
               .arg(strMachineName).arg(strMachinePath));
}

void UIMessageCenter::cannotDeleteMachine(const CMachine &machine)
{
    /* preserve the current error info before calling the object again */
    COMResult res(machine);

    message(mainWindowShown(),
            MessageType_Error,
            tr("Failed to remove the virtual machine <b>%1</b>.").arg(machine.GetName()),
            !machine.isOk() ? formatErrorInfo(machine) : formatErrorInfo(res));
}

void UIMessageCenter::cannotDeleteMachine(const CMachine &machine, const CProgress &progress)
{
    message(mainWindowShown(),
            MessageType_Error,
            tr("Failed to remove the virtual machine <b>%1</b>.").arg(machine.GetName()),
            formatErrorInfo(progress.GetErrorInfo()));
}

void UIMessageCenter::cannotDiscardSavedState(const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res(console);

    message(mainWindowShown(), MessageType_Error,
        tr("Failed to discard the saved state of the virtual machine <b>%1</b>.")
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(res));
}

void UIMessageCenter::notifyAboutCollisionOnGroupRemovingCantBeResolved(const QString &strName, const QString &strGroupName)
{
    message(&vboxGlobal().selectorWnd(),
            MessageType_Error,
            tr("<p>You are trying to move machine <nobr><b>%1</b></nobr> "
               "to group <nobr><b>%2</b></nobr> which already have sub-group <nobr><b>%1</b></nobr>.</p>"
               "<p>Please resolve this name-conflict and try again.</p>")
               .arg(strName, strGroupName),
            0, /* auto-confirm id */
            QIMessageBox::Ok | QIMessageBox::Default);
}

int UIMessageCenter::askAboutCollisionOnGroupRemoving(const QString &strName, const QString &strGroupName)
{
    return message(&vboxGlobal().selectorWnd(),
                   MessageType_Question,
                   tr("<p>You are trying to move group <nobr><b>%1</b></nobr> "
                      "to group <nobr><b>%2</b></nobr> which already have another item with the same name.</p>"
                      "<p>Would you like to automatically rename it?</p>")
                      .arg(strName, strGroupName),
                   0, /* auto-confirm id */
                   QIMessageBox::Ok,
                   QIMessageBox::Cancel | QIMessageBox::Escape | QIMessageBox::Default,
                   0,
                   tr("Rename"));
}

int UIMessageCenter::confirmMachineItemRemoval(const QStringList &names)
{
    return message(&vboxGlobal().selectorWnd(),
                   MessageType_Question,
                   tr("<p>You are about to remove following virtual "
                      "machine items from the machine list:</p>"
                      "<p><b>%1</b></p>"
                      "<p>Do you wish to proceed?</p>").arg(names.join(", ")),
                   0, /* auto-confirm id */
                   QIMessageBox::Ok,
                   QIMessageBox::Cancel | QIMessageBox::Escape | QIMessageBox::Default,
                   0,
                   tr("Remove"));
}

int UIMessageCenter::confirmMachineDeletion(const QList<CMachine> &machines)
{
    /* Enumerate VMs: */
    int cInacessibleVMCount = 0;
    bool fVMWithHDPresent = false;
    QString strVMNames;
    for (int i = 0; i < machines.size(); ++i)
    {
        /* Get iterated VM: */
        const CMachine &machine = machines[i];
        /* Prepare VM name: */
        QString strMachineName;
        if (machine.GetAccessible())
        {
            /* Get VM name: */
            strMachineName = machine.GetName();
            /* Enumerate attachments: */
            const CMediumAttachmentVector &attachments = machine.GetMediumAttachments();
            for (int i = 0; !fVMWithHDPresent && i < attachments.size(); ++i)
            {
                /* Get current attachment: */
                const CMediumAttachment &attachment = attachments.at(i);
                /* Check if the medium is a hard disk: */
                if (attachment.GetType() == KDeviceType_HardDisk)
                {
                    /* Check if that hard disk isn't shared.
                     * If hard disk is shared, it will *never* be deleted: */
                    QVector<QString> ids = attachment.GetMedium().GetMachineIds();
                    if (ids.size() == 1)
                    {
                        fVMWithHDPresent = true;
                        break;
                    }
                }
            }
        }
        else
        {
            /* Get VM name: */
            QFileInfo fi(machine.GetSettingsFilePath());
            strMachineName = VBoxGlobal::hasAllowedExtension(fi.completeSuffix(), VBoxFileExts) ? fi.completeBaseName() : fi.fileName();
            /* Increment inacessible VM count: */
            ++cInacessibleVMCount;
        }

        /* Compose VM name list: */
        strVMNames += QString(strVMNames.isEmpty() ? "<b>%1</b>" : ", <b>%1</b>").arg(strMachineName);
    }

    /* Prepare message text: */
    QString strText = cInacessibleVMCount == machines.size() ?
                      tr("<p>You are about to remove following inaccessible virtual machines from the machine list:</p>"
                         "<p>%1</p>"
                         "<p>Do you wish to proceed?</p>")
                         .arg(strVMNames) :
                      fVMWithHDPresent ?
                      tr("<p>You are about to remove following virtual machines from the machine list:</p>"
                         "<p>%1</p>"
                         "<p>Would you like to delete the files containing the virtual machine from your hard disk as well? "
                         "Doing this will also remove the files containing the machine's virtual hard disks "
                         "if they are not in use by another machine.</p>")
                         .arg(strVMNames) :
                      tr("<p>You are about to remove following virtual machines from the machine list:</p>"
                         "<p>%1</p>"
                         "<p>Would you like to delete the files containing the virtual machine from your hard disk as well?</p>")
                         .arg(strVMNames);

    /* Prepare message itself: */
    return cInacessibleVMCount == machines.size() ?
           message(&vboxGlobal().selectorWnd(),
                   MessageType_Question,
                   strText,
                   0, /* auto-confirm id */
                   QIMessageBox::Ok,
                   QIMessageBox::Cancel | QIMessageBox::Escape | QIMessageBox::Default,
                   0,
                   tr("Remove")) :
           message(&vboxGlobal().selectorWnd(),
                   MessageType_Question,
                   strText,
                   0, /* auto-confirm id */
                   QIMessageBox::Yes,
                   QIMessageBox::No,
                   QIMessageBox::Cancel | QIMessageBox::Escape | QIMessageBox::Default,
                   tr("Delete all files"),
                   tr("Remove only"));
}

bool UIMessageCenter::confirmDiscardSavedState(const QString &strNames)
{
    return messageOkCancel(&vboxGlobal().selectorWnd(), MessageType_Question,
        tr("<p>Are you sure you want to discard the saved state of "
           "the following virtual machines?</p><p><b>%1</b></p>"
           "<p>This operation is equivalent to resetting or powering off "
           "the machine without doing a proper shutdown of the guest OS.</p>")
           .arg(strNames),
        0 /* pcszAutoConfirmId */,
        tr("Discard", "saved state"));
}

void UIMessageCenter::cannotSetGroups(const CMachine &machine)
{
    /* Preserve error-info: */
    COMResult res(machine);
    /* Compose machine name: */
    QString strName = machine.GetName();
    if (strName.isEmpty())
        strName = QFileInfo(machine.GetSettingsFilePath()).baseName();
    /* Show the message: */
    message(mainWindowShown(), MessageType_Error,
            tr("Failed to set groups of the virtual machine <b>%1</b>.").arg(strName),
            formatErrorInfo(res));
}

bool UIMessageCenter::remindAboutInaccessibleMedia()
{
    int rc = message(&vboxGlobal().selectorWnd(), MessageType_Warning,
        tr("<p>One or more virtual hard disks, CD/DVD or "
           "floppy media are not currently accessible. As a result, you will "
           "not be able to operate virtual machines that use these media until "
           "they become accessible later.</p>"
           "<p>Press <b>Check</b> to open the Virtual Media Manager window and "
           "see what media are inaccessible, or press <b>Ignore</b> to "
           "ignore this message.</p>"),
        "remindAboutInaccessibleMedia",
        QIMessageBox::Ok | QIMessageBox::Default,
        QIMessageBox::Ignore | QIMessageBox::Escape,
        0,
        tr("Check", "inaccessible media message box"));

    return rc == QIMessageBox::Ok; /* implies !AutoConfirmed */
}

bool UIMessageCenter::confirmVMReset(const QString &strNames)
{
    return messageOkCancel(mainMachineWindowShown(), MessageType_Question,
        tr("<p>Do you really want to reset the following virtual machines?</p>"
           "<p><b>%1</b></p><p>This will cause any unsaved data "
           "in applications running inside it to be lost.</p>")
           .arg(strNames),
        "confirmVMReset" /* pcszAutoConfirmId */,
        tr("Reset", "machine"));
}

bool UIMessageCenter::confirmVMACPIShutdown(const QString &strNames)
{
    return messageOkCancel(mainMachineWindowShown(), MessageType_Question,
        tr("<p>Do you really want to send an ACPI shutdown signal "
           "to the following virtual machines?</p><p><b>%1</b></p>")
           .arg(strNames),
        "confirmVMACPIShutdown" /* pcszAutoConfirmId */,
        tr("ACPI Shutdown", "machine"));
}

bool UIMessageCenter::confirmVMPowerOff(const QString &strNames)
{
    return messageOkCancel(mainMachineWindowShown(), MessageType_Question,
        tr("<p>Do you really want to power off the following virtual machines?</p>"
           "<p><b>%1</b></p><p>This will cause any unsaved data in applications "
           "running inside it to be lost.</p>")
           .arg(strNames),
        "confirmVMPowerOff" /* pcszAutoConfirmId */,
        tr("Power Off", "machine"));
}

int UIMessageCenter::askAboutSnapshotRestoring(const QString &strSnapshotName, bool fAlsoCreateNewSnapshot)
{
    return fAlsoCreateNewSnapshot ?
           messageWithOption(mainWindowShown(), MessageType_Question,
                             tr("<p>You are about to restore snapshot <nobr><b>%1</b></nobr>.</p>"
                                "<p>You can create a snapshot of the current state of the virtual machine first by checking the box below; "
                                "if you do not do this the current state will be permanently lost. Do you wish to proceed?</p>")
                                .arg(strSnapshotName),
                             tr("Create a snapshot of the current machine state"),
                             !vboxGlobal().virtualBox().GetExtraDataStringList(GUI_InvertMessageOption).contains("askAboutSnapshotRestoring"),
                             QString() /* details */,
                             QIMessageBox::Ok | QIMessageBox::Default,
                             QIMessageBox::Cancel | QIMessageBox::Escape,
                             0 /* 3rd button */,
                             tr("Restore"), tr("Cancel"), QString() /* 3rd button text */) :
           message(mainWindowShown(), MessageType_Question,
                   tr("<p>Are you sure you want to restore snapshot <nobr><b>%1</b></nobr>?</p>").arg(strSnapshotName),
                   0 /* auto-confirmation token */,
                   QIMessageBox::Ok | QIMessageBox::Default,
                   QIMessageBox::Cancel | QIMessageBox::Escape,
                   0 /* 3rd button */,
                   tr("Restore"), tr("Cancel"), QString() /* 3rd button text */);
}

bool UIMessageCenter::askAboutSnapshotDeleting(const QString &strSnapshotName)
{
    return messageOkCancel(mainWindowShown(), MessageType_Question,
        tr("<p>Deleting the snapshot will cause the state information saved in it to be lost, and disk data spread over "
            "several image files that VirtualBox has created together with the snapshot will be merged into one file. This can be a lengthy process, and the information "
            "in the snapshot cannot be recovered.</p></p>Are you sure you want to delete the selected snapshot <b>%1</b>?</p>")
            .arg(strSnapshotName),
        /* Do NOT allow this message to be disabled! */
        NULL /* pcszAutoConfirmId */,
        tr("Delete"), tr("Cancel"));
}

bool UIMessageCenter::askAboutSnapshotDeletingFreeSpace(const QString &strSnapshotName,
                                                        const QString &strTargetImageName,
                                                        const QString &strTargetImageMaxSize,
                                                        const QString &strTargetFileSystemFree)
{
    return messageOkCancel(mainWindowShown(), MessageType_Question,
        tr("<p>Deleting the snapshot %1 will temporarily need more disk space. In the worst case the size of image %2 will grow by %3, "
            "however on this filesystem there is only %4 free.</p><p>Running out of disk space during the merge operation can result in "
            "corruption of the image and the VM configuration, i.e. loss of the VM and its data.</p><p>You may continue with deleting "
            "the snapshot at your own risk.</p>")
            .arg(strSnapshotName)
            .arg(strTargetImageName)
            .arg(strTargetImageMaxSize)
            .arg(strTargetFileSystemFree),
        /* Do NOT allow this message to be disabled! */
        NULL /* pcszAutoConfirmId */,
        tr("Delete"), tr("Cancel"));
}

void UIMessageCenter::cannotRestoreSnapshot(const CConsole &console,
                                            const QString &strSnapshotName)
{
    message(mainWindowShown(), MessageType_Error,
        tr("Failed to restore the snapshot <b>%1</b> of the virtual machine <b>%2</b>.")
            .arg(strSnapshotName)
            .arg(CConsole(console).GetMachine().GetName()),
        formatErrorInfo(console));
}

void UIMessageCenter::cannotRestoreSnapshot(const CProgress &progress,
                                            const QString &strSnapshotName)
{
    CConsole console(CProgress(progress).GetInitiator());

    message(mainWindowShown(), MessageType_Error,
        tr("Failed to restore the snapshot <b>%1</b> of the virtual machine <b>%2</b>.")
            .arg(strSnapshotName)
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(progress.GetErrorInfo()));
}

void UIMessageCenter::cannotDeleteSnapshot(const CConsole &console,
                                           const QString &strSnapshotName)
{
    message(mainWindowShown(), MessageType_Error,
        tr("Failed to delete the snapshot <b>%1</b> of the virtual machine <b>%2</b>.")
            .arg(strSnapshotName)
            .arg(CConsole(console).GetMachine().GetName()),
        formatErrorInfo(console));
}

void UIMessageCenter::cannotDeleteSnapshot(const CProgress &progress,
                                           const QString &strSnapshotName)
{
    CConsole console(CProgress(progress).GetInitiator());

    message(mainWindowShown(), MessageType_Error,
        tr("Failed to delete the snapshot <b>%1</b> of the virtual machine <b>%2</b>.")
            .arg(strSnapshotName)
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(progress.GetErrorInfo()));
}

void UIMessageCenter::cannotFindSnapshotByName(QWidget *pParent,
                                               const CMachine &machine,
                                               const QString &strName) const
{
    message(pParent ? pParent : mainWindowShown(), MessageType_Error,
             tr("Can't find snapshot named <b>%1</b>.")
             .arg(strName),
             formatErrorInfo(machine)
    );
}

void UIMessageCenter::cannotSetSystemProperties(const CSystemProperties &properties)
{
    message(mainWindowShown(), MessageType_Critical,
            tr("Failed to set global VirtualBox properties."),
            formatErrorInfo(properties));
}

void UIMessageCenter::cannotAccessUSB(const COMBaseWithEI &object)
{
    /* If IMachine::GetUSBController(), IHost::GetUSBDevices() etc. return
     * E_NOTIMPL, it means the USB support is intentionally missing
     * (as in the OSE version). Don't show the error message in this case. */
    COMResult res(object);
    if (res.rc() == E_NOTIMPL)
        return;

    message(mainWindowShown(), res.isWarning() ? MessageType_Warning : MessageType_Error,
             tr("Failed to access the USB subsystem."),
             formatErrorInfo(res),
             "cannotAccessUSB");
}

void UIMessageCenter::cannotLoadMachineSettings(const CMachine &machine, bool fStrict /*= true*/, QWidget *pParent /*= 0*/)
{
    /* This function is NOT use currently.
     * We are keeping it here just for convinience with Save analog. */

    /* Preserve error-info.
     * If COM result code is E_NOTIMPL, it means the requested
     * object or function is intentionally missing (as in the OSE version).
     * Don't show the error message in this case. */
    COMResult res(machine);
    if (!fStrict && res.rc() == E_NOTIMPL)
        return;

    /* Show the message: */
    message(pParent ? pParent : mainWindowShown(), MessageType_Error,
            tr("Failed to load the settings of the virtual machine "
               "<b>%1</b> from <b><nobr>%2</nobr></b>.")
               .arg(machine.GetName(), machine.GetSettingsFilePath()),
            formatErrorInfo(res));
}

void UIMessageCenter::cannotSaveMachineSettings(const CMachine &machine, QWidget *pParent /*= 0*/)
{
    /* Preserve error-info: */
    COMResult res(machine);
    /* Show the message: */
    message(pParent ? pParent : mainWindowShown(), MessageType_Error,
            tr("Failed to save the settings of the virtual machine "
               "<b>%1</b> to <b><nobr>%2</nobr></b>.")
               .arg(machine.GetName(), machine.GetSettingsFilePath()),
            formatErrorInfo(res));
}

void UIMessageCenter::warnAboutStateChange(QWidget *pParent)
{
    if (warningShown("warnAboutStateChange"))
        return;
    setWarningShown("warnAboutStateChange", true);

    message(pParent ? pParent : mainWindowShown(), MessageType_Warning,
            tr("The virtual machine that you are changing has been started. "
               "Only certain settings can be changed while a machine is running. "
               "All other changes will be lost if you close this window now."));

    setWarningShown("warnAboutStateChange", false);
}

bool UIMessageCenter::confirmSettingsReloading(QWidget *pParent)
{
    int rc = message(pParent, MessageType_Question,
                     tr("<p>The machine settings were changed while you were editing them. "
                        "You currently have unsaved setting changes.</p>"
                        "<p>Would you like to reload the changed settings or to keep your own changes?</p>"), 0,
                     QIMessageBox::Yes, QIMessageBox::No | QIMessageBox::Default | QIMessageBox::Escape, 0,
                     tr("Reload settings"), tr("Keep changes"), 0);
    return rc == QIMessageBox::Yes;
}

int UIMessageCenter::askAboutHardDiskAttachmentCreation(QWidget *pParent,
                                                        const QString &strControllerName)
{
    return message(pParent, MessageType_Question,
                   tr("<p>You are about to add a virtual hard disk to controller <b>%1</b>.</p>"
                      "<p>Would you like to create a new, empty file to hold the disk contents or select an existing one?</p>")
                   .arg(strControllerName),
                   0, /* pcszAutoConfirmId */
                   QIMessageBox::Yes,
                   QIMessageBox::No,
                   QIMessageBox::Cancel | QIMessageBox::Default | QIMessageBox::Escape,
                   tr("Create &new disk", "add attachment routine"),
                   tr("&Choose existing disk", "add attachment routine"));
}

int UIMessageCenter::askAboutOpticalAttachmentCreation(QWidget *pParent,
                                                       const QString &strControllerName)
{
    return message(pParent, MessageType_Question,
                   tr("<p>You are about to add a new CD/DVD drive to controller <b>%1</b>.</p>"
                      "<p>Would you like to choose a virtual CD/DVD disk to put in the drive "
                      "or to leave it empty for now?</p>")
                   .arg(strControllerName),
                   0, /* pcszAutoConfirmId */
                   QIMessageBox::Yes,
                   QIMessageBox::No,
                   QIMessageBox::Cancel | QIMessageBox::Default | QIMessageBox::Escape,
                   tr("&Choose disk", "add attachment routine"),
                   tr("Leave &empty", "add attachment routine"));
}

int UIMessageCenter::askAboutFloppyAttachmentCreation(QWidget *pParent,
                                                      const QString &strControllerName)
{
    return message(pParent, MessageType_Question,
                   tr("<p>You are about to add a new floppy drive to controller <b>%1</b>.</p>"
                      "<p>Would you like to choose a virtual floppy disk to put in the drive "
                      "or to leave it empty for now?</p>")
                   .arg(strControllerName),
                   0, /* pcszAutoConfirmId */
                   QIMessageBox::Yes,
                   QIMessageBox::No,
                   QIMessageBox::Cancel | QIMessageBox::Default | QIMessageBox::Escape,
                   tr("&Choose disk", "add attachment routine"),
                   tr("Leave &empty", "add attachment routine"));
}

int UIMessageCenter::confirmRemovingOfLastDVDDevice() const
{
    return messageOkCancel(QApplication::activeWindow(), MessageType_Info,
                            tr("<p>Are you sure you want to delete the CD/DVD-ROM device?</p>"
                               "<p>You will not be able to mount any CDs or ISO images "
                               "or install the Guest Additions without it!</p>"),
                            0, /* pcszAutoConfirmId */
                            tr("&Remove", "medium"));
}

int UIMessageCenter::confirmDeletingHostInterface(const QString &strName,
                                                  QWidget *pParent)
{
    return msgCenter().message(pParent, MessageType_Question,
        tr("<p>Deleting this host-only network will remove "
           "the host-only interface this network is based on. Do you want to "
           "remove the (host-only network) interface <nobr><b>%1</b>?</nobr></p>"
           "<p><b>Note:</b> this interface may be in use by one or more "
           "virtual network adapters belonging to one of your VMs. "
           "After it is removed, these adapters will no longer be usable until "
           "you correct their settings by either choosing a different interface "
           "name or a different adapter attachment type.</p>").arg(strName),
        0, /* pcszAutoConfirmId */
        QIMessageBox::Ok | QIMessageBox::Default,
        QIMessageBox::Cancel | QIMessageBox::Escape);
}

void UIMessageCenter::warnAboutIncorrectPort(QWidget *pParent) const
{
    message(pParent, MessageType_Error,
            tr("The current port forwarding rules are not valid. "
               "None of the host or guest port values may be set to zero."));
}

bool UIMessageCenter::confirmCancelingPortForwardingDialog(QWidget *pParent) const
{
    return messageOkCancel(pParent, MessageType_Question,
        tr("<p>There are unsaved changes in the port forwarding configuration.</p>"
           "<p>If you proceed your changes will be discarded.</p>"));
}

void UIMessageCenter::cannotChangeMediumType(QWidget *pParent,
                                             const CMedium &medium,
                                             KMediumType oldMediumType,
                                             KMediumType newMediumType)
{
    message(pParent ? pParent : mainWindowShown(), MessageType_Error,
            tr("<p>Error changing medium type from <b>%1</b> to <b>%2</b>.</p>")
                .arg(gpConverter->toString(oldMediumType)).arg(gpConverter->toString(newMediumType)),
            formatErrorInfo(medium));
}

bool UIMessageCenter::confirmReleaseMedium(QWidget *pParent,
                                           const UIMedium &aMedium,
                                           const QString &strUsage)
{
    /** @todo (translation-related): the gender of "the" in translations
     * will depend on the gender of aMedium.type(). */
    return messageOkCancel(pParent, MessageType_Question,
        tr("<p>Are you sure you want to release the %1 "
            "<nobr><b>%2</b></nobr>?</p>"
            "<p>This will detach it from the "
            "following virtual machine(s): <b>%3</b>.</p>")
            .arg(mediumToAccusative(aMedium.type()))
            .arg(aMedium.location())
            .arg(strUsage),
        0 /* pcszAutoConfirmId */,
        tr("Release", "detach medium"));
}

bool UIMessageCenter::confirmRemoveMedium(QWidget *pParent,
                                          const UIMedium &aMedium)
{
    /** @todo (translation-related): the gender of "the" in translations
     * will depend on the gender of aMedium.type(). */
    QString msg =
        tr("<p>Are you sure you want to remove the %1 "
           "<nobr><b>%2</b></nobr> from the list of known media?</p>")
            .arg(mediumToAccusative(aMedium.type()))
            .arg(aMedium.location());

    qulonglong caps = 0;
    QVector<KMediumFormatCapabilities> capabilities;
    capabilities = aMedium.medium().GetMediumFormat().GetCapabilities();
    for (int i = 0; i < capabilities.size(); i++)
        caps |= capabilities[i];

    if (aMedium.type() == UIMediumType_HardDisk &&
        caps & MediumFormatCapabilities_File)
    {
        if (aMedium.state() == KMediumState_Inaccessible)
            msg +=
                tr("Note that as this hard disk is inaccessible its "
                   "storage unit cannot be deleted right now.");
        else
            msg +=
                tr("The next dialog will let you choose whether you also "
                   "want to delete the storage unit of this hard disk or "
                   "keep it for later usage.");
    }
    else
        msg +=
            tr("<p>Note that the storage unit of this medium will not be "
               "deleted and that it will be possible to use it later again.</p>");

    return messageOkCancel(pParent, MessageType_Question, msg,
        "confirmRemoveMedium", /* pcszAutoConfirmId */
        tr("Remove", "medium"));
}

int UIMessageCenter::confirmDeleteHardDiskStorage(QWidget *pParent, const QString &strLocation)
{
    return message(pParent, MessageType_Question,
        tr("<p>Do you want to delete the storage unit of the hard disk "
           "<nobr><b>%1</b></nobr>?</p>"
           "<p>If you select <b>Delete</b> then the specified storage unit "
           "will be permanently deleted. This operation <b>cannot be "
           "undone</b>.</p>"
           "<p>If you select <b>Keep</b> then the hard disk will be only "
           "removed from the list of known hard disks, but the storage unit "
           "will be left untouched which makes it possible to add this hard "
           "disk to the list later again.</p>")
        .arg(strLocation),
        0, /* pcszAutoConfirmId */
        QIMessageBox::Yes,
        QIMessageBox::No | QIMessageBox::Default,
        QIMessageBox::Cancel | QIMessageBox::Escape,
        tr("Delete", "hard disk storage"),
        tr("Keep", "hard disk storage"));
}

void UIMessageCenter::cannotDeleteHardDiskStorage(QWidget *pParent,
                                                  const CMedium &medium,
                                                  const CProgress &progress)
{
    /* below, we use CMedium (medium) to preserve current error info
     * for formatErrorInfo() */

    message(pParent, MessageType_Error,
        tr("Failed to delete the storage unit of the hard disk <b>%1</b>.")
        .arg(CMedium(medium).GetLocation()),
        !medium.isOk() ? formatErrorInfo(medium) :
        !progress.isOk() ? formatErrorInfo(progress) :
        formatErrorInfo(progress.GetErrorInfo()));
}

void UIMessageCenter::cannotDetachDevice(QWidget *pParent,
                                         const CMachine &machine,
                                         UIMediumType type,
                                         const QString &strLocation,
                                         const StorageSlot &storageSlot)
{
    QString strMessage;
    switch (type)
    {
        case UIMediumType_HardDisk:
        {
            strMessage = tr("Failed to detach the hard disk (<nobr><b>%1</b></nobr>) from the slot <i>%2</i> of the machine <b>%3</b>.")
                           .arg(strLocation).arg(gpConverter->toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        case UIMediumType_DVD:
        {
            strMessage = tr("Failed to detach the CD/DVD device (<nobr><b>%1</b></nobr>) from the slot <i>%2</i> of the machine <b>%3</b>.")
                           .arg(strLocation).arg(gpConverter->toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        case UIMediumType_Floppy:
        {
            strMessage = tr("Failed to detach the floppy device (<nobr><b>%1</b></nobr>) from the slot <i>%2</i> of the machine <b>%3</b>.")
                           .arg(strLocation).arg(gpConverter->toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        default:
            break;
    }
    message(pParent ? pParent : mainWindowShown(), MessageType_Error, strMessage, formatErrorInfo(machine));
}

int UIMessageCenter::cannotRemountMedium(QWidget *pParent,
                                         const CMachine &machine,
                                         const UIMedium &aMedium,
                                         bool fMount,
                                         bool fRetry)
{
    /** @todo (translation-related): the gender of "the" in translations
     * will depend on the gender of aMedium.type(). */
    QString text;
    if (fMount)
    {
        text = tr("Unable to mount the %1 <nobr><b>%2</b></nobr> on the machine <b>%3</b>.");
        if (fRetry) text += tr(" Would you like to force mounting of this medium?");
    }
    else
    {
        text = tr("Unable to unmount the %1 <nobr><b>%2</b></nobr> from the machine <b>%3</b>.");
        if (fRetry) text += tr(" Would you like to force unmounting of this medium?");
    }
    if (fRetry)
    {
        return messageOkCancel(pParent ? pParent : mainWindowShown(), MessageType_Question, text
            .arg(mediumToAccusative(aMedium.type(), aMedium.isHostDrive()))
            .arg(aMedium.isHostDrive() ? aMedium.name() : aMedium.location())
            .arg(CMachine(machine).GetName()),
            formatErrorInfo(machine),
            0 /* Auto Confirm ID */,
            tr("Force Unmount"));
    }
    else
    {
        return message(pParent ? pParent : mainWindowShown(), MessageType_Error, text
            .arg(mediumToAccusative(aMedium.type(), aMedium.isHostDrive()))
            .arg(aMedium.isHostDrive() ? aMedium.name() : aMedium.location())
            .arg(CMachine(machine).GetName()),
            formatErrorInfo(machine));
    }
}

void UIMessageCenter::cannotOpenMedium(QWidget *pParent,
                                       const CVirtualBox &vbox,
                                       UIMediumType type,
                                       const QString &strLocation)
{
    /** @todo (translation-related): the gender of "the" in translations
     * will depend on the gender of aMedium.type(). */
    message(pParent ? pParent : mainWindowShown(), MessageType_Error,
        tr("Failed to open the %1 <nobr><b>%2</b></nobr>.")
            .arg(mediumToAccusative(type))
            .arg(strLocation),
        formatErrorInfo(vbox));
}

void UIMessageCenter::cannotCloseMedium(QWidget *pParent,
                                        const UIMedium &aMedium,
                                        const COMResult &rc)
{
    /** @todo (translation-related): the gender of "the" in translations
     * will depend on the gender of aMedium.type(). */
    message(pParent, MessageType_Error,
        tr("Failed to close the %1 <nobr><b>%2</b></nobr>.")
            .arg(mediumToAccusative(aMedium.type()))
            .arg(aMedium.location()),
        formatErrorInfo(rc));
}

void UIMessageCenter::cannotCreateMachine(const CVirtualBox &vbox, QWidget *pParent /* = 0 */)
{
    message(
        pParent ? pParent : mainWindowShown(),
        MessageType_Error,
        tr("Failed to create a new virtual machine."),
        formatErrorInfo(vbox)
    );
}

void UIMessageCenter::cannotCreateMachine(const CVirtualBox &vbox,
                                          const CMachine &machine,
                                          QWidget *pParent /* = 0 */)
{
    message(
        pParent ? pParent : mainWindowShown(),
        MessageType_Error,
        tr("Failed to create a new virtual machine <b>%1</b>.")
            .arg(machine.GetName()),
        formatErrorInfo(vbox)
    );
}

void UIMessageCenter::cannotRegisterMachine(const CVirtualBox &vbox,
                                            const CMachine &machine,
                                            QWidget *pParent)
{
    message(pParent ? pParent : mainWindowShown(),
            MessageType_Error,
            tr("Failed to register the virtual machine <b>%1</b>.")
            .arg(machine.GetName()),
            formatErrorInfo(vbox));
}

void UIMessageCenter::cannotCreateClone(const CMachine &machine,
                                        QWidget *pParent /* = 0 */)
{
    message(
        pParent ? pParent : mainWindowShown(),
        MessageType_Error,
        tr("Failed to clone the virtual machine <b>%1</b>.")
            .arg(machine.GetName()),
        formatErrorInfo(machine)
    );
}

void UIMessageCenter::cannotCreateClone(const CMachine &machine,
                                        const CProgress &progress,
                                        QWidget *pParent /* = 0 */)
{
    AssertWrapperOk(progress);

    message(pParent ? pParent : mainWindowShown(),
        MessageType_Error,
        tr("Failed to clone the virtual machine <b>%1</b>.")
            .arg(machine.GetName()),
        formatErrorInfo(progress.GetErrorInfo())
    );
}

void UIMessageCenter::cannotOverwriteHardDiskStorage(QWidget *pParent,
                                                     const QString &strLocation)
{
    message(pParent, MessageType_Info,
        tr("<p>The hard disk storage unit at location <b>%1</b> already "
           "exists. You cannot create a new virtual hard disk that uses this "
           "location because it can be already used by another virtual hard "
           "disk.</p>"
           "<p>Please specify a different location.</p>")
        .arg(strLocation));
}

void UIMessageCenter::cannotCreateHardDiskStorage(QWidget *pParent,
                                                  const CVirtualBox &vbox,
                                                  const QString &strLocation,
                                                  const CMedium &medium,
                                                  const CProgress &progress)
{
    message(pParent, MessageType_Error,
        tr("Failed to create the hard disk storage <nobr><b>%1</b>.</nobr>")
        .arg(strLocation),
        !vbox.isOk() ? formatErrorInfo(vbox) :
        !medium.isOk() ? formatErrorInfo(medium) :
        !progress.isOk() ? formatErrorInfo(progress) :
        formatErrorInfo(progress.GetErrorInfo()));
}

void UIMessageCenter::warnAboutCannotRemoveMachineFolder(QWidget *pParent, const QString &strFolderName)
{
    QFileInfo fi(strFolderName);
    message(pParent ? pParent : mainWindowShown(), MessageType_Critical,
            tr("<p>Cannot remove the machine folder <nobr><b>%1</b>.</nobr></p>"
               "<p>Please check that this folder really exists and that you have permissions to remove it.</p>")
               .arg(fi.fileName()));
}

void UIMessageCenter::warnAboutCannotRewriteMachineFolder(QWidget *pParent, const QString &strFolderName)
{
    QFileInfo fi(strFolderName);
    message(pParent ? pParent : mainWindowShown(), MessageType_Critical,
            tr("<p>Cannot create the machine folder <b>%1</b> in the parent folder <nobr><b>%2</b>.</nobr></p>"
               "<p>This folder already exists and possibly belongs to another machine.</p>")
               .arg(fi.fileName()).arg(fi.absolutePath()));
}

void UIMessageCenter::warnAboutCannotCreateMachineFolder(QWidget *pParent, const QString &strFolderName)
{
    QFileInfo fi(strFolderName);
    message(pParent ? pParent : mainWindowShown(), MessageType_Critical,
            tr("<p>Cannot create the machine folder <b>%1</b> in the parent folder <nobr><b>%2</b>.</nobr></p>"
               "<p>Please check that the parent really exists and that you have permissions to create the machine folder.</p>")
               .arg(fi.fileName()).arg(fi.absolutePath()));
}

bool UIMessageCenter::confirmHardDisklessMachine(QWidget *pParent)
{
    return message(pParent, MessageType_Warning,
        tr("You are about to create a new virtual machine without a hard drive. "
           "You will not be able to install an operating system on the machine "
           "until you add one. In the mean time you will only be able to start the "
           "machine using a virtual optical disk or from the network."),
        0, /* pcszAutoConfirmId */
        QIMessageBox::Ok,
        QIMessageBox::Cancel | QIMessageBox::Default | QIMessageBox::Escape,
        0,
        tr("Continue", "no hard disk attached"),
        tr("Go Back", "no hard disk attached")) == QIMessageBox::Ok;
}

void UIMessageCenter::cannotImportAppliance(CAppliance *pAppliance,
                                            QWidget *pParent /* = NULL */) const
{
    if (pAppliance->isNull())
    {
        message(pParent ? pParent : mainWindowShown(),
                MessageType_Error,
                tr("Failed to open appliance."));
    }
    else
    {
        /* Preserve the current error info before calling the object again */
        COMResult res(*pAppliance);

        /* Add the warnings in the case of an early error */
        QVector<QString> w = pAppliance->GetWarnings();
        QString wstr;
        foreach(const QString &str, w)
            wstr += QString("<br />Warning: %1").arg(str);
        if (!wstr.isEmpty())
            wstr = "<br />" + wstr;

        message(pParent ? pParent : mainWindowShown(),
                MessageType_Error,
                tr("Failed to open/interpret appliance <b>%1</b>.").arg(pAppliance->GetPath()),
                wstr +
                formatErrorInfo(res));
    }
}

void UIMessageCenter::cannotImportAppliance(const CProgress &progress,
                                            CAppliance* pAppliance,
                                            QWidget *pParent /* = NULL */) const
{
    AssertWrapperOk(progress);

    message(pParent ? pParent : mainWindowShown(),
            MessageType_Error,
            tr("Failed to import appliance <b>%1</b>.").arg(pAppliance->GetPath()),
            formatErrorInfo(progress.GetErrorInfo()));
}

void UIMessageCenter::cannotCheckFiles(const CProgress &progress,
                                       QWidget *pParent /* = NULL */) const
{
    AssertWrapperOk(progress);

    message(pParent ? pParent : mainWindowShown(),
            MessageType_Error,
            tr("Failed to check files."),
            formatErrorInfo(progress.GetErrorInfo()));
}

void UIMessageCenter::cannotRemoveFiles(const CProgress &progress,
                                        QWidget *pParent /* = NULL */) const
{
    AssertWrapperOk(progress);

    message(pParent ? pParent : mainWindowShown(),
            MessageType_Error,
            tr("Failed to remove file."),
            formatErrorInfo(progress.GetErrorInfo()));
}

bool UIMessageCenter::confirmExportMachinesInSaveState(const QStringList &strMachineNames,
                                                       QWidget *pParent /* = NULL */) const
{
    return messageOkCancel(pParent ? pParent : mainWindowShown(), MessageType_Warning,
        tr("<p>The %n following virtual machine(s) are currently in a saved state: <b>%1</b></p>"
           "<p>If you continue the runtime state of the exported machine(s) "
           "will be discarded. The other machine(s) will not be changed.</p>", "This text is never used with n == 0.  Feel free to drop the %n where possible, we only included it because of problems with Qt Linguist (but the user can see how many machines are in the list and doesn't need to be told).",
           strMachineNames.size()).arg(VBoxGlobal::toHumanReadableList(strMachineNames)),
        0 /* pcszAutoConfirmId */,
        tr("Continue"), tr("Cancel"));
}

void UIMessageCenter::cannotExportAppliance(CAppliance *pAppliance,
                                            QWidget *pParent /* = NULL */) const
{
    if (pAppliance->isNull())
    {
        message(pParent ? pParent : mainWindowShown(),
                MessageType_Error,
                tr("Failed to create appliance."));
    }
    else
    {
        /* Preserve the current error info before calling the object again */
        COMResult res(*pAppliance);

        message(pParent ? pParent : mainWindowShown(),
                MessageType_Error,
                tr("Failed to prepare the export of the appliance <b>%1</b>.").arg(pAppliance->GetPath()),
                formatErrorInfo(res));
    }
}

void UIMessageCenter::cannotExportAppliance(const CMachine &machine,
                                            CAppliance *pAppliance,
                                            QWidget *pParent /* = NULL */) const
{
    if (pAppliance->isNull() ||
        machine.isNull())
    {
        message(pParent ? pParent : mainWindowShown(),
                MessageType_Error,
                tr("Failed to create an appliance."));
    }
    else
    {
        message(pParent ? pParent : mainWindowShown(),
                MessageType_Error,
                tr("Failed to prepare the export of the appliance <b>%1</b>.").arg(pAppliance->GetPath()),
                formatErrorInfo(machine));
    }
}

void UIMessageCenter::cannotExportAppliance(const CProgress &progress,
                                            CAppliance* pAppliance,
                                            QWidget *pParent /* = NULL */) const
{
    AssertWrapperOk(progress);

    message(pParent ? pParent : mainWindowShown(),
            MessageType_Error,
            tr("Failed to export appliance <b>%1</b>.").arg(pAppliance->GetPath()),
            formatErrorInfo(progress.GetErrorInfo()));
}

void UIMessageCenter::showRuntimeError(const CConsole &console, bool fFatal,
                                       const QString &strErrorId,
                                       const QString &strErrorMsg) const
{
    QByteArray autoConfimId = "showRuntimeError.";

    CConsole console1 = console;
    KMachineState state = console1.GetState();
    MessageType type;
    QString severity;

    if (fFatal)
    {
        /* the machine must be paused on fFatal errors */
        Assert(state == KMachineState_Paused);
        if (state != KMachineState_Paused)
            console1.Pause();
        type = MessageType_Critical;
        severity = tr("<nobr>Fatal Error</nobr>", "runtime error info");
        autoConfimId += "fatal.";
    }
    else if (state == KMachineState_Paused)
    {
        type = MessageType_Error;
        severity = tr("<nobr>Non-Fatal Error</nobr>", "runtime error info");
        autoConfimId += "error.";
    }
    else
    {
        type = MessageType_Warning;
        severity = tr("<nobr>Warning</nobr>", "runtime error info");
        autoConfimId += "warning.";
    }

    autoConfimId += strErrorId.toUtf8();

    QString formatted("<!--EOM-->");

    if (!strErrorMsg.isEmpty())
        formatted.prepend(QString("<p>%1.</p>").arg(vboxGlobal().emphasize(strErrorMsg)));

    if (!strErrorId.isEmpty())
        formatted += QString("<table bgcolor=#EEEEEE border=0 cellspacing=0 "
                             "cellpadding=0 width=100%>"
                             "<tr><td>%1</td><td>%2</td></tr>"
                             "<tr><td>%3</td><td>%4</td></tr>"
                             "</table>")
                              .arg(tr("<nobr>Error ID: </nobr>", "runtime error info"),
                                    strErrorId)
                              .arg(tr("Severity: ", "runtime error info"),
                                    severity);

    if (!formatted.isEmpty())
        formatted = "<qt>" + formatted + "</qt>";

    int rc = 0;

    if (type == MessageType_Critical)
    {
        rc = message(mainMachineWindowShown(), type,
            tr("<p>A fatal error has occurred during virtual machine execution! "
               "The virtual machine will be powered off. Please copy the "
               "following error message using the clipboard to help diagnose "
               "the problem:</p>"),
            formatted, autoConfimId.data());

        /* always power down after a fFatal error */
        console1.PowerDown();
    }
    else if (type == MessageType_Error)
    {
        rc = message(mainMachineWindowShown(), type,
            tr("<p>An error has occurred during virtual machine execution! "
               "The error details are shown below. You may try to correct "
               "the error and resume the virtual machine "
               "execution.</p>"),
            formatted, autoConfimId.data());
    }
    else
    {
        rc = message(mainMachineWindowShown(), type,
            tr("<p>The virtual machine execution may run into an error "
               "condition as described below. "
               "We suggest that you take "
               "an appropriate action to avert the error."
               "</p>"),
            formatted, autoConfimId.data());
    }

    NOREF(rc);
}

bool UIMessageCenter::warnAboutVirtNotEnabled64BitsGuest(bool fHWVirtExSupported)
{
    if (fHWVirtExSupported)
        return messageOkCancel(mainWindowShown(), MessageType_Error,
            tr("<p>VT-x/AMD-V hardware acceleration has been enabled, but is "
                "not operational. Your 64-bit guest will fail to detect a "
                "64-bit CPU and will not be able to boot.</p><p>Please ensure "
                "that you have enabled VT-x/AMD-V properly in the BIOS of your "
                "host computer.</p>"),
            0 /* pcszAutoConfirmId */,
            tr("Close VM"), tr("Continue"));
    else
        return messageOkCancel(mainWindowShown(), MessageType_Error,
            tr("<p>VT-x/AMD-V hardware acceleration is not available on your system. "
                "Your 64-bit guest will fail to detect a 64-bit CPU and will "
                "not be able to boot."),
            0 /* pcszAutoConfirmId */,
            tr("Close VM"), tr("Continue"));
}

bool UIMessageCenter::warnAboutVirtNotEnabledGuestRequired(bool fHWVirtExSupported)
{
    if (fHWVirtExSupported)
        return messageOkCancel(mainWindowShown(), MessageType_Error,
            tr("<p>VT-x/AMD-V hardware acceleration has been enabled, but is "
                "not operational. Certain guests (e.g. OS/2 and QNX) require "
                "this feature.</p><p>Please ensure "
                "that you have enabled VT-x/AMD-V properly in the BIOS of your "
                "host computer.</p>"),
            0 /* pcszAutoConfirmId */,
            tr("Close VM"), tr("Continue"));
    else
        return messageOkCancel(mainWindowShown(), MessageType_Error,
            tr("<p>VT-x/AMD-V hardware acceleration is not available on your system. "
                "Certain guests (e.g. OS/2 and QNX) require this feature and will "
                "fail to boot without it.</p>"),
            0 /* pcszAutoConfirmId */,
            tr("Close VM"), tr("Continue"));
}

bool UIMessageCenter::cannotStartWithoutNetworkIf(const QString &strMachineName,
                                                  const QString &strIfNames)
{
    return messageOkCancel(mainMachineWindowShown(), MessageType_Error,
             tr("<p>Could not start the machine <b>%1</b> because the "
                "following physical network interfaces were not found:</p>"
                "<p><b>%2</b></p>"
                "<p>You can either change the machine's network "
                "settings or stop the machine.</p>")
             .arg(strMachineName)
             .arg(strIfNames),
             0, /* pcszAutoConfirmId */
             tr("Change Network Settings"),
             tr("Close Virtual Machine"));
}

void UIMessageCenter::cannotStartMachine(const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res(console);

    message(mainWindowShown(), MessageType_Error,
        tr("Failed to start the virtual machine <b>%1</b>.")
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(res));
}

void UIMessageCenter::cannotStartMachine(const CProgress &progress)
{
    AssertWrapperOk(progress);
    CConsole console(CProgress(progress).GetInitiator());
    AssertWrapperOk(console);

    message(
        mainWindowShown(),
        MessageType_Error,
        tr("Failed to start the virtual machine <b>%1</b>.")
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(progress.GetErrorInfo())
    );
}

void UIMessageCenter::cannotPauseMachine(const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res(console);

    message(mainWindowShown(), MessageType_Error,
        tr("Failed to pause the execution of the virtual machine <b>%1</b>.")
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(res));
}

void UIMessageCenter::cannotResumeMachine(const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res(console);

    message(mainWindowShown(), MessageType_Error,
        tr("Failed to resume the execution of the virtual machine <b>%1</b>.")
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(res));
}

void UIMessageCenter::cannotACPIShutdownMachine(const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res(console);

    message(mainWindowShown(), MessageType_Error,
        tr("Failed to send the ACPI Power Button press event to the "
            "virtual machine <b>%1</b>.")
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(res));
}

void UIMessageCenter::cannotSaveMachineState(const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res(console);

    message(mainWindowShown(), MessageType_Error,
        tr("Failed to save the state of the virtual machine <b>%1</b>.")
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(res));
}

void UIMessageCenter::cannotSaveMachineState(const CProgress &progress)
{
    AssertWrapperOk(progress);
    CConsole console(CProgress(progress).GetInitiator());
    AssertWrapperOk(console);

    message(
        mainWindowShown(),
        MessageType_Error,
        tr("Failed to save the state of the virtual machine <b>%1</b>.")
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(progress.GetErrorInfo())
    );
}

void UIMessageCenter::cannotTakeSnapshot(const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res(console);

    message(mainWindowShown(), MessageType_Error,
        tr("Failed to create a snapshot of the virtual machine <b>%1</b>.")
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(res));
}

void UIMessageCenter::cannotTakeSnapshot(const CProgress &progress)
{
    AssertWrapperOk(progress);
    CConsole console(CProgress(progress).GetInitiator());
    AssertWrapperOk(console);

    message(
        mainWindowShown(),
        MessageType_Error,
        tr("Failed to create a snapshot of the virtual machine <b>%1</b>.")
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(progress.GetErrorInfo())
    );
}

void UIMessageCenter::cannotStopMachine(const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res(console);

    message(mainWindowShown(), MessageType_Error,
        tr("Failed to stop the virtual machine <b>%1</b>.")
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(res));
}

void UIMessageCenter::cannotStopMachine(const CProgress &progress)
{
    AssertWrapperOk(progress);
    CConsole console(CProgress(progress).GetInitiator());
    AssertWrapperOk(console);

    message(mainWindowShown(), MessageType_Error,
        tr("Failed to stop the virtual machine <b>%1</b>.")
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(progress.GetErrorInfo()));
}

void UIMessageCenter::cannotSendACPIToMachine()
{
    message(mainWindowShown(),  MessageType_Warning,
        tr("You are trying to shut down the guest with the ACPI power "
            "button. This is currently not possible because the guest "
            "does not support software shutdown."));
}

bool UIMessageCenter::confirmInputCapture(bool *pfAutoConfirmed /* = NULL */)
{
    int rc = message(mainMachineWindowShown(), MessageType_Info,
        tr("<p>You have <b>clicked the mouse</b> inside the Virtual Machine display "
           "or pressed the <b>host key</b>. This will cause the Virtual Machine to "
           "<b>capture</b> the host mouse pointer (only if the mouse pointer "
           "integration is not currently supported by the guest OS) and the "
           "keyboard, which will make them unavailable to other applications "
           "running on your host machine."
           "</p>"
           "<p>You can press the <b>host key</b> at any time to <b>uncapture</b> the "
           "keyboard and mouse (if it is captured) and return them to normal "
           "operation. The currently assigned host key is shown on the status bar "
           "at the bottom of the Virtual Machine window, next to the&nbsp;"
           "<img src=:/hostkey_16px.png/>&nbsp;icon. This icon, together "
           "with the mouse icon placed nearby, indicate the current keyboard "
           "and mouse capture state."
           "</p>") +
        tr("<p>The host key is currently defined as <b>%1</b>.</p>",
           "additional message box paragraph")
            .arg(UIHostCombo::toReadableString(vboxGlobal().settings().hostCombo())),
        "confirmInputCapture",
        QIMessageBox::Ok | QIMessageBox::Default,
        QIMessageBox::Cancel | QIMessageBox::Escape,
        0,
        tr("Capture", "do input capture"));

    if (pfAutoConfirmed)
        *pfAutoConfirmed = (rc & AutoConfirmed);

    return (rc & QIMessageBox::ButtonMask) == QIMessageBox::Ok;
}

void UIMessageCenter::remindAboutAutoCapture()
{
    message(mainMachineWindowShown(), MessageType_Info,
        tr("<p>You have the <b>Auto capture keyboard</b> option turned on. "
           "This will cause the Virtual Machine to automatically <b>capture</b> "
           "the keyboard every time the VM window is activated and make it "
           "unavailable to other applications running on your host machine: "
           "when the keyboard is captured, all keystrokes (including system ones "
           "like Alt-Tab) will be directed to the VM."
           "</p>"
           "<p>You can press the <b>host key</b> at any time to <b>uncapture</b> the "
           "keyboard and mouse (if it is captured) and return them to normal "
           "operation. The currently assigned host key is shown on the status bar "
           "at the bottom of the Virtual Machine window, next to the&nbsp;"
           "<img src=:/hostkey_16px.png/>&nbsp;icon. This icon, together "
           "with the mouse icon placed nearby, indicate the current keyboard "
           "and mouse capture state."
           "</p>") +
        tr("<p>The host key is currently defined as <b>%1</b>.</p>",
           "additional message box paragraph")
            .arg(UIHostCombo::toReadableString(vboxGlobal().settings().hostCombo())),
        "remindAboutAutoCapture");
}

void UIMessageCenter::remindAboutMouseIntegration(bool fSupportsAbsolute)
{
    if (warningShown("remindAboutMouseIntegration"))
        return;
    setWarningShown("remindAboutMouseIntegration", true);

    static const char *kNames [2] =
    {
        "remindAboutMouseIntegrationOff",
        "remindAboutMouseIntegrationOn"
    };

    /* Close the previous (outdated) window if any. We use kName as
     * pcszAutoConfirmId which is also used as the widget name by default. */
    {
        QWidget *outdated =
            VBoxGlobal::findWidget(NULL, kNames [int (!fSupportsAbsolute)],
                                    "QIMessageBox");
        if (outdated)
            outdated->close();
    }

    if (fSupportsAbsolute)
    {
        message(mainMachineWindowShown(), MessageType_Info,
            tr("<p>The Virtual Machine reports that the guest OS supports "
               "<b>mouse pointer integration</b>. This means that you do not "
               "need to <i>capture</i> the mouse pointer to be able to use it "
               "in your guest OS -- all "
               "mouse actions you perform when the mouse pointer is over the "
               "Virtual Machine's display are directly sent to the guest OS. "
               "If the mouse is currently captured, it will be automatically "
               "uncaptured."
               "</p>"
               "<p>The mouse icon on the status bar will look like&nbsp;"
               "<img src=:/mouse_seamless_16px.png/>&nbsp;to inform you that mouse "
               "pointer integration is supported by the guest OS and is "
               "currently turned on."
               "</p>"
               "<p><b>Note</b>: Some applications may behave incorrectly in "
               "mouse pointer integration mode. You can always disable it for "
               "the current session (and enable it again) by selecting the "
               "corresponding action from the menu bar."
               "</p>"),
            kNames [1] /* pcszAutoConfirmId */);
    }
    else
    {
        message(mainMachineWindowShown(), MessageType_Info,
            tr("<p>The Virtual Machine reports that the guest OS does not "
               "support <b>mouse pointer integration</b> in the current video "
               "mode. You need to capture the mouse (by clicking over the VM "
               "display or pressing the host key) in order to use the "
               "mouse inside the guest OS.</p>"),
            kNames [0] /* pcszAutoConfirmId */);
    }

    setWarningShown("remindAboutMouseIntegration", false);
}

bool UIMessageCenter::remindAboutPausedVMInput()
{
    int rc = message(
        mainMachineWindowShown(),
        MessageType_Info,
        tr("<p>The Virtual Machine is currently in the <b>Paused</b> state and "
           "not able to see any keyboard or mouse input. If you want to "
           "continue to work inside the VM, you need to resume it by selecting the "
           "corresponding action from the menu bar.</p>"),
        "remindAboutPausedVMInput"
    );
    return !(rc & AutoConfirmed);
}

void UIMessageCenter::cannotEnterSeamlessMode(ULONG /* uWidth */,
                                              ULONG /* uHeight */,
                                              ULONG /* uBpp */,
                                              ULONG64 uMinVRAM)
{
    message(mainMachineWindowShown(), MessageType_Error,
             tr("<p>Could not enter seamless mode due to insufficient guest "
                  "video memory.</p>"
                  "<p>You should configure the virtual machine to have at "
                  "least <b>%1</b> of video memory.</p>")
             .arg(VBoxGlobal::formatSize(uMinVRAM)));
}

int UIMessageCenter::cannotEnterFullscreenMode(ULONG /* uWidth */,
                                               ULONG /* uHeight */,
                                               ULONG /* uBpp */,
                                               ULONG64 uMinVRAM)
{
    return message(mainMachineWindowShown(), MessageType_Warning,
             tr("<p>Could not switch the guest display to fullscreen mode due "
                 "to insufficient guest video memory.</p>"
                 "<p>You should configure the virtual machine to have at "
                 "least <b>%1</b> of video memory.</p>"
                 "<p>Press <b>Ignore</b> to switch to fullscreen mode anyway "
                 "or press <b>Cancel</b> to cancel the operation.</p>")
             .arg(VBoxGlobal::formatSize(uMinVRAM)),
             0, /* pcszAutoConfirmId */
             QIMessageBox::Ignore | QIMessageBox::Default,
             QIMessageBox::Cancel | QIMessageBox::Escape);
}

void UIMessageCenter::cannotSwitchScreenInSeamless(quint64 uMinVRAM)
{
    message(mainMachineWindowShown(), MessageType_Error,
            tr("<p>Could not change the guest screen to this host screen "
               "due to insufficient guest video memory.</p>"
               "<p>You should configure the virtual machine to have at "
               "least <b>%1</b> of video memory.</p>")
            .arg(VBoxGlobal::formatSize(uMinVRAM)));
}

int UIMessageCenter::cannotSwitchScreenInFullscreen(quint64 uMinVRAM)
{
    return message(mainMachineWindowShown(), MessageType_Warning,
                   tr("<p>Could not change the guest screen to this host screen "
                      "due to insufficient guest video memory.</p>"
                      "<p>You should configure the virtual machine to have at "
                      "least <b>%1</b> of video memory.</p>"
                      "<p>Press <b>Ignore</b> to switch the screen anyway "
                      "or press <b>Cancel</b> to cancel the operation.</p>")
                   .arg(VBoxGlobal::formatSize(uMinVRAM)),
                   0, /* pcszAutoConfirmId */
                   QIMessageBox::Ignore | QIMessageBox::Default,
                   QIMessageBox::Cancel | QIMessageBox::Escape);
}

bool UIMessageCenter::confirmGoingFullscreen(const QString &strHotKey)
{
    return messageOkCancel(mainMachineWindowShown(), MessageType_Info,
        tr("<p>The virtual machine window will be now switched to <b>fullscreen</b> mode. "
           "You can go back to windowed mode at any time by pressing <b>%1</b>.</p>"
           "<p>Note that the <i>Host</i> key is currently defined as <b>%2</b>.</p>"
           "<p>Note that the main menu bar is hidden in fullscreen mode. "
           "You can access it by pressing <b>Host+Home</b>.</p>")
            .arg(strHotKey)
            .arg(UIHostCombo::toReadableString(vboxGlobal().settings().hostCombo())),
        "confirmGoingFullscreen",
        tr("Switch", "fullscreen"));
}

bool UIMessageCenter::confirmGoingSeamless(const QString &strHotKey)
{
    return messageOkCancel(mainMachineWindowShown(), MessageType_Info,
        tr("<p>The virtual machine window will be now switched to <b>Seamless</b> mode. "
           "You can go back to windowed mode at any time by pressing <b>%1</b>.</p>"
           "<p>Note that the <i>Host</i> key is currently defined as <b>%2</b>.</p>"
           "<p>Note that the main menu bar is hidden in seamless mode. "
           "You can access it by pressing <b>Host+Home</b>.</p>")
            .arg(strHotKey)
            .arg(UIHostCombo::toReadableString(vboxGlobal().settings().hostCombo())),
        "confirmGoingSeamless",
        tr("Switch", "seamless"));
}

bool UIMessageCenter::confirmGoingScale(const QString &strHotKey)
{
    return messageOkCancel(mainMachineWindowShown(), MessageType_Info,
        tr("<p>The virtual machine window will be now switched to <b>Scale</b> mode. "
           "You can go back to windowed mode at any time by pressing <b>%1</b>.</p>"
           "<p>Note that the <i>Host</i> key is currently defined as <b>%2</b>.</p>"
           "<p>Note that the main menu bar is hidden in scale mode. "
           "You can access it by pressing <b>Host+Home</b>.</p>")
            .arg(strHotKey)
            .arg(UIHostCombo::toReadableString(vboxGlobal().settings().hostCombo())),
        "confirmGoingScale",
        tr("Switch", "scale"));
}

void UIMessageCenter::cannotAttachUSBDevice(const CConsole &console,
                                            const QString &device)
{
    /* preserve the current error info before calling the object again */
    COMResult res(console);

    message(mainMachineWindowShown(), MessageType_Error,
        tr("Failed to attach the USB device <b>%1</b> "
           "to the virtual machine <b>%2</b>.")
            .arg(device)
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(res));
}

void UIMessageCenter::cannotAttachUSBDevice(const CConsole &console,
                                            const QString &device,
                                            const CVirtualBoxErrorInfo &error)
{
    message(mainMachineWindowShown(), MessageType_Error,
        tr("Failed to attach the USB device <b>%1</b> "
           "to the virtual machine <b>%2</b>.")
            .arg(device)
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(error));
}

void UIMessageCenter::cannotDetachUSBDevice(const CConsole &console,
                                            const QString &device)
{
    /* preserve the current error info before calling the object again */
    COMResult res(console);

    message(mainMachineWindowShown(), MessageType_Error,
        tr("Failed to detach the USB device <b>%1</b> "
           "from the virtual machine <b>%2</b>.")
            .arg(device)
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(res));
}

void UIMessageCenter::cannotDetachUSBDevice(const CConsole &console,
                                            const QString &device,
                                            const CVirtualBoxErrorInfo &error)
{
    message(mainMachineWindowShown(), MessageType_Error,
        tr("Failed to detach the USB device <b>%1</b> "
           "from the virtual machine <b>%2</b>.")
            .arg(device)
            .arg(console.GetMachine().GetName()),
        formatErrorInfo(error));
}

void UIMessageCenter::remindAboutGuestAdditionsAreNotActive(QWidget *pParent)
{
    message(pParent, MessageType_Warning,
             tr("<p>The VirtualBox Guest Additions do not appear to be "
                "available on this virtual machine, and shared folders "
                "cannot be used without them. To use shared folders inside "
                "the virtual machine, please install the Guest Additions "
                "if they are not installed, or re-install them if they are "
                "not working correctly, by selecting <b>Install Guest Additions</b> "
                "from the <b>Devices</b> menu. "
                "If they are installed but the machine is not yet fully started "
                "then shared folders will be available once it is.</p>"),
             "remindAboutGuestAdditionsAreNotActive");
}

void UIMessageCenter::cannotMountGuestAdditions(const QString &strMachineName)
{
    message(mainMachineWindowShown(), MessageType_Error,
             tr("<p>Could not insert the VirtualBox Guest Additions "
                "installer CD image into the virtual machine <b>%1</b>, as the machine "
                "has no CD/DVD-ROM drives. Please add a drive using the "
                "storage page of the virtual machine settings dialog.</p>")
                 .arg(strMachineName));
}

bool UIMessageCenter::remindAboutGuruMeditation(const CConsole &console, const QString &strLogFolder)
{
    Q_UNUSED(console);

    int rc = message(mainMachineWindowShown(), MessageType_GuruMeditation,
        tr("<p>A critical error has occurred while running the virtual "
           "machine and the machine execution has been stopped.</p>"
           ""
           "<p>For help, please see the Community section on "
           "<a href=http://www.virtualbox.org>http://www.virtualbox.org</a> "
           "or your support contract. Please provide the contents of the "
           "log file <tt>VBox.log</tt> and the image file <tt>VBox.png</tt>, "
           "which you can find in the <nobr><b>%1</b></nobr> directory, "
           "as well as a description of what you were doing when this error "
           "happened. "
           ""
           "Note that you can also access the above files by selecting "
           "<b>Show Log</b> from the <b>Machine</b> menu of the main "
           "VirtualBox window.</p>"
           ""
           "<p>Press <b>OK</b> if you want to power off the machine "
           "or press <b>Ignore</b> if you want to leave it as is for debugging. "
           "Please note that debugging requires special knowledge and tools, so "
           "it is recommended to press <b>OK</b> now.</p>")
            .arg(strLogFolder),
        0, /* pcszAutoConfirmId */
        QIMessageBox::Ok | QIMessageBox::Default,
        QIMessageBox::Ignore | QIMessageBox::Escape);

    return rc == QIMessageBox::Ok;
}

bool UIMessageCenter::askAboutCancelAllNetworkRequest(QWidget *pParent)
{
    return messageOkCancel(pParent, MessageType_Question, tr("Do you wish to cancel all current network operations?"));
}

void UIMessageCenter::showUpdateSuccess(const QString &strVersion, const QString &strLink)
{
    message(networkManagerOrMainWindowShown(), MessageType_Info,
            tr("<p>A new version of VirtualBox has been released! Version <b>%1</b> is available at <a href=\"http://www.virtualbox.org/\">virtualbox.org</a>.</p>"
               "<p>You can download this version using the link:</p>"
               "<p><a href=%2>%3</a></p>")
            .arg(strVersion, strLink, strLink));
}

void UIMessageCenter::showUpdateNotFound()
{
    message(networkManagerOrMainWindowShown(), MessageType_Info,
            tr("You are already running the most recent version of VirtualBox."));
}

bool UIMessageCenter::cannotFindGuestAdditions()
{
    return messageYesNo(mainMachineWindowShown(), MessageType_Question,
                    tr("<p>Could not find the VirtualBox Guest Additions "
                       "CD image file.</nobr></p><p>Do you wish to "
                       "download this CD image from the Internet?</p>"));
}

bool UIMessageCenter::confirmDownloadAdditions(const QString &strUrl, qulonglong uSize)
{
    QLocale loc(VBoxGlobal::languageId());
    return messageOkCancel(networkManagerOrMainMachineWindowShown(), MessageType_Question,
                           tr("<p>Are you sure you want to download the VirtualBox "
                              "Guest Additions CD image from "
                              "<nobr><a href=\"%1\">%2</a></nobr> "
                              "(size %3 bytes)?</p>").arg(strUrl).arg(strUrl).arg(loc.toString(uSize)),
                           0, /* pcszAutoConfirmId */
                           tr("Download", "additions"));
}

bool UIMessageCenter::confirmMountAdditions(const QString &strUrl, const QString &strSrc)
{
    return messageOkCancel(networkManagerOrMainMachineWindowShown(), MessageType_Question,
                           tr("<p>The VirtualBox Guest Additions CD image has been "
                              "successfully downloaded from "
                              "<nobr><a href=\"%1\">%2</a></nobr> "
                              "and saved locally as <nobr><b>%3</b>.</nobr></p>"
                              "<p>Do you wish to register this CD image and mount it "
                              "on the virtual CD/DVD drive?</p>")
                               .arg(strUrl).arg(strUrl).arg(strSrc),
                           0, /* pcszAutoConfirmId */
                           tr("Mount", "additions"));
}

void UIMessageCenter::warnAboutAdditionsCantBeSaved(const QString &strTarget)
{
    message(networkManagerOrMainMachineWindowShown(), MessageType_Error,
            tr("<p>Failed to save the downloaded file as <nobr><b>%1</b>.</nobr></p>")
               .arg(QDir::toNativeSeparators(strTarget)));
}

bool UIMessageCenter::askAboutUserManualDownload(const QString &strMissedLocation)
{
    return messageOkCancel(mainWindowShown(), MessageType_Question,
                           tr("<p>Could not find the VirtualBox User Manual "
                              "<nobr><b>%1</b>.</nobr></p><p>Do you wish to "
                              "download this file from the Internet?</p>")
                              .arg(strMissedLocation),
                           0, /* Auto-confirm Id */
                           tr("Download", "additions"));
}

bool UIMessageCenter::confirmUserManualDownload(const QString &strURL, qulonglong uSize)
{
    QLocale loc(VBoxGlobal::languageId());
    return messageOkCancel(networkManagerOrMainWindowShown(), MessageType_Question,
                           tr("<p>Are you sure you want to download the VirtualBox "
                              "User Manual from "
                              "<nobr><a href=\"%1\">%2</a></nobr> "
                              "(size %3 bytes)?</p>").arg(strURL).arg(strURL).arg(loc.toString(uSize)),
                           0, /* Auto-confirm Id */
                           tr("Download", "additions"));
}

void UIMessageCenter::warnAboutUserManualDownloaded(const QString &strURL, const QString &strTarget)
{
    message(networkManagerOrMainWindowShown(), MessageType_Warning,
            tr("<p>The VirtualBox User Manual has been "
               "successfully downloaded from "
               "<nobr><a href=\"%1\">%2</a></nobr> "
               "and saved locally as <nobr><b>%3</b>.</nobr></p>")
               .arg(strURL).arg(strURL).arg(strTarget));
}

void UIMessageCenter::warnAboutUserManualCantBeSaved(const QString &strURL, const QString &strTarget)
{
    message(networkManagerOrMainWindowShown(), MessageType_Error,
            tr("<p>The VirtualBox User Manual has been "
               "successfully downloaded from "
               "<nobr><a href=\"%1\">%2</a></nobr> "
               "but can't be saved locally as <nobr><b>%3</b>.</nobr></p>"
               "<p>Please choose another location for that file.</p>")
               .arg(strURL).arg(strURL).arg(strTarget));
}

bool UIMessageCenter::proposeDownloadExtensionPack(const QString &strExtPackName, const QString &strExtPackVersion)
{
    return messageOkCancel(mainWindowShown(),
                           MessageType_Question,
                           tr("<p>You have an old version (%1) of the <b><nobr>%2</nobr></b> installed.</p>"
                              "<p>Do you wish to download latest one from the Internet?</p>")
                              .arg(strExtPackVersion).arg(strExtPackName),
                           0, /* Auto-confirm Id */
                           tr("Download", "extension pack"));
}

bool UIMessageCenter::requestUserDownloadExtensionPack(const QString &strExtPackName, const QString &strExtPackVersion, const QString &strVBoxVersion)
{
    return message(mainWindowShown(), MessageType_Info,
                   tr("<p>You have version %1 of the <b><nobr>%2</nobr></b> installed.</p>"
                      "<p>You should download and install version %3 of this extension pack from Oracle!</p>")
                      .arg(strExtPackVersion).arg(strExtPackName).arg(strVBoxVersion),
                      0, /* Auto-confirm Id */
                      QIMessageBox::Ok | QIMessageBox::Default,
                      0,
                      0,
                      tr("Ok", "extension pack"));
}

bool UIMessageCenter::confirmDownloadExtensionPack(const QString &strExtPackName, const QString &strURL, qulonglong uSize)
{
    QLocale loc(VBoxGlobal::languageId());
    return messageOkCancel(networkManagerOrMainWindowShown(), MessageType_Question,
                           tr("<p>Are you sure you want to download the <b><nobr>%1</nobr></b> "
                              "from <nobr><a href=\"%2\">%2</a></nobr> (size %3 bytes)?</p>")
                              .arg(strExtPackName, strURL, loc.toString(uSize)),
                           0, /* Auto-confirm Id */
                           tr("Download", "extension pack"));
}

bool UIMessageCenter::proposeInstallExtentionPack(const QString &strExtPackName, const QString &strFrom, const QString &strTo)
{
    return messageOkCancel(networkManagerOrMainWindowShown(), MessageType_Question,
                           tr("<p>The <b><nobr>%1</nobr></b> has been "
                              "successfully downloaded from <nobr><a href=\"%2\">%2</a></nobr> "
                              "and saved locally as <nobr><b>%3</b>.</nobr></p>"
                              "<p>Do you wish to install this extension pack?</p>")
                              .arg(strExtPackName, strFrom, strTo),
                           0, /* Auto-confirm Id */
                           tr ("Install", "extension pack"));
}

void UIMessageCenter::warnAboutExtentionPackCantBeSaved(const QString &strExtPackName, const QString &strFrom, const QString &strTo)
{
    message(networkManagerOrMainWindowShown(), MessageType_Error,
            tr("<p>The <b><nobr>%1</nobr></b> has been "
               "successfully downloaded from <nobr><a href=\"%2\">%2</a></nobr> "
               "but can't be saved locally as <nobr><b>%3</b>.</nobr></p>"
               "<p>Please choose another location for that file.</p>")
               .arg(strExtPackName, strFrom, strTo));
}

void UIMessageCenter::cannotUpdateGuestAdditions(const CProgress &progress,
                                                 QWidget *pParent /* = NULL */) const
{
    AssertWrapperOk(progress);

    message(pParent ? pParent : mainWindowShown(),
            MessageType_Error,
            tr("Failed to update Guest Additions. The Guest Additions installation image will be mounted to provide a manual installation."),
            formatErrorInfo(progress.GetErrorInfo()));
}

void UIMessageCenter::cannotOpenExtPack(const QString &strFilename,
                                        const CExtPackManager &extPackManager,
                                        QWidget *pParent)
{
    message(pParent ? pParent : mainWindowShown(),
            MessageType_Error,
            tr("Failed to open the Extension Pack <b>%1</b>.").arg(strFilename),
            formatErrorInfo(extPackManager));
}

void UIMessageCenter::badExtPackFile(const QString &strFilename,
                                     const CExtPackFile &extPackFile,
                                     QWidget *pParent)
{
    message(pParent ? pParent : mainWindowShown(),
            MessageType_Error,
            tr("Failed to open the Extension Pack <b>%1</b>.").arg(strFilename),
            "<!--EOM-->" + extPackFile.GetWhyUnusable());
}

void UIMessageCenter::cannotInstallExtPack(const QString &strFilename,
                                           const CExtPackFile &extPackFile,
                                           const CProgress &progress,
                                           QWidget *pParent)
{
    if (!pParent)
        pParent = mainWindowShown();
    QString strErrInfo = !extPackFile.isOk() || progress.isNull()
                       ? formatErrorInfo(extPackFile) : formatErrorInfo(progress.GetErrorInfo());
    message(pParent,
            MessageType_Error,
            tr("Failed to install the Extension Pack <b>%1</b>.").arg(strFilename),
            strErrInfo);
}

void UIMessageCenter::cannotUninstallExtPack(const QString &strPackName, const CExtPackManager &extPackManager,
                                             const CProgress &progress, QWidget *pParent)
{
    if (!pParent)
        pParent = mainWindowShown();
    QString strErrInfo = !extPackManager.isOk() || progress.isNull()
                       ? formatErrorInfo(extPackManager) : formatErrorInfo(progress.GetErrorInfo());
    message(pParent,
            MessageType_Error,
            tr("Failed to uninstall the Extension Pack <b>%1</b>.").arg(strPackName),
            strErrInfo);
}

bool UIMessageCenter::confirmInstallingPackage(const QString &strPackName, const QString &strPackVersion,
                                               const QString &strPackDescription, QWidget *pParent)
{
    return messageOkCancel(pParent ? pParent : mainWindowShown(),
                           MessageType_Question,
                           tr("<p>You are about to install a VirtualBox extension pack. "
                              "Extension packs complement the functionality of VirtualBox and can contain system level software "
                              "that could be potentially harmful to your system. Please review the description below and only proceed "
                              "if you have obtained the extension pack from a trusted source.</p>"
                              "<p><table cellpadding=0 cellspacing=0>"
                              "<tr><td><b>Name:&nbsp;&nbsp;</b></td><td>%1</td></tr>"
                              "<tr><td><b>Version:&nbsp;&nbsp;</b></td><td>%2</td></tr>"
                              "<tr><td><b>Description:&nbsp;&nbsp;</b></td><td>%3</td></tr>"
                              "</table></p>")
                           .arg(strPackName).arg(strPackVersion).arg(strPackDescription),
                           0,
                           tr("&Install"));
}

bool UIMessageCenter::confirmReplacePackage(const QString &strPackName, const QString &strPackVersionNew,
                                            const QString &strPackVersionOld, const QString &strPackDescription,
                                            QWidget *pParent)
{
    if (!pParent)
        pParent = pParent ? pParent : mainWindowShown(); /* this is boring stuff that messageOkCancel should do! */

    QString strBelehrung = tr("Extension packs complement the functionality of VirtualBox and can contain "
                              "system level software that could be potentially harmful to your system. "
                              "Please review the description below and only proceed if you have obtained "
                              "the extension pack from a trusted source.");

    QByteArray  ba1     = strPackVersionNew.toUtf8();
    QByteArray  ba2     = strPackVersionOld.toUtf8();
    int         iVerCmp = RTStrVersionCompare(ba1.constData(), ba2.constData());

    bool fRc;
    if (iVerCmp > 0)
        fRc = messageOkCancel(pParent,
                              MessageType_Question,
                              tr("<p>An older version of the extension pack is already installed, would you like to upgrade? "
                                 "<p>%1</p>"
                                 "<p><table cellpadding=0 cellspacing=0>"
                                 "<tr><td><b>Name:&nbsp;&nbsp;</b></td><td>%2</td></tr>"
                                 "<tr><td><b>New Version:&nbsp;&nbsp;</b></td><td>%3</td></tr>"
                                 "<tr><td><b>Current Version:&nbsp;&nbsp;</b></td><td>%4</td></tr>"
                                 "<tr><td><b>Description:&nbsp;&nbsp;</b></td><td>%5</td></tr>"
                                 "</table></p>")
                              .arg(strBelehrung).arg(strPackName).arg(strPackVersionNew).arg(strPackVersionOld).arg(strPackDescription),
                              0,
                              tr("&Upgrade"));
    else if (iVerCmp < 0)
        fRc = messageOkCancel(pParent,
                              MessageType_Question,
                              tr("<p>An newer version of the extension pack is already installed, would you like to downgrade? "
                                 "<p>%1</p>"
                                 "<p><table cellpadding=0 cellspacing=0>"
                                 "<tr><td><b>Name:&nbsp;&nbsp;</b></td><td>%2</td></tr>"
                                 "<tr><td><b>New Version:&nbsp;&nbsp;</b></td><td>%3</td></tr>"
                                 "<tr><td><b>Current Version:&nbsp;&nbsp;</b></td><td>%4</td></tr>"
                                 "<tr><td><b>Description:&nbsp;&nbsp;</b></td><td>%5</td></tr>"
                                 "</table></p>")
                              .arg(strBelehrung).arg(strPackName).arg(strPackVersionNew).arg(strPackVersionOld).arg(strPackDescription),
                              0,
                              tr("&Downgrade"));
    else
        fRc = messageOkCancel(pParent,
                              MessageType_Question,
                              tr("<p>The extension pack is already installed with the same version, would you like reinstall it? "
                                 "<p>%1</p>"
                                 "<p><table cellpadding=0 cellspacing=0>"
                                 "<tr><td><b>Name:&nbsp;&nbsp;</b></td><td>%2</td></tr>"
                                 "<tr><td><b>Version:&nbsp;&nbsp;</b></td><td>%3</td></tr>"
                                 "<tr><td><b>Description:&nbsp;&nbsp;</b></td><td>%4</td></tr>"
                                 "</table></p>")
                              .arg(strBelehrung).arg(strPackName).arg(strPackVersionOld).arg(strPackDescription),
                              0,
                              tr("&Reinstall"));
    return fRc;
}

bool UIMessageCenter::confirmRemovingPackage(const QString &strPackName, QWidget *pParent)
{
    return messageOkCancel(pParent ? pParent : mainWindowShown(),
                            MessageType_Question,
                            tr("<p>You are about to remove the VirtualBox extension pack <b>%1</b>.</p>"
                               "<p>Are you sure you want to proceed?</p>").arg(strPackName),
                            0,
                            tr("&Remove"));
}

void UIMessageCenter::notifyAboutExtPackInstalled(const QString &strPackName, QWidget *pParent)
{
    message(pParent ? pParent : mainWindowShown(),
             MessageType_Info,
             tr("The extension pack <br><nobr><b>%1</b><nobr><br> was installed successfully.").arg(strPackName));
}

void UIMessageCenter::cannotOpenLicenseFile(QWidget *pParent, const QString &strPath)
{
    message(pParent, MessageType_Error,
            tr("Failed to open the license file <nobr><b>%1</b></nobr>. "
               "Check file permissions.").arg(strPath));
}

bool UIMessageCenter::askForOverridingFile(const QString &strPath, QWidget *pParent /* = 0 */)
{
    return messageYesNo(pParent, MessageType_Question,
                        tr("A file named <b>%1</b> already exists. "
                           "Are you sure you want to replace it?<br /><br />"
                           "Replacing it will overwrite its contents.").arg(strPath));
}

bool UIMessageCenter::askForOverridingFiles(const QVector<QString> &strPaths, QWidget *pParent /* = 0 */)
{
    /* If it is only one file use the single question versions above: */
    if (strPaths.size() == 1)
        return askForOverridingFile(strPaths.at(0), pParent);
    else if (strPaths.size() > 1)
        return messageYesNo(pParent, MessageType_Question,
                            tr("The following files already exist:<br /><br />%1<br /><br />"
                               "Are you sure you want to replace them? "
                               "Replacing them will overwrite their contents.")
                               .arg(QStringList(strPaths.toList()).join("<br />")));
    else
        return true;
}

bool UIMessageCenter::askForOverridingFileIfExists(const QString &strPath, QWidget *pParent /* = 0 */)
{
    QFileInfo fi(strPath);
    if (fi.exists())
        return askForOverridingFile(strPath, pParent);
    else
        return true;
}

bool UIMessageCenter::askForOverridingFilesIfExists(const QVector<QString> &strPaths, QWidget *pParent /* = 0 */)
{
    QVector<QString> existingFiles;
    foreach(const QString &file, strPaths)
    {
        QFileInfo fi(file);
        if (fi.exists())
            existingFiles << fi.absoluteFilePath();
    }
    /* If it is only one file use the single question versions above: */
    if (existingFiles.size() == 1)
        return askForOverridingFileIfExists(existingFiles.at(0), pParent);
    else if (existingFiles.size() > 1)
        return askForOverridingFiles(existingFiles, pParent);
    else
        return true;
}

/* static */
QString UIMessageCenter::mediumToAccusative(UIMediumType type, bool fIsHostDrive /* = false */)
{
    QString strType =
        type == UIMediumType_HardDisk ?
            tr("hard disk", "failed to mount ...") :
        type == UIMediumType_DVD && fIsHostDrive ?
            tr("CD/DVD", "failed to mount ... host-drive") :
        type == UIMediumType_DVD && !fIsHostDrive ?
            tr("CD/DVD image", "failed to mount ...") :
        type == UIMediumType_Floppy && fIsHostDrive ?
            tr("floppy", "failed to mount ... host-drive") :
        type == UIMediumType_Floppy && !fIsHostDrive ?
            tr("floppy image", "failed to mount ...") :
        QString();

    Assert(!strType.isNull());
    return strType;
}

/* static */
QString UIMessageCenter::formatRC(HRESULT rc)
{
    QString str;

    PCRTCOMERRMSG msg = NULL;
    const char *errMsg = NULL;

    /* first, try as is (only set bit 31 bit for warnings) */
    if (SUCCEEDED_WARNING(rc))
        msg = RTErrCOMGet(rc | 0x80000000);
    else
        msg = RTErrCOMGet(rc);

    if (msg != NULL)
        errMsg = msg->pszDefine;

#if defined (Q_WS_WIN)

    PCRTWINERRMSG winMsg = NULL;

    /* if not found, try again using RTErrWinGet with masked off top 16bit */
    if (msg == NULL)
    {
        winMsg = RTErrWinGet(rc & 0xFFFF);

        if (winMsg != NULL)
            errMsg = winMsg->pszDefine;
    }

#endif

    if (errMsg != NULL && *errMsg != '\0')
        str.sprintf("%s (0x%08X)", errMsg, rc);
    else
        str.sprintf("0x%08X", rc);

    return str;
}

/* static */
QString UIMessageCenter::formatErrorInfo(const COMErrorInfo &info, HRESULT wrapperRC /* = S_OK */)
{
    QString formatted = errorInfoToString(info, wrapperRC);
    return QString("<qt>%1</qt>").arg(formatted);
}

/* static */
QString UIMessageCenter::formatErrorInfo(const CVirtualBoxErrorInfo &info)
{
    return formatErrorInfo(COMErrorInfo(info));
}

/* static */
QString UIMessageCenter::formatErrorInfo(const COMBaseWithEI &wrapper)
{
    Assert(wrapper.lastRC() != S_OK);
    return formatErrorInfo(wrapper.errorInfo(), wrapper.lastRC());
}

/* static */
QString UIMessageCenter::formatErrorInfo(const COMResult &rc)
{
    Assert(rc.rc() != S_OK);
    return formatErrorInfo(rc.errorInfo(), rc.rc());
}

void UIMessageCenter::cannotCreateHostInterface(const CHost &host, QWidget *pParent /* = 0 */)
{
    message(pParent ? pParent : mainWindowShown(), MessageType_Error,
            tr("Failed to create the host-only network interface."),
            formatErrorInfo(host));
}

void UIMessageCenter::cannotCreateHostInterface(const CProgress &progress, QWidget *pParent /* = 0 */)
{
    message(pParent ? pParent : mainWindowShown(), MessageType_Error,
            tr("Failed to create the host-only network interface."),
            formatErrorInfo(progress.GetErrorInfo()));
}

void UIMessageCenter::cannotRemoveHostInterface(const CHost &host, const CHostNetworkInterface &iface, QWidget *pParent /* = 0 */)
{
    message(pParent ? pParent : mainWindowShown(), MessageType_Error,
            tr("Failed to remove the host network interface <b>%1</b>.")
               .arg(iface.GetName()),
            formatErrorInfo(host));
}

void UIMessageCenter::cannotRemoveHostInterface(const CProgress &progress, const CHostNetworkInterface &iface, QWidget *pParent /* = 0 */)
{
    message(pParent ? pParent : mainWindowShown(), MessageType_Error,
            tr("Failed to remove the host network interface <b>%1</b>.")
               .arg(iface.GetName()),
            formatErrorInfo(progress.GetErrorInfo()));
}

void UIMessageCenter::cannotAttachDevice(const CMachine &machine, UIMediumType type,
                                         const QString &strLocation, const StorageSlot &storageSlot,
                                         QWidget *pParent /* = 0 */)
{
    QString strMessage;
    switch (type)
    {
        case UIMediumType_HardDisk:
        {
            strMessage = tr("Failed to attach the hard disk (<nobr><b>%1</b></nobr>) to the slot <i>%2</i> of the machine <b>%3</b>.")
                           .arg(strLocation).arg(gpConverter->toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        case UIMediumType_DVD:
        {
            strMessage = tr("Failed to attach the CD/DVD device (<nobr><b>%1</b></nobr>) to the slot <i>%2</i> of the machine <b>%3</b>.")
                           .arg(strLocation).arg(gpConverter->toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        case UIMediumType_Floppy:
        {
            strMessage = tr("Failed to attach the floppy device (<nobr><b>%1</b></nobr>) to the slot <i>%2</i> of the machine <b>%3</b>.")
                           .arg(strLocation).arg(gpConverter->toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        default:
            break;
    }
    message(pParent ? pParent : mainWindowShown(), MessageType_Error, strMessage, formatErrorInfo(machine));
}

void UIMessageCenter::cannotCreateSharedFolder(const CMachine &machine, const QString &strName,
                                               const QString &strPath, QWidget *pParent /* = 0 */)
{
    message(pParent ? pParent : mainMachineWindowShown(), MessageType_Error,
            tr("Failed to create the shared folder <b>%1</b> "
               "(pointing to <nobr><b>%2</b></nobr>) "
               "for the virtual machine <b>%3</b>.")
               .arg(strName)
               .arg(strPath)
               .arg(CMachine(machine).GetName()),
            formatErrorInfo(machine));
}

void UIMessageCenter::cannotRemoveSharedFolder(const CMachine &machine, const QString &strName,
                                               const QString &strPath, QWidget *pParent /* = 0 */)
{
    message(pParent ? pParent : mainMachineWindowShown(), MessageType_Error,
            tr("Failed to remove the shared folder <b>%1</b> "
               "(pointing to <nobr><b>%2</b></nobr>) "
               "from the virtual machine <b>%3</b>.")
               .arg(strName)
               .arg(strPath)
               .arg(CMachine(machine).GetName()),
            formatErrorInfo(machine));
}

void UIMessageCenter::cannotCreateSharedFolder(const CConsole &console, const QString &strName,
                                               const QString &strPath, QWidget *pParent /* = 0 */)
{
    message(pParent ? pParent : mainMachineWindowShown(), MessageType_Error,
            tr("Failed to create the shared folder <b>%1</b> "
               "(pointing to <nobr><b>%2</b></nobr>) "
               "for the virtual machine <b>%3</b>.")
               .arg(strName)
               .arg(strPath)
               .arg(CConsole(console).GetMachine().GetName()),
            formatErrorInfo(console));
}

void UIMessageCenter::cannotRemoveSharedFolder(const CConsole &console, const QString &strName,
                                               const QString &strPath, QWidget *pParent /* = 0 */)
{
    message(pParent ? pParent : mainMachineWindowShown(), MessageType_Error,
            tr("<p>Failed to remove the shared folder <b>%1</b> "
               "(pointing to <nobr><b>%2</b></nobr>) "
               "from the virtual machine <b>%3</b>.</p>"
               "<p>Please close all programs in the guest OS that "
               "may be using this shared folder and try again.</p>")
               .arg(strName)
               .arg(strPath)
               .arg(CConsole(console).GetMachine().GetName()),
            formatErrorInfo(console));
}

#ifdef VBOX_WITH_DRAG_AND_DROP
void UIMessageCenter::cannotDropData(const CGuest &guest,
                                     QWidget *pParent /* = 0 */) const
{
    message(pParent ? pParent : mainWindowShown(),
            MessageType_Error,
            tr("Failed to drop data."),
            formatErrorInfo(guest));
}

void UIMessageCenter::cannotDropData(const CProgress &progress,
                                     QWidget *pParent /* = 0 */) const
{
    AssertWrapperOk(progress);

    message(pParent ? pParent : mainWindowShown(),
            MessageType_Error,
            tr("Failed to drop data."),
            formatErrorInfo(progress.GetErrorInfo()));
}
#endif /* VBOX_WITH_DRAG_AND_DROP */

void UIMessageCenter::remindAboutWrongColorDepth(ulong uRealBPP, ulong uWantedBPP)
{
    emit sigRemindAboutWrongColorDepth(uRealBPP, uWantedBPP);
}

void UIMessageCenter::remindAboutUnsupportedUSB2(const QString &strExtPackName, QWidget *pParent)
{
    emit sigRemindAboutUnsupportedUSB2(strExtPackName, pParent);
}

void UIMessageCenter::sltShowHelpWebDialog()
{
    vboxGlobal().openURL("http://www.virtualbox.org");
}

void UIMessageCenter::sltShowHelpAboutDialog()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString strFullVersion;
    if (vboxGlobal().brandingIsActive())
    {
        strFullVersion = QString("%1 r%2 - %3").arg(vbox.GetVersion())
                                               .arg(vbox.GetRevision())
                                               .arg(vboxGlobal().brandingGetKey("Name"));
    }
    else
    {
        strFullVersion = QString("%1 r%2").arg(vbox.GetVersion())
                                          .arg(vbox.GetRevision());
    }
    AssertWrapperOk(vbox);

    (new VBoxAboutDlg(mainWindowShown(), strFullVersion))->show();
}

void UIMessageCenter::sltShowHelpHelpDialog()
{
#ifndef VBOX_OSE
    /* For non-OSE version we just open it: */
    sltShowUserManual(vboxGlobal().helpFile());
#else /* #ifndef VBOX_OSE */
    /* For OSE version we have to check if it present first: */
    QString strUserManualFileName1 = vboxGlobal().helpFile();
    QString strShortFileName = QFileInfo(strUserManualFileName1).fileName();
    QString strUserManualFileName2 = QDir(vboxGlobal().virtualBox().GetHomeFolder()).absoluteFilePath(strShortFileName);
    /* Show if user manual already present: */
    if (QFile::exists(strUserManualFileName1))
        sltShowUserManual(strUserManualFileName1);
    else if (QFile::exists(strUserManualFileName2))
        sltShowUserManual(strUserManualFileName2);
    /* If downloader is running already: */
    else if (UIDownloaderUserManual::current())
    {
        /* Just show network access manager: */
        gNetworkManager->show();
    }
    /* Else propose to download user manual: */
    else if (askAboutUserManualDownload(strUserManualFileName1))
    {
        /* Create User Manual downloader: */
        UIDownloaderUserManual *pDl = UIDownloaderUserManual::create();
        /* After downloading finished => show User Manual: */
        connect(pDl, SIGNAL(sigDownloadFinished(const QString&)), this, SLOT(sltShowUserManual(const QString&)));
        /* Start downloading: */
        pDl->start();
    }
#endif /* #ifdef VBOX_OSE */
}

void UIMessageCenter::sltResetSuppressedMessages()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    vbox.SetExtraData(GUI_SuppressMessages, QString());
}

void UIMessageCenter::sltShowUserManual(const QString &strLocation)
{
#if defined (Q_WS_WIN32)
    HtmlHelp(GetDesktopWindow(), strLocation.utf16(), HH_DISPLAY_TOPIC, NULL);
#elif defined (Q_WS_X11)
# ifndef VBOX_OSE
    char szViewerPath[RTPATH_MAX];
    int rc;
    rc = RTPathAppPrivateArch(szViewerPath, sizeof(szViewerPath));
    AssertRC(rc);
    QProcess::startDetached(QString(szViewerPath) + "/kchmviewer", QStringList(strLocation));
# else /* #ifndef VBOX_OSE */
    vboxGlobal().openURL("file://" + strLocation);
# endif /* #ifdef VBOX_OSE */
#elif defined (Q_WS_MAC)
    vboxGlobal().openURL("file://" + strLocation);
#endif
}

void UIMessageCenter::sltShowMessageBox(QWidget *pParent, MessageType type,
                                        const QString &strMessage, const QString &strDetails,
                                        int iButton1, int iButton2, int iButton3,
                                        const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3,
                                        const QString &strAutoConfirmId) const
{
    /* Now we can show a message-box directly: */
    showMessageBox(pParent, type,
                   strMessage, strDetails,
                   iButton1, iButton2, iButton3,
                   strButtonText1, strButtonText2, strButtonText3,
                   strAutoConfirmId);
}

void UIMessageCenter::sltRemindAboutWrongColorDepth(ulong uRealBPP, ulong uWantedBPP)
{
    const char *kName = "remindAboutWrongColorDepth";

    /* Close the previous (outdated) window if any. We use kName as
     * pcszAutoConfirmId which is also used as the widget name by default. */
    {
        QWidget *outdated = VBoxGlobal::findWidget(NULL, kName, "QIMessageBox");
        if (outdated)
            outdated->close();
    }

    message(mainMachineWindowShown(), MessageType_Info,
            tr("<p>The virtual machine window is optimized to work in "
               "<b>%1&nbsp;bit</b> color mode but the "
               "virtual display is currently set to <b>%2&nbsp;bit</b>.</p>"
               "<p>Please open the display properties dialog of the guest OS and "
               "select a <b>%3&nbsp;bit</b> color mode, if it is available, for "
               "best possible performance of the virtual video subsystem.</p>"
               "<p><b>Note</b>. Some operating systems, like OS/2, may actually "
               "work in 32&nbsp;bit mode but report it as 24&nbsp;bit "
               "(16 million colors). You may try to select a different color "
               "mode to see if this message disappears or you can simply "
               "disable the message now if you are sure the required color "
               "mode (%4&nbsp;bit) is not available in the guest OS.</p>")
               .arg(uWantedBPP).arg(uRealBPP).arg(uWantedBPP).arg(uWantedBPP),
            kName);
}

void UIMessageCenter::sltRemindAboutUnsupportedUSB2(const QString &strExtPackName, QWidget *pParent)
{
    if (warningShown("sltRemindAboutUnsupportedUSB2"))
        return;
    setWarningShown("sltRemindAboutUnsupportedUSB2", true);

    message(pParent ? pParent : mainMachineWindowShown(), MessageType_Warning,
            tr("<p>USB 2.0 is currently enabled for this virtual machine. "
               "However, this requires the <b><nobr>%1</nobr></b> to be installed.</p>"
               "<p>Please install the Extension Pack from the VirtualBox download site. "
               "After this you will be able to re-enable USB 2.0. "
               "It will be disabled in the meantime unless you cancel the current settings changes.</p>")
               .arg(strExtPackName));

    setWarningShown("sltRemindAboutUnsupportedUSB2", false);
}

UIMessageCenter::UIMessageCenter()
{
    /* Register required objects as meta-types: */
    qRegisterMetaType<CProgress>();
    qRegisterMetaType<CHost>();
    qRegisterMetaType<CMachine>();
    qRegisterMetaType<CConsole>();
    qRegisterMetaType<CHostNetworkInterface>();
    qRegisterMetaType<UIMediumType>();
    qRegisterMetaType<StorageSlot>();

    /* Prepare interthread connection: */
    qRegisterMetaType<MessageType>();
    connect(this, SIGNAL(sigToShowMessageBox(QWidget*, MessageType,
                                             const QString&, const QString&,
                                             int, int, int,
                                             const QString&, const QString&, const QString&,
                                             const QString&)),
            this, SLOT(sltShowMessageBox(QWidget*, MessageType,
                                         const QString&, const QString&,
                                         int, int, int,
                                         const QString&, const QString&, const QString&,
                                         const QString&)),
            Qt::BlockingQueuedConnection);

    /* Prepare synchronization connection: */
    connect(this, SIGNAL(sigRemindAboutWrongColorDepth(ulong, ulong)),
            this, SLOT(sltRemindAboutWrongColorDepth(ulong, ulong)), Qt::QueuedConnection);
    connect(this, SIGNAL(sigRemindAboutUnsupportedUSB2(const QString&, QWidget*)),
            this, SLOT(sltRemindAboutUnsupportedUSB2(const QString&, QWidget*)), Qt::QueuedConnection);

    /* Translations for Main.
     * Please make sure they corresponds to the strings coming from Main one-by-one symbol! */
    tr("Could not load the Host USB Proxy Service (VERR_FILE_NOT_FOUND). The service might not be installed on the host computer");
    tr("VirtualBox is not currently allowed to access USB devices.  You can change this by adding your user to the 'vboxusers' group.  Please see the user manual for a more detailed explanation");
    tr("VirtualBox is not currently allowed to access USB devices.  You can change this by allowing your user to access the 'usbfs' folder and files.  Please see the user manual for a more detailed explanation");
    tr("The USB Proxy Service has not yet been ported to this host");
    tr("Could not load the Host USB Proxy service");
}

/* Returns a reference to the global VirtualBox message center instance: */
UIMessageCenter &UIMessageCenter::instance()
{
    static UIMessageCenter global_instance;
    return global_instance;
}

QString UIMessageCenter::errorInfoToString(const COMErrorInfo &info,
                                           HRESULT wrapperRC /* = S_OK */)
{
    /* Compose complex details string with internal <!--EOM--> delimiter to
     * make it possible to split string into info & details parts which will
     * be used separately in QIMessageBox */
    QString formatted;

    /* Check if details text is NOT empty: */
    QString strDetailsInfo = info.text();
    if (!strDetailsInfo.isEmpty())
    {
        /* Check if details text written in English (latin1) and translated: */
        if (strDetailsInfo == QString::fromLatin1(strDetailsInfo.toLatin1()) &&
            strDetailsInfo != tr(strDetailsInfo.toLatin1().constData()))
            formatted += QString("<p>%1.</p>").arg(vboxGlobal().emphasize(tr(strDetailsInfo.toLatin1().constData())));
        else
            formatted += QString("<p>%1.</p>").arg(vboxGlobal().emphasize(strDetailsInfo));
    }

    formatted += "<!--EOM--><table bgcolor=#EEEEEE border=0 cellspacing=0 "
                 "cellpadding=0 width=100%>";

    bool haveResultCode = false;

    if (info.isBasicAvailable())
    {
#if defined (Q_WS_WIN)
        haveResultCode = info.isFullAvailable();
        bool haveComponent = true;
        bool haveInterfaceID = true;
#else /* defined (Q_WS_WIN) */
        haveResultCode = true;
        bool haveComponent = info.isFullAvailable();
        bool haveInterfaceID = info.isFullAvailable();
#endif

        if (haveResultCode)
        {
            formatted += QString("<tr><td>%1</td><td><tt>%2</tt></td></tr>")
                .arg(tr("Result&nbsp;Code: ", "error info"))
                .arg(formatRC(info.resultCode()));
        }

        if (haveComponent)
            formatted += QString("<tr><td>%1</td><td>%2</td></tr>")
                .arg(tr("Component: ", "error info"), info.component());

        if (haveInterfaceID)
        {
            QString s = info.interfaceID();
            if (!info.interfaceName().isEmpty())
                s = info.interfaceName() + ' ' + s;
            formatted += QString("<tr><td>%1</td><td>%2</td></tr>")
                .arg(tr("Interface: ", "error info"), s);
        }

        if (!info.calleeIID().isNull() && info.calleeIID() != info.interfaceID())
        {
            QString s = info.calleeIID();
            if (!info.calleeName().isEmpty())
                s = info.calleeName() + ' ' + s;
            formatted += QString("<tr><td>%1</td><td>%2</td></tr>")
                .arg(tr("Callee: ", "error info"), s);
        }
    }

    if (FAILED (wrapperRC) &&
        (!haveResultCode || wrapperRC != info.resultCode()))
    {
        formatted += QString("<tr><td>%1</td><td><tt>%2</tt></td></tr>")
            .arg(tr("Callee&nbsp;RC: ", "error info"))
            .arg(formatRC(wrapperRC));
    }

    formatted += "</table>";

    if (info.next())
        formatted = formatted + "<!--EOP-->" + errorInfoToString(*info.next());

    return formatted;
}

int UIMessageCenter::showMessageBox(QWidget *pParent, MessageType type,
                                    const QString &strMessage, const QString &strDetails,
                                    int iButton1, int iButton2, int iButton3,
                                    const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3,
                                    const QString &strAutoConfirmId) const
{
    /* Choose the 'default' button: */
    if (iButton1 == 0 && iButton2 == 0 && iButton3 == 0)
        iButton1 = QIMessageBox::Ok | QIMessageBox::Default;

    /* Check if message-box was auto-confirmed before: */
    CVirtualBox vbox;
    QStringList confirmedMessageList;
    if (!strAutoConfirmId.isEmpty())
    {
        vbox = vboxGlobal().virtualBox();
        confirmedMessageList = vbox.GetExtraData(GUI_SuppressMessages).split(',');
        if (confirmedMessageList.contains(strAutoConfirmId))
        {
            int iResultCode = AutoConfirmed;
            if (iButton1 & QIMessageBox::Default)
                iResultCode |= (iButton1 & QIMessageBox::ButtonMask);
            if (iButton2 & QIMessageBox::Default)
                iResultCode |= (iButton2 & QIMessageBox::ButtonMask);
            if (iButton3 & QIMessageBox::Default)
                iResultCode |= (iButton3 & QIMessageBox::ButtonMask);
            return iResultCode;
        }
    }

    /* Choose title and icon: */
    QString title;
    QIMessageBox::Icon icon;
    switch (type)
    {
        default:
        case MessageType_Info:
            title = tr("VirtualBox - Information", "msg box title");
            icon = QIMessageBox::Information;
            break;
        case MessageType_Question:
            title = tr("VirtualBox - MessageType_Question", "msg box title");
            icon = QIMessageBox::Question;
            break;
        case MessageType_Warning:
            title = tr("VirtualBox - MessageType_Warning", "msg box title");
            icon = QIMessageBox::Warning;
            break;
        case MessageType_Error:
            title = tr("VirtualBox - MessageType_Error", "msg box title");
            icon = QIMessageBox::Critical;
            break;
        case MessageType_Critical:
            title = tr("VirtualBox - MessageType_Critical MessageType_Error", "msg box title");
            icon = QIMessageBox::Critical;
            break;
        case MessageType_GuruMeditation:
            title = "VirtualBox - Guru Meditation"; /* don't translate this */
            icon = QIMessageBox::GuruMeditation;
            break;
    }

    /* Create message-box: */
    QWidget *pMessageBoxParent = mwManager().realParentWindow(pParent);
    QPointer<QIMessageBox> pMessageBox = new QIMessageBox(title, strMessage, icon,
                                                          iButton1, iButton2, iButton3,
                                                          pMessageBoxParent, strAutoConfirmId.toAscii().constData());
    mwManager().registerNewParent(pMessageBox, pMessageBoxParent);

    /* Prepare auto-confirmation check-box: */
    if (!strAutoConfirmId.isEmpty())
    {
        pMessageBox->setFlagText(tr("Do not show this message again", "msg box flag"));
        pMessageBox->setFlagChecked(false);
    }

    /* Configure details: */
    if (!strDetails.isEmpty())
        pMessageBox->setDetailsText(strDetails);

    /* Configure button-text: */
    if (!strButtonText1.isNull())
        pMessageBox->setButtonText(0, strButtonText1);
    if (!strButtonText2.isNull())
        pMessageBox->setButtonText(1, strButtonText2);
    if (!strButtonText3.isNull())
        pMessageBox->setButtonText(2, strButtonText3);

    /* Show message-box: */
    int iResultCode = pMessageBox->exec();

    /* Make sure message-box still valid: */
    if (!pMessageBox)
        return iResultCode;

    /* Remember auto-confirmation check-box value: */
    if (!strAutoConfirmId.isEmpty())
    {
        if (pMessageBox->isFlagChecked())
        {
            confirmedMessageList << strAutoConfirmId;
            vbox.SetExtraData(GUI_SuppressMessages, confirmedMessageList.join(","));
        }
    }

    /* Delete message-box: */
    delete pMessageBox;

    /* Return result-code: */
    return iResultCode;
}

