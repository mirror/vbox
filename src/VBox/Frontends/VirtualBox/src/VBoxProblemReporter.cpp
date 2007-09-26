/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxProblemReporter class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxProblemReporter.h"

#include "VBoxGlobal.h"
#include "VBoxSelectorWnd.h"
#include "VBoxConsoleWnd.h"

#include "VBoxAboutDlg.h"

#include <qmessagebox.h>
#include <qprogressdialog.h>
#include <qcursor.h>
#include <qprocess.h>
#include <qeventloop.h>
#include <qregexp.h>
#ifdef Q_WS_MAC
# include <qpushbutton.h>
#endif

#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/path.h>

#if defined (Q_WS_WIN32)
#include <Htmlhelp.h>
#endif


/**
 *  A QProgressDialog enhancement that allows to:
 *
 *  1) prevent closing the dialog when it has no cancel button;
 *  2) effectively track the IProgress object completion
 *     (w/o using IProgress::waitForCompletion() and w/o blocking the UI thread
 *     in any other way for too long).
 *
 *  @note The CProgress instance is passed as a non-const reference to the
 *        constructor (to memorize COM errors if they happen), and therefore
 *        must not be destroyed before the created VBoxProgressDialog instance
 *        is destroyed.
 */
class VBoxProgressDialog : public QProgressDialog
{
public:

    VBoxProgressDialog (CProgress &aProgress, const QString &aTitle,
                        int aMinDuration = 2000, QWidget *aCreator = 0,
                        const char *aName = 0)
        : QProgressDialog (aCreator, aName, true,
                           WStyle_Customize | WStyle_DialogBorder | WStyle_Title)
        , mProgress (aProgress)
        , mCalcelEnabled (true)
        , mOpCount (mProgress.GetOperationCount())
        , mCurOp (mProgress.GetOperation() + 1)
        , mLoopLevel (-1)
        , mEnded (false)
    {
        if (mOpCount > 1)
            setLabelText (QString (sOpDescTpl)
                          .arg (mProgress.GetOperationDescription())
                          .arg (mCurOp).arg (mOpCount));
        else
            setLabelText (QString ("%1...")
                          .arg (mProgress.GetOperationDescription()));
        setCancelButtonText (QString::null);
        setTotalSteps (100);
        setCaption (QString ("%1: %2")
                    .arg (aTitle, mProgress.GetDescription()));
        setMinimumDuration (aMinDuration);
        setCancelEnabled (false);
        setProgress (0);
    }

    int run (int aRefreshInterval);

    bool cancelEnabled() const { return mCalcelEnabled; }
    void setCancelEnabled (bool aEnabled) { mCalcelEnabled = aEnabled; }

protected:

    virtual void timerEvent (QTimerEvent *e);

    virtual void reject() { if (mCalcelEnabled) QProgressDialog::reject(); };

    virtual void closeEvent (QCloseEvent *e)
    {
        if (mCalcelEnabled)
            QProgressDialog::closeEvent (e);
        else
            e->ignore();
    }

private:

    CProgress &mProgress;
    bool mCalcelEnabled;
    const ulong mOpCount;
    ulong mCurOp;
    int mLoopLevel;
    bool mEnded;

    static const char *sOpDescTpl;
};

//static
const char *VBoxProgressDialog::sOpDescTpl = "%1... (%2/%3)";

int VBoxProgressDialog::run (int aRefreshInterval)
{
    if (mProgress.isOk())
    {
        /* start a refresh timer */
        startTimer (aRefreshInterval);
        mLoopLevel = qApp->eventLoop()->loopLevel();
        /* enter the modal loop */
        qApp->eventLoop()->enterLoop();
        killTimers();
        mLoopLevel = -1;
        mEnded = false;
        return result();
    }
    return Rejected;
}

//virtual
void VBoxProgressDialog::timerEvent (QTimerEvent *e)
{
    bool justEnded = false;

    if (!mEnded && (!mProgress.isOk() || mProgress.GetCompleted()))
    {
        /* dismiss the dialog -- the progress is no more valid */
        killTimer (e->timerId());
        if (mProgress.isOk())
        {
            setProgress (100);
            setResult (Accepted);
        }
        else
            setResult (Rejected);
        mEnded = justEnded = true;
    }

    if (mEnded)
    {
        if (mLoopLevel != -1)
        {
            /* we've entered the loop in run() */
            if (mLoopLevel + 1 == qApp->eventLoop()->loopLevel())
            {
                /* it's our loop, exit it */
                qApp->eventLoop()->exitLoop();
            }
            else
            {
                Assert (mLoopLevel + 1 < qApp->eventLoop()->loopLevel());
                /* restart the timer to watch for the loop level to drop */
                if (justEnded)
                    startTimer (50);
            }
        }
        else
            Assert (justEnded);
        return;
    }

    /* update the progress dialog */
    ulong newOp = mProgress.GetOperation() + 1;
    if (newOp != mCurOp)
    {
        mCurOp = newOp;
        setLabelText (QString (sOpDescTpl)
            .arg (mProgress.GetOperationDescription())
            .arg (mCurOp).arg (mOpCount));
    }
    setProgress (mProgress.GetPercent());
}


/** @class VBoxProblemReporter
 *
 *  The VBoxProblemReporter class is a central place to handle all
 *  problem/error situations that happen during application
 *  runtime and require the user's attention. Its role is to
 *  describe the problem and/or the cause of the error to the user and give
 *  him the opportunity to select an action (when appropriate).
 *
 *  Every problem sutiation has its own (correspondingly named) method in
 *  this class that takes a list of arguments necessary to describe the
 *  situation and to provide the appropriate actions. The method then
 *  returns the choice to the caller.
 */

/**
 *  Returns a reference to the global VirtualBox problem reporter instance.
 */
VBoxProblemReporter &VBoxProblemReporter::instance()
{
    static VBoxProblemReporter vboxProblem_instance;
    return vboxProblem_instance;
}

bool VBoxProblemReporter::isValid()
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
 *  If autoConfirmId is not null, then the message box will contain a
 *  checkbox "Don't show this message again" below the message text and its
 *  state will be saved in the global config. When the checkbox is in the
 *  checked state, this method doesn't actually show the message box but
 *  returns immediately. The return value in this case is the same as if the
 *  user has pressed the Enter key or the default button, but with
 *  AutoConfirmed bit set (AutoConfirmed alone is returned when no default
 *  button is defined in button arguments).
 *
 *  @param  parent
 *      parent widget or 0 to use the desktop as the parent. Also,
 *      #mainWindowShown can be used to determine the currently shown VBox
 *      main window (Selector or Console)
 *  @param  type
 *      one of values of the Type enum, that defines the message box
 *      title and icon
 *  @param  msg
 *      message text to display (can contain sinmple Qt-html tags)
 *  @param  details
 *      detailed message description displayed under the main message text using
 *      QTextEdit (that provides rich text support and scrollbars when necessary).
 *      If equals to QString::null, no details text box is shown
 *  @param  autoConfirmId
 *      ID used to save the auto confirmation state across calls. If null,
 *      the auto confirmation feature is turned off (and no checkbox is shown)
 *  @param  b1
 *      first button code or 0, see QIMessageBox
 *  @param  b2
 *      second button code or 0, see QIMessageBox
 *  @param  b3
 *      third button code or 0, see QIMessageBox
 *  @param  name
 *      name of the underlying QIMessageBox object. If NULL, a value of
 *      autoConfirmId is used.
 *
 *  @return
 *      code of the button pressed by the user
 */
int VBoxProblemReporter::message (QWidget *parent, Type type, const QString &msg,
                                  const QString &details,
                                  const char *autoConfirmId,
                                  int b1, int b2, int b3,
                                  const char *name)
{
    if (b1 == 0 && b2 == 0 && b3 == 0)
        b1 = QIMessageBox::Ok | QIMessageBox::Default;

    CVirtualBox vbox;
    QStringList msgs;

    if (autoConfirmId)
    {
        vbox = vboxGlobal().virtualBox();
        msgs = QStringList::split (',', vbox.GetExtraData (VBoxDefs::GUI_SuppressMessages));
        if (msgs.findIndex (autoConfirmId) >= 0) {
            int rc = AutoConfirmed;
            if (b1 & QIMessageBox::Default) rc |= (b1 & QIMessageBox::ButtonMask);
            if (b2 & QIMessageBox::Default) rc |= (b2 & QIMessageBox::ButtonMask);
            if (b3 & QIMessageBox::Default) rc |= (b3 & QIMessageBox::ButtonMask);
            return rc;
        }
    }

    QString title;
    QIMessageBox::Icon icon;

    switch (type)
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

    if (!name)
        name = autoConfirmId;
    QIMessageBox *box = new QIMessageBox (title, msg, icon, b1, b2, b3, parent, name);

    if (details)
    {
        box->setDetailsText (details);
        box->setDetailsShown (true);
    }

    if (autoConfirmId)
    {
        box->setFlagText (tr ("Do not show this message again", "msg box flag"));
        box->setFlagChecked (false);
    }

    int rc = box->exec();

    if (autoConfirmId)
    {
        if (box->isFlagChecked())
        {
            msgs << autoConfirmId;
            vbox.SetExtraData (VBoxDefs::GUI_SuppressMessages, msgs.join (","));
        }
    }

    delete box;

    return rc;
}

/** @fn message (QWidget *, Type, const QString &, const char *, int, int, int, const char *)
 *
 *  A shortcut to #message() that doesn't require to specify the details
 *  text (QString::null is assumed).
 */

/**
 *  A shortcut to #message() that shows 'Yes' and 'No' buttons ('Yes' is the
 *  default) and returns true when the user selects Yes.
 */
bool VBoxProblemReporter::messageYesNo (
    QWidget *parent, Type type, const QString &msg,
    const QString &details,
    const char *autoConfirmId,
    const char *name
)
{
    return (message (parent, type, msg, details, autoConfirmId,
                     QIMessageBox::Yes | QIMessageBox::Default,
                     QIMessageBox::No | QIMessageBox::Escape,
                     0, name) & QIMessageBox::ButtonMask) == QIMessageBox::Yes;
}

/** @fn messageYesNo (QWidget *, Type, const QString &, const char *, const char *)
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
    QApplication::setOverrideCursor (QCursor (WaitCursor));

    VBoxProgressDialog progressDlg (aProgress, aTitle, aMinDuration,
                                    aParent, "progressDlg");

    /* run the dialog with the 100 ms refresh interval */
    progressDlg.run (100);

    QApplication::restoreOverrideCursor();

    return true;
}

/**
 *  Returns what main window (selector or console) is now shown, or
 *  zero if none of them. The selector window takes precedence.
 */
QWidget *VBoxProblemReporter::mainWindowShown()
{
    /* It may happen that this method is called during VBoxGlobal
     * initialization or even after it failed (for example, to show some
     * error message). Return no main window in this case. */
    if (!vboxGlobal().isValid())
        return 0;

#if defined (VBOX_GUI_SEPARATE_VM_PROCESS)
    if (vboxGlobal().isVMConsoleProcess())
    {
        if (vboxGlobal().consoleWnd().isShown())
            return &vboxGlobal().consoleWnd();
    }
    else
    {
        if (vboxGlobal().selectorWnd().isShown())
            return &vboxGlobal().selectorWnd();
    }
#else
    if (vboxGlobal().consoleWnd().isShown())
        return &vboxGlobal().consoleWnd();
    if (vboxGlobal().selectorWnd().isShown())
        return &vboxGlobal().selectorWnd();
#endif
    return 0;
}

// Problem handlers
/////////////////////////////////////////////////////////////////////////////

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

void VBoxProblemReporter::cannotFindLanguage (const QString &aLangID,
                                              const QString &aNlsPath)
{
    message
        (0, VBoxProblemReporter::Error,
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
    message
        (0, VBoxProblemReporter::Error,
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
    message (
        0,
        Critical,
        tr ("<p>Failed to initialize COM or to find the VirtualBox COM server. "
            "Most likely, the VirtualBox server is not running "
            "or failed to start.</p>"
            "<p>The application will now terminate.</p>"),
        formatErrorInfo (COMErrorInfo(), rc)
    );
}

void VBoxProblemReporter::cannotCreateVirtualBox (const CVirtualBox &vbox)
{
    message (
        0,
        Critical,
        tr ("<p>Failed to create the VirtualBox COM object.</p>"
            "<p>The application will now terminate.</p>"),
        formatErrorInfo (vbox)
    );
}

void VBoxProblemReporter::cannotLoadGlobalConfig (const CVirtualBox &vbox,
                                                  const QString &error)
{
    message (mainWindowShown(), Critical,
        tr ("<p>Failed to load the global GUI configuration.</p>"
            "<p>The application will now terminate.</p>"),
        !vbox.isOk() ? formatErrorInfo (vbox)
                     : QString ("<p>%1</p>").arg (VBoxGlobal::highlight (error)));
}

void VBoxProblemReporter::cannotSaveGlobalConfig (const CVirtualBox &vbox)
{
    message (
        mainWindowShown(),
        Critical,
        tr ("<p>Failed to save the global GUI configuration.<p>"),
        formatErrorInfo (vbox)
    );
}

void VBoxProblemReporter::cannotSetSystemProperties (const CSystemProperties &props)
{
    message (
        mainWindowShown(),
        Critical,
        tr ("Failed to set global VirtualBox properties."),
        formatErrorInfo (props)
    );
}

void VBoxProblemReporter::cannotAccessUSB (const COMBase &obj)
{
    /* if there is no error info available, it should mean that
     * IMachine::GetUSBController(), IHost::GetUSBDevices() etc. just returned
     * E_NOTIMPL, as for the OSE version. Don't show the error message in this
     * case since it's normal. */
    COMResult res (obj);
    if (res.rc() == E_NOTIMPL && !res.errorInfo().isBasicAvailable())
        return;

    message (mainWindowShown(), Error,
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
             tr ("Failed to save the settings of the virtual machine <b>%1</b>.")
                 .arg (machine.GetName()),
             formatErrorInfo (res));
}

/**
 *  @param  strict  if |true| then show the message even if there is no basic
 *                  error info available
 */
void VBoxProblemReporter::cannotLoadMachineSettings (const CMachine &machine,
                                                     bool strict /* = true */,
                                                     QWidget *parent /* = 0 */)
{
    COMResult res (machine);
    if (!strict && !res.errorInfo().isBasicAvailable())
        return;

    message (parent ? parent : mainWindowShown(), Error,
             tr ("Failed to load the settings of the virtual machine <b>%1</b>.")
                 .arg (machine.GetName()),
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
    CConsole console = CProgress (progress).GetInitiator();
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
    CConsole console = CProgress (progress).GetInitiator();
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
    CConsole console = CProgress (progress).GetInitiator();
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

void VBoxProblemReporter::cannotDiscardSnapshot (const CConsole &console,
                                                 const CSnapshot &snapshot)
{
    message (
        mainWindowShown(),
        Error,
        tr ("Failed to discard the snapshot <b>%1</b> of the virtual "
            "machine <b>%2</b>.")
            .arg (snapshot.GetName())
            .arg (CConsole (console).GetMachine().GetName()),
        formatErrorInfo (console));
}

void VBoxProblemReporter::cannotDiscardSnapshot (const CProgress &progress,
                                                 const CSnapshot &snapshot)
{
    CConsole console = CProgress (progress).GetInitiator();

    message (
        mainWindowShown(),
        Error,
        tr ("Failed to discard the snapshot <b>%1</b> of the virtual "
            "machine <b>%2</b>.")
            .arg (snapshot.GetName())
            .arg (console.GetMachine().GetName()),
        formatErrorInfo (progress.GetErrorInfo()));
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
    CConsole console = CProgress (progress).GetInitiator();

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
    CConsole console = CProgress (progress).GetInitiator();

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

void VBoxProblemReporter::cannotEnterSeamlessMode (ULONG aWidth,
                                                   ULONG aHeight,
                                                   ULONG aBpp)
{
    message (&vboxGlobal().consoleWnd(), Error,
             tr ("<p>Could not enter seamless mode due to insufficient guest "
                 "video memory.</p>"
                 "<p>You should configure the VM to have at least <b>%1</b> "
                 "of video memory.</p>")
             .arg (VBoxGlobal::formatSize ((ULONG64) (aWidth * aHeight *
                                                      aBpp / 8 + _1M - 1))));
}

bool VBoxProblemReporter::confirmMachineDeletion (const CMachine &machine)
{
    QString msg;
    QString name;

    if (machine.GetAccessible())
    {
        name = machine.GetName();
        msg = tr ("<p>Are you sure you want to permanently delete "
                  "the virtual machine <b>%1</b>?</p>"
                  "<p>This operation cannot be undone.</p>")
                  .arg (name);
    }
    else
    {
        /* this should be in sync with VBoxVMListBoxItem::recache() */
        QFileInfo fi (machine.GetSettingsFilePath());
        name = fi.extension().lower() == "xml" ?
               fi.baseName (true) : fi.fileName();
        msg = tr ("<p>Are you sure you want to unregister the inaccessible "
                  "virtual machine <b>%1</b>?</p>"
                  "<p>You will no longer be able to register it back from "
                  "GUI.</p>")
                  .arg (name);
    }

    return messageYesNo (&vboxGlobal().selectorWnd(), Question, msg);
}

bool VBoxProblemReporter::confirmDiscardSavedState (const CMachine &machine)
{
    return messageYesNo (
        &vboxGlobal().selectorWnd(),
        Question,
        tr ("<p>Are you sure you want to discard the saved state of "
            "the virtual machine <b>%1</b>?</p>"
            "<p>This operation is equivalent to resetting or powering off "
            "the machine without doing a proper shutdown by means of the "
            "guest OS.</p>")
            .arg (machine.GetName())
    );
}

bool VBoxProblemReporter::confirmReleaseImage (QWidget *parent,
                                               const QString &usage)
{
    return messageYesNo (
        parent,
        Question,
        tr ("<p>Releasing this media image will detach it from the "
            "following virtual machine(s): <b>%1</b>.</p>"
            "<p>Continue?</p>")
            .arg (usage),
        "confirmReleaseImage"
    );
}

void VBoxProblemReporter::sayCannotOverwriteHardDiskImage (QWidget *parent,
                                                           const QString &src)
{
    message (
        parent,
        Info,
        tr (
            "<p>The image file <b>%1</b> already exists. "
            "You cannot create a new virtual hard disk that uses this file, "
            "because it can be already used by another virtual hard disk.</p>"
            "<p>Please specify a different image file name.</p>"
        )
            .arg (src)
    );
}

int VBoxProblemReporter::confirmHardDiskImageDeletion (QWidget *parent,
                                                        const QString &src)
{
    return message (parent, Question,
        tr ("<p>Do you want to delete this hard disk's image file "
            "<nobr><b>%1</b>?</nobr></p>"
            "<p>If you select <b>No</b> then the virtual hard disk will be "
            "unregistered and removed from the collection, but the image file "
            "will be left on your physical disk.</p>"
            "<p>If you select <b>Yes</b> then the image file will be permanently "
            "deleted after unregistering the hard disk. This operation "
            "cannot be undone.</p>")
            .arg (src),
        0, /* autoConfirmId */
        QIMessageBox::Yes,
        QIMessageBox::No | QIMessageBox::Default,
        QIMessageBox::Cancel | QIMessageBox::Escape);
}

void VBoxProblemReporter::cannotDeleteHardDiskImage (QWidget *parent,
                                                     const CVirtualDiskImage &vdi)
{
    /* below, we use CHardDisk (hd) to preserve current error info
     * for formatErrorInfo() */

    message (parent, Error,
        tr ("Failed to delete the virtual hard disk image <b>%1</b>.")
            .arg (CVirtualDiskImage (vdi).GetFilePath()),
        formatErrorInfo (vdi));
}

int VBoxProblemReporter::confirmHardDiskUnregister (QWidget *parent,
                                                    const QString &src)
{
    return message (parent, Question,
        tr ("<p>Do you want to remove (unregister) the virtual hard disk "
            "<nobr><b>%1</b>?</nobr></p>")
            .arg (src),
        0, /* autoConfirmId */
        QIMessageBox::Ok | QIMessageBox::Default,
        QIMessageBox::Cancel | QIMessageBox::Escape);
}


void VBoxProblemReporter::cannotCreateHardDiskImage (
    QWidget *parent, const CVirtualBox &vbox, const QString &src,
    const CVirtualDiskImage &vdi, const CProgress &progress
) {
    message (parent,Error,
        tr ("Failed to create the virtual hard disk image <nobr><b>%1</b>.</nobr>")
            .arg (src),
        !vbox.isOk() ? formatErrorInfo (vbox) :
        !vdi.isOk() ? formatErrorInfo (vdi) :
        formatErrorInfo (progress.GetErrorInfo()));
}

void VBoxProblemReporter::cannotAttachHardDisk (
    QWidget *parent, const CMachine &m, const QUuid &id,
    CEnums::DiskControllerType ctl, LONG dev)
{
    message (parent, Error,
        tr ("Failed to attach a hard disk image with UUID %1 "
            "to the device slot %2 of the controller %3 of the machine <b>%4</b>.")
            .arg (id).arg (vboxGlobal().toString (ctl, dev))
            .arg (vboxGlobal().toString (ctl))
            .arg (CMachine (m).GetName()),
        formatErrorInfo (m));
}

void VBoxProblemReporter::cannotDetachHardDisk (
    QWidget *parent, const CMachine &m,
    CEnums::DiskControllerType ctl, LONG dev)
{
    message (parent, Error,
        tr ("Failed to detach a hard disk image "
            "from the device slot %1 of the controller %2 of the machine <b>%3</b>.")
            .arg (vboxGlobal().toString (ctl, dev))
            .arg (vboxGlobal().toString (ctl))
            .arg (CMachine (m).GetName()),
        formatErrorInfo (m));
}

void VBoxProblemReporter::cannotRegisterMedia (
    QWidget *parent, const CVirtualBox &vbox,
    VBoxDefs::DiskType type, const QString &src)
{
    QString media = type == VBoxDefs::HD ? tr ("hard disk") :
                    type == VBoxDefs::CD ? tr ("CD/DVD image") :
                    type == VBoxDefs::FD ? tr ("floppy image") :
                    QString::null;

    Assert (!media.isNull());

    message (parent, Error,
        tr ("Failed to register the %1 <nobr><b>%2</b></nobr>.")
            .arg (media)
            .arg (src),
        formatErrorInfo (vbox));
}

void VBoxProblemReporter::cannotUnregisterMedia (
    QWidget *parent, const CVirtualBox &vbox,
    VBoxDefs::DiskType type, const QString &src)
{
    QString media = type == VBoxDefs::HD ? tr ("hard disk") :
                    type == VBoxDefs::CD ? tr ("CD/DVD image") :
                    type == VBoxDefs::FD ? tr ("floppy image") :
                    QString::null;

    Assert (!media.isNull());

    message (parent, Error,
        tr ("Failed to unregister the %1 <nobr><b>%2</b></nobr>.")
            .arg (media)
            .arg (src),
        formatErrorInfo (vbox));
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

    message (
        mainWindowShown(),
        Error,
        tr ("Failed to open a session for the virtual machine <b>%1</b>.")
            .arg (machine.GetName()),
        !vbox.isOk() ? formatErrorInfo (vbox) :
                       formatErrorInfo (progress.GetErrorInfo())
    );
}

void VBoxProblemReporter::cannotGetMediaAccessibility (const CUnknown &unk)
{
    QString src;
    CHardDisk hd;
    CDVDImage dvd;
    CFloppyImage floppy;

    if (!(hd = unk).isNull())
        src = hd.GetLocation();
    else
    if (!(dvd = unk).isNull())
        src = dvd.GetFilePath();
    else
    if (!(floppy = unk).isNull())
        src = floppy.GetFilePath();
    else
        AssertMsgFailed (("Not a valid CUnknown\n"));

    message (qApp->activeWindow(), Error,
        tr ("Failed to get the accessibility state of the media "
            "<nobr><b>%1</b></nobr>. Some of the registered media may "
            "become inaccessible.")
            .arg (src),
        formatErrorInfo (unk));
}

#if defined Q_WS_WIN

void VBoxProblemReporter::cannotCreateHostInterface (
    const CHost &host, const QString &name, QWidget *parent)
{
    message (parent ? parent : mainWindowShown(), Error,
        tr ("Failed to create the host network interface <b>%1</b>.")
            .arg (name),
        formatErrorInfo (host));
}

void VBoxProblemReporter::cannotCreateHostInterface (
    const CProgress &progress, const QString &name, QWidget *parent)
{
    message (parent ? parent : mainWindowShown(), Error,
        tr ("Failed to create the host network interface <b>%1</b>.")
            .arg (name),
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
                    0, /* autoConfirmId */
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

int VBoxProblemReporter::confirmDownloadAdditions (const QString &aURL,
                                                   ulong aSize)
{
    return message (&vboxGlobal().consoleWnd(), Question,
                    tr ("<p>Are you sure you want to download the VirtualBox "
                        "Guest Additions CD image from "
                        "<nobr><a href=\"%1\">%2</a></nobr> "
                        "(size %3 bytes)?</p>").arg (aURL).arg (aURL).arg (aSize),
                    0, /* autoConfirmId */
                    QIMessageBox::Yes | QIMessageBox::Default,
                    QIMessageBox::No | QIMessageBox::Escape);
}

int VBoxProblemReporter::confirmMountAdditions (const QString &aURL,
                                                const QString &aSrc)
{
    return message (&vboxGlobal().consoleWnd(), Question,
                    tr ("<p>The VirtualBox Guest Additions CD image has been "
                        "successfully downloaded from "
                        "<nobr><a href=\"%1\">%2</a></nobr> "
                        "and saved locally as <nobr><b>%3</b>.</nobr></p>"
                        "<p>Do you want to register this CD image and mount it "
                        "on the virtual CD/DVD drive?</p>")
                        .arg (aURL).arg (aURL).arg (aSrc),
                    0, /* autoConfirmId */
                    QIMessageBox::Yes | QIMessageBox::Default,
                    QIMessageBox::No | QIMessageBox::Escape);
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
                 "registration service.</p><p>%1</p>")
                 .arg (aReason));
}

void VBoxProblemReporter::showRegisterResult (QWidget *aParent,
                                              const QString &aResult)
{
    aResult == "OK" ?
        message (aParent, Info,
                 tr ("<p>Congratulations! You have successfully registered the "
                     "VirtualBox product.</p>"
                     "<p>Thank you for finding time to fill out the "
                     "registration form!</p>")) :
        message (aParent, Error,
                 tr ("<p>Failed to register the VirtualBox product (%1).</p>")
                 .arg (aResult));
}

/** @return false if the dialog wasn't actually shown (i.e. it was autoconfirmed) */
bool VBoxProblemReporter::remindAboutInputCapture()
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
            "<img src=hostkey_16px.png/>&nbsp;icon. This icon, together "
            "with the mouse icon placed nearby, indicate the current keyboard "
            "and mouse capture state."
            "</p>"),
        "remindAboutInputCapture");

    return !(rc & AutoConfirmed);
}

/** @return false if the dialog wasn't actually shown (i.e. it was autoconfirmed) */
bool VBoxProblemReporter::remindAboutAutoCapture()
{
    int rc = message ( &vboxGlobal().consoleWnd(), Info,
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
            "<img src=hostkey_16px.png/>&nbsp;icon. This icon, together "
            "with the mouse icon placed nearby, indicate the current keyboard "
            "and mouse capture state."
            "</p>"),
        "remindAboutAutoCapture");

    return !(rc & AutoConfirmed);
}

/** @return false if the dialog wasn't actually shown (i.e. it was autoconfirmed) */
bool VBoxProblemReporter::remindAboutMouseIntegration (bool supportsAbsolute)
{
    static const char *kNames [2] =
    {
        "remindAboutMouseIntegrationOff",
        "remindAboutMouseIntegrationOn"
    };

    /* Close the previous (outdated) window if any. We use kName as
     * autoConfirmId which is also used as the widget name by default. */
    {
        QWidget *outdated =
            VBoxGlobal::findWidget (NULL, kNames [int (!supportsAbsolute)],
                                    "QIMessageBox");
        if (outdated)
            outdated->close();
    }

    if (supportsAbsolute)
    {
        int rc = message (&vboxGlobal().consoleWnd(), Info,
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
                "<img src=mouse_seamless_16px.png/>&nbsp;to inform you that mouse "
                "pointer integration is supported by the guest OS and is "
                "currently turned on."
                "</p>"
                "<p><b>Note</b>: Some applications may behave incorrectly in "
                "mouse pointer integration mode. You can always disable it for "
                "the current session (and enable it again) by selecting the "
                "corresponding action from the menu bar."
                "</p>"),
            kNames [1] /* autoConfirmId */);

        return !(rc & AutoConfirmed);
    }
    else
    {
        int rc = message (&vboxGlobal().consoleWnd(), Info,
            tr ("<p>The Virtual Machine reports that the guest OS does not "
                "support <b>mouse pointer integration</b> in the current video "
                "mode. You need to capture the mouse (by clicking over the VM "
                "display or pressing the host key) in order to use the "
                "mouse inside the guest OS.</p>"),
            kNames [0] /* autoConfirmId */);

        return !(rc & AutoConfirmed);
    }
}

/** @return false if the dialog wasn't actually shown (i.e. it was autoconfirmed) */
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
    int rc = message (
        &vboxGlobal().selectorWnd(),
        Warning,
        tr ("<p>One or more of the registered virtual hard disks, CD/DVD or "
            "floppy media are not currently accessible. As a result, you will "
            "not be able to operate virtual machines that use these media until "
            "they become accessible later.</p>"
            "<p>Press <b>OK</b> to open the Virtual Disk Manager window and "
            "see what media are inaccessible, or press <b>Ignore</b> to "
            "ignore this message.</p>"),
        "remindAboutInaccessibleMedia",
        QIMessageBox::Ok | QIMessageBox::Default,
        QIMessageBox::Ignore | QIMessageBox::Escape
    );

    return rc == QIMessageBox::Ok && !(rc & AutoConfirmed);
}

/**
 *  @param  fullscreen hot key as defined in the menu
 *  @param  current host key as in the global settings
 *  @return true if the user has chosen to go fullscreen.
 */
void VBoxProblemReporter::remindAboutGoingFullscreen (const QString &hotKey,
                                                      const QString &hostKey)
{
    int rc = message (&vboxGlobal().consoleWnd(), Info,
        tr ("<p>The virtual machine window will be now switched to "
            "<b>fullscreen</b> mode. "
            "You can go back to windowed mode at any time by pressing "
            "<b>%1</b>. Note that the <i>Host</i> key is currently "
            "defined as <b>%1</b>.</p>"
            "<p>Note that the main menu bar is hidden fullscreen mode. You "
            "can access it by pressing <b>Host+Home</b>.</p>")
            .arg (hotKey).arg (hostKey),
        "remindAboutGoingFullscreen");
    NOREF(rc);
}

/**
 *  @param  fullscreen hot key as defined in the menu
 *  @param  current host key as in the global settings
 *  @return true if the user has chosen to go fullscreen.
 */
void VBoxProblemReporter::remindAboutGoingSeamless (const QString &hotKey,
                                                    const QString &hostKey)
{
    int rc = message (&vboxGlobal().consoleWnd(), Info,
        tr ("<p>The virtual machine window will be now switched to "
            "<b>Seamless</b> mode. "
            "You can go back to windowed mode at any time by pressing "
            "<b>%1</b>. Note that the <i>Host</i> key is currently "
            "defined as <b>%1</b>.</p>"
            "<p>Note that the main menu bar is hidden seamless mode. You "
            "can access it by pressing <b>Host+Home</b>.</p>")
            .arg (hotKey).arg (hostKey),
        "remindAboutGoingSeamless");
    NOREF(rc);
}

void VBoxProblemReporter::remindAboutWrongColorDepth (ulong aRealBPP,
                                                      ulong aWantedBPP)
{
    const char *kName = "remindAboutWrongColorDepth";

    /* Close the previous (outdated) window if any. We use kName as
     * autoConfirmId which is also used as the widget name by default. */
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
            "VirualBox window.</p>"
            ""
            "<p>Press <b>OK</b> if you want to power off the machine "
            "or press <b>Ignore</b> if you want to leave it as is for debugging. "
            "Please note that debugging requires special knowledge and tools, so "
            "it is recommended to press <b>OK</b> now.</p>")
            .arg (aLogFolder),
        0, /* autoConfirmId */
        QIMessageBox::Ok | QIMessageBox::Default,
        QIMessageBox::Ignore | QIMessageBox::Escape);

    return rc == QIMessageBox::Ok;
}

int VBoxProblemReporter::remindAboutUnsetHD (QWidget *aParent)
{
    return message (
        aParent,
        Warning,
        tr ("<p>You didn't attach a hard disk to the new virtual machine. "
            "The machine will not be able to boot unless you attach "
            "a hard disk with a guest operating system or some other bootable "
            "media to it later using the machine settings dialog or the First "
            "Run Wizard.</p><p>Do you want to continue?</p>"),
        0, /* autoConfirmId */
        QIMessageBox::Yes,
        QIMessageBox::No | QIMessageBox::Default | QIMessageBox::Escape);
}

void VBoxProblemReporter::cannotRunInSelectorMode()
{
    message (mainWindowShown(), Critical,
             tr ("<p>Cannot run VirtualBox in <i>VM Selector</i> "
                 "mode due to local restrictions.</p>"
                 "<p>The application will now terminate.</p>"));
}

void VBoxProblemReporter::showRuntimeError (const CConsole &aConsole, bool fatal,
                                            const QString &errorID,
                                            const QString &errorMsg)
{
    /// @todo (r=dmik) it's just a preliminary box. We need to:
    //  - for fatal errors and non-fatal with-retry errors, listen for a
    //    VM state signal to automatically close the message if the VM
    //    (externally) leaves the Paused state while it is shown.
    //  - make warning messages modeless
    //  - add common buttons like Retry/Save/PowerOff/whatever

    CConsole console = aConsole;
    CEnums::MachineState state = console.GetState();
    Type type;
    QString severity;

    if (fatal)
    {
        /* the machine must be paused on fatal errors */
        Assert (state == CEnums::Paused);
        if (state != CEnums::Paused)
            console.Pause();
        type = Critical;
        severity = tr ("<nobr>Fatal Error</nobr>", "runtime error info");
    }
    else if (state == CEnums::Paused)
    {
        type = Error;
        severity = tr ("<nobr>Non-Fatal Error</nobr>", "runtime error info");
    }
    else
    {
        type = Warning;
        severity = tr ("<nobr>Warning</nobr>", "runtime error info");
    }

    QString formatted;

    if (!errorMsg.isEmpty())
        formatted += QString ("<table bgcolor=#FFFFFF border=0 cellspacing=0 "
                              "cellpadding=0 width=100%>"
                              "<tr><td><p>%1.</p></td></tr>"
                              "</table><p></p>")
                              .arg (VBoxGlobal::highlight (errorMsg));

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
            formatted);

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
            formatted);
    }
    else
    {
        rc = message (&vboxGlobal().consoleWnd(), type,
            tr ("<p>The virtual machine execution may run into an error "
                "condition as described below. "
                "You may ignore this message, but it is suggested to perform "
                "an appropriate action to make sure the described error will "
                "not happen.</p>"),
            formatted);
    }

    NOREF(rc);
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
    QString formatted;

    if (aInfo.text())
        formatted += QString ("<table bgcolor=#FFFFFF border=0 cellspacing=0 "
                              "cellpadding=0 width=100%>"
                              "<tr><td><p>%1.</p></td></tr>"
                              "</table><p></p>")
                              .arg (VBoxGlobal::highlight (aInfo.text()));

    formatted += "<table bgcolor=#EEEEEE border=0 cellspacing=0 "
                 "cellpadding=0 width=100%>";

    bool haveResultCode = false;

    if (aInfo.isBasicAvailable())
    {
#if defined (Q_WS_WIN)
        haveResultCode = aInfo.isFullAvailable();
        bool haveComponent = true;
        bool haveInterfaceID = true;
#else // !Q_WS_WIN
        haveResultCode = true;
        bool haveComponent = aInfo.isFullAvailable();
        bool haveInterfaceID = aInfo.isFullAvailable();
#endif

        if (haveResultCode)
        {
#if defined (Q_WS_WIN)
            /* format the error code, masking off the top 16 bits */
            PCRTWINERRMSG msg;
            msg = RTErrWinGet(aInfo.resultCode());
            /* try again with masked off top 16bit if not found */
            if (!msg->iCode)
                msg = RTErrWinGet(aInfo.resultCode() & 0xFFFF);
            formatted += QString ("<tr><td>%1</td><td><tt>%2 (0x%3)</tt></td></tr>")
                .arg (tr ("Result&nbsp;Code: ", "error info"))
                .arg (msg->pszDefine)
                .arg (uint (aInfo.resultCode()), 8, 16);
#else
            formatted += QString ("<tr><td>%1</td><td><tt>0x%2</tt></td></tr>")
                .arg (tr ("Result&nbsp;Code: ", "error info"))
                .arg (uint (aInfo.resultCode()), 8, 16);
#endif
        }

        if (haveComponent)
            formatted += QString ("<tr><td>%1</td><td>%2</td></tr>")
                .arg (tr ("Component: ", "error info"), aInfo.component());

        if (haveInterfaceID)
        {
            QString s = aInfo.interfaceID();
            if (aInfo.interfaceName())
                s = aInfo.interfaceName() + ' ' + s;
            formatted += QString ("<tr><td>%1</td><td>%2</td></tr>")
                .arg (tr ("Interface: ", "error info"), s);
        }

        if (!aInfo.calleeIID().isNull() && aInfo.calleeIID() != aInfo.interfaceID())
        {
            QString s = aInfo.calleeIID();
            if (aInfo.calleeName())
                s = aInfo.calleeName() + ' ' + s;
            formatted += QString ("<tr><td>%1</td><td>%2</td></tr>")
                .arg (tr ("Callee: ", "error info"), s);
        }
    }

    if (FAILED (aWrapperRC) &&
        (!haveResultCode || aWrapperRC != aInfo.resultCode()))
    {
#if defined (Q_WS_WIN)
        /* format the error code */
        PCRTWINERRMSG msg;
        msg = RTErrWinGet(aWrapperRC);
        /* try again with masked off top 16bit if not found */
        if (!msg->iCode)
            msg = RTErrWinGet(aWrapperRC & 0xFFFF);
        formatted += QString ("<tr><td>%1</td><td><tt>%2 (0x%3)</tt></td></tr>")
            .arg (tr ("Callee&nbsp;RC: ", "error info"))
            .arg (msg->pszDefine)
            .arg (uint (aWrapperRC), 8, 16);
#else
        formatted += QString ("<tr><td>%1</td><td><tt>0x%2</tt></td></tr>")
            .arg (tr ("Callee&nbsp;RC: ", "error info"))
            .arg (uint (aWrapperRC), 8, 16);
#endif
    }
    formatted += "</table>";

    if (aInfo.next())
        formatted = doFormatErrorInfo (*aInfo.next()) +  "<p></p>" +
                    formatted;

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
    QString COMVersion = vbox.GetVersion();
    AssertWrapperOk (vbox);

    VBoxAboutDlg dlg (mainWindowShown(), "VBoxAboutDlg");
    dlg.setup (COMVersion);
    dlg.exec();
}

void VBoxProblemReporter::showHelpHelpDialog()
{
#ifndef VBOX_OSE
#if defined (Q_WS_WIN32)
    QString fullHelpFilePath = qApp->applicationDirPath() + "/VirtualBox.chm";

    HtmlHelp (GetDesktopWindow(), fullHelpFilePath.ucs2(),
              HH_DISPLAY_TOPIC, NULL);
#elif defined (Q_WS_X11)
    char szDocsPath[RTPATH_MAX];
    char szViewerPath[RTPATH_MAX];
    int rc;

    rc = RTPathAppDocs (szDocsPath, sizeof (szDocsPath));
    Assert(RT_SUCCESS(rc));
    rc = RTPathAppPrivateArch (szViewerPath, sizeof (szViewerPath));

    QString fullProgPath = QString(szDocsPath);
    QProcess kchmViewer (QString(szViewerPath) + "/kchmviewer");
    kchmViewer.addArgument (fullProgPath + "/VirtualBox.chm");
    kchmViewer.launch ("");
#elif defined (Q_WS_MAC)
    QProcess openApp (QString("/usr/bin/open"));
    openApp.addArgument (qApp->applicationDirPath() + "/UserManual.pdf");
    openApp.launch ("");
#endif
#endif /* VBOX_OSE */
}

void VBoxProblemReporter::resetSuppressedMessages()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    vbox.SetExtraData (VBoxDefs::GUI_SuppressMessages, QString::null);
}

/** @fn vboxProblem
 *
 *  Shortcut to the static VBoxProblemReporter::instance() method, for
 *  convenience.
 */
