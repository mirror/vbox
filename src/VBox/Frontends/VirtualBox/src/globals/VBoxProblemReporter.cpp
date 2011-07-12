/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxProblemReporter class implementation
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

/* Global includes */
#include <QDir>
#include <QDesktopWidget>
#include <QFileInfo>
#include <QThread>
#ifdef Q_WS_MAC
# include <QPushButton>
#endif

#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/path.h>

/* Local includes */
#include "VBoxProblemReporter.h"
#include "VBoxGlobal.h"
#include "VBoxSelectorWnd.h"
#include "UIProgressDialog.h"
#include "UIDownloaderUserManual.h"
#include "UIMachine.h"
#include "VBoxAboutDlg.h"
#include "UIHotKeyEditor.h"
#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

#if defined (Q_WS_WIN32)
# include <Htmlhelp.h>
#endif

bool VBoxProblemReporter::isAnyWarningShown()
{
    /* Check if at least one warning is alive!
     * All warnings are stored in m_warnings list as live pointers,
     * this is why if some warning was deleted from another place,
     * its pointer here will equal NULL... */
    for (int i = 0; i < m_warnings.size(); ++i)
        if (m_warnings[i])
            return true;
    return false;
}

bool VBoxProblemReporter::isAlreadyShown(const QString &strWarningName) const
{
    return m_shownWarnings.contains(strWarningName);
}

void VBoxProblemReporter::setShownStatus(const QString &strWarningName)
{
    if (!m_shownWarnings.contains(strWarningName))
        m_shownWarnings.append(strWarningName);
}

void VBoxProblemReporter::clearShownStatus(const QString &strWarningName)
{
    if (m_shownWarnings.contains(strWarningName))
        m_shownWarnings.removeAll(strWarningName);
}

void VBoxProblemReporter::closeAllWarnings()
{
    /* Broadcast signal to perform emergent
     * closing + deleting all the opened warnings. */
    emit sigToCloseAllWarnings();
}

/**
 *  Shows a message box of the given type with the given text and with buttons
 *  according to arguments b1, b2 and b3 (in the same way as QMessageBox does
 *  it), and returns the user's choice.
 *
 *  When all button arguments are zero, a single 'Ok' button is shown.
 *
 *  If aAutoConfirmId is not null, then the message box will contain a
 *  checkbox "Don't show this message again" below the message text and its
 *  state will be saved in the global config. When the checkbox is in the
 *  checked state, this method doesn't actually show the message box but
 *  returns immediately. The return value in this case is the same as if the
 *  user has pressed the Enter key or the default button, but with
 *  AutoConfirmed bit set (AutoConfirmed alone is returned when no default
 *  button is defined in button arguments).
 *
 *  @param  aParent
 *      Parent widget or 0 to use the desktop as the parent. Also,
 *      #mainWindowShown can be used to determine the currently shown VBox
 *      main window (Selector or Console).
 *  @param  aType
 *      One of values of the Type enum, that defines the message box
 *      title and icon.
 *  @param  aMessage
 *      Message text to display (can contain simple Qt-html tags).
 *  @param  aDetails
 *      Detailed message description displayed under the main message text using
 *      QTextEdit (that provides rich text support and scrollbars when necessary).
 *      If equals to QString::null, no details text box is shown.
 *  @param  aAutoConfirmId
 *      ID used to save the auto confirmation state across calls. If null,
 *      the auto confirmation feature is turned off (and no checkbox is shown)
 *  @param  aButton1
 *      First button code or 0, see QIMessageBox for a list of codes.
 *  @param  aButton2
 *      Second button code or 0, see QIMessageBox for a list of codes.
 *  @param  aButton3
 *      Third button code or 0, see QIMessageBox for a list of codes.
 *  @param  aText1
 *      Optional custom text for the first button.
 *  @param  aText2
 *      Optional custom text for the second button.
 *  @param  aText3
 *      Optional custom text for the third button.
 *
 *  @return
 *      code of the button pressed by the user
 */
int VBoxProblemReporter::message (QWidget *aParent, Type aType, const QString &aMessage,
                                  const QString &aDetails /* = QString::null */,
                                  const char *aAutoConfirmId /* = 0 */,
                                  int aButton1 /* = 0 */, int aButton2 /* = 0 */,
                                  int aButton3 /* = 0 */,
                                  const QString &aText1 /* = QString::null */,
                                  const QString &aText2 /* = QString::null */,
                                  const QString &aText3 /* = QString::null */) const
{
    if (aButton1 == 0 && aButton2 == 0 && aButton3 == 0)
        aButton1 = QIMessageBox::Ok | QIMessageBox::Default;

    CVirtualBox vbox;
    QStringList msgs;

    if (aAutoConfirmId)
    {
        vbox = vboxGlobal().virtualBox();
        msgs = vbox.GetExtraData (VBoxDefs::GUI_SuppressMessages).split (',');
        if (msgs.contains (aAutoConfirmId)) {
            int rc = AutoConfirmed;
            if (aButton1 & QIMessageBox::Default)
                rc |= (aButton1 & QIMessageBox::ButtonMask);
            if (aButton2 & QIMessageBox::Default)
                rc |= (aButton2 & QIMessageBox::ButtonMask);
            if (aButton3 & QIMessageBox::Default)
                rc |= (aButton3 & QIMessageBox::ButtonMask);
            return rc;
        }
    }

    QString title;
    QIMessageBox::Icon icon;

    switch (aType)
    {
        default:
        case Info:
            title = tr ("VirtualBox - Information", "msg box title");
            icon = QIMessageBox::Information;
            break;
        case Question:
            title = tr ("VirtualBox - Question", "msg box title");
            icon = QIMessageBox::Question;
            break;
        case Warning:
            title = tr ("VirtualBox - Warning", "msg box title");
            icon = QIMessageBox::Warning;
            break;
        case Error:
            title = tr ("VirtualBox - Error", "msg box title");
            icon = QIMessageBox::Critical;
            break;
        case Critical:
            title = tr ("VirtualBox - Critical Error", "msg box title");
            icon = QIMessageBox::Critical;
            break;
        case GuruMeditation:
            title = "VirtualBox - Guru Meditation"; /* don't translate this */
            icon = QIMessageBox::GuruMeditation;
            break;
    }

    QPointer<QIMessageBox> box = new QIMessageBox (title, aMessage, icon, aButton1, aButton2,
                                                   aButton3, aParent, aAutoConfirmId);
    connect(this, SIGNAL(sigToCloseAllWarnings()), box, SLOT(deleteLater()));

    if (!aText1.isNull())
        box->setButtonText (0, aText1);
    if (!aText2.isNull())
        box->setButtonText (1, aText2);
    if (!aText3.isNull())
        box->setButtonText (2, aText3);

    if (!aDetails.isEmpty())
        box->setDetailsText (aDetails);

    if (aAutoConfirmId)
    {
        box->setFlagText (tr ("Do not show this message again", "msg box flag"));
        box->setFlagChecked (false);
    }

    m_warnings << box;
    int rc = box->exec();
    if (box && m_warnings.contains(box))
        m_warnings.removeAll(box);

    if (aAutoConfirmId)
    {
        if (box && box->isFlagChecked())
        {
            msgs << aAutoConfirmId;
            vbox.SetExtraData (VBoxDefs::GUI_SuppressMessages, msgs.join (","));
        }
    }

    if (box)
        delete box;

    return rc;
}

/** @fn message (QWidget *, Type, const QString &, const char *, int, int,
 *               int, const QString &, const QString &, const QString &)
 *
 *  A shortcut to #message() that doesn't require to specify the details
 *  text (QString::null is assumed).
 */

/** @fn messageYesNo (QWidget *, Type, const QString &, const QString &, const char *)
 *
 *  A shortcut to #message() that shows 'Yes' and 'No' buttons ('Yes' is the
 *  default) and returns true when the user selects Yes.
 */

/** @fn messageYesNo (QWidget *, Type, const QString &, const char *)
 *
 *  A shortcut to #messageYesNo() that doesn't require to specify the details
 *  text (QString::null is assumed).
 */

int VBoxProblemReporter::messageWithOption(QWidget *pParent,
                                           Type type,
                                           const QString &strMessage,
                                           const QString &strOptionText,
                                           bool fDefaultOptionValue /* = true */,
                                           const QString &strDetails /* = QString::null */,
                                           int iButton1 /* = 0 */,
                                           int iButton2 /* = 0 */,
                                           int iButton3 /* = 0 */,
                                           const QString &strButtonName1 /* = QString::null */,
                                           const QString &strButtonName2 /* = QString::null */,
                                           const QString &strButtonName3 /* = QString::null */) const
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
        case Info:
            strTitle = tr("VirtualBox - Information", "msg box title");
            icon = QIMessageBox::Information;
            break;
        case Question:
            strTitle = tr("VirtualBox - Question", "msg box title");
            icon = QIMessageBox::Question;
            break;
        case Warning:
            strTitle = tr("VirtualBox - Warning", "msg box title");
            icon = QIMessageBox::Warning;
            break;
        case Error:
            strTitle = tr("VirtualBox - Error", "msg box title");
            icon = QIMessageBox::Critical;
            break;
        case Critical:
            strTitle = tr("VirtualBox - Critical Error", "msg box title");
            icon = QIMessageBox::Critical;
            break;
        case GuruMeditation:
            strTitle = "VirtualBox - Guru Meditation"; /* don't translate this */
            icon = QIMessageBox::GuruMeditation;
            break;
    }

    /* Create message-box: */
    if (QPointer<QIMessageBox> pBox = new QIMessageBox(strTitle, strMessage, icon,
                                                       iButton1, iButton2, iButton3, pParent))
    {
        /* Append the list of all warnings with current: */
        m_warnings << pBox;

        /* Setup message-box connections: */
        connect(this, SIGNAL(sigToCloseAllWarnings()), pBox, SLOT(deleteLater()));

        /* Assign other text values: */
        if (!strOptionText.isNull())
        {
            pBox->setFlagText(strOptionText);
            pBox->setFlagChecked(fDefaultOptionValue);
        }
        if (!strButtonName1.isNull())
            pBox->setButtonText(0, strButtonName1);
        if (!strButtonName2.isNull())
            pBox->setButtonText(1, strButtonName2);
        if (!strButtonName3.isNull())
            pBox->setButtonText(2, strButtonName3);
        if (!strDetails.isEmpty())
            pBox->setDetailsText(strDetails);

        /* Show the message box: */
        int iResultCode = pBox->exec();

        /* Its possible what message-box will be deleted during some event-processing procedure,
         * in that case pBox will be null right after pBox->exec() returns from it's event-pool,
         * so we have to check this too: */
        if (pBox)
        {
            /* Cleanup the list of all warnings from current: */
            if (m_warnings.contains(pBox))
                m_warnings.removeAll(pBox);

            /* Check if option was chosen: */
            if (pBox->isFlagChecked())
                iResultCode |= QIMessageBox::OptionChosen;

            /* Destroy message-box: */
            if (pBox)
                delete pBox;

            /* Return final result: */
            return iResultCode;
        }
    }
    return 0;
}

/**
 *  Shows a modal progress dialog using a CProgress object passed as an
 *  argument to track the progress.
 *
 *  @param aProgress    progress object to track
 *  @param aTitle       title prefix
 *  @param aParent      parent widget
 *  @param aMinDuration time (ms) that must pass before the dialog appears
 */
bool VBoxProblemReporter::showModalProgressDialog(CProgress &progress, const QString &strTitle, const QString &strImage /* = "" */, QWidget *pParent /* = 0 */, bool fSheetOnDarwin /* = false */, int cMinDuration /* = 2000 */)
{
    QPixmap *pPixmap = 0;
    if (!strImage.isEmpty())
        pPixmap = new QPixmap(strImage);

    UIProgressDialog progressDlg(progress, strTitle, pPixmap, fSheetOnDarwin, cMinDuration, pParent ? pParent : mainWindowShown());
    /* Run the dialog with the 350 ms refresh interval. */
    progressDlg.run(350);

    if (pPixmap)
        delete pPixmap;

    return true;
}

/**
 *  Returns what main window (VM selector or main VM window) is now shown, or
 *  zero if none of them. Main VM window takes precedence.
 */
QWidget* VBoxProblemReporter::mainWindowShown() const
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

/**
 *  Returns main machine window is now shown, or zero if none of them.
 */
QWidget* VBoxProblemReporter::mainMachineWindowShown() const
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

bool VBoxProblemReporter::askForOverridingFile (const QString& aPath, QWidget *aParent /* = NULL */) const
{
    return messageYesNo (aParent, Question, tr ("A file named <b>%1</b> already exists. Are you sure you want to replace it?<br /><br />Replacing it will overwrite its contents.").arg (aPath));
}

bool VBoxProblemReporter::askForOverridingFiles (const QVector<QString>& aPaths, QWidget *aParent /* = NULL */) const
{
    if (aPaths.size() == 1)
        /* If it is only one file use the single question versions above */
        return askForOverridingFile (aPaths.at (0), aParent);
    else if (aPaths.size() > 1)
        return messageYesNo (aParent, Question, tr ("The following files already exist:<br /><br />%1<br /><br />Are you sure you want to replace them? Replacing them will overwrite their contents.").arg (QStringList(aPaths.toList()).join ("<br />")));
    else
        return true;
}

bool VBoxProblemReporter::askForOverridingFileIfExists (const QString& aPath, QWidget *aParent /* = NULL */) const
{
    QFileInfo fi (aPath);
    if (fi.exists())
        return askForOverridingFile (aPath, aParent);
    else
        return true;
}

bool VBoxProblemReporter::askForOverridingFilesIfExists (const QVector<QString>& aPaths, QWidget *aParent /* = NULL */) const
{
    QVector<QString> existingFiles;
    foreach (const QString &file, aPaths)
    {
        QFileInfo fi (file);
        if (fi.exists())
            existingFiles << fi.absoluteFilePath();
    }
    if (existingFiles.size() == 1)
        /* If it is only one file use the single question versions above */
        return askForOverridingFileIfExists (existingFiles.at (0), aParent);
    else if (existingFiles.size() > 1)
        return askForOverridingFiles (existingFiles, aParent);
    else
        return true;
}

void VBoxProblemReporter::checkForMountedWrongUSB() const
{
#ifdef RT_OS_LINUX
    QFile file ("/proc/mounts");
    if (file.exists() && file.open (QIODevice::ReadOnly | QIODevice::Text))
    {
        QStringList contents;
        for (;;)
        {
            QByteArray line = file.readLine();
            if (line.isEmpty())
                break;
            contents << line;
        }
        QStringList grep1 (contents.filter ("/sys/bus/usb/drivers"));
        QStringList grep2 (grep1.filter ("usbfs"));
        if (!grep2.isEmpty())
            message (mainWindowShown(), Warning,
                     tr ("You seem to have the USBFS filesystem mounted at /sys/bus/usb/drivers. "
                         "We strongly recommend that you change this, as it is a severe mis-configuration of "
                         "your system which could cause USB devices to fail in unexpected ways."),
                     "checkForMountedWrongUSB");
    }
#endif
}

void VBoxProblemReporter::showBETAWarning()
{
    message
        (0, Warning,
         tr ("You are running a prerelease version of VirtualBox. "
             "This version is not suitable for production use."));
}

void VBoxProblemReporter::showBEBWarning()
{
    message
        (0, Warning,
         tr ("You are running an EXPERIMENTAL build of VirtualBox. "
             "This version is not suitable for production use."));
}

#ifdef Q_WS_X11
void VBoxProblemReporter::cannotFindLicenseFiles (const QString &aPath)
{
    message
        (0, VBoxProblemReporter::Error,
         tr ("Failed to find license files in "
             "<nobr><b>%1</b></nobr>.")
             .arg (aPath));
}
#endif

void VBoxProblemReporter::cannotOpenLicenseFile (QWidget *aParent,
                                                 const QString &aPath)
{
    message
        (aParent, VBoxProblemReporter::Error,
         tr ("Failed to open the license file <nobr><b>%1</b></nobr>. "
             "Check file permissions.")
             .arg (aPath));
}

void VBoxProblemReporter::cannotOpenURL (const QString &aURL)
{
    message
        (mainWindowShown(), VBoxProblemReporter::Error,
         tr ("Failed to open <tt>%1</tt>. Make sure your desktop environment "
             "can properly handle URLs of this type.")
         .arg (aURL));
}

void VBoxProblemReporter::cannotFindLanguage (const QString &aLangID,
                                              const QString &aNlsPath)
{
    message (0, VBoxProblemReporter::Error,
        tr ("<p>Could not find a language file for the language "
            "<b>%1</b> in the directory <b><nobr>%2</nobr></b>.</p>"
            "<p>The language will be temporarily reset to the system "
            "default language. Please go to the <b>Preferences</b> "
            "dialog which you can open from the <b>File</b> menu of the "
            "main VirtualBox window, and select one of the existing "
            "languages on the <b>Language</b> page.</p>")
             .arg (aLangID).arg (aNlsPath));
}

void VBoxProblemReporter::cannotLoadLanguage (const QString &aLangFile)
{
    message (0, VBoxProblemReporter::Error,
        tr ("<p>Could not load the language file <b><nobr>%1</nobr></b>. "
            "<p>The language will be temporarily reset to English (built-in). "
            "Please go to the <b>Preferences</b> "
            "dialog which you can open from the <b>File</b> menu of the "
            "main VirtualBox window, and select one of the existing "
            "languages on the <b>Language</b> page.</p>")
             .arg (aLangFile));
}

void VBoxProblemReporter::cannotInitCOM (HRESULT rc)
{
    message (0, Critical,
        tr ("<p>Failed to initialize COM or to find the VirtualBox COM server. "
            "Most likely, the VirtualBox server is not running "
            "or failed to start.</p>"
            "<p>The application will now terminate.</p>"),
        formatErrorInfo (COMErrorInfo(), rc));
}

void VBoxProblemReporter::cannotCreateVirtualBox (const CVirtualBox &vbox)
{
    message (0, Critical,
        tr ("<p>Failed to create the VirtualBox COM object.</p>"
            "<p>The application will now terminate.</p>"),
        formatErrorInfo (vbox));
}

void VBoxProblemReporter::cannotLoadGlobalConfig (const CVirtualBox &vbox,
                                                  const QString &error)
{
    /* preserve the current error info before calling the object again */
    COMResult res (vbox);

    message (mainWindowShown(), Critical,
        tr ("<p>Failed to load the global GUI configuration from "
            "<b><nobr>%1</nobr></b>.</p>"
            "<p>The application will now terminate.</p>")
             .arg (vbox.GetSettingsFilePath()),
        !res.isOk() ? formatErrorInfo (res)
                    : QString ("<!--EOM--><p>%1</p>").arg (vboxGlobal().emphasize (error)));
}

void VBoxProblemReporter::cannotSaveGlobalConfig (const CVirtualBox &vbox)
{
    /* preserve the current error info before calling the object again */
    COMResult res (vbox);

    message (mainWindowShown(), Critical,
        tr ("<p>Failed to save the global GUI configuration to "
            "<b><nobr>%1</nobr></b>.</p>"
            "<p>The application will now terminate.</p>")
             .arg (vbox.GetSettingsFilePath()),
        formatErrorInfo (res));
}

void VBoxProblemReporter::cannotSetSystemProperties (const CSystemProperties &props)
{
    message (mainWindowShown(), Critical,
        tr ("Failed to set global VirtualBox properties."),
        formatErrorInfo (props));
}

void VBoxProblemReporter::cannotAccessUSB (const COMBaseWithEI &aObj)
{
    /* If IMachine::GetUSBController(), IHost::GetUSBDevices() etc. return
     * E_NOTIMPL, it means the USB support is intentionally missing
     * (as in the OSE version). Don't show the error message in this case. */
    COMResult res (aObj);
    if (res.rc() == E_NOTIMPL)
        return;

    message (mainWindowShown(), res.isWarning() ? Warning : Error,
             tr ("Failed to access the USB subsystem."),
             formatErrorInfo (res),
             "cannotAccessUSB");
}

void VBoxProblemReporter::cannotCreateMachine (const CVirtualBox &vbox,
                                               QWidget *parent /* = 0 */)
{
    message (
        parent ? parent : mainWindowShown(),
        Error,
        tr ("Failed to create a new virtual machine."),
        formatErrorInfo (vbox)
    );
}

void VBoxProblemReporter::cannotCreateMachine (const CVirtualBox &vbox,
                                               const CMachine &machine,
                                               QWidget *parent /* = 0 */)
{
    message (
        parent ? parent : mainWindowShown(),
        Error,
        tr ("Failed to create a new virtual machine <b>%1</b>.")
            .arg (machine.GetName()),
        formatErrorInfo (vbox)
    );
}

void VBoxProblemReporter::cannotOpenMachine(QWidget *pParent, const QString &strMachinePath, const CVirtualBox &vbox)
{
    message(pParent ? pParent : mainWindowShown(),
            Error,
            tr("Failed to open virtual machine located in %1.")
               .arg(strMachinePath),
            formatErrorInfo(vbox));
}

void VBoxProblemReporter::cannotRegisterMachine(const CVirtualBox &vbox,
                                                const CMachine &machine,
                                                QWidget *pParent)
{
    message(pParent ? pParent : mainWindowShown(),
            Error,
            tr("Failed to register the virtual machine <b>%1</b>.")
            .arg(machine.GetName()),
            formatErrorInfo(vbox));
}

void VBoxProblemReporter::cannotReregisterMachine(QWidget *pParent, const QString &strMachinePath, const QString &strMachineName)
{
    message(pParent ? pParent : mainWindowShown(),
            Error,
            tr("Failed to add virtual machine <b>%1</b> located in <i>%2</i> because its already present.")
               .arg(strMachineName).arg(strMachinePath));
}

void VBoxProblemReporter::
cannotApplyMachineSettings (const CMachine &machine, const COMResult &res)
{
    message (
        mainWindowShown(),
        Error,
        tr ("Failed to apply the settings to the virtual machine <b>%1</b>.")
            .arg (machine.GetName()),
        formatErrorInfo (res)
    );
}

void VBoxProblemReporter::cannotSaveMachineSettings (const CMachine &machine,
                                                     QWidget *parent /* = 0 */)
{
    /* preserve the current error info before calling the object again */
    COMResult res (machine);

    message (parent ? parent : mainWindowShown(), Error,
             tr ("Failed to save the settings of the virtual machine "
                 "<b>%1</b> to <b><nobr>%2</nobr></b>.")
                 .arg (machine.GetName(), machine.GetSettingsFilePath()),
             formatErrorInfo (res));
}

/**
 * @param  strict  If |false|, this method will silently return if the COM
 *                 result code is E_NOTIMPL.
 */
void VBoxProblemReporter::cannotLoadMachineSettings (const CMachine &machine,
                                                     bool strict /* = true */,
                                                     QWidget *parent /* = 0 */)
{
    /* If COM result code is E_NOTIMPL, it means the requested object or
     * function is intentionally missing (as in the OSE version). Don't show
     * the error message in this case. */
    COMResult res (machine);
    if (!strict && res.rc() == E_NOTIMPL)
        return;

    message (parent ? parent : mainWindowShown(), Error,
             tr ("Failed to load the settings of the virtual machine "
                 "<b>%1</b> from <b><nobr>%2</nobr></b>.")
                .arg (machine.GetName(), machine.GetSettingsFilePath()),
             formatErrorInfo (res));
}

bool VBoxProblemReporter::confirmedSettingsReloading(QWidget *pParent)
{
    int rc = message(pParent, Question,
                     tr("<p>The machine settings were changed while you were editing them. "
                        "You currently have unsaved setting changes.</p>"
                        "<p>Would you like to reload the changed settings or to keep your own changes?</p>"), 0,
                     QIMessageBox::Yes, QIMessageBox::No | QIMessageBox::Default | QIMessageBox::Escape, 0,
                     tr("Reload settings"), tr("Keep changes"), 0);
    return rc == QIMessageBox::Yes;
}

void VBoxProblemReporter::warnAboutStateChange(QWidget *pParent)
{
    if (isAlreadyShown("warnAboutStateChange"))
        return;
    setShownStatus("warnAboutStateChange");

    message(pParent ? pParent : mainWindowShown(), Warning,
            tr("The state of the virtual machine you currently edit has changed. "
               "Only settings which are editable at runtime are saved when you press OK. "
               "All changes to other settings will be lost."));

    clearShownStatus("warnAboutStateChange");
}

void VBoxProblemReporter::cannotStartMachine (const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res (console);

    message (mainWindowShown(), Error,
        tr ("Failed to start the virtual machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (res));
}

void VBoxProblemReporter::cannotStartMachine (const CProgress &progress)
{
    AssertWrapperOk (progress);
    CConsole console (CProgress (progress).GetInitiator());
    AssertWrapperOk (console);

    message (
        mainWindowShown(),
        Error,
        tr ("Failed to start the virtual machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (progress.GetErrorInfo())
    );
}

void VBoxProblemReporter::cannotPauseMachine (const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res (console);

    message (mainWindowShown(), Error,
        tr ("Failed to pause the execution of the virtual machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (res));
}

void VBoxProblemReporter::cannotResumeMachine (const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res (console);

    message (mainWindowShown(),  Error,
        tr ("Failed to resume the execution of the virtual machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (res));
}

void VBoxProblemReporter::cannotACPIShutdownMachine (const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res (console);

    message (mainWindowShown(),  Error,
        tr ("Failed to send the ACPI Power Button press event to the "
            "virtual machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (res));
}

void VBoxProblemReporter::cannotSaveMachineState (const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res (console);

    message (mainWindowShown(), Error,
        tr ("Failed to save the state of the virtual machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (res));
}

void VBoxProblemReporter::cannotSaveMachineState (const CProgress &progress)
{
    AssertWrapperOk (progress);
    CConsole console (CProgress (progress).GetInitiator());
    AssertWrapperOk (console);

    message (
        mainWindowShown(),
        Error,
        tr ("Failed to save the state of the virtual machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (progress.GetErrorInfo())
    );
}

void VBoxProblemReporter::cannotCreateClone(const CMachine &machine,
                                            QWidget *pParent /* = 0 */)
{
    message(
        pParent ? pParent : mainWindowShown(),
        Error,
        tr ("Failed to clone the virtual machine <b>%1</b>.")
            .arg(machine.GetName()),
        formatErrorInfo(machine)
    );
}

void VBoxProblemReporter::cannotCreateClone(const CMachine &machine,
                                            const CProgress &progress,
                                            QWidget *pParent /* = 0 */)
{
    AssertWrapperOk(progress);

    message(
        pParent ? pParent : mainWindowShown(),
        Error,
        tr ("Failed to clone the virtual machine <b>%1</b>.")
            .arg(machine.GetName()),
        formatErrorInfo(progress.GetErrorInfo())
    );
}

void VBoxProblemReporter::cannotTakeSnapshot (const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res (console);

    message (mainWindowShown(), Error,
        tr ("Failed to create a snapshot of the virtual machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (res));
}

void VBoxProblemReporter::cannotTakeSnapshot (const CProgress &progress)
{
    AssertWrapperOk (progress);
    CConsole console (CProgress (progress).GetInitiator());
    AssertWrapperOk (console);

    message (
        mainWindowShown(),
        Error,
        tr ("Failed to create a snapshot of the virtual machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (progress.GetErrorInfo())
    );
}

void VBoxProblemReporter::cannotStopMachine (const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res (console);

    message (mainWindowShown(), Error,
        tr ("Failed to stop the virtual machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (res));
}

void VBoxProblemReporter::cannotStopMachine (const CProgress &progress)
{
    AssertWrapperOk (progress);
    CConsole console (CProgress (progress).GetInitiator());
    AssertWrapperOk (console);

    message (mainWindowShown(), Error,
        tr ("Failed to stop the virtual machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (progress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotDeleteMachine(const CMachine &machine)
{
    /* preserve the current error info before calling the object again */
    COMResult res (machine);

    message(mainWindowShown(),
            Error,
            tr("Failed to remove the virtual machine <b>%1</b>.").arg(machine.GetName()),
            !machine.isOk() ? formatErrorInfo(machine) : formatErrorInfo(res));
}

void VBoxProblemReporter::cannotDeleteMachine(const CMachine &machine, const CProgress &progress)
{
    message(mainWindowShown(),
            Error,
            tr("Failed to remove the virtual machine <b>%1</b>.").arg(machine.GetName()),
            formatErrorInfo(progress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotDiscardSavedState (const CConsole &console)
{
    /* preserve the current error info before calling the object again */
    COMResult res (console);

    message (mainWindowShown(), Error,
        tr ("Failed to discard the saved state of the virtual machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (res));
}

void VBoxProblemReporter::cannotSendACPIToMachine()
{
    message (mainWindowShown(),  Warning,
        tr ("You are trying to shut down the guest with the ACPI power "
            "button. This is currently not possible because the guest "
            "does not support software shutdown."));
}

bool VBoxProblemReporter::warnAboutVirtNotEnabled64BitsGuest(bool fHWVirtExSupported)
{
    if (fHWVirtExSupported)
        return messageOkCancel (mainWindowShown(), Error,
            tr ("<p>VT-x/AMD-V hardware acceleration has been enabled, but is "
                "not operational. Your 64-bit guest will fail to detect a "
                "64-bit CPU and will not be able to boot.</p><p>Please ensure "
                "that you have enabled VT-x/AMD-V properly in the BIOS of your "
                "host computer.</p>"),
            0 /* aAutoConfirmId */,
            tr ("Close VM"), tr ("Continue"));
    else
        return messageOkCancel (mainWindowShown(), Error,
            tr ("<p>VT-x/AMD-V hardware acceleration is not available on your system. "
                "Your 64-bit guest will fail to detect a 64-bit CPU and will "
                "not be able to boot."),
            0 /* aAutoConfirmId */,
            tr ("Close VM"), tr ("Continue"));
}

bool VBoxProblemReporter::warnAboutVirtNotEnabledGuestRequired(bool fHWVirtExSupported)
{
    if (fHWVirtExSupported)
        return messageOkCancel (mainWindowShown(), Error,
            tr ("<p>VT-x/AMD-V hardware acceleration has been enabled, but is "
                "not operational. Certain guests (e.g. OS/2 and QNX) require "
                "this feature.</p><p>Please ensure "
                "that you have enabled VT-x/AMD-V properly in the BIOS of your "
                "host computer.</p>"),
            0 /* aAutoConfirmId */,
            tr ("Close VM"), tr ("Continue"));
    else
        return messageOkCancel (mainWindowShown(), Error,
            tr ("<p>VT-x/AMD-V hardware acceleration is not available on your system. "
                "Certain guests (e.g. OS/2 and QNX) require this feature and will "
                "fail to boot without it.</p>"),
            0 /* aAutoConfirmId */,
            tr ("Close VM"), tr ("Continue"));
}

int VBoxProblemReporter::askAboutSnapshotRestoring(const QString &strSnapshotName, bool fAlsoCreateNewSnapshot)
{
    return fAlsoCreateNewSnapshot ?
           messageWithOption(mainWindowShown(), Question,
                             tr("<p>You are about to restore snapshot <b>%1</b>.</p>"
                                "<p>You can create a snapshot of the current state of the virtual machine first by checking the box below; "
                                "if you do not do this the current state will be permanently lost. Do you wish to proceed?</p>")
                                .arg(strSnapshotName),
                             tr("Create a snapshot of the current machine state"),
                             true /* choose option by default */,
                             QString::null /* details */,
                             QIMessageBox::Ok, QIMessageBox::Cancel, 0 /* 3rd button */,
                             tr("Restore"), tr("Cancel"), QString::null /* 3rd button text */) :
           message(mainWindowShown(), Question,
                   tr("<p>Are you sure you want to restore snapshot <b>%1</b>?</p>").arg(strSnapshotName),
                   0 /* auto-confirmation token */,
                   QIMessageBox::Ok, QIMessageBox::Cancel, 0 /* 3rd button */,
                   tr("Restore"), tr("Cancel"), QString::null /* 3rd button text */);
}

bool VBoxProblemReporter::askAboutSnapshotDeleting (const QString &aSnapshotName)
{
    return messageOkCancel (mainWindowShown(), Question,
        tr ("<p>Deleting the snapshot will cause the state information saved in it to be lost, and disk data spread over "
            "several image files that VirtualBox has created together with the snapshot will be merged into one file. This can be a lengthy process, and the information "
            "in the snapshot cannot be recovered.</p></p>Are you sure you want to delete the selected snapshot <b>%1</b>?</p>")
            .arg (aSnapshotName),
        /* Do NOT allow this message to be disabled! */
        NULL /* aAutoConfirmId */,
        tr ("Delete"), tr ("Cancel"));
}

bool VBoxProblemReporter::askAboutSnapshotDeletingFreeSpace (const QString &aSnapshotName,
                                                             const QString &aTargetImageName,
                                                             const QString &aTargetImageMaxSize,
                                                             const QString &aTargetFilesystemFree)
{
    return messageOkCancel (mainWindowShown(), Question,
        tr ("<p>Deleting the snapshot %1 will temporarily need more disk space. In the worst case the size of image %2 will grow by %3, "
            "however on this filesystem there is only %4 free.</p><p>Running out of disk space during the merge operation can result in "
            "corruption of the image and the VM configuration, i.e. loss of the VM and its data.</p><p>You may continue with deleting "
            "the snapshot at your own risk.</p>")
            .arg (aSnapshotName)
            .arg (aTargetImageName)
            .arg (aTargetImageMaxSize)
            .arg (aTargetFilesystemFree),
        /* Do NOT allow this message to be disabled! */
        NULL /* aAutoConfirmId */,
        tr ("Delete"), tr ("Cancel"));
}

void VBoxProblemReporter::cannotRestoreSnapshot (const CConsole &aConsole,
                                                 const QString &aSnapshotName)
{
    message (mainWindowShown(), Error,
        tr ("Failed to restore the snapshot <b>%1</b> of the virtual machine <b>%2</b>.")
            .arg (aSnapshotName)
            .arg (CConsole (aConsole).GetMachine().GetName()),
        formatErrorInfo (aConsole));
}

void VBoxProblemReporter::cannotRestoreSnapshot (const CProgress &aProgress,
                                                 const QString &aSnapshotName)
{
    CConsole console (CProgress (aProgress).GetInitiator());

    message (mainWindowShown(), Error,
        tr ("Failed to restore the snapshot <b>%1</b> of the virtual machine <b>%2</b>.")
            .arg (aSnapshotName)
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (aProgress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotDeleteSnapshot (const CConsole &aConsole,
                                                const QString &aSnapshotName)
{
    message (mainWindowShown(), Error,
        tr ("Failed to delete the snapshot <b>%1</b> of the virtual machine <b>%2</b>.")
            .arg (aSnapshotName)
            .arg (CConsole (aConsole).GetMachine().GetName()),
        formatErrorInfo (aConsole));
}

void VBoxProblemReporter::cannotDeleteSnapshot (const CProgress &aProgress,
                                                const QString &aSnapshotName)
{
    CConsole console (CProgress (aProgress).GetInitiator());

    message (mainWindowShown(), Error,
        tr ("Failed to delete the snapshot <b>%1</b> of the virtual machine <b>%2</b>.")
            .arg (aSnapshotName)
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (aProgress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotFindMachineByName (const CVirtualBox &vbox,
                                                   const QString &name)
{
    message (
        QApplication::desktop()->screen(QApplication::desktop()->primaryScreen()),
        Error,
        tr ("There is no virtual machine named <b>%1</b>.")
            .arg (name),
        formatErrorInfo (vbox)
    );
}

void VBoxProblemReporter::cannotEnterSeamlessMode (ULONG /* aWidth */,
                                                   ULONG /* aHeight */,
                                                   ULONG /* aBpp */,
                                                   ULONG64 aMinVRAM)
{
    message (mainMachineWindowShown(), Error,
             tr ("<p>Could not enter seamless mode due to insufficient guest "
                  "video memory.</p>"
                  "<p>You should configure the virtual machine to have at "
                  "least <b>%1</b> of video memory.</p>")
             .arg (VBoxGlobal::formatSize (aMinVRAM)));
}

int VBoxProblemReporter::cannotEnterFullscreenMode (ULONG /* aWidth */,
                                                    ULONG /* aHeight */,
                                                    ULONG /* aBpp */,
                                                    ULONG64 aMinVRAM)
{
    return message (mainMachineWindowShown(), Warning,
             tr ("<p>Could not switch the guest display to fullscreen mode due "
                 "to insufficient guest video memory.</p>"
                 "<p>You should configure the virtual machine to have at "
                 "least <b>%1</b> of video memory.</p>"
                 "<p>Press <b>Ignore</b> to switch to fullscreen mode anyway "
                 "or press <b>Cancel</b> to cancel the operation.</p>")
             .arg (VBoxGlobal::formatSize (aMinVRAM)),
             0, /* aAutoConfirmId */
             QIMessageBox::Ignore | QIMessageBox::Default,
             QIMessageBox::Cancel | QIMessageBox::Escape);
}

void VBoxProblemReporter::cannotSwitchScreenInSeamless(quint64 minVRAM)
{
    message(mainMachineWindowShown(), Error,
            tr("<p>Could not change the guest screen to this host screen "
               "due to insufficient guest video memory.</p>"
               "<p>You should configure the virtual machine to have at "
               "least <b>%1</b> of video memory.</p>")
            .arg(VBoxGlobal::formatSize(minVRAM)));
}

int VBoxProblemReporter::cannotSwitchScreenInFullscreen(quint64 minVRAM)
{
    return message(mainMachineWindowShown(), Warning,
                   tr("<p>Could not change the guest screen to this host screen "
                      "due to insufficient guest video memory.</p>"
                      "<p>You should configure the virtual machine to have at "
                      "least <b>%1</b> of video memory.</p>"
                      "<p>Press <b>Ignore</b> to switch the screen anyway "
                      "or press <b>Cancel</b> to cancel the operation.</p>")
                   .arg(VBoxGlobal::formatSize(minVRAM)),
                   0, /* aAutoConfirmId */
                   QIMessageBox::Ignore | QIMessageBox::Default,
                   QIMessageBox::Cancel | QIMessageBox::Escape);
}

int VBoxProblemReporter::cannotEnterFullscreenMode()
{
    return message(mainMachineWindowShown(), Error,
             tr ("<p>Can not switch the guest display to fullscreen mode. You "
                 "have more virtual screens configured than physical screens are "
                 "attached to your host.</p><p>Please either lower the virtual "
                 "screens in your VM configuration or attach additional screens "
                 "to your host.</p>"),
             0, /* aAutoConfirmId */
             QIMessageBox::Ok | QIMessageBox::Default);
}

int VBoxProblemReporter::cannotEnterSeamlessMode()
{
    return message(mainMachineWindowShown(), Error,
             tr ("<p>Can not switch the guest display to seamless mode. You "
                 "have more virtual screens configured than physical screens are "
                 "attached to your host.</p><p>Please either lower the virtual "
                 "screens in your VM configuration or attach additional screens "
                 "to your host.</p>"),
             0, /* aAutoConfirmId */
             QIMessageBox::Ok | QIMessageBox::Default);
}

int VBoxProblemReporter::confirmMachineDeletion(const CMachine &machine)
{
    if (machine.GetAccessible())
    {
        int cDisks = 0;
        const CMediumAttachmentVector &attachments = machine.GetMediumAttachments();
        for (int i = 0; i < attachments.size(); ++i)
        {
            const CMediumAttachment &attachment = attachments.at(i);
            /* Check if the medium is a harddisk: */
            if (attachment.GetType() == KDeviceType_HardDisk)
            {
                /* Check if the disk isn't shared.
                 * If the disk is shared, it will be *never* deleted. */
                QVector<QString> ids = attachment.GetMedium().GetMachineIds();
                if (ids.size() == 1)
                    ++cDisks;
            }
        }
        const QString strBase = tr("<p>You are about to remove the virtual machine <b>%1</b> from the machine list.</p>"
                                   "<p>Would you like to delete the files containing the virtual machine from your hard disk as well?</p>")
                                   .arg(machine.GetName());
        const QString strExtd = tr("<p>You are about to remove the virtual machine <b>%1</b> from the machine list.</p>"
                                   "<p>Would you like to delete the files containing the virtual machine from your hard disk as well? "
                                   "Doing this will also remove the files containing the machine's virtual hard disks "
                                   "if they are not in use by another machine.</p>")
                                   .arg(machine.GetName());
        return message(&vboxGlobal().selectorWnd(),
                       Question,
                       cDisks == 0 ? strBase : strExtd,
                       0, /* auto-confirm id */
                       QIMessageBox::Yes,
                       QIMessageBox::No,
                       QIMessageBox::Cancel | QIMessageBox::Escape | QIMessageBox::Default,
                       tr("Delete all files"),
                       tr("Remove only"));
    }
    else
    {
        /* This should be in sync with UIVMListBoxItem::recache(): */
        QFileInfo fi(machine.GetSettingsFilePath());
        const QString strName = VBoxGlobal::hasAllowedExtension(fi.completeSuffix(), VBoxDefs::VBoxFileExts) ? fi.completeBaseName() : fi.fileName();
        const QString strBase = tr("You are about to remove the inaccessible virtual machine "
                                   "<b>%1</b> from the machine list. Do you wish to proceed?")
                                   .arg(strName);
        return message(&vboxGlobal().selectorWnd(),
                       Question,
                       strBase,
                       0, /* auto-confirm id */
                       QIMessageBox::Ok,
                       QIMessageBox::Cancel | QIMessageBox::Escape | QIMessageBox::Default,
                       0,
                       tr("Remove"));
    }
}

bool VBoxProblemReporter::confirmDiscardSavedState (const CMachine &machine)
{
    return messageOkCancel (&vboxGlobal().selectorWnd(), Question,
        tr ("<p>Are you sure you want to discard the saved state of "
            "the virtual machine <b>%1</b>?</p>"
            "<p>This operation is equivalent to resetting or powering off "
            "the machine without doing a proper shutdown of the "
            "guest OS.</p>")
            .arg (machine.GetName()),
        0 /* aAutoConfirmId */,
        tr ("Discard", "saved state"));
}

void VBoxProblemReporter::cannotChangeMediumType(QWidget *pParent, const CMedium &medium, KMediumType oldMediumType, KMediumType newMediumType)
{
    message(pParent ? pParent : mainWindowShown(), Error,
            tr("<p>Error changing medium type from <b>%1</b> to <b>%2</b>.</p>")
                .arg(vboxGlobal().toString(oldMediumType)).arg(vboxGlobal().toString(newMediumType)),
            formatErrorInfo(medium));
}

bool VBoxProblemReporter::confirmReleaseMedium (QWidget *aParent,
                                                const VBoxMedium &aMedium,
                                                const QString &aUsage)
{
    /** @todo (translation-related): the gender of "the" in translations
     * will depend on the gender of aMedium.type(). */
    return messageOkCancel (aParent, Question,
        tr ("<p>Are you sure you want to release the %1 "
            "<nobr><b>%2</b></nobr>?</p>"
            "<p>This will detach it from the "
            "following virtual machine(s): <b>%3</b>.</p>")
            .arg (mediumToAccusative (aMedium.type()))
            .arg (aMedium.location())
            .arg (aUsage),
        0 /* aAutoConfirmId */,
        tr ("Release", "detach medium"));
}

bool VBoxProblemReporter::confirmRemoveMedium (QWidget *aParent,
                                               const VBoxMedium &aMedium)
{
    /** @todo (translation-related): the gender of "the" in translations
     * will depend on the gender of aMedium.type(). */
    QString msg =
        tr ("<p>Are you sure you want to remove the %1 "
            "<nobr><b>%2</b></nobr> from the list of known media?</p>")
            .arg (mediumToAccusative (aMedium.type()))
            .arg (aMedium.location());

    if (aMedium.type() == VBoxDefs::MediumType_HardDisk &&
        aMedium.medium().GetMediumFormat().GetCapabilities() & MediumFormatCapabilities_File)
    {
        if (aMedium.state() == KMediumState_Inaccessible)
            msg +=
                tr ("Note that as this hard disk is inaccessible its "
                    "storage unit cannot be deleted right now.");
        else
            msg +=
                tr ("The next dialog will let you choose whether you also "
                    "want to delete the storage unit of this hard disk or "
                    "keep it for later usage.");
    }
    else
        msg +=
            tr ("<p>Note that the storage unit of this medium will not be "
                "deleted and that it will be possible to use it later again.</p>");

    return messageOkCancel (aParent, Question, msg,
        "confirmRemoveMedium", /* aAutoConfirmId */
        tr ("Remove", "medium"));
}

void VBoxProblemReporter::sayCannotOverwriteHardDiskStorage (
    QWidget *aParent, const QString &aLocation)
{
    message (aParent, Info,
        tr ("<p>The hard disk storage unit at location <b>%1</b> already "
            "exists. You cannot create a new virtual hard disk that uses this "
            "location because it can be already used by another virtual hard "
            "disk.</p>"
            "<p>Please specify a different location.</p>")
            .arg (aLocation));
}

int VBoxProblemReporter::confirmDeleteHardDiskStorage (
    QWidget *aParent, const QString &aLocation)
{
    return message (aParent, Question,
        tr ("<p>Do you want to delete the storage unit of the hard disk "
            "<nobr><b>%1</b></nobr>?</p>"
            "<p>If you select <b>Delete</b> then the specified storage unit "
            "will be permanently deleted. This operation <b>cannot be "
            "undone</b>.</p>"
            "<p>If you select <b>Keep</b> then the hard disk will be only "
            "removed from the list of known hard disks, but the storage unit "
            "will be left untouched which makes it possible to add this hard "
            "disk to the list later again.</p>")
            .arg (aLocation),
        0, /* aAutoConfirmId */
        QIMessageBox::Yes,
        QIMessageBox::No | QIMessageBox::Default,
        QIMessageBox::Cancel | QIMessageBox::Escape,
        tr ("Delete", "hard disk storage"),
        tr ("Keep", "hard disk storage"));
}

void VBoxProblemReporter::cannotDeleteHardDiskStorage (QWidget *aParent,
                                                       const CMedium &aHD,
                                                       const CProgress &aProgress)
{
    /* below, we use CMedium (aHD) to preserve current error info
     * for formatErrorInfo() */

    message (aParent, Error,
        tr ("Failed to delete the storage unit of the hard disk <b>%1</b>.")
            .arg (CMedium (aHD).GetLocation()),
        !aHD.isOk() ? formatErrorInfo (aHD) :
        !aProgress.isOk() ? formatErrorInfo (aProgress) :
        formatErrorInfo (aProgress.GetErrorInfo()));
}

int VBoxProblemReporter::askAboutHardDiskAttachmentCreation(QWidget *pParent,
                                                            const QString &strControllerName)
{
    return message(pParent, Question,
                   tr("<p>You are about to add a virtual hard disk to controller <b>%1</b>.</p>"
                      "<p>Would you like to create a new, empty file to hold the disk contents or select an existing one?</p>")
                      .arg(strControllerName),
                   0, /* aAutoConfirmId */
                   QIMessageBox::Yes,
                   QIMessageBox::No,
                   QIMessageBox::Cancel | QIMessageBox::Default | QIMessageBox::Escape,
                   tr("Create &new disk", "add attachment routine"),
                   tr("&Choose existing disk", "add attachment routine"));
}

int VBoxProblemReporter::askAboutOpticalAttachmentCreation(QWidget *pParent,
                                                           const QString &strControllerName)
{
    return message(pParent, Question,
                   tr("<p>You are about to add a new CD/DVD drive to controller <b>%1</b>.</p>"
                      "<p>Would you like to choose a virtual CD/DVD disk to put in the drive "
                      "or to leave it empty for now?</p>")
                      .arg(strControllerName),
                   0, /* aAutoConfirmId */
                   QIMessageBox::Yes,
                   QIMessageBox::No,
                   QIMessageBox::Cancel | QIMessageBox::Default | QIMessageBox::Escape,
                   tr("&Choose disk", "add attachment routine"),
                   tr("Leave &empty", "add attachment routine"));
}

int VBoxProblemReporter::askAboutFloppyAttachmentCreation(QWidget *pParent,
                                                          const QString &strControllerName)
{
    return message(pParent, Question,
                   tr("<p>You are about to add a new floppy drive to controller <b>%1</b>.</p>"
                      "<p>Would you like to choose a virtual floppy disk to put in the drive "
                      "or to leave it empty for now?</p>")
                      .arg(strControllerName),
                   0, /* aAutoConfirmId */
                   QIMessageBox::Yes,
                   QIMessageBox::No,
                   QIMessageBox::Cancel | QIMessageBox::Default | QIMessageBox::Escape,
                   tr("&Choose disk", "add attachment routine"),
                   tr("Leave &empty", "add attachment routine"));
}

int VBoxProblemReporter::confirmRemovingOfLastDVDDevice() const
{
    return messageOkCancel (QApplication::activeWindow(), Info,
                            tr ("<p>Are you sure you want to delete the CD/DVD-ROM device?</p>"
                                "<p>You will not be able to mount any CDs or ISO images "
                                "or install the Guest Additions without it!</p>"),
                            0, /* aAutoConfirmId */
                            tr ("&Remove", "medium"));
}

void VBoxProblemReporter::cannotCreateHardDiskStorage (
    QWidget *aParent, const CVirtualBox &aVBox, const QString &aLocation,
    const CMedium &aHD, const CProgress &aProgress)
{
    message (aParent, Error,
        tr ("Failed to create the hard disk storage <nobr><b>%1</b>.</nobr>")
            .arg (aLocation),
        !aVBox.isOk() ? formatErrorInfo (aVBox) :
        !aHD.isOk() ? formatErrorInfo (aHD) :
        !aProgress.isOk() ? formatErrorInfo (aProgress) :
        formatErrorInfo (aProgress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotDetachDevice(QWidget *pParent, const CMachine &machine,
                                             VBoxDefs::MediumType type, const QString &strLocation, const StorageSlot &storageSlot)
{
    QString strMessage;
    switch (type)
    {
        case VBoxDefs::MediumType_HardDisk:
        {
            strMessage = tr("Failed to detach the hard disk (<nobr><b>%1</b></nobr>) from the slot <i>%2</i> of the machine <b>%3</b>.")
                           .arg(strLocation).arg(vboxGlobal().toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        case VBoxDefs::MediumType_DVD:
        {
            strMessage = tr("Failed to detach the CD/DVD device (<nobr><b>%1</b></nobr>) from the slot <i>%2</i> of the machine <b>%3</b>.")
                           .arg(strLocation).arg(vboxGlobal().toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        case VBoxDefs::MediumType_Floppy:
        {
            strMessage = tr("Failed to detach the floppy device (<nobr><b>%1</b></nobr>) from the slot <i>%2</i> of the machine <b>%3</b>.")
                           .arg(strLocation).arg(vboxGlobal().toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        default:
            break;
    }
    message(pParent ? pParent : mainWindowShown(), Error, strMessage, formatErrorInfo(machine));
}

int VBoxProblemReporter::cannotRemountMedium (QWidget *aParent, const CMachine &aMachine,
                                              const VBoxMedium &aMedium, bool aMount, bool aRetry)
{
    /** @todo (translation-related): the gender of "the" in translations
     * will depend on the gender of aMedium.type(). */
    QString text;
    if (aMount)
    {
        text = tr ("Unable to mount the %1 <nobr><b>%2</b></nobr> on the machine <b>%3</b>.");
        if (aRetry) text += tr (" Would you like to force mounting of this medium?");
    }
    else
    {
        text = tr ("Unable to unmount the %1 <nobr><b>%2</b></nobr> from the machine <b>%3</b>.");
        if (aRetry) text += tr (" Would you like to force unmounting of this medium?");
    }
    if (aRetry)
    {
        return messageOkCancel (aParent ? aParent : mainWindowShown(), Question, text
            .arg (mediumToAccusative (aMedium.type(), aMedium.isHostDrive()))
            .arg (aMedium.isHostDrive() ? aMedium.name() : aMedium.location())
            .arg (CMachine (aMachine).GetName()),
            formatErrorInfo (aMachine),
            0 /* Auto Confirm ID */,
            tr ("Force Unmount"));
    }
    else
    {
        return message (aParent ? aParent : mainWindowShown(), Error, text
            .arg (mediumToAccusative (aMedium.type(), aMedium.isHostDrive()))
            .arg (aMedium.isHostDrive() ? aMedium.name() : aMedium.location())
            .arg (CMachine (aMachine).GetName()),
            formatErrorInfo (aMachine));
    }
}

void VBoxProblemReporter::cannotOpenMedium (
    QWidget *aParent, const CVirtualBox &aVBox,
    VBoxDefs::MediumType aType, const QString &aLocation)
{
    /** @todo (translation-related): the gender of "the" in translations
     * will depend on the gender of aMedium.type(). */
    message (aParent ? aParent : mainWindowShown(), Error,
        tr ("Failed to open the %1 <nobr><b>%2</b></nobr>.")
            .arg (mediumToAccusative (aType))
            .arg (aLocation),
        formatErrorInfo (aVBox));
}

void VBoxProblemReporter::cannotCloseMedium (
    QWidget *aParent, const VBoxMedium &aMedium, const COMResult &aResult)
{
    /** @todo (translation-related): the gender of "the" in translations
     * will depend on the gender of aMedium.type(). */
    message (aParent, Error,
        tr ("Failed to close the %1 <nobr><b>%2</b></nobr>.")
            .arg (mediumToAccusative (aMedium.type()))
            .arg (aMedium.location()),
        formatErrorInfo (aResult));
}

void VBoxProblemReporter::cannotOpenSession (const CSession &session)
{
    Assert (session.isNull());

    message (
        mainWindowShown(),
        Error,
        tr ("Failed to create a new session."),
        formatErrorInfo (session)
    );
}

void VBoxProblemReporter::cannotOpenSession (
    const CVirtualBox &vbox, const CMachine &machine,
    const CProgress &progress
) {
    Assert (!vbox.isOk() || progress.isOk());

    QString name = machine.GetName();
    if (name.isEmpty())
        name = QFileInfo (machine.GetSettingsFilePath()).baseName();

    message (
        mainWindowShown(),
        Error,
        tr ("Failed to open a session for the virtual machine <b>%1</b>.")
            .arg (name),
        !vbox.isOk() ? formatErrorInfo (vbox) :
                       formatErrorInfo (progress.GetErrorInfo())
    );
}

void VBoxProblemReporter::cannotGetMediaAccessibility (const VBoxMedium &aMedium)
{
    message (qApp->activeWindow(), Error,
        tr ("Failed to determine the accessibility state of the medium "
            "<nobr><b>%1</b></nobr>.")
            .arg (aMedium.location()),
        formatErrorInfo (aMedium.result()));
}

int VBoxProblemReporter::confirmDeletingHostInterface (const QString &aName,
                                                       QWidget *aParent)
{
    return vboxProblem().message (aParent, VBoxProblemReporter::Question,
        tr ("<p>Deleting this host-only network will remove "
            "the host-only interface this network is based on. Do you want to "
            "remove the (host-only network) interface <nobr><b>%1</b>?</nobr></p>"
            "<p><b>Note:</b> this interface may be in use by one or more "
            "virtual network adapters belonging to one of your VMs. "
            "After it is removed, these adapters will no longer be usable until "
            "you correct their settings by either choosing a different interface "
            "name or a different adapter attachment type.</p>").arg (aName),
        0, /* autoConfirmId */
        QIMessageBox::Ok | QIMessageBox::Default,
        QIMessageBox::Cancel | QIMessageBox::Escape);
}

void VBoxProblemReporter::cannotAttachUSBDevice (const CConsole &console,
                                                 const QString &device)
{
    /* preserve the current error info before calling the object again */
    COMResult res (console);

    message (mainMachineWindowShown(), Error,
        tr ("Failed to attach the USB device <b>%1</b> "
            "to the virtual machine <b>%2</b>.")
            .arg (device)
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (res));
}

void VBoxProblemReporter::cannotAttachUSBDevice (const CConsole &console,
                                                 const QString &device,
                                                 const CVirtualBoxErrorInfo &error)
{
    message (mainMachineWindowShown(), Error,
        tr ("Failed to attach the USB device <b>%1</b> "
            "to the virtual machine <b>%2</b>.")
            .arg (device)
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (error));
}

void VBoxProblemReporter::cannotDetachUSBDevice (const CConsole &console,
                                                 const QString &device)
{
    /* preserve the current error info before calling the object again */
    COMResult res (console);

    message (mainMachineWindowShown(), Error,
        tr ("Failed to detach the USB device <b>%1</b> "
            "from the virtual machine <b>%2</b>.")
            .arg (device)
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (res));
}

void VBoxProblemReporter::cannotDetachUSBDevice (const CConsole &console,
                                                 const QString &device,
                                                 const CVirtualBoxErrorInfo &error)
{
    message (mainMachineWindowShown(), Error,
        tr ("Failed to detach the USB device <b>%1</b> "
            "from the virtual machine <b>%2</b>.")
            .arg (device)
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (error));
}

void VBoxProblemReporter::remindAboutGuestAdditionsAreNotActive(QWidget *pParent)
{
    message (pParent, Warning,
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

int VBoxProblemReporter::cannotFindGuestAdditions (const QString &aSrc1,
                                                   const QString &aSrc2)
{
    return message (mainMachineWindowShown(), Question,
                    tr ("<p>Could not find the VirtualBox Guest Additions "
                        "CD image file <nobr><b>%1</b></nobr> or "
                        "<nobr><b>%2</b>.</nobr></p><p>Do you wish to "
                        "download this CD image from the Internet?</p>")
                        .arg (aSrc1).arg (aSrc2),
                    0, /* aAutoConfirmId */
                    QIMessageBox::Yes | QIMessageBox::Default,
                    QIMessageBox::No | QIMessageBox::Escape);
}

void VBoxProblemReporter::cannotDownloadGuestAdditions (const QString &aURL,
                                                        const QString &aReason)
{
    message (mainMachineWindowShown(), Error,
             tr ("<p>Failed to download the VirtualBox Guest Additions CD "
                 "image from <nobr><a href=\"%1\">%2</a>.</nobr></p><p>%3</p>")
                 .arg (aURL).arg (aURL).arg (aReason));
}

void VBoxProblemReporter::cannotMountGuestAdditions (const QString &aMachineName)
{
    message (mainMachineWindowShown(), Error,
             tr ("<p>Could not insert the VirtualBox Guest Additions "
                 "installer CD image into the virtual machine <b>%1</b>, as the machine "
                 "has no CD/DVD-ROM drives. Please add a drive using the "
                 "storage page of the virtual machine settings dialog.</p>")
                 .arg (aMachineName));
}

bool VBoxProblemReporter::confirmDownloadAdditions (const QString &aURL,
                                                    ulong aSize)
{
    return messageOkCancel (mainMachineWindowShown(), Question,
        tr ("<p>Are you sure you want to download the VirtualBox "
            "Guest Additions CD image from "
            "<nobr><a href=\"%1\">%2</a></nobr> "
            "(size %3 bytes)?</p>").arg (aURL).arg (aURL).arg (aSize),
        0, /* aAutoConfirmId */
        tr ("Download", "additions"));
}

bool VBoxProblemReporter::confirmMountAdditions (const QString &aURL,
                                                const QString &aSrc)
{
    return messageOkCancel (mainMachineWindowShown(), Question,
        tr ("<p>The VirtualBox Guest Additions CD image has been "
            "successfully downloaded from "
            "<nobr><a href=\"%1\">%2</a></nobr> "
            "and saved locally as <nobr><b>%3</b>.</nobr></p>"
            "<p>Do you wish to register this CD image and mount it "
            "on the virtual CD/DVD drive?</p>")
            .arg (aURL).arg (aURL).arg (aSrc),
        0, /* aAutoConfirmId */
        tr ("Mount", "additions"));
}

bool VBoxProblemReporter::askAboutUserManualDownload(const QString &strMissedLocation)
{
    return messageOkCancel(mainWindowShown(), Question,
                           tr("<p>Could not find the VirtualBox User Manual "
                              "<nobr><b>%1</b>.</nobr></p><p>Do you wish to "
                              "download this file from the Internet?</p>")
                              .arg(strMissedLocation),
                           0, /* Auto-confirm Id */
                           tr ("Download", "additions"));
}

bool VBoxProblemReporter::confirmUserManualDownload(const QString &strURL, ulong uSize)
{
    return messageOkCancel(mainWindowShown(), Question,
                           tr ("<p>Are you sure you want to download the VirtualBox "
                               "User Manual from "
                               "<nobr><a href=\"%1\">%2</a></nobr> "
                               "(size %3 bytes)?</p>").arg(strURL).arg(strURL).arg(uSize),
                           0, /* Auto-confirm Id */
                           tr ("Download", "additions"));
}

void VBoxProblemReporter::warnAboutUserManualCantBeDownloaded(const QString &strURL, const QString &strReason)
{
    message(mainWindowShown(), Error,
            tr("<p>Failed to download the VirtualBox User Manual "
               "from <nobr><a href=\"%1\">%2</a>.</nobr></p><p>%3</p>")
               .arg(strURL).arg(strURL).arg(strReason));
}

void VBoxProblemReporter::warnAboutUserManualDownloaded(const QString &strURL, const QString &strTarget)
{
    message(mainWindowShown(), Warning,
            tr("<p>The VirtualBox User Manual has been "
               "successfully downloaded from "
               "<nobr><a href=\"%1\">%2</a></nobr> "
               "and saved locally as <nobr><b>%3</b>.</nobr></p>")
               .arg(strURL).arg(strURL).arg(strTarget));
}

void VBoxProblemReporter::warnAboutUserManualCantBeSaved(const QString &strURL, const QString &strTarget)
{
    message(mainWindowShown(), Error,
            tr("<p>The VirtualBox User Manual has been "
               "successfully downloaded from "
               "<nobr><a href=\"%1\">%2</a></nobr> "
               "but can't be saved locally as <nobr><b>%3</b>.</nobr></p>"
               "<p>Please choose another location for that file.</p>")
               .arg(strURL).arg(strURL).arg(strTarget));
}

void VBoxProblemReporter::cannotConnectRegister (QWidget *aParent,
                                                 const QString &aURL,
                                                 const QString &aReason)
{
    /* we don't want to expose the registration script URL to the user
     * if he simply doesn't have an internet connection */
    Q_UNUSED (aURL);

    message (aParent, Error,
             tr ("<p>Failed to connect to the VirtualBox online "
                 "registration service due to the following error:</p><p><b>%1</b></p>")
                 .arg (aReason));
}

void VBoxProblemReporter::showRegisterResult (QWidget *aParent,
                                              const QString &aResult)
{
    if (aResult == "OK")
    {
        /* On successful registration attempt */
        message (aParent, Info,
                 tr ("<p>Congratulations! You have been successfully registered "
                     "as a user of VirtualBox.</p>"
                     "<p>Thank you for finding time to fill out the "
                     "registration form!</p>"));
    }
    else
    {
        QString parsed;

        /* Else parse and translate special key-words */
        if (aResult == "AUTHFAILED")
            parsed = tr ("<p>Invalid e-mail address or password specified.</p>");

        message (aParent, Error,
                 tr ("<p>Failed to register the VirtualBox product.</p><p>%1</p>")
                 .arg (parsed.isNull() ? aResult : parsed));
    }
}

void VBoxProblemReporter::showUpdateSuccess (QWidget *aParent,
                                             const QString &aVersion,
                                             const QString &aLink)
{
    message (aParent, Info,
             tr ("<p>A new version of VirtualBox has been released! Version <b>%1</b> is available at <a href=\"http://www.virtualbox.org/\">virtualbox.org</a>.</p>"
                 "<p>You can download this version using the link:</p>"
                 "<p><a href=%2>%3</a></p>")
                 .arg (aVersion, aLink, aLink));
}

void VBoxProblemReporter::showUpdateFailure (QWidget *aParent,
                                             const QString &aReason)
{
    message (aParent, Error,
             tr ("<p>Unable to obtain the new version information "
                 "due to the following error:</p><p><b>%1</b></p>")
                 .arg (aReason));
}

void VBoxProblemReporter::showUpdateNotFound (QWidget *aParent)
{
    message (aParent, Info,
             tr ("You are already running the most recent version of VirtualBox."
                 ""));
}

/**
 *  @return @c true if the user has confirmed input capture (this is always
 *  the case if the dialog was autoconfirmed). @a aAutoConfirmed, when not
 *  NULL, will receive @c true if the dialog wasn't actually shown.
 */
bool VBoxProblemReporter::confirmInputCapture (bool *aAutoConfirmed /* = NULL */)
{
    int rc = message (mainMachineWindowShown(), Info,
        tr ("<p>You have <b>clicked the mouse</b> inside the Virtual Machine display "
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
        tr ("<p>The host key is currently defined as <b>%1</b>.</p>",
            "additional message box paragraph")
            .arg (UIHotKeyCombination::toReadableString (vboxGlobal().settings().hostCombo())),
        "confirmInputCapture",
        QIMessageBox::Ok | QIMessageBox::Default,
        QIMessageBox::Cancel | QIMessageBox::Escape,
        0,
        tr ("Capture", "do input capture"));

    if (aAutoConfirmed)
        *aAutoConfirmed = (rc & AutoConfirmed);

    return (rc & QIMessageBox::ButtonMask) == QIMessageBox::Ok;
}

void VBoxProblemReporter::remindAboutAutoCapture()
{
    message (mainMachineWindowShown(), Info,
        tr ("<p>You have the <b>Auto capture keyboard</b> option turned on. "
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
        tr ("<p>The host key is currently defined as <b>%1</b>.</p>",
            "additional message box paragraph")
            .arg (UIHotKeyCombination::toReadableString (vboxGlobal().settings().hostCombo())),
        "remindAboutAutoCapture");
}

void VBoxProblemReporter::remindAboutMouseIntegration (bool aSupportsAbsolute)
{
    if (isAlreadyShown("remindAboutMouseIntegration"))
        return;
    setShownStatus("remindAboutMouseIntegration");

    static const char *kNames [2] =
    {
        "remindAboutMouseIntegrationOff",
        "remindAboutMouseIntegrationOn"
    };

    /* Close the previous (outdated) window if any. We use kName as
     * aAutoConfirmId which is also used as the widget name by default. */
    {
        QWidget *outdated =
            VBoxGlobal::findWidget (NULL, kNames [int (!aSupportsAbsolute)],
                                    "QIMessageBox");
        if (outdated)
            outdated->close();
    }

    if (aSupportsAbsolute)
    {
        message (mainMachineWindowShown(), Info,
            tr ("<p>The Virtual Machine reports that the guest OS supports "
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
            kNames [1] /* aAutoConfirmId */);
    }
    else
    {
        message (mainMachineWindowShown(), Info,
            tr ("<p>The Virtual Machine reports that the guest OS does not "
                "support <b>mouse pointer integration</b> in the current video "
                "mode. You need to capture the mouse (by clicking over the VM "
                "display or pressing the host key) in order to use the "
                "mouse inside the guest OS.</p>"),
            kNames [0] /* aAutoConfirmId */);
    }

    clearShownStatus("remindAboutMouseIntegration");
}

/**
 * @return @c false if the dialog wasn't actually shown (i.e. it was
 * autoconfirmed).
 */
bool VBoxProblemReporter::remindAboutPausedVMInput()
{
    int rc = message (
        mainMachineWindowShown(),
        Info,
        tr (
            "<p>The Virtual Machine is currently in the <b>Paused</b> state and "
            "not able to see any keyboard or mouse input. If you want to "
            "continue to work inside the VM, you need to resume it by selecting the "
            "corresponding action from the menu bar.</p>"
        ),
        "remindAboutPausedVMInput"
    );
    return !(rc & AutoConfirmed);
}

/** @return true if the user has chosen to show the Disk Manager Window */
bool VBoxProblemReporter::remindAboutInaccessibleMedia()
{
    int rc = message (&vboxGlobal().selectorWnd(), Warning,
        tr ("<p>One or more virtual hard disks, CD/DVD or "
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
        tr ("Check", "inaccessible media message box"));

    return rc == QIMessageBox::Ok; /* implies !AutoConfirmed */
}

/**
 * Shows user a proposal to either convert configuration files or
 * Exit the application leaving all as already is.
 *
 * @param aFileList      List of files for auto-conversion (may use Qt HTML).
 * @param aAfterRefresh  @true when called after the VM refresh.
 *
 * @return QIMessageBox::Ok (Save + Backup), QIMessageBox::Cancel (Exit)
 */
int VBoxProblemReporter::warnAboutSettingsAutoConversion (const QString &aFileList,
                                                          bool aAfterRefresh)
{
    if (!aAfterRefresh)
    {
        /* Common variant for all VMs */
        return message (mainWindowShown(), Info,
            tr ("<p>Your existing VirtualBox settings files will be automatically "
                "converted from the old format to a new format required by the "
                "new version of VirtualBox.</p>"
                "<p>Press <b>OK</b> to start VirtualBox now or press <b>Exit</b> if "
                "you want to terminate the VirtualBox "
                "application without any further actions.</p>"),
            NULL /* aAutoConfirmId */,
            QIMessageBox::Ok | QIMessageBox::Default,
            QIMessageBox::Cancel | QIMessageBox::Escape,
            0,
            0,
            tr ("E&xit", "warnAboutSettingsAutoConversion message box"));
    }
    else
    {
        /* Particular VM variant */
        return message (mainWindowShown(), Info,
            tr ("<p>The following VirtualBox settings files will be automatically "
                "converted from the old format to a new format required by the "
                "new version of VirtualBox.</p>"
                "<p>Press <b>OK</b> to start VirtualBox now or press <b>Exit</b> if "
                "you want to terminate the VirtualBox "
                "application without any further actions.</p>"),
            QString ("<!--EOM-->%1").arg (aFileList),
            NULL /* aAutoConfirmId */,
            QIMessageBox::Ok | QIMessageBox::Default,
            QIMessageBox::Cancel | QIMessageBox::Escape,
            0,
            0,
            tr ("E&xit", "warnAboutSettingsAutoConversion message box"));
    }
}

/**
 *  @param aHotKey Fullscreen hot key as defined in the menu.
 *
 *  @return @c true if the user has chosen to go fullscreen (this is always
 *  the case if the dialog was autoconfirmed).
 */
bool VBoxProblemReporter::confirmGoingFullscreen (const QString &aHotKey)
{
    return messageOkCancel (mainMachineWindowShown(), Info,
        tr ("<p>The virtual machine window will be now switched to <b>fullscreen</b> mode. "
            "You can go back to windowed mode at any time by pressing <b>%1</b>.</p>"
            "<p>Note that the <i>Host</i> key is currently defined as <b>%2</b>.</p>"
            "<p>Note that the main menu bar is hidden in fullscreen mode. "
            "You can access it by pressing <b>Host+Home</b>.</p>")
            .arg (aHotKey)
            .arg (UIHotKeyCombination::toReadableString (vboxGlobal().settings().hostCombo())),
        "confirmGoingFullscreen",
        tr ("Switch", "fullscreen"));
}

/**
 *  @param aHotKey Seamless hot key as defined in the menu.
 *
 *  @return @c true if the user has chosen to go seamless (this is always
 *  the case if the dialog was autoconfirmed).
 */
bool VBoxProblemReporter::confirmGoingSeamless (const QString &aHotKey)
{
    return messageOkCancel (mainMachineWindowShown(), Info,
        tr ("<p>The virtual machine window will be now switched to <b>Seamless</b> mode. "
            "You can go back to windowed mode at any time by pressing <b>%1</b>.</p>"
            "<p>Note that the <i>Host</i> key is currently defined as <b>%2</b>.</p>"
            "<p>Note that the main menu bar is hidden in seamless mode. "
            "You can access it by pressing <b>Host+Home</b>.</p>")
            .arg (aHotKey)
            .arg (UIHotKeyCombination::toReadableString (vboxGlobal().settings().hostCombo())),
        "confirmGoingSeamless",
        tr ("Switch", "seamless"));
}

/**
 *  @param aHotKey Scale hot key as defined in the menu.
 *
 *  @return @c true if the user has chosen to go scale (this is always
 *  the case if the dialog was autoconfirmed).
 */
bool VBoxProblemReporter::confirmGoingScale (const QString &aHotKey)
{
    return messageOkCancel (mainMachineWindowShown(), Info,
        tr ("<p>The virtual machine window will be now switched to <b>Scale</b> mode. "
            "You can go back to windowed mode at any time by pressing <b>%1</b>.</p>"
            "<p>Note that the <i>Host</i> key is currently defined as <b>%2</b>.</p>"
            "<p>Note that the main menu bar is hidden in scale mode. "
            "You can access it by pressing <b>Host+Home</b>.</p>")
            .arg (aHotKey)
            .arg (UIHotKeyCombination::toReadableString (vboxGlobal().settings().hostCombo())),
        "confirmGoingScale",
        tr ("Switch", "scale"));
}

/**
 *  Returns @c true if the user has selected to power off the machine.
 */
bool VBoxProblemReporter::remindAboutGuruMeditation (const CConsole &aConsole,
                                                     const QString &aLogFolder)
{
    Q_UNUSED (aConsole);

    int rc = message (mainMachineWindowShown(), GuruMeditation,
        tr ("<p>A critical error has occurred while running the virtual "
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
            .arg (aLogFolder),
        0, /* aAutoConfirmId */
        QIMessageBox::Ok | QIMessageBox::Default,
        QIMessageBox::Ignore | QIMessageBox::Escape);

    return rc == QIMessageBox::Ok;
}

/**
 *  @return @c true if the user has selected to reset the machine.
 */
bool VBoxProblemReporter::confirmVMReset (QWidget *aParent)
{
    return messageOkCancel (aParent ? aParent : mainMachineWindowShown(), Question,
        tr ("<p>Do you really want to reset the virtual machine?</p>"
            "<p>This will cause any unsaved data in applications running inside "
            "it to be lost.</p>"),
        "confirmVMReset" /* aAutoConfirmId */,
        tr ("Reset", "machine"));
}

void VBoxProblemReporter::warnAboutCannotCreateMachineFolder(QWidget *pParent, const QString &strFolderName)
{
    QFileInfo fi(strFolderName);
    message(pParent ? pParent : mainWindowShown(), Critical,
            tr("<p>Cannot create the machine folder <b>%1</b> in the parent folder <nobr><b>%2</b>.</nobr></p>"
               "<p>Please check that the parent really exists and that you have permissions to create the machine folder.</p>")
               .arg(fi.fileName()).arg(fi.absolutePath()));
}

/**
 *  @return @c true if the user has selected to continue without attaching a
 *  hard disk.
 */
bool VBoxProblemReporter::confirmHardDisklessMachine (QWidget *aParent)
{
    return message (aParent, Warning,
        tr ("<p>You didn't attach a hard disk to the new virtual machine. "
            "The machine will not be able to boot unless you attach "
            "a hard disk with a guest operating system or some other bootable "
            "media to it later using the machine settings dialog or the First "
            "Run Wizard.</p><p>Do you wish to continue?</p>"),
        0, /* aAutoConfirmId */
        QIMessageBox::Ok,
        QIMessageBox::Cancel | QIMessageBox::Default | QIMessageBox::Escape,
        0,
        tr ("Continue", "no hard disk attached"),
        tr ("Go Back", "no hard disk attached")) == QIMessageBox::Ok;
}

void VBoxProblemReporter::cannotRunInSelectorMode()
{
    message (mainWindowShown(), Critical,
        tr ("<p>Cannot run VirtualBox in <i>VM Selector</i> "
            "mode due to local restrictions.</p>"
            "<p>The application will now terminate.</p>"));
}

void VBoxProblemReporter::cannotImportAppliance (CAppliance *aAppliance, QWidget *aParent /* = NULL */) const
{
    if (aAppliance->isNull())
    {
        message (aParent ? aParent : mainWindowShown(),
                 Error,
                 tr ("Failed to open appliance."));
    }else
    {
        /* Preserve the current error info before calling the object again */
        COMResult res (*aAppliance);

        /* Add the warnings in the case of an early error */
        QVector<QString> w = aAppliance->GetWarnings();
        QString wstr;
        foreach (const QString &str, w)
            wstr += QString ("<br />Warning: %1").arg (str);
        if (!wstr.isEmpty())
            wstr = "<br />" + wstr;

        message (aParent ? aParent : mainWindowShown(),
                 Error,
                 tr ("Failed to open/interpret appliance <b>%1</b>.").arg (aAppliance->GetPath()),
                 wstr +
                 formatErrorInfo (res));
    }
}

void VBoxProblemReporter::cannotImportAppliance (const CProgress &aProgress, CAppliance* aAppliance, QWidget *aParent /* = NULL */) const
{
    AssertWrapperOk (aProgress);

    message (aParent ? aParent : mainWindowShown(),
             Error,
             tr ("Failed to import appliance <b>%1</b>.").arg (aAppliance->GetPath()),
             formatErrorInfo (aProgress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotCheckFiles (const CProgress &aProgress, QWidget *aParent /* = NULL */) const
{
    AssertWrapperOk (aProgress);

    message (aParent ? aParent : mainWindowShown(),
             Error,
             tr ("Failed to check files."),
             formatErrorInfo (aProgress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotRemoveFiles (const CProgress &aProgress, QWidget *aParent /* = NULL */) const
{
    AssertWrapperOk (aProgress);

    message (aParent ? aParent : mainWindowShown(),
             Error,
             tr ("Failed to remove file."),
             formatErrorInfo (aProgress.GetErrorInfo()));
}

bool VBoxProblemReporter::confirmExportMachinesInSaveState(const QStringList &machineNames, QWidget *pParent /* = NULL */) const
{
    return messageOkCancel(pParent ? pParent : mainWindowShown(), Warning,
        tr("<p>The virtual machine(s) <b>%1</b> are currently in a saved state.</p>"
           "<p>If you continue the runtime state of the exported machine(s) "
           "will be discarded. Note that the existing machine(s) are not "
           "changed.</p>", "",
           machineNames.size()).arg(VBoxGlobal::toHumanReadableList(machineNames)),
        0 /* aAutoConfirmId */,
        tr("Continue"), tr("Cancel"));
}

void VBoxProblemReporter::cannotExportAppliance (CAppliance *aAppliance, QWidget *aParent /* = NULL */) const
{
    if (aAppliance->isNull())
    {
        message (aParent ? aParent : mainWindowShown(),
                 Error,
                 tr ("Failed to create appliance."));
    }else
    {
        /* Preserve the current error info before calling the object again */
        COMResult res (*aAppliance);

        message (aParent ? aParent : mainWindowShown(),
                 Error,
                 tr ("Failed to prepare the export of the appliance <b>%1</b>.").arg (aAppliance->GetPath()),
                 formatErrorInfo (res));
    }
}

void VBoxProblemReporter::cannotExportAppliance (const CMachine &aMachine, CAppliance *aAppliance, QWidget *aParent /* = NULL */) const
{
    if (aAppliance->isNull() ||
        aMachine.isNull())
    {
        message (aParent ? aParent : mainWindowShown(),
                 Error,
                 tr ("Failed to create an appliance."));
    }else
    {
        message (aParent ? aParent : mainWindowShown(),
                 Error,
                 tr ("Failed to prepare the export of the appliance <b>%1</b>.").arg (aAppliance->GetPath()),
                 formatErrorInfo (aMachine));
    }
}

void VBoxProblemReporter::cannotExportAppliance (const CProgress &aProgress, CAppliance* aAppliance, QWidget *aParent /* = NULL */) const
{
    AssertWrapperOk (aProgress);

    message (aParent ? aParent : mainWindowShown(),
             Error,
             tr ("Failed to export appliance <b>%1</b>.").arg (aAppliance->GetPath()),
             formatErrorInfo (aProgress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotUpdateGuestAdditions (const CProgress &aProgress, QWidget *aParent /* = NULL */) const
{
    AssertWrapperOk (aProgress);

    message (aParent ? aParent : mainWindowShown(),
             Error,
             tr ("Failed to update Guest Additions. The Guest Additions installation image will be mounted to provide a manual installation."),
             formatErrorInfo (aProgress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotOpenExtPack(const QString &strFilename, const CExtPackManager &extPackManager, QWidget *pParent)
{
    message (pParent ? pParent : mainWindowShown(),
             Error,
             tr("Failed to open the Extension Pack <b>%1</b>.").arg(strFilename),
             formatErrorInfo(extPackManager));
}

void VBoxProblemReporter::badExtPackFile(const QString &strFilename, const CExtPackFile &extPackFile, QWidget *pParent)
{
    message (pParent ? pParent : mainWindowShown(),
             Error,
             tr("Failed to open the Extension Pack <b>%1</b>.").arg(strFilename),
             "<!--EOM-->" + extPackFile.GetWhyUnusable());
}

void VBoxProblemReporter::cannotInstallExtPack(const QString &strFilename, const CExtPackFile &extPackFile, const CProgress &progress, QWidget *pParent)
{
    if (!pParent)
        pParent = mainWindowShown();
    QString strErrInfo = !extPackFile.isOk() || progress.isNull()
                       ? formatErrorInfo(extPackFile) : formatErrorInfo(progress.GetErrorInfo());
    message (pParent,
             Error,
             tr("Failed to install the Extension Pack <b>%1</b>.").arg(strFilename),
             strErrInfo);
}

void VBoxProblemReporter::cannotUninstallExtPack(const QString &strPackName, const CExtPackManager &extPackManager,
                                                 const CProgress &progress, QWidget *pParent)
{
    if (!pParent)
        pParent = mainWindowShown();
    QString strErrInfo = !extPackManager.isOk() || progress.isNull()
                       ? formatErrorInfo(extPackManager) : formatErrorInfo(progress.GetErrorInfo());
    message (pParent,
             Error,
             tr("Failed to uninstall the Extension Pack <b>%1</b>.").arg(strPackName),
             strErrInfo);
}

bool VBoxProblemReporter::confirmInstallingPackage(const QString &strPackName, const QString &strPackVersion,
                                                   const QString &strPackDescription, QWidget *pParent)
{
    return messageOkCancel (pParent ? pParent : mainWindowShown(),
                            Question,
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

bool VBoxProblemReporter::confirmReplacePackage(const QString &strPackName, const QString &strPackVersionNew,
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
                              Question,
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
                              Question,
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
                              Question,
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

bool VBoxProblemReporter::confirmRemovingPackage(const QString &strPackName, QWidget *pParent)
{
    return messageOkCancel (pParent ? pParent : mainWindowShown(),
                            Question,
                            tr("<p>You are about to remove the VirtualBox extension pack <b>%1</b>.</p>"
                               "<p>Are you sure you want to proceed?</p>").arg(strPackName),
                            0,
                            tr("&Remove"));
}

void VBoxProblemReporter::notifyAboutExtPackInstalled(const QString &strPackName, QWidget *pParent)
{
    message (pParent ? pParent : mainWindowShown(),
             Info,
             tr("The extension pack <br><nobr><b>%1</b><nobr><br> was installed successfully.").arg(strPackName));
}

void VBoxProblemReporter::warnAboutIncorrectPort (QWidget *pParent) const
{
    message(pParent, Error,
            tr("The current port forwarding rules are not valid. "
               "None of the host or guest port values may be set to zero."));
}

bool VBoxProblemReporter::confirmCancelingPortForwardingDialog (QWidget *pParent) const
{
    return messageOkCancel(pParent, Question,
        tr("<p>There are unsaved changes in the port forwarding configuration.</p>"
           "<p>If you proceed your changes will be discarded.</p>"));
}

void VBoxProblemReporter::showRuntimeError (const CConsole &aConsole, bool fatal,
                                            const QString &errorID,
                                            const QString &errorMsg) const
{
    /// @todo (r=dmik) it's just a preliminary box. We need to:
    //  - for fatal errors and non-fatal with-retry errors, listen for a
    //    VM state signal to automatically close the message if the VM
    //    (externally) leaves the Paused state while it is shown.
    //  - make warning messages modeless
    //  - add common buttons like Retry/Save/PowerOff/whatever

    QByteArray autoConfimId = "showRuntimeError.";

    CConsole console = aConsole;
    KMachineState state = console.GetState();
    Type type;
    QString severity;

    if (fatal)
    {
        /* the machine must be paused on fatal errors */
        Assert (state == KMachineState_Paused);
        if (state != KMachineState_Paused)
            console.Pause();
        type = Critical;
        severity = tr ("<nobr>Fatal Error</nobr>", "runtime error info");
        autoConfimId += "fatal.";
    }
    else if (state == KMachineState_Paused)
    {
        type = Error;
        severity = tr ("<nobr>Non-Fatal Error</nobr>", "runtime error info");
        autoConfimId += "error.";
    }
    else
    {
        type = Warning;
        severity = tr ("<nobr>Warning</nobr>", "runtime error info");
        autoConfimId += "warning.";
    }

    autoConfimId += errorID.toUtf8();

    QString formatted ("<!--EOM-->");

    if (!errorMsg.isEmpty())
        formatted.prepend (QString ("<p>%1.</p>").arg (vboxGlobal().emphasize (errorMsg)));

    if (!errorID.isEmpty())
        formatted += QString ("<table bgcolor=#EEEEEE border=0 cellspacing=0 "
                              "cellpadding=0 width=100%>"
                              "<tr><td>%1</td><td>%2</td></tr>"
                              "<tr><td>%3</td><td>%4</td></tr>"
                              "</table>")
                              .arg (tr ("<nobr>Error ID: </nobr>", "runtime error info"),
                                    errorID)
                              .arg (tr ("Severity: ", "runtime error info"),
                                    severity);

    if (!formatted.isEmpty())
        formatted = "<qt>" + formatted + "</qt>";

    int rc = 0;

    if (type == Critical)
    {
        rc = message (mainMachineWindowShown(), type,
            tr ("<p>A fatal error has occurred during virtual machine execution! "
                "The virtual machine will be powered off. Please copy the "
                "following error message using the clipboard to help diagnose "
                "the problem:</p>"),
            formatted, autoConfimId.data());

        /* always power down after a fatal error */
        console.PowerDown();
    }
    else if (type == Error)
    {
        rc = message (mainMachineWindowShown(), type,
            tr ("<p>An error has occurred during virtual machine execution! "
                "The error details are shown below. You may try to correct "
                "the error and resume the virtual machine "
                "execution.</p>"),
            formatted, autoConfimId.data());
    }
    else
    {
        rc = message (mainMachineWindowShown(), type,
            tr ("<p>The virtual machine execution may run into an error "
                "condition as described below. "
                "We suggest that you take "
                "an appropriate action to avert the error."
                "</p>"),
            formatted, autoConfimId.data());
    }

    NOREF (rc);
}

/* static */
QString VBoxProblemReporter::mediumToAccusative (VBoxDefs::MediumType aType, bool aIsHostDrive /* = false */)
{
    QString type =
        aType == VBoxDefs::MediumType_HardDisk ?
            tr ("hard disk", "failed to mount ...") :
        aType == VBoxDefs::MediumType_DVD && aIsHostDrive ?
            tr ("CD/DVD", "failed to mount ... host-drive") :
        aType == VBoxDefs::MediumType_DVD && !aIsHostDrive ?
            tr ("CD/DVD image", "failed to mount ...") :
        aType == VBoxDefs::MediumType_Floppy && aIsHostDrive ?
            tr ("floppy", "failed to mount ... host-drive") :
        aType == VBoxDefs::MediumType_Floppy && !aIsHostDrive ?
            tr ("floppy image", "failed to mount ...") :
        QString::null;

    Assert (!type.isNull());
    return type;
}

/**
 * Formats the given COM result code as a human-readable string.
 *
 * If a mnemonic name for the given result code is found, a string in format
 * "MNEMONIC_NAME (0x12345678)" is returned where the hex number is the result
 * code as is. If no mnemonic name is found, then the raw hex number only is
 * returned (w/o parenthesis).
 *
 * @param aRC   COM result code to format.
 */
/* static */
QString VBoxProblemReporter::formatRC (HRESULT aRC)
{
    QString str;

    PCRTCOMERRMSG msg = NULL;
    const char *errMsg = NULL;

    /* first, try as is (only set bit 31 bit for warnings) */
    if (SUCCEEDED_WARNING (aRC))
        msg = RTErrCOMGet (aRC | 0x80000000);
    else
        msg = RTErrCOMGet (aRC);

    if (msg != NULL)
        errMsg = msg->pszDefine;

#if defined (Q_WS_WIN)

    PCRTWINERRMSG winMsg = NULL;

    /* if not found, try again using RTErrWinGet with masked off top 16bit */
    if (msg == NULL)
    {
        winMsg = RTErrWinGet (aRC & 0xFFFF);

        if (winMsg != NULL)
            errMsg = winMsg->pszDefine;
    }

#endif

    if (errMsg != NULL && *errMsg != '\0')
        str.sprintf ("%s (0x%08X)", errMsg, aRC);
    else
        str.sprintf ("0x%08X", aRC);

    return str;
}

/* static */
QString VBoxProblemReporter::formatErrorInfo (const COMErrorInfo &aInfo,
                                              HRESULT aWrapperRC /* = S_OK */)
{
    QString formatted = doFormatErrorInfo (aInfo, aWrapperRC);
    return QString ("<qt>%1</qt>").arg (formatted);
}

void VBoxProblemReporter::cannotCreateHostInterface(const CHost &host, QWidget *pParent /* = 0 */)
{
    if (thread() == QThread::currentThread())
        sltCannotCreateHostInterface(host, pParent);
    else
        emit sigCannotCreateHostInterface(host, pParent);
}

void VBoxProblemReporter::cannotCreateHostInterface(const CProgress &progress, QWidget *pParent /* = 0 */)
{
    if (thread() == QThread::currentThread())
        sltCannotCreateHostInterface(progress, pParent);
    else
        emit sigCannotCreateHostInterface(progress, pParent);
}

void VBoxProblemReporter::cannotRemoveHostInterface(const CHost &host, const CHostNetworkInterface &iface,
                                                    QWidget *pParent /* = 0 */)
{
    if (thread() == QThread::currentThread())
        sltCannotRemoveHostInterface(host, iface ,pParent);
    else
        emit sigCannotRemoveHostInterface(host, iface, pParent);
}

void VBoxProblemReporter::cannotRemoveHostInterface(const CProgress &progress, const CHostNetworkInterface &iface,
                                                    QWidget *pParent /* = 0 */)
{
    if (thread() == QThread::currentThread())
        sltCannotRemoveHostInterface(progress, iface, pParent);
    else
        emit sigCannotRemoveHostInterface(progress, iface, pParent);
}

void VBoxProblemReporter::cannotAttachDevice(const CMachine &machine, VBoxDefs::MediumType type,
                                             const QString &strLocation, const StorageSlot &storageSlot,
                                             QWidget *pParent /* = 0 */)
{
    if (thread() == QThread::currentThread())
        sltCannotAttachDevice(machine, type, strLocation, storageSlot, pParent);
    else
        emit sigCannotAttachDevice(machine, type, strLocation, storageSlot, pParent);
}

void VBoxProblemReporter::cannotCreateSharedFolder(const CMachine &machine, const QString &strName,
                                                   const QString &strPath, QWidget *pParent /* = 0 */)
{
    if (thread() == QThread::currentThread())
        sltCannotCreateSharedFolder(machine, strName, strPath, pParent);
    else
        emit sigCannotCreateSharedFolder(machine, strName, strPath, pParent);
}

void VBoxProblemReporter::cannotRemoveSharedFolder(const CMachine &machine, const QString &strName,
                                                   const QString &strPath, QWidget *pParent /* = 0 */)
{
    if (thread() == QThread::currentThread())
        sltCannotRemoveSharedFolder(machine, strName, strPath, pParent);
    else
        emit sigCannotRemoveSharedFolder(machine, strName, strPath, pParent);
}

void VBoxProblemReporter::cannotCreateSharedFolder(const CConsole &console, const QString &strName,
                                                   const QString &strPath, QWidget *pParent /* = 0 */)
{
    if (thread() == QThread::currentThread())
        sltCannotCreateSharedFolder(console, strName, strPath, pParent);
    else
        emit sigCannotCreateSharedFolder(console, strName, strPath, pParent);
}

void VBoxProblemReporter::cannotRemoveSharedFolder(const CConsole &console, const QString &strName,
                                                   const QString &strPath, QWidget *pParent /* = 0 */)
{
    if (thread() == QThread::currentThread())
        sltCannotRemoveSharedFolder(console, strName, strPath, pParent);
    else
        emit sigCannotRemoveSharedFolder(console, strName, strPath, pParent);
}

void VBoxProblemReporter::remindAboutWrongColorDepth(ulong uRealBPP, ulong uWantedBPP)
{
    emit sigRemindAboutWrongColorDepth(uRealBPP, uWantedBPP);
}

void VBoxProblemReporter::showGenericError(COMBaseWithEI *object, QWidget *pParent /* = 0 */)
{
    if (   !object
        || object->lastRC() == S_OK)
        return;

    message(pParent ? pParent : mainWindowShown(),
            Error,
            tr("Sorry, some generic error happens."),
            formatErrorInfo(*object));
}

// Public slots
/////////////////////////////////////////////////////////////////////////////

void VBoxProblemReporter::remindAboutUnsupportedUSB2(const QString &strExtPackName, QWidget *pParent)
{
    emit sigRemindAboutUnsupportedUSB2(strExtPackName, pParent);
}

void VBoxProblemReporter::showHelpWebDialog()
{
    vboxGlobal().openURL ("http://www.virtualbox.org");
}

void VBoxProblemReporter::showHelpAboutDialog()
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

void VBoxProblemReporter::showHelpHelpDialog()
{
#ifndef VBOX_OSE
    /* For non-OSE version we just open it: */
    sltShowUserManual(vboxGlobal().helpFile());
#else /* #ifndef VBOX_OSE */
    /* For OSE version we have to check if it present first: */
    QString strUserManualFileName1 = vboxGlobal().helpFile();
    QString strShortFileName = QFileInfo(strUserManualFileName1).fileName();
    QString strUserManualFileName2 = QDir(vboxGlobal().virtualBox().GetHomeFolder()).absoluteFilePath(strShortFileName);
    if (QFile::exists(strUserManualFileName1))
    {
        sltShowUserManual(strUserManualFileName1);
    }
    else if (QFile::exists(strUserManualFileName2))
    {
        sltShowUserManual(strUserManualFileName2);
    }
    else if (!UIDownloaderUserManual::current() && askAboutUserManualDownload(strUserManualFileName1))
    {
        /* Create User Manual downloader: */
        UIDownloaderUserManual *pDl = UIDownloaderUserManual::create();
        /* Configure User Manual downloader: */
        CVirtualBox vbox = vboxGlobal().virtualBox();
        pDl->addSource(QString("http://download.virtualbox.org/virtualbox/%1/").arg(vbox.GetVersion().remove("_OSE")) + strShortFileName);
        pDl->addSource(QString("http://download.virtualbox.org/virtualbox/") + strShortFileName);
        pDl->setTarget(strUserManualFileName2);
        pDl->setParentWidget(mainWindowShown());
        /* After the download is finished => show the document: */
        connect(pDl, SIGNAL(sigDownloadFinished(const QString&)), this, SLOT(sltShowUserManual(const QString&)));
        /* Notify listeners: */
        emit sigDownloaderUserManualCreated();
        /* Start the downloader: */
        pDl->startDownload();
    }
#endif /* #ifdef VBOX_OSE */
}

void VBoxProblemReporter::resetSuppressedMessages()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    vbox.SetExtraData (VBoxDefs::GUI_SuppressMessages, QString::null);
}

void VBoxProblemReporter::sltShowUserManual(const QString &strLocation)
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

void VBoxProblemReporter::sltCannotCreateHostInterface(const CHost &host, QWidget *pParent)
{
    message(pParent ? pParent : mainWindowShown(), Error,
            tr("Failed to create the host-only network interface."),
            formatErrorInfo(host));
}

void VBoxProblemReporter::sltCannotCreateHostInterface(const CProgress &progress, QWidget *pParent)
{
    message(pParent ? pParent : mainWindowShown(), Error,
            tr("Failed to create the host-only network interface."),
            formatErrorInfo(progress.GetErrorInfo()));
}

void VBoxProblemReporter::sltCannotRemoveHostInterface(const CHost &host, const CHostNetworkInterface &iface, QWidget *pParent)
{
    message(pParent ? pParent : mainWindowShown(), Error,
            tr("Failed to remove the host network interface <b>%1</b>.")
               .arg(iface.GetName()),
            formatErrorInfo(host));
}

void VBoxProblemReporter::sltCannotRemoveHostInterface(const CProgress &progress, const CHostNetworkInterface &iface, QWidget *pParent)
{
    message(pParent ? pParent : mainWindowShown(), Error,
            tr("Failed to remove the host network interface <b>%1</b>.")
               .arg(iface.GetName()),
            formatErrorInfo(progress.GetErrorInfo()));
}

void VBoxProblemReporter::sltCannotAttachDevice(const CMachine &machine, VBoxDefs::MediumType type,
                                                const QString &strLocation, const StorageSlot &storageSlot,
                                                QWidget *pParent)
{
    QString strMessage;
    switch (type)
    {
        case VBoxDefs::MediumType_HardDisk:
        {
            strMessage = tr("Failed to attach the hard disk (<nobr><b>%1</b></nobr>) to the slot <i>%2</i> of the machine <b>%3</b>.")
                           .arg(strLocation).arg(vboxGlobal().toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        case VBoxDefs::MediumType_DVD:
        {
            strMessage = tr("Failed to attach the CD/DVD device (<nobr><b>%1</b></nobr>) to the slot <i>%2</i> of the machine <b>%3</b>.")
                           .arg(strLocation).arg(vboxGlobal().toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        case VBoxDefs::MediumType_Floppy:
        {
            strMessage = tr("Failed to attach the floppy device (<nobr><b>%1</b></nobr>) to the slot <i>%2</i> of the machine <b>%3</b>.")
                           .arg(strLocation).arg(vboxGlobal().toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        default:
            break;
    }
    message(pParent ? pParent : mainWindowShown(), Error, strMessage, formatErrorInfo(machine));
}

void VBoxProblemReporter::sltCannotCreateSharedFolder(const CMachine &machine, const QString &strName,
                                                      const QString &strPath, QWidget *pParent)
{
    message(pParent ? pParent : mainMachineWindowShown(), Error,
            tr("Failed to create the shared folder <b>%1</b> "
               "(pointing to <nobr><b>%2</b></nobr>) "
               "for the virtual machine <b>%3</b>.")
               .arg(strName)
               .arg(strPath)
               .arg(CMachine(machine).GetName()),
            formatErrorInfo(machine));
}

void VBoxProblemReporter::sltCannotRemoveSharedFolder(const CMachine &machine, const QString &strName,
                                                      const QString &strPath, QWidget *pParent)
{
    message(pParent ? pParent : mainMachineWindowShown(), Error,
            tr("Failed to remove the shared folder <b>%1</b> "
               "(pointing to <nobr><b>%2</b></nobr>) "
               "from the virtual machine <b>%3</b>.")
               .arg(strName)
               .arg(strPath)
               .arg(CMachine(machine).GetName()),
            formatErrorInfo(machine));
}

void VBoxProblemReporter::sltCannotCreateSharedFolder(const CConsole &console, const QString &strName,
                                                      const QString &strPath, QWidget *pParent)
{
    message(pParent ? pParent : mainMachineWindowShown(), Error,
            tr("Failed to create the shared folder <b>%1</b> "
               "(pointing to <nobr><b>%2</b></nobr>) "
               "for the virtual machine <b>%3</b>.")
               .arg(strName)
               .arg(strPath)
               .arg(CConsole(console).GetMachine().GetName()),
            formatErrorInfo(console));
}

void VBoxProblemReporter::sltCannotRemoveSharedFolder(const CConsole &console, const QString &strName,
                                                      const QString &strPath, QWidget *pParent)
{
    message(pParent ? pParent : mainMachineWindowShown(), Error,
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

void VBoxProblemReporter::sltRemindAboutWrongColorDepth(ulong uRealBPP, ulong uWantedBPP)
{
    const char *kName = "remindAboutWrongColorDepth";

    /* Close the previous (outdated) window if any. We use kName as
     * aAutoConfirmId which is also used as the widget name by default. */
    {
        QWidget *outdated = VBoxGlobal::findWidget(NULL, kName, "QIMessageBox");
        if (outdated)
            outdated->close();
    }

    message(mainMachineWindowShown(), Info,
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

void VBoxProblemReporter::sltRemindAboutUnsupportedUSB2(const QString &strExtPackName, QWidget *pParent)
{
    if (isAlreadyShown("sltRemindAboutUnsupportedUSB2"))
        return;
    setShownStatus("sltRemindAboutUnsupportedUSB2");

    message(pParent ? pParent : mainMachineWindowShown(), Warning,
            tr("<p>USB 2.0 is currently enabled for this virtual machine. "
               "However this requires the <b><nobr>%1</nobr></b> to be installed.</p>"
               "<p>Please install the Extension Pack from the VirtualBox download site. "
               "After this you will be able to re-enable USB 2.0. "
               "It will be disabled in the meantime unless you cancel the current settings changes.</p>")
               .arg(strExtPackName));

    clearShownStatus("sltRemindAboutUnsupportedUSB2");
}

VBoxProblemReporter::VBoxProblemReporter()
{
    /* Register required objects as meta-types: */
    qRegisterMetaType<CProgress>();
    qRegisterMetaType<CHost>();
    qRegisterMetaType<CMachine>();
    qRegisterMetaType<CConsole>();
    qRegisterMetaType<CHostNetworkInterface>();
    qRegisterMetaType<VBoxDefs::MediumType>();
    qRegisterMetaType<StorageSlot>();

    /* Prepare required connections: */
    connect(this, SIGNAL(sigCannotCreateHostInterface(const CHost&, QWidget*)),
            this, SLOT(sltCannotCreateHostInterface(const CHost&, QWidget*)),
            Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(sigCannotCreateHostInterface(const CProgress&, QWidget*)),
            this, SLOT(sltCannotCreateHostInterface(const CProgress&, QWidget*)),
            Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(sigCannotRemoveHostInterface(const CHost&, const CHostNetworkInterface&, QWidget*)),
            this, SLOT(sltCannotRemoveHostInterface(const CHost&, const CHostNetworkInterface&, QWidget*)),
            Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(sigCannotRemoveHostInterface(const CProgress&, const CHostNetworkInterface&, QWidget*)),
            this, SLOT(sltCannotRemoveHostInterface(const CProgress&, const CHostNetworkInterface&, QWidget*)),
            Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(sigCannotAttachDevice(const CMachine&, VBoxDefs::MediumType, const QString&, const StorageSlot&, QWidget*)),
            this, SLOT(sltCannotAttachDevice(const CMachine&, VBoxDefs::MediumType, const QString&, const StorageSlot&, QWidget*)),
            Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(sigCannotCreateSharedFolder(const CMachine&, const QString&, const QString&, QWidget*)),
            this, SLOT(sltCannotCreateSharedFolder(const CMachine&, const QString&, const QString&, QWidget*)),
            Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(sigCannotRemoveSharedFolder(const CMachine&, const QString&, const QString&, QWidget*)),
            this, SLOT(sltCannotRemoveSharedFolder(const CMachine&, const QString&, const QString&, QWidget*)),
            Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(sigCannotCreateSharedFolder(const CConsole&, const QString&, const QString&, QWidget*)),
            this, SLOT(sltCannotCreateSharedFolder(const CConsole&, const QString&, const QString&, QWidget*)),
            Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(sigCannotRemoveSharedFolder(const CConsole&, const QString&, const QString&, QWidget*)),
            this, SLOT(sltCannotRemoveSharedFolder(const CConsole&, const QString&, const QString&, QWidget*)),
            Qt::BlockingQueuedConnection);
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

/* Returns a reference to the global VirtualBox problem reporter instance: */
VBoxProblemReporter &VBoxProblemReporter::instance()
{
    static VBoxProblemReporter global_instance;
    return global_instance;
}

QString VBoxProblemReporter::doFormatErrorInfo (const COMErrorInfo &aInfo,
                                                HRESULT aWrapperRC /* = S_OK */)
{
    /* Compose complex details string with internal <!--EOM--> delimiter to
     * make it possible to split string into info & details parts which will
     * be used separately in QIMessageBox */
    QString formatted;

    /* Check if details text is NOT empty: */
    QString strDetailsInfo = aInfo.text();
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

    if (aInfo.isBasicAvailable())
    {
#if defined (Q_WS_WIN)
        haveResultCode = aInfo.isFullAvailable();
        bool haveComponent = true;
        bool haveInterfaceID = true;
#else /* defined (Q_WS_WIN) */
        haveResultCode = true;
        bool haveComponent = aInfo.isFullAvailable();
        bool haveInterfaceID = aInfo.isFullAvailable();
#endif

        if (haveResultCode)
        {
            formatted += QString ("<tr><td>%1</td><td><tt>%2</tt></td></tr>")
                .arg (tr ("Result&nbsp;Code: ", "error info"))
                .arg (formatRC (aInfo.resultCode()));
        }

        if (haveComponent)
            formatted += QString ("<tr><td>%1</td><td>%2</td></tr>")
                .arg (tr ("Component: ", "error info"), aInfo.component());

        if (haveInterfaceID)
        {
            QString s = aInfo.interfaceID();
            if (!aInfo.interfaceName().isEmpty())
                s = aInfo.interfaceName() + ' ' + s;
            formatted += QString ("<tr><td>%1</td><td>%2</td></tr>")
                .arg (tr ("Interface: ", "error info"), s);
        }

        if (!aInfo.calleeIID().isNull() && aInfo.calleeIID() != aInfo.interfaceID())
        {
            QString s = aInfo.calleeIID();
            if (!aInfo.calleeName().isEmpty())
                s = aInfo.calleeName() + ' ' + s;
            formatted += QString ("<tr><td>%1</td><td>%2</td></tr>")
                .arg (tr ("Callee: ", "error info"), s);
        }
    }

    if (FAILED (aWrapperRC) &&
        (!haveResultCode || aWrapperRC != aInfo.resultCode()))
    {
        formatted += QString ("<tr><td>%1</td><td><tt>%2</tt></td></tr>")
            .arg (tr ("Callee&nbsp;RC: ", "error info"))
            .arg (formatRC (aWrapperRC));
    }

    formatted += "</table>";

    if (aInfo.next())
        formatted = formatted + "<!--EOP-->" + doFormatErrorInfo (*aInfo.next());

    return formatted;
}

