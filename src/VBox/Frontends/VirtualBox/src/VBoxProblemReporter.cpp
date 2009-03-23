/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxProblemReporter class implementation
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

#include "VBoxProblemReporter.h"

#include "VBoxGlobal.h"
#include "VBoxSelectorWnd.h"
#include "VBoxConsoleWnd.h"

#include "VBoxAboutDlg.h"

#include "QIHotKeyEdit.h"

#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

/* Qt includes */
#include <QProgressDialog>
#include <QProcess>
#include <QFileInfo>
#ifdef Q_WS_MAC
# include <QPushButton>
#endif

#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/path.h>

#if defined (Q_WS_WIN32)
#include <Htmlhelp.h>
#endif

////////////////////////////////////////////////////////////////////////////////
// VBoxProgressDialog class
////////////////////////////////////////////////////////////////////////////////

/**
 * A QProgressDialog enhancement that allows to:
 *
 * 1) prevent closing the dialog when it has no cancel button;
 * 2) effectively track the IProgress object completion (w/o using
 *    IProgress::waitForCompletion() and w/o blocking the UI thread in any other
 *    way for too long).
 *
 * @note The CProgress instance is passed as a non-const reference to the
 *       constructor (to memorize COM errors if they happen), and therefore must
 *       not be destroyed before the created VBoxProgressDialog instance is
 *       destroyed.
 */
class VBoxProgressDialog : public QProgressDialog
{
public:

    VBoxProgressDialog (CProgress &aProgress, const QString &aTitle,
                        int aMinDuration = 2000, QWidget *aCreator = 0)
        : QProgressDialog (aCreator,
                           Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint)
        , mProgress (aProgress)
        , mEventLoop (new QEventLoop (this))
        , mCalcelEnabled (false)
        , mOpCount (mProgress.GetOperationCount())
        , mCurOp (mProgress.GetOperation() + 1)
        , mEnded (false)
    {
        setModal (true);
#ifdef Q_WS_MAC
        ::darwinSetHidesAllTitleButtons (this);
        ::darwinSetShowsResizeIndicator (this, false);
#endif /* Q_WS_MAC */

        if (mOpCount > 1)
            setLabelText (QString (sOpDescTpl)
                          .arg (mProgress.GetOperationDescription())
                          .arg (mCurOp).arg (mOpCount));
        else
            setLabelText (QString ("%1...")
                          .arg (mProgress.GetOperationDescription()));
        setCancelButtonText (QString::null);
        setMaximum (100);
        setWindowTitle (QString ("%1: %2")
                    .arg (aTitle, mProgress.GetDescription()));
        setMinimumDuration (aMinDuration);
        setValue (0);
    }

    int run (int aRefreshInterval)
    {
        if (mProgress.isOk())
        {
            /* Start refresh timer */
            int id = startTimer (aRefreshInterval);

            /* The progress dialog is automatically shown after the duration is over */

            /* Enter the modal loop */
            mEventLoop->exec();

            /* Kill refresh timer */
            killTimer (id);

            return result();
        }
        return Rejected;
    }

    /* These methods disabled for now as not used anywhere */
    // bool cancelEnabled() const { return mCalcelEnabled; }
    // void setCancelEnabled (bool aEnabled) { mCalcelEnabled = aEnabled; }

protected:

    virtual void timerEvent (QTimerEvent * /* aEvent */)
    {
        if (!mEnded && (!mProgress.isOk() || mProgress.GetCompleted()))
        {
            /* Progress finished */
            if (mProgress.isOk())
            {
                setValue (100);
                setResult (Accepted);
            }
            /* Progress is not valid */
            else
                setResult (Rejected);

            /* Request to exit loop */
            mEnded = true;

            /* The progress will be finalized
             * on next timer iteration. */
            return;
        }

        if (mEnded)
        {
            /* Exit loop if it is running */
            if (mEventLoop->isRunning())
                mEventLoop->quit();
            return;
        }

        /* Update the progress dialog */
        ulong newOp = mProgress.GetOperation() + 1;
        if (newOp != mCurOp)
        {
            mCurOp = newOp;
            setLabelText (QString (sOpDescTpl)
                .arg (mProgress.GetOperationDescription())
                .arg (mCurOp).arg (mOpCount));
        }
        setValue (mProgress.GetPercent());
    }

    virtual void reject() { if (mCalcelEnabled) QProgressDialog::reject(); }

    virtual void closeEvent (QCloseEvent *aEvent)
    {
        if (mCalcelEnabled)
            QProgressDialog::closeEvent (aEvent);
        else
            aEvent->ignore();
    }

private:

    CProgress &mProgress;
    QEventLoop *mEventLoop;
    bool mCalcelEnabled;
    const ulong mOpCount;
    ulong mCurOp;
    bool mEnded;

    static const char *sOpDescTpl;
};

const char *VBoxProgressDialog::sOpDescTpl = "%1... (%2/%3)";

////////////////////////////////////////////////////////////////////////////////
// VBoxProblemReporter class
////////////////////////////////////////////////////////////////////////////////

/**
 *  Returns a reference to the global VirtualBox problem reporter instance.
 */
VBoxProblemReporter &VBoxProblemReporter::instance()
{
    static VBoxProblemReporter vboxProblem_instance;
    return vboxProblem_instance;
}

bool VBoxProblemReporter::isValid() const
{
    return qApp != 0;
}

// Helpers
/////////////////////////////////////////////////////////////////////////////

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
 *      Message text to display (can contain sinmple Qt-html tags).
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

    QIMessageBox *box = new QIMessageBox (title, aMessage, icon, aButton1, aButton2,
                                          aButton3, aParent, aAutoConfirmId);

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

    int rc = box->exec();

    if (aAutoConfirmId)
    {
        if (box->isFlagChecked())
        {
            msgs << aAutoConfirmId;
            vbox.SetExtraData (VBoxDefs::GUI_SuppressMessages, msgs.join (","));
        }
    }

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

/**
 *  Shows a modal progress dialog using a CProgress object passed as an
 *  argument to track the progress.
 *
 *  @param aProgress    progress object to track
 *  @param aTitle       title prefix
 *  @param aParent      parent widget
 *  @param aMinDuration time (ms) that must pass before the dialog appears
 */
bool VBoxProblemReporter::showModalProgressDialog (
    CProgress &aProgress, const QString &aTitle, QWidget *aParent,
    int aMinDuration)
{
    QApplication::setOverrideCursor (QCursor (Qt::WaitCursor));

    VBoxProgressDialog progressDlg (aProgress, aTitle, aMinDuration,
                                    aParent);

    /* run the dialog with the 100 ms refresh interval */
    progressDlg.run (100);

    QApplication::restoreOverrideCursor();

    return true;
}

/**
 *  Returns what main window (selector or console) is now shown, or
 *  zero if none of them. The selector window takes precedence.
 */
QWidget *VBoxProblemReporter::mainWindowShown() const
{
    /* It may happen that this method is called during VBoxGlobal
     * initialization or even after it failed (for example, to show some
     * error message). Return no main window in this case. */
    if (!vboxGlobal().isValid())
        return 0;

#if defined (VBOX_GUI_SEPARATE_VM_PROCESS)
    if (vboxGlobal().isVMConsoleProcess())
    {
        if (vboxGlobal().consoleWnd().isVisible())
            return &vboxGlobal().consoleWnd();
    }
    else
    {
        if (vboxGlobal().selectorWnd().isVisible())
            return &vboxGlobal().selectorWnd();
    }
#else
    if (vboxGlobal().consoleWnd().isVisible())
        return &vboxGlobal().consoleWnd();
    if (vboxGlobal().selectorWnd().isVisible())
        return &vboxGlobal().selectorWnd();
#endif
    return 0;
}

// Generic Problem handlers
/////////////////////////////////////////////////////////////////////////////

bool VBoxProblemReporter::askForOverridingFileIfExists (const QString& aPath, QWidget *aParent /* = NULL */) const
{
    QFileInfo fi (aPath);
    if (fi.exists())
        return messageYesNo (aParent, Question, tr ("A file named <b>%1</b> already exists. Are you sure you want to replace it?<br /><br />The file already exists in \"%2\". Replacing it will overwrite its contents.").arg (fi.fileName()). arg (fi.absolutePath()));
    else
        return true;
}

bool VBoxProblemReporter::askForOverridingFilesIfExists (const QStringList& aPaths, QWidget *aParent /* = NULL */) const
{
    QStringList existingFiles;
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
        return messageYesNo (aParent, Question, tr ("The following files exists already:<br /><br />%1<br /><br />Are you sure you want to replace them? Replacing them will overwrite there contents.").arg (existingFiles.join ("<br />")));
    else
        return true;
}
// Special Problem handlers
/////////////////////////////////////////////////////////////////////////////

void VBoxProblemReporter::showBETAWarning()
{
    message
        (0, Warning,
         tr ("You're running a prerelease version of VirtualBox. "
             "This version is not designed for production use."));
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

void VBoxProblemReporter::cannotOpenLicenseFile (QWidget *aParent,
                                                 const QString &aPath)
{
    message
        (aParent, VBoxProblemReporter::Error,
         tr ("Failed to open the license file <nobr><b>%1</b></nobr>. "
             "Check file permissions.")
             .arg (aPath));
}
#endif

void VBoxProblemReporter::cannotOpenURL (const QString &aURL)
{
    message
        (mainWindowShown(), VBoxProblemReporter::Error,
         tr ("Failed to open <tt>%1</tt>. Make sure your desktop environment "
             "can properly handle URLs of this type.")
         .arg (aURL));
}

void VBoxProblemReporter::
cannotCopyFile (const QString &aSrc, const QString &aDst, int aVRC)
{
    PCRTSTATUSMSG msg = RTErrGet (aVRC);
    Assert (msg);

    QString err = QString ("%1: %2").arg (msg->pszDefine, msg->pszMsgShort);
    if (err.endsWith ("."))
        err.truncate (err.length() - 1);

    message (mainWindowShown(), VBoxProblemReporter::Error,
        tr ("Failed to copy file <b><nobr>%1</nobr></b> to "
             "<b><nobr>%2</nobr></b> (%3).")
             .arg (aSrc, aDst, err));
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

void VBoxProblemReporter::cannotSaveGlobalSettings (const CVirtualBox &vbox,
                                                    QWidget *parent /* = 0 */)
{
    /* preserve the current error info before calling the object again */
    COMResult res (vbox);

    message (parent ? parent : mainWindowShown(), Error,
             tr ("<p>Failed to save the global VirtualBox settings to "
                 "<b><nobr>%1</nobr></b>.</p>")
                 .arg (vbox.GetSettingsFilePath()),
             formatErrorInfo (res));
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
                    : QString ("<p>%1.</p>").arg (vboxGlobal().emphasize (error)));
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

#ifdef RT_OS_LINUX
    /* xxx There is no macro to turn an error into a warning, but we need
     *     to do that here. */
    if (res.rc() == (VBOX_E_HOST_ERROR & ~0x80000000))
    {
        message (mainWindowShown(), VBoxProblemReporter::Warning,
                 tr ("Could not access USB on the host system, because "
                     "neither the USB file system (usbfs) nor the DBus "
                     "and hal services are currently available. If you "
                     "wish to use host USB devices inside guest systems, "
                     "you must correct this and restart VirtualBox."),
                 formatErrorInfo (res),
                 "cannotAccessUSB" /* aAutoConfirmId */);
        return;
    }
#endif
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

void VBoxProblemReporter::cannotDeleteMachine (const CVirtualBox &vbox,
                                               const CMachine &machine)
{
    /* preserve the current error info before calling the object again */
    COMResult res (machine);

    message (mainWindowShown(), Error,
        tr ("Failed to remove the virtual machine <b>%1</b>.")
            .arg (machine.GetName()),
        !vbox.isOk() ? formatErrorInfo (vbox) : formatErrorInfo (res));
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
            "does not use the ACPI subsystem."));
}

bool VBoxProblemReporter::warnAboutVirtNotEnabled()
{
    return messageOkCancel (mainWindowShown(), Error,
        tr ("<p>VT-x/AMD-V hardware acceleration has been enabled, but is "
            "not operational. Your 64 bits guest will fail to detect a 64 "
            "bits CPU and will not be able to boot.</p><p>Please check if you "
            "have enabled VT-x/AMD-V properly in the BIOS of your host "
            "computer.</p>"),
        0 /* aAutoConfirmId */,
        tr ("Close VM"), tr ("Continue"));
}

void VBoxProblemReporter::cannotSetSnapshotFolder (const CMachine &aMachine,
                                                   const QString &aPath)
{
    message (
        mainWindowShown(),
        Error,
        tr ("Failed to change the snapshot folder path of the "
            "virtual machine <b>%1<b> to <nobr><b>%2</b></nobr>.")
            .arg (aMachine.GetName())
            .arg (aPath),
        formatErrorInfo (aMachine));
}

bool VBoxProblemReporter::askAboutSnapshotAndStateDiscarding()
{
    return messageOkCancel (mainWindowShown(), Question,
        tr ("<p>Are you sure you wish to delete the selected snapshot "
            "and saved state?</p>"),
        "confirmSnapshotAndStateDiscarding" /* aAutoConfirmId */,
        tr ("Discard"), tr ("Cancel"));
}

void VBoxProblemReporter::cannotDiscardSnapshot (const CConsole &aConsole,
                                                 const QString &aSnapshotName)
{
    message (mainWindowShown(), Error,
        tr ("Failed to discard the snapshot <b>%1</b> of the virtual "
            "machine <b>%2</b>.")
            .arg (aSnapshotName)
            .arg (CConsole (aConsole).GetMachine().GetName()),
        formatErrorInfo (aConsole));
}

void VBoxProblemReporter::cannotDiscardSnapshot (const CProgress &aProgress,
                                                 const QString &aSnapshotName)
{
    CConsole console (CProgress (aProgress).GetInitiator());

    message (mainWindowShown(), Error,
        tr ("Failed to discard the snapshot <b>%1</b> of the virtual "
            "machine <b>%2</b>.")
            .arg (aSnapshotName)
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (aProgress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotDiscardCurrentState (const CConsole &console)
{
    message (
        mainWindowShown(),
        Error,
        tr ("Failed to discard the current state of the virtual "
            "machine <b>%1</b>.")
            .arg (CConsole (console).GetMachine().GetName()),
        formatErrorInfo (console));
}

void VBoxProblemReporter::cannotDiscardCurrentState (const CProgress &progress)
{
    CConsole console (CProgress (progress).GetInitiator());

    message (
        mainWindowShown(),
        Error,
        tr ("Failed to discard the current state of the virtual "
            "machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (progress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotDiscardCurrentSnapshotAndState (const CConsole &console)
{
    message (
        mainWindowShown(),
        Error,
        tr ("Failed to discard the current snapshot and the current state "
            "of the virtual machine <b>%1</b>.")
            .arg (CConsole (console).GetMachine().GetName()),
        formatErrorInfo (console));
}

void VBoxProblemReporter::cannotDiscardCurrentSnapshotAndState (const CProgress &progress)
{
    CConsole console (CProgress (progress).GetInitiator());

    message (
        mainWindowShown(),
        Error,
        tr ("Failed to discard the current snapshot and the current state "
            "of the virtual machine <b>%1</b>.")
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (progress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotFindMachineByName (const CVirtualBox &vbox,
                                                   const QString &name)
{
    message (
        mainWindowShown(),
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
    message (&vboxGlobal().consoleWnd(), Error,
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
    return message (&vboxGlobal().consoleWnd(), Warning,
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

bool VBoxProblemReporter::confirmMachineDeletion (const CMachine &machine)
{
    QString msg;
    QString button;
    QString name;

    if (machine.GetAccessible())
    {
        name = machine.GetName();
        msg = tr ("<p>Are you sure you want to permanently delete "
                  "the virtual machine <b>%1</b>?</p>"
                  "<p>This operation cannot be undone.</p>")
                  .arg (name);
        button = tr ("Delete", "machine");
    }
    else
    {
        /* this should be in sync with VBoxVMListBoxItem::recache() */
        QFileInfo fi (machine.GetSettingsFilePath());
        name = fi.suffix().toLower() == "xml" ?
               fi.completeBaseName() : fi.fileName();
        msg = tr ("<p>Are you sure you want to unregister the inaccessible "
                  "virtual machine <b>%1</b>?</p>"
                  "<p>You will no longer be able to register it back from "
                  "GUI.</p>")
                  .arg (name);
        button = tr ("Unregister", "machine");
    }

    return messageOkCancel (&vboxGlobal().selectorWnd(), Question, msg,
                            0 /* aAutoConfirmId */, button);
}

bool VBoxProblemReporter::confirmDiscardSavedState (const CMachine &machine)
{
    return messageOkCancel (&vboxGlobal().selectorWnd(), Question,
        tr ("<p>Are you sure you want to discard the saved state of "
            "the virtual machine <b>%1</b>?</p>"
            "<p>This operation is equivalent to resetting or powering off "
            "the machine without doing a proper shutdown by means of the "
            "guest OS.</p>")
            .arg (machine.GetName()),
        0 /* aAutoConfirmId */,
        tr ("Discard", "saved state"));
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
            .arg (toAccusative (aMedium.type()))
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
            .arg (toAccusative (aMedium.type()))
            .arg (aMedium.location());

    if (aMedium.type() == VBoxDefs::MediaType_HardDisk)
    {
        if (aMedium.state() == KMediaState_Inaccessible)
            msg +=
                tr ("Note that this hard disk is inaccessible so that its "
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
                "deleted and therefore it will be possible to add it to "
                "the list later again.</p>");

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
                                                       const CHardDisk &aHD,
                                                       const CProgress &aProgress)
{
    /* below, we use CHardDisk (aHD) to preserve current error info
     * for formatErrorInfo() */

    message (aParent, Error,
        tr ("Failed to delete the storage unit of the hard disk <b>%1</b>.")
            .arg (CHardDisk (aHD).GetLocation()),
        !aHD.isOk() ? formatErrorInfo (aHD) :
        !aProgress.isOk() ? formatErrorInfo (aProgress) :
        formatErrorInfo (aProgress.GetErrorInfo()));
}

int VBoxProblemReporter::confirmDetachAddControllerSlots (QWidget *aParent) const
{
    return messageOkCancel (aParent, Question,
        tr ("<p>There are hard disks attached to ports of the additional controller. "
            "If you disable the additional controller, all these hard disks "
            "will be automatically detached.</p>"
            "<p>Are you sure that you want to "
            "disable the additional controller?</p>"),
        0 /* aAutoConfirmId */,
        tr ("Disable", "hard disk"));
}

int VBoxProblemReporter::confirmChangeAddControllerSlots (QWidget *aParent) const
{
    return messageOkCancel (aParent, Question,
        tr ("<p>There are hard disks attached to ports of the additional controller. "
            "If you change the additional controller, all these hard disks "
            "will be automatically detached.</p>"
            "<p>Are you sure that you want to "
            "change the additional controller?</p>"),
        0 /* aAutoConfirmId */,
        tr ("Change", "hard disk"));
}

int VBoxProblemReporter::confirmRunNewHDWzdOrVDM (QWidget* aParent)
{
    return message (aParent, Info,
        tr ("<p>There are no unused hard disks available for the newly created "
            "attachment.</p>"
            "<p>Press the <b>Create</b> button to start the <i>New Virtual "
            "Disk</i> wizard and create a new hard disk, or press the "
            "<b>Select</b> button to open the <i>Virtual Media Manager</i> "
            "and select what to do.</p>"),
        0, /* aAutoConfirmId */
        QIMessageBox::Yes,
        QIMessageBox::No | QIMessageBox::Default,
        QIMessageBox::Cancel | QIMessageBox::Escape,
        tr ("&Create", "hard disk"),
        tr ("Select", "hard disk"));
}

void VBoxProblemReporter::cannotCreateHardDiskStorage (
    QWidget *aParent, const CVirtualBox &aVBox, const QString &aLocation,
    const CHardDisk &aHD, const CProgress &aProgress)
{
    message (aParent, Error,
        tr ("Failed to create the hard disk storage <nobr><b>%1</b>.</nobr>")
            .arg (aLocation),
        !aVBox.isOk() ? formatErrorInfo (aVBox) :
        !aHD.isOk() ? formatErrorInfo (aHD) :
        !aProgress.isOk() ? formatErrorInfo (aProgress) :
        formatErrorInfo (aProgress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotAttachHardDisk (
    QWidget *aParent, const CMachine &aMachine, const QString &aLocation,
    KStorageBus aBus, LONG aChannel, LONG aDevice)
{
    message (aParent, Error,
        tr ("Failed to attach the hard disk <nobr><b>%1</b></nobr> "
            "to the slot <i>%2</i> of the machine <b>%3</b>.")
            .arg (aLocation)
            .arg (vboxGlobal().toFullString (aBus, aChannel, aDevice))
            .arg (CMachine (aMachine).GetName()),
        formatErrorInfo (aMachine));
}

void VBoxProblemReporter::cannotDetachHardDisk (
    QWidget *aParent, const CMachine &aMachine, const QString &aLocation,
    KStorageBus aBus, LONG aChannel, LONG aDevice)
{
    message (aParent, Error,
        tr ("Failed to detach the hard disk <nobr><b>%1</b></nobr> "
            "from the slot <i>%2</i> of the machine <b>%3</b>.")
            .arg (aLocation)
            .arg (vboxGlobal().toFullString (aBus, aChannel, aDevice))
            .arg (CMachine (aMachine).GetName()),
         formatErrorInfo (aMachine));
}

void VBoxProblemReporter::
cannotMountMedium (QWidget *aParent, const CMachine &aMachine,
                   const VBoxMedium &aMedium, const COMResult &aResult)
{
    /** @todo (translation-related): the gender of "the" in translations
     * will depend on the gender of aMedium.type(). */
    message (aParent, Error,
        tr ("Failed to mount the %1 <nobr><b>%2</b></nobr> "
            "to the machine <b>%3</b>.")
            .arg (toAccusative (aMedium.type()))
            .arg (aMedium.location())
            .arg (CMachine (aMachine).GetName()),
      formatErrorInfo (aResult));
}

void VBoxProblemReporter::
cannotUnmountMedium (QWidget *aParent, const CMachine &aMachine,
                     const VBoxMedium &aMedium, const COMResult &aResult)
{
    /** @todo (translation-related): the gender of "the" in translations
     * will depend on the gender of aMedium.type(). */
    message (aParent, Error,
        tr ("Failed to unmount the %1 <nobr><b>%2</b></nobr> "
            "from the machine <b>%3</b>.")
            .arg (toAccusative (aMedium.type()))
            .arg (aMedium.location())
            .arg (CMachine (aMachine).GetName()),
      formatErrorInfo (aResult));
}

void VBoxProblemReporter::cannotOpenMedium (
    QWidget *aParent, const CVirtualBox &aVBox,
    VBoxDefs::MediaType aType, const QString &aLocation)
{
    /** @todo (translation-related): the gender of "the" in translations
     * will depend on the gender of aMedium.type(). */
    message (aParent, Error,
        tr ("Failed to open the %1 <nobr><b>%2</b></nobr>.")
            .arg (toAccusative (aType))
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
            .arg (toAccusative (aMedium.type()))
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
        tr ("Failed to get the accessibility state of the medium "
            "<nobr><b>%1</b></nobr>.")
            .arg (aMedium.location()),
        formatErrorInfo (aMedium.result()));
}

#if defined Q_WS_WIN

int VBoxProblemReporter::confirmDeletingHostInterface (const QString &aName,
                                                       QWidget *aParent)
{
    return vboxProblem().message (aParent, VBoxProblemReporter::Question,
        tr ("<p>Do you want to remove the selected host network interface "
            "<nobr><b>%1</b>?</nobr></p>"
            "<p><b>Note:</b> This interface may be in use by one or more "
            "network adapters of this or another VM. After it is removed, these "
            "adapters will no longer work until you correct their settings by "
            "either choosing a different interface name or a different adapter "
            "attachment type.</p>").arg (aName),
        0, /* autoConfirmId */
        QIMessageBox::Ok | QIMessageBox::Default,
        QIMessageBox::Cancel | QIMessageBox::Escape);
}

void VBoxProblemReporter::cannotCreateHostInterface (
    const CHost &host, QWidget *parent)
{
    message (parent ? parent : mainWindowShown(), Error,
        tr ("Failed to create the host-only network interface."),
        formatErrorInfo (host));
}

void VBoxProblemReporter::cannotCreateHostInterface (
    const CProgress &progress, QWidget *parent)
{
    message (parent ? parent : mainWindowShown(), Error,
        tr ("Failed to create the host-only network interface."),
        formatErrorInfo (progress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotRemoveHostInterface (
    const CHost &host, const CHostNetworkInterface &iface, QWidget *parent)
{
    message (parent ? parent : mainWindowShown(), Error,
        tr ("Failed to remove the host network interface <b>%1</b>.")
            .arg (iface.GetName()),
        formatErrorInfo (host));
}

void VBoxProblemReporter::cannotRemoveHostInterface (
    const CProgress &progress, const CHostNetworkInterface &iface, QWidget *parent)
{
    message (parent ? parent : mainWindowShown(), Error,
        tr ("Failed to remove the host network interface <b>%1</b>.")
            .arg (iface.GetName()),
        formatErrorInfo (progress.GetErrorInfo()));
}

#endif

void VBoxProblemReporter::cannotAttachUSBDevice (const CConsole &console,
                                                 const QString &device)
{
    /* preserve the current error info before calling the object again */
    COMResult res (console);

    message (&vboxGlobal().consoleWnd(), Error,
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
    message (&vboxGlobal().consoleWnd(), Error,
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

    message (&vboxGlobal().consoleWnd(), Error,
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
    message (&vboxGlobal().consoleWnd(), Error,
        tr ("Failed to detach the USB device <b>%1</b> "
            "from the virtual machine <b>%2</b>.")
            .arg (device)
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (error));
}

void VBoxProblemReporter::cannotCreateSharedFolder (QWidget        *aParent,
                                                    const CMachine &aMachine,
                                                    const QString  &aName,
                                                    const QString  &aPath)
{
    /* preserve the current error info before calling the object again */
    COMResult res (aMachine);

    message (aParent, Error,
             tr ("Failed to create a shared folder <b>%1</b> "
                 "(pointing to <nobr><b>%2</b></nobr>) "
                 "for the virtual machine <b>%3</b>.")
                 .arg (aName)
                 .arg (aPath)
                 .arg (aMachine.GetName()),
             formatErrorInfo (res));
}

void VBoxProblemReporter::cannotRemoveSharedFolder (QWidget        *aParent,
                                                    const CMachine &aMachine,
                                                    const QString  &aName,
                                                    const QString  &aPath)
{
    /* preserve the current error info before calling the object again */
    COMResult res (aMachine);

    message (aParent, Error,
             tr ("Failed to remove the shared folder <b>%1</b> "
                 "(pointing to <nobr><b>%2</b></nobr>) "
                 "from the virtual machine <b>%3</b>.")
                 .arg (aName)
                 .arg (aPath)
                 .arg (aMachine.GetName()),
             formatErrorInfo (res));
}

void VBoxProblemReporter::cannotCreateSharedFolder (QWidget        *aParent,
                                                    const CConsole &aConsole,
                                                    const QString  &aName,
                                                    const QString  &aPath)
{
    /* preserve the current error info before calling the object again */
    COMResult res (aConsole);

    message (aParent, Error,
             tr ("Failed to create a shared folder <b>%1</b> "
                 "(pointing to <nobr><b>%2</b></nobr>) "
                 "for the virtual machine <b>%3</b>.")
                 .arg (aName)
                 .arg (aPath)
                 .arg (aConsole.GetMachine().GetName()),
             formatErrorInfo (res));
}

void VBoxProblemReporter::cannotRemoveSharedFolder (QWidget        *aParent,
                                                    const CConsole &aConsole,
                                                    const QString  &aName,
                                                    const QString  &aPath)
{
    /* preserve the current error info before calling the object again */
    COMResult res (aConsole);

    message (aParent, Error,
             tr ("<p>Failed to remove the shared folder <b>%1</b> "
                 "(pointing to <nobr><b>%2</b></nobr>) "
                 "from the virtual machine <b>%3</b>.</p>"
                 "<p>Please close all programs in the guest OS that "
                 "may be using this shared folder and try again.</p>")
                 .arg (aName)
                 .arg (aPath)
                 .arg (aConsole.GetMachine().GetName()),
             formatErrorInfo (res));
}

int VBoxProblemReporter::cannotFindGuestAdditions (const QString &aSrc1,
                                                   const QString &aSrc2)
{
    return message (&vboxGlobal().consoleWnd(), Question,
                    tr ("<p>Could not find the VirtualBox Guest Additions "
                        "CD image file <nobr><b>%1</b></nobr> or "
                        "<nobr><b>%2</b>.</nobr></p><p>Do you want to "
                        "download this CD image from the Internet?</p>")
                        .arg (aSrc1).arg (aSrc2),
                    0, /* aAutoConfirmId */
                    QIMessageBox::Yes | QIMessageBox::Default,
                    QIMessageBox::No | QIMessageBox::Escape);
}

void VBoxProblemReporter::cannotDownloadGuestAdditions (const QString &aURL,
                                                        const QString &aReason)
{
    message (&vboxGlobal().consoleWnd(), Error,
             tr ("<p>Failed to download the VirtualBox Guest Additions CD "
                 "image from <nobr><a href=\"%1\">%2</a>.</nobr></p><p>%3</p>")
                 .arg (aURL).arg (aURL).arg (aReason));
}

bool VBoxProblemReporter::confirmDownloadAdditions (const QString &aURL,
                                                    ulong aSize)
{
    return messageOkCancel (&vboxGlobal().consoleWnd(), Question,
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
    return messageOkCancel (&vboxGlobal().consoleWnd(), Question,
        tr ("<p>The VirtualBox Guest Additions CD image has been "
            "successfully downloaded from "
            "<nobr><a href=\"%1\">%2</a></nobr> "
            "and saved locally as <nobr><b>%3</b>.</nobr></p>"
            "<p>Do you want to register this CD image and mount it "
            "on the virtual CD/DVD drive?</p>")
            .arg (aURL).arg (aURL).arg (aSrc),
        0, /* aAutoConfirmId */
        tr ("Mount", "additions"));
}

void VBoxProblemReporter::warnAboutTooOldAdditions (QWidget *aParent,
                                                    const QString &aInstalledVer,
                                                    const QString &aExpectedVer)
{
    message (aParent, VBoxProblemReporter::Error,
        tr ("<p>VirtualBox Guest Additions installed in the Guest OS are too "
            "old: the installed version is %1, the expected version is %2. "
            "Some features that require Guest Additions (mouse integration, "
            "guest display auto-resize) will most likely stop "
            "working properly.</p>"
            "<p>Please update Guest Additions to the current version by choosing "
            "<b>Install Guest Additions</b> from the <b>Devices</b> "
            "menu.</p>")
             .arg (aInstalledVer).arg (aExpectedVer),
        "warnAboutTooOldAdditions");
}

void VBoxProblemReporter::warnAboutOldAdditions (QWidget *aParent,
                                                 const QString &aInstalledVer,
                                                 const QString &aExpectedVer)
{
    message (aParent, VBoxProblemReporter::Warning,
        tr ("<p>VirtualBox Guest Additions installed in the Guest OS are "
            "outdated: the installed version is %1, the expected version is %2. "
            "Some features that require Guest Additions (mouse integration, "
            "guest display auto-resize) may not work as expected.</p>"
            "<p>It is recommended to update Guest Additions to the current version "
            " by choosing <b>Install Guest Additions</b> from the <b>Devices</b> "
            "menu.</p>")
             .arg (aInstalledVer).arg (aExpectedVer),
        "warnAboutOldAdditions");
}

void VBoxProblemReporter::warnAboutNewAdditions (QWidget *aParent,
                                                 const QString &aInstalledVer,
                                                 const QString &aExpectedVer)
{
    message (aParent, VBoxProblemReporter::Error,
        tr ("<p>VirtualBox Guest Additions installed in the Guest OS are "
            "too recent for this version of VirtualBox: the installed version "
            "is %1, the expected version is %2.</p>"
            "<p>Using a newer version of Additions with an older version of "
            "VirtualBox is not supported. Please install the current version "
            "of Guest Additions by choosing <b>Install Guest Additions</b> "
            "from the <b>Devices</b> menu.</p>")
             .arg (aInstalledVer).arg (aExpectedVer),
        "warnAboutNewAdditions");
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
    aResult == "OK" ?
        message (aParent, Info,
                 tr ("<p>Congratulations! You have been successfully registered "
                     "as a user of VirtualBox.</p>"
                     "<p>Thank you for finding time to fill out the "
                     "registration form!</p>")) :
        message (aParent, Error,
                 tr ("<p>Failed to register the VirtualBox product</p><p>%1</p>")
                 .arg (aResult));
}

void VBoxProblemReporter::showUpdateSuccess (QWidget *aParent,
                                             const QString &aVersion,
                                             const QString &aLink)
{
    message (aParent, Info,
             tr ("<p>A new version of VirtualBox has been released! Version <b>%1</b> is available at <a href=\"http://www.virtualbox.org/\">virtualbox.org</a>.</p>"
                 "<p>You can download this version from this direct link:</p>"
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
             tr ("You have already installed the latest VirtualBox "
                 "version. Please repeat the version check later."));
}

/**
 *  @return @c true if the user has confirmed input capture (this is always
 *  the case if the dialog was autoconfirmed). @a aAutoConfirmed, when not
 *  NULL, will receive @c true if the dialog wasn't actually shown.
 */
bool VBoxProblemReporter::confirmInputCapture (bool *aAutoConfirmed /* = NULL */)
{
    int rc = message (&vboxGlobal().consoleWnd(), Info,
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
            .arg (QIHotKeyEdit::keyName (vboxGlobal().settings().hostKey())),
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
    message (&vboxGlobal().consoleWnd(), Info,
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
            .arg (QIHotKeyEdit::keyName (vboxGlobal().settings().hostKey())),
        "remindAboutAutoCapture");
}

void VBoxProblemReporter::remindAboutMouseIntegration (bool aSupportsAbsolute)
{
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
        message (&vboxGlobal().consoleWnd(), Info,
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
        message (&vboxGlobal().consoleWnd(), Info,
            tr ("<p>The Virtual Machine reports that the guest OS does not "
                "support <b>mouse pointer integration</b> in the current video "
                "mode. You need to capture the mouse (by clicking over the VM "
                "display or pressing the host key) in order to use the "
                "mouse inside the guest OS.</p>"),
            kNames [0] /* aAutoConfirmId */);
    }
}

/**
 * @return @c false if the dialog wasn't actually shown (i.e. it was
 * autoconfirmed).
 */
bool VBoxProblemReporter::remindAboutPausedVMInput()
{
    int rc = message (
        &vboxGlobal().consoleWnd(),
        Info,
        tr (
            "<p>The Virtual Machine is currently in the <b>Paused</b> state and "
            "therefore does not accept any keyboard or mouse input. If you want to "
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
 * Shows a list of auto-converted files and asks the user to either Save, Backup
 * or Cancel to leave them as is and exit VirtualBox.
 *
 * @param aFormatVersion    Recent settings file format version.
 * @param aFileList         List of auto-converted files (may use Qt HTML).
 * @param aAfterRefresh     @true when called after the VM refresh.
 *
 * @return QIMessageBox::Yes (Save), QIMessageBox::No (Backup),
 *         QIMessageBox::Cancel (Exit)
 */
int VBoxProblemReporter::warnAboutAutoConvertedSettings (const QString &aFormatVersion,
                                                         const QString &aFileList,
                                                         bool aAfterRefresh)
{
    if (!aAfterRefresh)
    {
        int rc = message (mainWindowShown(), Info,
            tr ("<p>Your existing VirtualBox settings files were automatically "
                "converted from the old format to a new format necessary for the "
                "new version of VirtualBox.</p>"
                "<p>Press <b>OK</b> to start VirtualBox now or press <b>More</b> if "
                "you want to get more information about what files were converted "
                "and access additional actions.</p>"
                "<p>Press <b>Exit</b> to terminate the VirtualBox "
                "application without saving the results of the conversion to "
                "disk.</p>"),
            NULL /* aAutoConfirmId */,
            QIMessageBox::Ok | QIMessageBox::Default,
            QIMessageBox::No,
            QIMessageBox::Cancel | QIMessageBox::Escape,
            0,
            tr ("&More", "warnAboutAutoConvertedSettings message box"),
            tr ("E&xit", "warnAboutAutoConvertedSettings message box"));

        /* in the simplest case we backup */
        if (rc == QIMessageBox::Ok)
            return QIMessageBox::No;

        if (rc == QIMessageBox::Cancel)
            return QIMessageBox::Cancel;
    }

    return message (mainWindowShown(), Info,
        tr ("<p>The following VirtualBox settings files have been "
            "automatically converted to the new settings file format "
            "version <b>%1</b>.</p>"
            "<p>However, the results of the conversion were not saved back "
            "to disk yet. Please press:</p>"
            "<ul>"
            "<li><b>Backup</b> to create backup copies of the settings files in "
            "the old format before saving them in the new format;</li>"
            "<li><b>Overwrite</b> to save all auto-converted files without "
            "creating backup copies (it will not be possible to use these "
            "settings files with an older version of VirtualBox "
            "afterwards);</li>"
            "%2"
            "</ul>"
            "<p>It is recommended to always select <b>Backup</b> because in "
            "this case it will be possible to go back to the previous "
            "version of VirtualBox (if necessary) without losing your current "
            "settings. See the VirtualBox Manual for more information about "
            "downgrading.</p>")
            .arg (aFormatVersion)
            .arg (aAfterRefresh ? QString::null :
                  tr ("<li><b>Exit</b> to terminate VirtualBox without saving "
                      "the results of the conversion to disk.</li>")),
        QString ("<!--EOM-->%1").arg (aFileList),
        NULL /* aAutoConfirmId */,
        QIMessageBox::Yes,
        aAfterRefresh ? (QIMessageBox::No | QIMessageBox::Default | QIMessageBox::Escape) :
                        (QIMessageBox::No | QIMessageBox::Default),
        aAfterRefresh ? 0 : (QIMessageBox::Cancel | QIMessageBox::Escape),
        tr ("O&verwrite", "warnAboutAutoConvertedSettings message box"),
        tr ("&Backup", "warnAboutAutoConvertedSettings message box"),
        aAfterRefresh ? QString::null :
            tr ("E&xit", "warnAboutAutoConvertedSettings message box"));
}

/**
 *  @param aHotKey Fullscreen hot key as defined in the menu.
 *
 *  @return @c true if the user has chosen to go fullscreen (this is always
 *  the case if the dialog was autoconfirmed).
 */
bool VBoxProblemReporter::confirmGoingFullscreen (const QString &aHotKey)
{
    return messageOkCancel (&vboxGlobal().consoleWnd(), Info,
        tr ("<p>The virtual machine window will be now switched to "
            "<b>fullscreen</b> mode. "
            "You can go back to windowed mode at any time by pressing "
            "<b>%1</b>. Note that the <i>Host</i> key is currently "
            "defined as <b>%2</b>.</p>"
            "<p>Note that the main menu bar is hidden in fullscreen mode. You "
            "can access it by pressing <b>Host+Home</b>.</p>")
            .arg (aHotKey)
            .arg (QIHotKeyEdit::keyName (vboxGlobal().settings().hostKey())),
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
    return messageOkCancel (&vboxGlobal().consoleWnd(), Info,
        tr ("<p>The virtual machine window will be now switched to "
            "<b>Seamless</b> mode. "
            "You can go back to windowed mode at any time by pressing "
            "<b>%1</b>. Note that the <i>Host</i> key is currently "
            "defined as <b>%2</b>.</p>"
            "<p>Note that the main menu bar is hidden in seamless mode. You "
            "can access it by pressing <b>Host+Home</b>.</p>")
            .arg (aHotKey)
            .arg (QIHotKeyEdit::keyName (vboxGlobal().settings().hostKey())),
        "confirmGoingSeamless",
        tr ("Switch", "seamless"));
}

void VBoxProblemReporter::remindAboutWrongColorDepth (ulong aRealBPP,
                                                      ulong aWantedBPP)
{
    const char *kName = "remindAboutWrongColorDepth";

    /* Close the previous (outdated) window if any. We use kName as
     * aAutoConfirmId which is also used as the widget name by default. */
    {
        QWidget *outdated = VBoxGlobal::findWidget (NULL, kName, "QIMessageBox");
        if (outdated)
            outdated->close();
    }

    int rc = message (&vboxGlobal().consoleWnd(), Info,
        tr ("<p>The virtual machine window is optimized to work in "
            "<b>%1&nbsp;bit</b> color mode but the color quality of the "
            "virtual display is currently set to <b>%2&nbsp;bit</b>.</p>"
            "<p>Please open the display properties dialog of the guest OS and "
            "select a <b>%3&nbsp;bit</b> color mode, if it is available, for "
            "best possible performance of the virtual video subsystem.</p>"
            "<p><b>Note</b>. Some operating systems, like OS/2, may actually "
            "work in 32&nbsp;bit mode but report it as 24&nbsp;bit "
            "(16 million colors). You may try to select a different color "
            "quality to see if this message disappears or you can simply "
            "disable the message now if you are sure the required color "
            "quality (%4&nbsp;bit) is not available in the given guest OS.</p>")
            .arg (aWantedBPP).arg (aRealBPP).arg (aWantedBPP).arg (aWantedBPP),
        kName);
    NOREF(rc);
}

/**
 *  Returns @c true if the user has selected to power off the machine.
 */
bool VBoxProblemReporter::remindAboutGuruMeditation (const CConsole &aConsole,
                                                     const QString &aLogFolder)
{
    Q_UNUSED (aConsole);

    int rc = message (&vboxGlobal().consoleWnd(), GuruMeditation,
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
    return messageOkCancel (aParent, Question,
        tr ("<p>Do you really want to reset the virtual machine?</p>"
            "<p>When the machine is reset, unsaved data of all applications "
            "running inside it will be lost.</p>"),
        "confirmVMReset" /* aAutoConfirmId */,
        tr ("Reset", "machine"));
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
            "Run Wizard.</p><p>Do you want to continue?</p>"),
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

void VBoxProblemReporter::cannotExportAppliance (CAppliance *aAppliance, QWidget *aParent /* = NULL */) const
{
    if (aAppliance->isNull())
    {
        message (aParent ? aParent : mainWindowShown(),
                 Error,
                 tr ("Failed to create an appliance."));
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

    QString formatted;

    if (!errorMsg.isEmpty())
        formatted += QString ("<p>%1.</p><!--EOM-->")
                              .arg (vboxGlobal().emphasize (errorMsg));

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
        rc = message (&vboxGlobal().consoleWnd(), type,
            tr ("<p>A fatal error has occurred during virtual machine execution! "
                "The virtual machine will be powered off. It is suggested to "
                "use the clipboard to copy the following error message for "
                "further examination:</p>"),
            formatted, autoConfimId.data());

        /* always power down after a fatal error */
        console.PowerDown();
    }
    else if (type == Error)
    {
        rc = message (&vboxGlobal().consoleWnd(), type,
            tr ("<p>An error has occurred during virtual machine execution! "
                "The error details are shown below. You can try to correct "
                "the described error and resume the virtual machine "
                "execution.</p>"),
            formatted, autoConfimId.data());
    }
    else
    {
        rc = message (&vboxGlobal().consoleWnd(), type,
            tr ("<p>The virtual machine execution may run into an error "
                "condition as described below. "
                "You may ignore this message, but it is suggested to perform "
                "an appropriate action to make sure the described error will "
                "not happen.</p>"),
            formatted, autoConfimId.data());
    }

    NOREF (rc);
}

/* static */
QString VBoxProblemReporter::toAccusative (VBoxDefs::MediaType aType)
{
    QString type =
        aType == VBoxDefs::MediaType_HardDisk ?
            tr ("hard disk", "failed to close ...") :
        aType == VBoxDefs::MediaType_DVD ?
            tr ("CD/DVD image", "failed to close ...") :
        aType == VBoxDefs::MediaType_Floppy ?
            tr ("floppy image", "failed to close ...") :
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

/* static */
QString VBoxProblemReporter::doFormatErrorInfo (const COMErrorInfo &aInfo,
                                                HRESULT aWrapperRC /* = S_OK */)
{
    /* Compose complex details string with internal <!--EOM--> delimiter to
     * make it possible to split string into info & details parts which will
     * be used separately in QIMessageBox */
    QString formatted;

    if (!aInfo.text().isEmpty())
        formatted += QString ("<p>%1.</p><!--EOM-->").arg (vboxGlobal().emphasize (aInfo.text()));

    formatted += "<table bgcolor=#EEEEEE border=0 cellspacing=0 "
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

// Public slots
/////////////////////////////////////////////////////////////////////////////

void VBoxProblemReporter::showHelpWebDialog()
{
    vboxGlobal().openURL ("http://www.virtualbox.org");
}

void VBoxProblemReporter::showHelpAboutDialog()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString fullVersion (QString ("%1 r%2").arg (vbox.GetVersion())
                                          .arg (vbox.GetRevision()));
    AssertWrapperOk (vbox);

    // this (QWidget*) cast is necessary to work around a gcc-3.2 bug */
    VBoxAboutDlg ((QWidget*)mainWindowShown(), fullVersion).exec();
}

void VBoxProblemReporter::showHelpHelpDialog()
{
#ifndef VBOX_OSE
    QString manual = vboxGlobal().helpFile();
# if defined (Q_WS_WIN32)
    HtmlHelp (GetDesktopWindow(), manual.utf16(),
              HH_DISPLAY_TOPIC, NULL);
# elif defined (Q_WS_X11)
    char szViewerPath[RTPATH_MAX];
    int rc;

    rc = RTPathAppPrivateArch (szViewerPath, sizeof (szViewerPath));
    AssertRC (rc);

    QProcess::startDetached (QString(szViewerPath) + "/kchmviewer",
                             QStringList (manual));
# elif defined (Q_WS_MAC)
    vboxGlobal().openURL ("file://" + manual);
# endif
#endif /* VBOX_OSE */
}

void VBoxProblemReporter::resetSuppressedMessages()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    vbox.SetExtraData (VBoxDefs::GUI_SuppressMessages, QString::null);
}

