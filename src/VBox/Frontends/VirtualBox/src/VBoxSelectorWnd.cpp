/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxSelectorWnd class implementation
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

#include "VBoxProblemReporter.h"
#include "VBoxSelectorWnd.h"
#include "VBoxVMListBox.h"
#include "VBoxConsoleWnd.h"
#include "VBoxToolBar.h"

#include "VBoxSnapshotsWgt.h"
#include "VBoxNewVMWzd.h"
#include "VBoxDiskImageManagerDlg.h"
#include "VBoxVMSettingsDlg.h"
#include "VBoxGlobalSettingsDlg.h"
#include "VBoxVMLogViewer.h"

#include <qlabel.h>
#include <qtextbrowser.h>
#include <qmenubar.h>
#include <qpopupmenu.h>
#include <qmessagebox.h>
#include <qwidgetlist.h>
#include <qtabwidget.h>
#include <qwidgetstack.h>
#include <qbutton.h>
#include <qprocess.h>

#include <qlayout.h>
#include <qvbox.h>

#ifdef Q_WS_X11
#include <iprt/env.h>
#endif


// VBoxVMDetailsView class
////////////////////////////////////////////////////////////////////////////////

/**
 *  Two-page widget stack to represent VM details: one page for normal details
 *  and another one for inaccessibility errors.
 */
class VBoxVMDetailsView : public QWidgetStack
{
    Q_OBJECT

public:

    VBoxVMDetailsView (QWidget *aParent, const char *aName,
                       QAction *aRefreshAction = NULL);

    void languageChange();

    void setDetailsText (const QString &aText)
    {
        mDetailsText->setText (aText);
        raiseWidget (0);
    }

    void setErrorText (const QString &aText)
    {
        createErrPage();
        mErrText->setText (aText);
        raiseWidget (1);
    }

    void setEmpty()
    {
        mDetailsText->setText (QString::null);
        raiseWidget (0);
    }

signals:

    void linkClicked (const QString &aURL);

private slots:

    void gotLinkClicked (const QString &aURL)
    {
        QString text = mDetailsText->text();
        emit linkClicked (aURL);
        /* QTextBrowser will try to get the URL from the mime source factory
         * and show an empty "page" after a failure. Reset the text to avoid
         * this. */
        mDetailsText->setText (text);
    }

private:

    void createErrPage();

    QTextBrowser *mDetailsText;

    QWidget *mErrBox;
    QLabel *mErrLabel;
    QTextBrowser *mErrText;
    QToolButton *mRefreshButton;
    QAction *mRefreshAction;
};

VBoxVMDetailsView::VBoxVMDetailsView (QWidget *aParent, const char *aName,
                                      QAction *aRefreshAction /* = NULL */)
    : QWidgetStack (aParent, aName)
    , mErrBox (NULL), mErrLabel (NULL), mErrText (NULL)
    , mRefreshButton (NULL)
    , mRefreshAction (aRefreshAction)
{
    Assert (mRefreshAction);

    /* create normal details page */

    mDetailsText = new QTextBrowser (mErrBox);
    mDetailsText->setFocusPolicy (QWidget::StrongFocus);
    mDetailsText->setLinkUnderline (false);
    /* make "transparent" */
    mDetailsText->setFrameShape (QFrame::NoFrame);
    mDetailsText->setPaper (backgroundBrush());

    connect (mDetailsText, SIGNAL (linkClicked (const QString &)),
            this, SLOT (gotLinkClicked (const QString &)));

    addWidget (mDetailsText, 0);
}

void VBoxVMDetailsView::createErrPage()
{
    /* create inaccessible details page */

    if (mErrBox)
        return;

    mErrBox = new QWidget();

    QVBoxLayout *layout = new QVBoxLayout (mErrBox);
    layout->setSpacing (10);

    mErrLabel = new QLabel (mErrBox);
    mErrLabel->setAlignment (WordBreak);
    mErrLabel->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->add (mErrLabel);

    mErrText = new QTextBrowser (mErrBox);
    mErrText->setFocusPolicy (QWidget::StrongFocus);
    mErrText->setLinkUnderline (false);
    layout->add (mErrText);

    if (mRefreshAction)
    {
        mRefreshButton = new QToolButton (mErrBox);
        mRefreshButton->setFocusPolicy (QWidget::StrongFocus);

        QHBoxLayout *hLayout = new QHBoxLayout (layout);
        hLayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Expanding,
                                                 QSizePolicy::Minimum));
        hLayout->add (mRefreshButton);

        connect (mRefreshButton, SIGNAL (clicked()),
                 mRefreshAction, SIGNAL (activated()));
    }

    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum,
                                            QSizePolicy::Expanding));

    addWidget (mErrBox, 1);

    languageChange();
}

void VBoxVMDetailsView::languageChange()
{
    if (mErrLabel)
        mErrLabel->setText (tr (
            "The selected virtual machine is <i>inaccessible</i>. Please "
            "inspect the error message shown below and press the "
            "<b>Refresh</b> button if you want to repeat the accessibility "
            "check:"));

    if (mRefreshAction && mRefreshButton)
    {
        mRefreshButton->setText (mRefreshAction->menuText());
        /* If we set the combination of the text label and icon
         * below, we lose the Alt+R shortcut functionality... */
        mRefreshButton->setTextLabel (mRefreshAction->text());
        mRefreshButton->setAccel (mRefreshAction->accel());
        mRefreshButton->setIconSet (mRefreshAction->iconSet());
        mRefreshButton->setTextPosition (QToolButton::BesideIcon);
        mRefreshButton->setUsesTextLabel (true);
    }
}

// VBoxVMDescriptionPage class
////////////////////////////////////////////////////////////////////////////////

/**
 *  Comments page widget to represent VM comments.
 */
class VBoxVMDescriptionPage : public QWidget
{
    Q_OBJECT

public:

    VBoxVMDescriptionPage (VBoxSelectorWnd *, const char *);
    ~VBoxVMDescriptionPage() {}

    void setMachineItem (VBoxVMListBoxItem *aItem);

    void languageChange();
    void updateState();

private slots:

    void goToSettings();

private:

    VBoxVMListBoxItem *mItem;

    VBoxSelectorWnd *mParent;
    QToolButton *mBtnEdit;
    QTextBrowser *mBrowser;
    QLabel *mLabel;
};

VBoxVMDescriptionPage::VBoxVMDescriptionPage (VBoxSelectorWnd *aParent,
                                              const char *aName)
    : QWidget (aParent, aName)
    , mItem (NULL), mParent (aParent)
    , mBtnEdit (0), mBrowser (0), mLabel (0)
{
    /* main layout */
    QVBoxLayout *mainLayout = new QVBoxLayout (this, 0, 10, "mainLayout");

    /* mBrowser */
    mBrowser = new QTextBrowser (this, "mBrowser");
    mBrowser->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Expanding);
    mBrowser->setFocusPolicy (QWidget::StrongFocus);
    mBrowser->setLinkUnderline (false);
    mainLayout->addWidget (mBrowser);
    /* hidden by default */
    mBrowser->setHidden (true);

    mLabel = new QLabel (this, "mLabel");
    mLabel->setFrameStyle (mBrowser->frameStyle());
    mLabel->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Expanding);
    mLabel->setAlignment (AlignCenter | WordBreak);
    mainLayout->addWidget (mLabel);
    /* always disabled */
    mLabel->setEnabled (false);

    /* button layout */
    QHBoxLayout *btnLayout = new QHBoxLayout (mainLayout, 10, "btnLayout");
    btnLayout->addItem (new QSpacerItem (0, 0,
                                         QSizePolicy::Expanding,
                                         QSizePolicy::Minimum));

    /* button */
    mBtnEdit = new QToolButton (this, "mBtnEdit");
    mBtnEdit->setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Fixed);
    mBtnEdit->setFocusPolicy (QWidget::StrongFocus);
    mBtnEdit->setIconSet (VBoxGlobal::iconSet ("edit_description_16px.png",
                                               "edit_description_disabled_16px.png"));
    mBtnEdit->setTextPosition (QToolButton::BesideIcon);
    mBtnEdit->setUsesTextLabel (true);
    connect (mBtnEdit, SIGNAL (clicked()), this, SLOT (goToSettings()));
    btnLayout->addWidget (mBtnEdit);

    mainLayout->addItem (new QSpacerItem (0, 0,
                                          QSizePolicy::Expanding,
                                          QSizePolicy::Minimum));

    /* apply language settings */
    languageChange();

    updateState();
}

/**
 * The machine list @a aItem is used to access cached machine data w/o making
 * unnecessary RPC calls.
 */
void VBoxVMDescriptionPage::setMachineItem (VBoxVMListBoxItem *aItem)
{
    mItem = aItem;

    QString text = aItem ? aItem->machine().GetDescription() : QString::null;

    if (!text.isEmpty())
    {
        mLabel->setHidden (true);
        mBrowser->setText (text);
        mBrowser->setShown (true);
    }
    else
    {
        mBrowser->setHidden (true);
        mBrowser->clear();
        mLabel->setShown (true);
    }

    /* check initial machine and session states */
    updateState();
}

void VBoxVMDescriptionPage::languageChange()
{
    mLabel->setText (tr ("No description. Press the Edit button below to add it."));

    mBtnEdit->setTextLabel (tr ("Edit"));
    mBtnEdit->setAccel (tr ("Ctrl+E"));
    QToolTip::add (mBtnEdit, tr ("Edit (Ctrl+E)"));
    mBtnEdit->adjustSize();
    mBtnEdit->updateGeometry();
}

/**
 * Called by the parent from machineStateChanged() and sessionStateChanged()
 * signal handlers. We cannot connect to these signals ourselves because we
 * use the VBoxVMListBoxItem which needs to be properly updated by the parent
 * first.
 */
void VBoxVMDescriptionPage::updateState()
{
    /// @todo disabling the edit button for a saved VM will not be necessary
    /// when we implement the selective VM Settings dialog, where only fields
    /// that can be changed in the saved state, can be changed.

    if (mItem)
    {
        bool saved = mItem->state() == KMachineState_Saved;
        bool busy = mItem->sessionState() != KSessionState_Closed;
        mBtnEdit->setEnabled (!saved && !busy);
    }
    else
        mBtnEdit->setEnabled (false);
}

void VBoxVMDescriptionPage::goToSettings()
{
    mParent->vmSettings ("#general", "teDescription");
}

// VBoxSelectorWnd class
////////////////////////////////////////////////////////////////////////////////

/** \class VBoxSelectorWnd
 *
 *  The VBoxSelectorWnd class is a VM selector window, one of two main VBox
 *  GUI windows.
 *
 *  This window appears when the user starts the VirtualBox executable.
 *  It allows to view the list of configured VMs, their settings
 *  and the current state, create, reconfigure, delete and start VMs.
 */

/**
 *  Constructs the VM selector window.
 *
 *  @param aSelf pointer to a variable where to store |this| right after
 *               this object's constructor is called (necessary to avoid
 *               recursion in VBoxGlobal::selectorWnd())
 */
VBoxSelectorWnd::
VBoxSelectorWnd (VBoxSelectorWnd **aSelf, QWidget* aParent, const char* aName,
                 WFlags aFlags)
    : QMainWindow (aParent, aName, aFlags)
    , doneInaccessibleWarningOnce (false)
{
    if (aSelf)
        *aSelf = this;

    statusBar();

#ifndef Q_WS_WIN
    /* Application icon. On Win32, it's built-in to the executable. */
    setIcon (QPixmap::fromMimeSource ("VirtualBox_48px.png"));
#endif

    /* actions */

    fileDiskMgrAction = new QAction( this, "fileDiskMgrAction" );
    fileDiskMgrAction->setIconSet (VBoxGlobal::iconSet ("diskim_16px.png"));
    fileSettingsAction = new QAction(this, "fileSettingsAction");
    fileSettingsAction->setIconSet (VBoxGlobal::iconSet ("global_settings_16px.png"));
    fileExitAction = new QAction (this, "fileExitAction");
    fileExitAction->setIconSet (VBoxGlobal::iconSet ("exit_16px.png"));

    vmNewAction = new QAction (this, "vmNewAction");
    vmNewAction->setIconSet (VBoxGlobal::iconSetEx (
        "vm_new_32px.png", "new_16px.png"));
    vmConfigAction = new QAction (this, "vmConfigAction");
    vmConfigAction->setIconSet (VBoxGlobal::iconSetEx (
        "vm_settings_32px.png", "settings_16px.png",
        "vm_settings_disabled_32px.png", "settings_dis_16px.png"));
    vmDeleteAction = new QAction( this, "vmDeleteAction" );
    vmDeleteAction->setIconSet (VBoxGlobal::iconSetEx (
        "vm_delete_32px.png", "delete_16px.png",
        "vm_delete_disabled_32px.png", "delete_dis_16px.png"));
    vmStartAction = new QAction( this, "vmStartAction" );
    vmStartAction->setIconSet (VBoxGlobal::iconSetEx (
        "vm_start_32px.png", "start_16px.png",
        "vm_start_disabled_32px.png", "start_dis_16px.png"));
    vmDiscardAction = new QAction (this, "vmDiscardAction");
    vmDiscardAction->setIconSet (VBoxGlobal::iconSetEx (
        "vm_discard_32px.png", "discard_16px.png",
        "vm_discard_disabled_32px.png", "discard_dis_16px.png"));
    vmPauseAction = new QAction (this, "vmPauseAction");
    vmPauseAction->setToggleAction (true);
    vmPauseAction->setIconSet (VBoxGlobal::iconSetEx (
        "vm_pause_32px.png", "pause_16px.png",
        "vm_pause_disabled_32px.png", "pause_disabled_16px.png"));
    vmRefreshAction = new QAction (this, "vmRefreshAction");
    vmRefreshAction->setIconSet (VBoxGlobal::iconSetEx (
        "vm_refresh_32px.png", "refresh_16px.png",
        "vm_refresh_disabled_32px.png", "refresh_disabled_16px.png"));
    vmShowLogsAction = new QAction (this, "vmShowLogsAction");
    vmShowLogsAction->setIconSet (VBoxGlobal::iconSetEx (
        "vm_show_logs_32px.png", "show_logs_16px.png",
        "vm_show_logs_disabled_32px.png", "show_logs_disabled_16px.png"));

    helpContentsAction = new QAction (this, "helpContentsAction");
    helpContentsAction->setIconSet (VBoxGlobal::iconSet ("help_16px.png"));
    helpWebAction = new QAction (this, "helpWebAction");
    helpWebAction->setIconSet (VBoxGlobal::iconSet ("site_16px.png"));
    helpRegisterAction = new QAction (this, "helpRegisterAction");
    helpRegisterAction->setIconSet (VBoxGlobal::iconSet ("register_16px.png",
                                                         "register_disabled_16px.png"));
    helpAboutAction = new QAction (this, "helpAboutAction");
    helpAboutAction->setIconSet (VBoxGlobal::iconSet ("about_16px.png"));
    helpResetMessagesAction = new QAction( this, "helpResetMessagesAction" );
    helpResetMessagesAction->setIconSet (VBoxGlobal::iconSet ("reset_16px.png"));

    /* subwidgets */

    /* central widget & horizontal layout */
    setCentralWidget (new QWidget (this, "centralWidget"));
    QHBoxLayout *centralLayout =
        new QHBoxLayout (centralWidget(), 5, 9, "centralLayout");

    /* left vertical box */
    QVBox *leftVBox = new QVBox (centralWidget(), "leftWidget");
    leftVBox->setSpacing (5);
    /* right vertical box */
    QVBox *rightVBox = new QVBox (centralWidget(), "rightWidget");
    rightVBox->setSpacing (5);
    centralLayout->addWidget (leftVBox, 3);
    centralLayout->addWidget (rightVBox, 5);

    /* VM list toolbar */
    VBoxToolBar *vmTools = new VBoxToolBar (this, leftVBox, "vmTools");

    /* VM list box */
    vmListBox = new VBoxVMListBox (leftVBox, "vmListBox");

    /* VM tab widget containing details and snapshots tabs */
    vmTabWidget = new QTabWidget (rightVBox, "vmTabWidget");
    vmTabWidget->setMargin (10);

    /* VM details view */
    vmDetailsView = new VBoxVMDetailsView (NULL, "vmDetailsView",
                                           vmRefreshAction);
    vmTabWidget->addTab (vmDetailsView,
                         VBoxGlobal::iconSet ("settings_16px.png"),
                         QString::null);

    /* VM snapshots list */
    vmSnapshotsWgt = new VBoxSnapshotsWgt (NULL, "vmSnapshotsWgt");
    vmTabWidget->addTab (vmSnapshotsWgt,
                         VBoxGlobal::iconSet ("take_snapshot_16px.png",
                                              "take_snapshot_dis_16px.png"),
                         QString::null);

    /* VM comments page */
    vmDescriptionPage = new VBoxVMDescriptionPage (this, "vmDescriptionPage");
    vmTabWidget->addTab (vmDescriptionPage,
                         VBoxGlobal::iconSet ("description_16px.png",
                                              "description_disabled_16px.png"),
                         QString::null);

    /* add actions to the toolbar */

    setUsesTextLabel (true);
    setUsesBigPixmaps (true);
    vmTools->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Preferred);

    vmNewAction->addTo (vmTools);
    vmTools->addSeparator();
    vmConfigAction->addTo (vmTools);
    vmDeleteAction->addTo (vmTools);
    vmTools->addSeparator();
    vmStartAction->addTo (vmTools);
    vmDiscardAction->addTo (vmTools);
#ifdef Q_WS_MAC
    vmTools->setMacStyle();
#endif

    /* add actions to menubar */

    QPopupMenu *fileMenu = new QPopupMenu(this, "fileMenu");
    fileDiskMgrAction->addTo( fileMenu );
    fileMenu->insertSeparator();
    fileSettingsAction->addTo(fileMenu);
    fileMenu->insertSeparator();
    fileExitAction->addTo(fileMenu);

    menuBar()->insertItem( QString::null, fileMenu, 1);

    QPopupMenu *vmMenu = new QPopupMenu (this, "vmMenu");
    vmNewAction->addTo (vmMenu);
    vmMenu->insertSeparator();
    vmConfigAction->addTo (vmMenu);
    vmDeleteAction->addTo (vmMenu);
    vmMenu->insertSeparator();
    vmStartAction->addTo (vmMenu);
    vmDiscardAction->addTo (vmMenu);
    vmMenu->insertSeparator();
    vmPauseAction->addTo (vmMenu);
    vmMenu->insertSeparator();
    vmRefreshAction->addTo (vmMenu);
    vmMenu->insertSeparator();
    vmShowLogsAction->addTo (vmMenu);

    menuBar()->insertItem (QString::null, vmMenu, 2);

    mVMCtxtMenu = new QPopupMenu (this, "mVMCtxtMenu");
    vmConfigAction->addTo (mVMCtxtMenu);
    vmDeleteAction->addTo (mVMCtxtMenu);
    mVMCtxtMenu->insertSeparator();
    vmStartAction->addTo (mVMCtxtMenu);
    vmDiscardAction->addTo (mVMCtxtMenu);
    mVMCtxtMenu->insertSeparator();
    vmPauseAction->addTo (mVMCtxtMenu);
    mVMCtxtMenu->insertSeparator();
    vmRefreshAction->addTo (mVMCtxtMenu);
    mVMCtxtMenu->insertSeparator();
    vmShowLogsAction->addTo (mVMCtxtMenu);

    QPopupMenu *helpMenu = new QPopupMenu( this, "helpMenu" );
    helpContentsAction->addTo (helpMenu);
    helpWebAction->addTo (helpMenu);
    helpMenu->insertSeparator();
#ifdef VBOX_WITH_REGISTRATION
    helpRegisterAction->addTo (helpMenu);
    helpRegisterAction->setEnabled (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_RegistrationDlgWinID).isEmpty());
#endif
    helpAboutAction->addTo (helpMenu);
    helpMenu->insertSeparator();
    helpResetMessagesAction->addTo (helpMenu);

    menuBar()->insertItem (QString::null, helpMenu, 3);

    languageChange();

    /* restore the position of the window */
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        QString winPos = vbox.GetExtraData (VBoxDefs::GUI_LastWindowPosition);

        QRect ar = QApplication::desktop()->availableGeometry (pos());
        bool ok = false, max = false;
        int x = 0, y = 0, w = 0, h = 0;
        x = winPos.section (',', 0, 0).toInt (&ok);
        if (ok)
            y = winPos.section (',', 1, 1).toInt (&ok);
        if (ok)
            w = winPos.section (',', 2, 2).toInt (&ok);
        if (ok)
            h = winPos.section (',', 3, 3).toInt (&ok);
        if (ok)
            max = winPos.section (',', 4, 4) == VBoxDefs::GUI_LastWindowPosition_Max;
        if (ok)
        {
            move (x, y);
            resize (QSize (w, h).expandedTo (minimumSizeHint())
                .boundedTo (ar.size()));
            if (max)
                /* maximize if needed */
                showMaximized();
        }
        else
        {
            resize (QSize (770, 550).expandedTo (minimumSizeHint())
                .boundedTo (ar.size()));
        }
    }

    /* restore the position of vm selector */
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        QString prevVMId = vbox.GetExtraData (VBoxDefs::GUI_LastVMSelected);

        VBoxVMListBoxItem *item = vmListBox->item (QUuid (prevVMId));
        if (item)
            vmListBox->setSelected (item, true);
    }

    clearWState (WState_Polished);

    /* signals and slots connections */
    connect (fileDiskMgrAction, SIGNAL (activated()), this, SLOT(fileDiskMgr()));
    connect (fileSettingsAction, SIGNAL (activated()), this, SLOT(fileSettings()));
    connect (fileExitAction, SIGNAL (activated()), this, SLOT (fileExit()));
    connect (vmNewAction, SIGNAL (activated()), this, SLOT (vmNew()));
    connect (vmConfigAction, SIGNAL (activated()), this, SLOT (vmSettings()));
    connect (vmDeleteAction, SIGNAL (activated()), this, SLOT (vmDelete()));
    connect (vmStartAction, SIGNAL (activated()), this, SLOT (vmStart()));
    connect (vmDiscardAction, SIGNAL (activated()), this, SLOT (vmDiscard()));
    connect (vmPauseAction, SIGNAL (toggled (bool)), this, SLOT (vmPause (bool)));
    connect (vmRefreshAction, SIGNAL (activated()), this, SLOT (vmRefresh()));
    connect (vmShowLogsAction, SIGNAL (activated()), this, SLOT (vmShowLogs()));

    connect (helpContentsAction, SIGNAL (activated()),
             &vboxProblem(), SLOT (showHelpHelpDialog()));
    connect (helpWebAction, SIGNAL (activated()),
             &vboxProblem(), SLOT (showHelpWebDialog()));
    connect (helpRegisterAction, SIGNAL (activated()),
             &vboxGlobal(), SLOT (showRegistrationDialog()));
    connect (&vboxGlobal(), SIGNAL (canShowRegDlg (bool)),
             helpRegisterAction, SLOT (setEnabled (bool)));
    connect (helpAboutAction, SIGNAL (activated()),
             &vboxProblem(), SLOT (showHelpAboutDialog()));
    connect (helpResetMessagesAction, SIGNAL (activated()),
             &vboxProblem(), SLOT (resetSuppressedMessages()));

    connect (vmListBox, SIGNAL (currentChanged(QListBoxItem *)),
             this, SLOT (vmListBoxCurrentChanged()));
    connect (vmListBox, SIGNAL (selected (QListBoxItem *)),
             this, SLOT (vmStart()));
    connect (vmListBox, SIGNAL (contextMenuRequested (QListBoxItem *, const QPoint &)),
             this, SLOT (showContextMenu (QListBoxItem *, const QPoint &)));

    connect (vmDetailsView, SIGNAL (linkClicked (const QString &)),
            this, SLOT (vmSettings (const QString &)));

    /* listen to media enumeration signals */
    connect (&vboxGlobal(), SIGNAL (mediaEnumStarted()),
             this, SLOT (mediaEnumStarted()));
    connect (&vboxGlobal(), SIGNAL (mediaEnumFinished (const VBoxMediaList &)),
             this, SLOT (mediaEnumFinished (const VBoxMediaList &)));

    /* connect VirtualBox callback events */
    connect (&vboxGlobal(), SIGNAL (machineStateChanged (const VBoxMachineStateChangeEvent &)),
             this, SLOT (machineStateChanged (const VBoxMachineStateChangeEvent &)));
    connect (&vboxGlobal(), SIGNAL (machineDataChanged (const VBoxMachineDataChangeEvent &)),
             this, SLOT (machineDataChanged (const VBoxMachineDataChangeEvent &)));
    connect (&vboxGlobal(), SIGNAL (machineRegistered (const VBoxMachineRegisteredEvent &)),
             this, SLOT (machineRegistered (const VBoxMachineRegisteredEvent &)));
    connect (&vboxGlobal(), SIGNAL (sessionStateChanged (const VBoxSessionStateChangeEvent &)),
             this, SLOT (sessionStateChanged (const VBoxSessionStateChangeEvent &)));
    connect (&vboxGlobal(), SIGNAL (snapshotChanged (const VBoxSnapshotEvent &)),
             this, SLOT (snapshotChanged (const VBoxSnapshotEvent &)));

    /* bring the VM list to the focus */
    vmListBox->setFocus();
}

VBoxSelectorWnd::~VBoxSelectorWnd()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* save the position of the window */
    {
        QString winPos = QString ("%1,%2,%3,%4")
                                 .arg (normal_pos.x()).arg (normal_pos.y())
                                 .arg (normal_size.width())
                                 .arg (normal_size.height());
        if (isMaximized())
            winPos += QString (",%1").arg (VBoxDefs::GUI_LastWindowPosition_Max);

        vbox.SetExtraData (VBoxDefs::GUI_LastWindowPosition, winPos);
    }
    /* save vm selector position */
    {
        QListBoxItem *item = vmListBox->selectedItem();
        QString curVMId = item ?
            QString (static_cast<VBoxVMListBoxItem*> (item)->id()) :
            QString::null;
        vbox.SetExtraData (VBoxDefs::GUI_LastVMSelected, curVMId);
    }
}

//
// Public slots
/////////////////////////////////////////////////////////////////////////////

void VBoxSelectorWnd::fileDiskMgr()
{
    VBoxDiskImageManagerDlg::showModeless();
}

void VBoxSelectorWnd::fileSettings()
{
    VBoxGlobalSettings settings = vboxGlobal().settings();
    CSystemProperties props = vboxGlobal().virtualBox().GetSystemProperties();

    VBoxGlobalSettingsDlg dlg (this, "VBoxGlobalSettingsDlg");
    dlg.getFrom (props, settings);

    if (dlg.exec() == QDialog::Accepted)
    {
        VBoxGlobalSettings s = settings;
        dlg.putBackTo (props, s);
        if (!props.isOk())
            vboxProblem().cannotSetSystemProperties (props);
        else
        {
            // see whether the user has changed something or not
            if (!(settings == s))
                vboxGlobal().setSettings (s);
        }
    }
}

void VBoxSelectorWnd::fileExit()
{
    close();
}

void VBoxSelectorWnd::vmNew()
{
    VBoxNewVMWzd wzd;
    if (wzd.exec() == QDialog::Accepted)
    {
        CMachine m = wzd.machine();

        /* wait until the list is updated by OnMachineRegistered() */
        VBoxVMListBoxItem *item = 0;
        while (!item)
        {
            qApp->processEvents();
            item = vmListBox->item (m.GetId());
        }
        /*
         *  we must use setSelected() instead of setCurrentItem() below because
         *  setCurrentItem() does nothing after the first item has been added
         *  to the list (since it is already the current one), but we still
         *  need currentChanged() to be emitted to sync things (QListBox doesn't
         *  emit currentChanged() when the current item index changes from -1
         *  to 0, i.e. when the first item is being added -- seems to be a Qt
         *  bug).
         */
        vmListBox->setSelected (item, true);
    }
}

/**
 *  Opens the VM settings dialog.
 *
 *  @param  aCategory   Category to select in the settings dialog. See
 *                      VBoxVMSettingsDlg::setup().
 *  @param  aControl    Widget name to select in the settings dialog. See
 *                      VBoxVMSettingsDlg::setup().
 */
void VBoxSelectorWnd::vmSettings (const QString &aCategory, const QString &aControl)
{
    if (!aCategory.isEmpty() && aCategory [0] != '#')
    {
        /* Assume it's a href from the Details HTML */
        vboxGlobal().openURL (aCategory);
        return;
    }

    VBoxVMListBoxItem *item = (VBoxVMListBoxItem *) vmListBox->selectedItem();

    AssertMsgReturnVoid (item, ("Item must be always selected here"));

    // open a direct session to modify VM settings
    QUuid id = item->id();
    CSession session = vboxGlobal().openSession (id);
    if (session.isNull())
        return;

    CMachine m = session.GetMachine();
    AssertMsgReturn (!m.isNull(), ("Machine must not be null"), (void) 0);

    VBoxVMSettingsDlg dlg (this, "VBoxVMSettingsDlg");
    dlg.getFromMachine (m);
    dlg.setup (aCategory, aControl);

    if (dlg.exec() == QDialog::Accepted)
    {
        QString oldName = m.GetName();
        COMResult res = dlg.putBackToMachine();
        if (res.isOk())
        {
            m.SaveSettings();
            if (m.isOk())
            {
                if (oldName.compare (m.GetName()))
                    vmListBox->sort();
            }
            else
            {
                vboxProblem().cannotSaveMachineSettings (m);
            }
        }
        else
        {
            vboxProblem().cannotApplyMachineSettings (m, res);
        }
    }

    vmListBox->setFocus();

    session.Close();
}

void VBoxSelectorWnd::vmDelete()
{
    VBoxVMListBoxItem *item = (VBoxVMListBoxItem *) vmListBox->selectedItem();

    AssertMsgReturn (item, ("Item must be always selected here"), (void) 0);

    if (vboxProblem().confirmMachineDeletion (item->machine()))
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        QUuid id = item->id();
        bool ok = false;
        if (item->accessible())
        {
            /* open a direct session to modify VM settings */
            CSession session = vboxGlobal().openSession (id);
            if (session.isNull())
                return;
            CMachine machine = session.GetMachine();
            /* detach all hard disks before unregistering */
            {
                CHardDiskAttachmentEnumerator hen
                    = machine.GetHardDiskAttachments().Enumerate();
                while (hen.HasMore())
                {
                    CHardDiskAttachment att = hen.GetNext();
                    machine.DetachHardDisk (att.GetBus(), att.GetChannel(), att.GetDevice());
                }
            }
            /* commit changes */
            machine.SaveSettings();
            if (!machine.isOk())
                vboxProblem().cannotSaveMachineSettings (machine);
            else
                ok = true;
            session.Close();
        }
        else
            ok = true;

        if (ok)
        {
            CMachine machine = item->machine();
            vbox.UnregisterMachine (id);
            if (vbox.isOk() && item->accessible())
            {
                /* delete machine settings */
                machine.DeleteSettings();
                /* remove the item shortly: cmachine it refers to is no longer valid! */
                vmListBox->removeItem (vmListBox->index (item));
            }
            if (!vbox.isOk() || !machine.isOk())
                vboxProblem().cannotDeleteMachine (vbox, machine);
        }
    }
}

void VBoxSelectorWnd::vmStart()
{
    /* we always get here when vmListBox emits the selected() signal,
     * so we must explicitly check if the action is enabled or not. */
    if (!vmStartAction->isEnabled())
        return;

    VBoxVMListBoxItem *item = (VBoxVMListBoxItem *) vmListBox->selectedItem();

    AssertMsg (item, ("Item must be always selected here"));

#if defined (VBOX_GUI_SEPARATE_VM_PROCESS)

    AssertMsg (!vboxGlobal().isVMConsoleProcess(),
               ("Must NOT be a VM console process"));

    /* just switch to the VM window if it already exists */
    if (item->canSwitchTo())
    {
        item->switchTo();
        return;
    }

    AssertMsg (item->state() < KMachineState_Running,
               ("Machine must be PoweredOff/Saved/Aborted"));

    QUuid id = item->id();
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CSession session;

    session.createInstance (CLSID_Session);
    if (session.isNull())
    {
        vboxProblem().cannotOpenSession (session);
        return;
    }

#if defined (Q_OS_WIN32)
    /* allow the started VM process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);
#endif

    QString env;
#if defined (Q_WS_X11)
    /* make sure the VM process will start on the same display as the Selector */
    {
        const char *display = RTEnvGet ("DISPLAY");
        if (display)
            env.sprintf ("DISPLAY=%s", display);
    }
#endif

    CProgress progress = vbox.OpenRemoteSession (session, id, "GUI/Qt3", env);
    if (!vbox.isOk())
    {
        vboxProblem().cannotOpenSession (vbox, item->machine());
        return;
    }

    /* show the "VM spawning" progress dialog */
    vboxProblem().showModalProgressDialog (progress, item->name(), this);

    if (progress.GetResultCode() != 0)
        vboxProblem().cannotOpenSession (vbox, item->machine(), progress);

    session.Close();

#else // !VBOX_GUI_SEPARATE_VM_PROCESS

    if (!vboxGlobal().startMachine (id))
        return;

    hide();

#endif
}

void VBoxSelectorWnd::vmDiscard()
{
    VBoxVMListBoxItem *item = (VBoxVMListBoxItem *) vmListBox->selectedItem();

    AssertMsgReturn (item, ("Item must be always selected here"), (void) 0);

    if (!vboxProblem().confirmDiscardSavedState (item->machine()))
        return;

    /* open a session to modify VM settings */
    QUuid id = item->id();
    CSession session;
    CVirtualBox vbox = vboxGlobal().virtualBox();
    session.createInstance (CLSID_Session);
    if (session.isNull())
    {
        vboxProblem().cannotOpenSession (session);
        return;
    }
    vbox.OpenSession (session, id);
    if (!vbox.isOk())
    {
        vboxProblem().cannotOpenSession (vbox, item->machine());
        return;
    }

    CConsole console = session.GetConsole();
    console.DiscardSavedState();
    if (!console.isOk())
        vboxProblem().cannotDiscardSavedState (console);

    session.Close();
}

void VBoxSelectorWnd::vmPause (bool aPause)
{
    VBoxVMListBoxItem *item = (VBoxVMListBoxItem *) vmListBox->selectedItem();

    AssertMsgReturn (item, ("Item must be always selected here"), (void) 0);

    CSession session = vboxGlobal().openExistingSession (item->id());
    if (session.isNull())
        return;

    CConsole console = session.GetConsole();
    if (console.isNull())
        return;

    if (aPause)
        console.Pause();
    else
        console.Resume();

    bool ok = console.isOk();
    if (!ok)
    {
        if (aPause)
            vboxProblem().cannotPauseMachine (console);
        else
            vboxProblem().cannotResumeMachine (console);
    }

    session.Close();
}

void VBoxSelectorWnd::vmRefresh()
{
    VBoxVMListBoxItem *item = (VBoxVMListBoxItem *) vmListBox->selectedItem();

    AssertMsgReturn (item, ("Item must be always selected here"), (void) 0);

    refreshVMItem (item->id(),
                   true /* aDetails */,
                   true /* aSnapshot */,
                   true /* aDescription */);
}

void VBoxSelectorWnd::vmShowLogs()
{
    VBoxVMListBoxItem *item = (VBoxVMListBoxItem *) vmListBox->selectedItem();
    CMachine machine = item->machine();
    VBoxVMLogViewer::createLogViewer (machine);
}

void VBoxSelectorWnd::refreshVMList()
{
    vmListBox->refresh();
    vmListBoxCurrentChanged();
}

void VBoxSelectorWnd::refreshVMItem (const QUuid &aID, bool aDetails,
                                                       bool aSnapshots,
                                                       bool aDescription)
{
    vmListBox->refresh (aID);
    VBoxVMListBoxItem *item = (VBoxVMListBoxItem *) vmListBox->selectedItem();
    if (item && item->id() == aID)
        vmListBoxCurrentChanged (aDetails, aSnapshots, aDescription);
}

void VBoxSelectorWnd::showContextMenu (QListBoxItem *aItem, const QPoint &aPoint)
{
    if (aItem)
        mVMCtxtMenu->exec (aPoint);
}

// Protected members
/////////////////////////////////////////////////////////////////////////////

bool VBoxSelectorWnd::event (QEvent *e)
{
    switch (e->type())
    {
        /* By handling every Resize and Move we keep track of the normal
         * (non-minimized and non-maximized) window geometry. Shame on Qt
         * that it doesn't provide this geometry in its public APIs. */

        case QEvent::Resize:
        {
            QResizeEvent *re = (QResizeEvent *) e;
            if ((windowState() & (WindowMaximized | WindowMinimized |
                                  WindowFullScreen)) == 0)
                normal_size = re->size();
            break;
        }
        case QEvent::Move:
        {
            if ((windowState() & (WindowMaximized | WindowMinimized |
                                  WindowFullScreen)) == 0)
                normal_pos = pos();
            break;
        }
        case QEvent::LanguageChange:
        {
            languageChange();
            break;
        }

        default:
            break;
    }

    return QMainWindow::event (e);
}

// Private members
/////////////////////////////////////////////////////////////////////////////

/**
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void VBoxSelectorWnd::languageChange()
{
#ifdef VBOX_OSE
    setCaption (tr ("VirtualBox OSE"));
#else
    setCaption (tr ("Sun xVM VirtualBox"));
#endif

    vmTabWidget->changeTab (vmDetailsView, tr ("&Details"));
    /* note: Snapshots and Details tabs are changed dynamically by
     * vmListBoxCurrentChanged() */

    /* ensure the details and screenshot view are updated */
    vmListBoxCurrentChanged();

    fileDiskMgrAction->setMenuText (tr ("Virtual &Disk Manager..."));
    fileDiskMgrAction->setAccel (tr ("Ctrl+D"));
    fileDiskMgrAction->setStatusTip (tr ("Display the Virtual Disk Manager dialog"));

#ifdef Q_WS_MAC
    /*
     * Macification: Getting the right menu as application preference menu item.
     *
     * QMenuBar::isCommand() in qmenubar_mac.cpp doesn't recognize "Setting"(s)
     * unless it's in the first position. So, we use the Mac term here to make
     * sure we get picked instead of the VM settings.
     *
     * Now, since both QMenuBar and we translate these strings, it's going to
     * be really interesting to see how this plays on non-english systems...
     */
    fileSettingsAction->setMenuText (tr ("&Preferences...", "global settings"));
#else
    /*
     * ...and on other platforms we use "Preferences" as well. The #ifdef is
     * left because of the possible localization problems on Mac we first need
     * to figure out.
     */
    fileSettingsAction->setMenuText (tr ("&Preferences...", "global settings"));
#endif
    fileSettingsAction->setAccel (tr ("Ctrl+G"));
    fileSettingsAction->setStatusTip (tr ("Display the global settings dialog"));

    fileExitAction->setMenuText (tr ("E&xit"));
    fileExitAction->setAccel (tr ("Ctrl+Q"));
    fileExitAction->setStatusTip (tr ("Close application"));

    vmNewAction->setMenuText (tr ("&New..."));
    vmNewAction->setText (tr ("New"));
    vmNewAction->setAccel (tr ("Ctrl+N"));
    vmNewAction->setStatusTip (tr ("Create a new virtual machine"));

    vmConfigAction->setMenuText (tr ("&Settings..."));
    vmConfigAction->setText (tr ("Settings"));
    vmConfigAction->setAccel (tr ("Ctrl+S"));
    vmConfigAction->setStatusTip (tr ("Configure the selected virtual machine"));

    vmDeleteAction->setMenuText (tr ("&Delete"));
    vmDeleteAction->setText (tr ("Delete"));
    vmDeleteAction->setStatusTip (tr ("Delete the selected virtual machine"));

    /* Note: vmStartAction text is set up in vmListBoxCurrentChanged() */

    vmDiscardAction->setMenuText (tr ("D&iscard"));
    vmDiscardAction->setText (tr ("Discard"));
    vmDiscardAction->setStatusTip (
        tr ("Discard the saved state of the selected virtual machine"));

    vmRefreshAction->setMenuText (tr ("&Refresh"));
    vmRefreshAction->setText (tr ("Refresh"));
    vmRefreshAction->setAccel (tr ("Ctrl+R"));
    vmRefreshAction->setStatusTip (
        tr ("Refresh the accessibility state of the selected virtual machine"));

    vmShowLogsAction->setMenuText (tr ("Show &Log..."));
    vmShowLogsAction->setText (tr ("Show Log..."));
    vmShowLogsAction->setAccel (tr ("Ctrl+L"));
    vmShowLogsAction->setStatusTip (
        tr ("Show the log files of the selected virtual machine"));

    helpContentsAction->setMenuText (tr ("&Contents..."));
    helpContentsAction->setAccel (tr ("F1"));
    helpContentsAction->setStatusTip (tr ("Show the online help contents"));

    helpWebAction->setMenuText (tr ("&VirtualBox Web Site..."));
    helpWebAction->setStatusTip (
        tr ("Open the browser and go to the VirtualBox product web site"));

    helpRegisterAction->setMenuText (tr ("R&egister VirtualBox..."));
    helpRegisterAction->setStatusTip (
        tr ("Open VirtualBox registration form"));

    helpAboutAction->setMenuText (tr ("&About VirtualBox..."));
    helpAboutAction->setStatusTip (tr ("Show a dialog with product information"));

    helpResetMessagesAction->setMenuText (tr ("&Reset All Warnings"));
    helpResetMessagesAction->setStatusTip (
        tr ("Cause all suppressed warnings and messages to be shown again"));

    if (menuBar()->findItem(1))
        menuBar()->findItem(1)->setText(tr("&File"));
    if (menuBar()->findItem(2))
        menuBar()->findItem(2)->setText (tr ("&Machine"));
    if (menuBar()->findItem(3))
        menuBar()->findItem(3)->setText (tr ("&Help"));

    vmDetailsView->languageChange();
    vmDescriptionPage->languageChange();
}

//
// Private slots
/////////////////////////////////////////////////////////////////////////////

void VBoxSelectorWnd::vmListBoxCurrentChanged (bool aRefreshDetails,
                                               bool aRefreshSnapshots,
                                               bool aRefreshDescription)
{
    if (!vmListBox->selectedItem() && vmListBox->currentItem() >= 0)
    {
        /* selected always follows current */
        vmListBox->setSelected (vmListBox->currentItem(), true);
    }

    vmListBox->ensureCurrentVisible();
    VBoxVMListBoxItem *item = (VBoxVMListBoxItem *) vmListBox->selectedItem();

    if (item && item->accessible())
    {
        CMachine m = item->machine();

        KMachineState state = item->state();
        bool running = item->sessionState() != KSessionState_Closed;
        bool modifyEnabled = !running && state != KMachineState_Saved;

        if (aRefreshDetails)
        {
            vmDetailsView->setDetailsText (
                vboxGlobal().detailsReport (m, false /* isNewVM */,
                                            modifyEnabled /* withLinks */));
        }
        if (aRefreshSnapshots)
        {
            /* update the snapshots tab name */
            QString name = tr ("&Snapshots");
            ULONG count = item->snapshotCount();
            if (count)
                name += QString (" (%1)").arg (count);
            vmTabWidget->changeTab (vmSnapshotsWgt, name);
            /* refresh the snapshots widget */
            vmSnapshotsWgt->setMachine (m);
            /* ensure the tab is enabled */
            vmTabWidget->setTabEnabled (vmSnapshotsWgt, true);
        }
        if (aRefreshDescription)
        {
            /* update the description tab name */
            QString name = m.GetDescription().isEmpty() ?
                tr ("D&escription") : tr ("D&escription *");
            vmTabWidget->changeTab (vmDescriptionPage, name);
            /* refresh the description widget */
            vmDescriptionPage->setMachineItem (item);
            /* ensure the tab is enabled */
            vmTabWidget->setTabEnabled (vmDescriptionPage, true);
        }

        /* enable/disable modify actions */
        vmConfigAction->setEnabled (modifyEnabled);
        vmDeleteAction->setEnabled (modifyEnabled);
        vmDiscardAction->setEnabled (state == KMachineState_Saved && !running);
        vmPauseAction->setEnabled (state == KMachineState_Running ||
                                   state == KMachineState_Paused);

        /* change the Start button text accordingly */
        if (state >= KMachineState_Running)
        {
            vmStartAction->setMenuText (tr ("S&how"));
            vmStartAction->setText (tr ("Show"));
            vmStartAction->setStatusTip (
                tr ("Switch to the window of the selected virtual machine"));

            vmStartAction->setEnabled (item->canSwitchTo());
        }
        else
        {
            vmStartAction->setMenuText (tr ("S&tart"));
            vmStartAction->setText (tr ("Start"));
            vmStartAction->setStatusTip (
                tr ("Start the selected virtual machine"));

            vmStartAction->setEnabled (!running);
        }

        /* change the Pause/Resume button text accordingly */
        if (state == KMachineState_Paused)
        {
            vmPauseAction->setMenuText (tr ("R&esume"));
            vmPauseAction->setText (tr ("Resume"));
            vmPauseAction->setAccel (tr ("Ctrl+P"));
            vmPauseAction->setStatusTip (
                tr ("Resume the execution of the virtual machine"));
            vmPauseAction->blockSignals (true);
            vmPauseAction->setOn (true);
            vmPauseAction->blockSignals (false);
        }
        else
        {
            vmPauseAction->setMenuText (tr ("&Pause"));
            vmPauseAction->setText (tr ("Pause"));
            vmPauseAction->setAccel (tr ("Ctrl+P"));
            vmPauseAction->setStatusTip (
                tr ("Suspend the execution of the virtual machine"));
            vmPauseAction->blockSignals (true);
            vmPauseAction->setOn (false);
            vmPauseAction->blockSignals (false);
        }

        /* disable Refresh for accessible machines */
        vmRefreshAction->setEnabled (false);

        /* enable the show log item for the selected vm */
        vmShowLogsAction->setEnabled (true);
    }
    else
    {
        /* Note that the machine becomes inaccessible (or if the last VM gets
         * deleted), we have to update all fields, ignoring input
         * arguments. */

        if (item)
        {
            /* the VM is inaccessible */
            vmDetailsView->setErrorText (
                VBoxProblemReporter::formatErrorInfo (item->accessError()));
            vmRefreshAction->setEnabled (true);
        }
        else
        {
            /* default HTML support in Qt is terrible so just try to get
             * something really simple */
            vmDetailsView->setDetailsText
                (tr ("<h3>"
                     "Welcome to VirtualBox!</h3>"
                     "<p>The left part of this window is intended to display "
                     "a list of all virtual machines on your computer. "
                     "The list is empty now because you haven't created any virtual "
                     "machines yet."
                     "<img src=welcome.png align=right/></p>"
                     "<p>In order to create a new virtual machine, press the "
                     "<b>New</b> button in the main tool bar located "
                     "at the top of the window.</p>"
                     "<p>You can press the <b>F1</b> key to get instant help, "
                     "or visit "
                     "<a href=http://www.virtualbox.org>www.virtualbox.org</a> "
                     "for the latest information and news.</p>"));
            vmRefreshAction->setEnabled (false);
        }

        /* empty and disable other tabs */

        vmSnapshotsWgt->setMachine (CMachine());
        vmTabWidget->changeTab (vmSnapshotsWgt, tr ("&Snapshots"));
        vmTabWidget->setTabEnabled (vmSnapshotsWgt, false);

        vmDescriptionPage->setMachineItem (NULL);
        vmTabWidget->changeTab (vmDescriptionPage, tr ("D&escription"));
        vmTabWidget->setTabEnabled (vmDescriptionPage, false);

        /* disable modify actions */
        vmConfigAction->setEnabled (false);
        vmDeleteAction->setEnabled (item != NULL);
        vmDiscardAction->setEnabled (false);
        vmPauseAction->setEnabled (false);

        /* change the Start button text accordingly */
        vmStartAction->setMenuText (tr ("S&tart"));
        vmStartAction->setText (tr ("Start"));
        vmStartAction->setStatusTip (
            tr ("Start the selected virtual machine"));
        vmStartAction->setEnabled (false);

        /* disable the show log item for the selected vm */
        vmShowLogsAction->setEnabled (false);
    }
}

void VBoxSelectorWnd::mediaEnumStarted()
{
    /* refresh the current details to pick up hard disk sizes */
    vmListBoxCurrentChanged (true /* aRefreshDetails */);
}

void VBoxSelectorWnd::mediaEnumFinished (const VBoxMediaList &list)
{
    /* refresh the current details to pick up hard disk sizes */
    vmListBoxCurrentChanged (true /* aRefreshDetails */);

    /* we warn about inaccessible media only once (after media emumeration
     * started from main() at startup), to avoid annoying the user */
    if (doneInaccessibleWarningOnce)
        return;

    doneInaccessibleWarningOnce = true;

    do
    {
        /* ignore the signal if a modal widget is currently active (we won't be
         * able to properly show the modeless VDI manager window in this case) */
        if (QApplication::activeModalWidget())
            break;

        /* ignore the signal if a VBoxDiskImageManagerDlg window is active */
        if (qApp->activeWindow() &&
            !strcmp (qApp->activeWindow()->className(), "VBoxDiskImageManagerDlg"))
            break;

        /* look for at least one inaccessible media */
        VBoxMediaList::const_iterator it;
        for (it = list.begin(); it != list.end(); ++ it)
            if ((*it).status == VBoxMedia::Inaccessible)
                break;

        if (it != list.end() && vboxProblem().remindAboutInaccessibleMedia())
        {
            /* Show the VDM dialog but don't refresh once more after a
             * just-finished refresh */
            VBoxDiskImageManagerDlg::showModeless (false /* aRefresh */);
        }
    }
    while (0);
}

void VBoxSelectorWnd::machineStateChanged (const VBoxMachineStateChangeEvent &e)
{
    refreshVMItem (e.id,
                   false /* aDetails */,
                   false /* aSnapshots */,
                   false /* aDescription */);

    /* simulate a state change signal */
    vmDescriptionPage->updateState();
}

void VBoxSelectorWnd::machineDataChanged (const VBoxMachineDataChangeEvent &e)
{
    refreshVMItem (e.id,
                   true  /* aDetails */,
                   false /* aSnapshots */,
                   true  /* aDescription */);
}

void VBoxSelectorWnd::machineRegistered (const VBoxMachineRegisteredEvent &e)
{
    if (e.registered)
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        CMachine m = vbox.GetMachine (e.id);
        if (!m.isNull())
        {
            new VBoxVMListBoxItem (vmListBox, m);
            vmListBox->sort();
        }
        /* m.isNull() is ok (theoretically, the machine could have been
         * already deregistered by some other client at this point) */
    }
    else
    {
        VBoxVMListBoxItem *item = vmListBox->item (e.id);
        if (item)
            vmListBox->removeItem (vmListBox->index (item));
        /* item = 0 is ok (if we originated this event then the item
         * has been already removed) */
    }
}

void VBoxSelectorWnd::sessionStateChanged (const VBoxSessionStateChangeEvent &e)
{
    refreshVMItem (e.id,
                   true  /* aDetails */,
                   false /* aSnapshots */,
                   false /* aDescription */);

    /* simulate a state change signal */
    vmDescriptionPage->updateState();
}

void VBoxSelectorWnd::snapshotChanged (const VBoxSnapshotEvent &aEvent)
{
    refreshVMItem (aEvent.machineId,
                   false /* aDetails */,
                   true  /* aSnapshot */,
                   false /* aDescription */);
}

#include "VBoxSelectorWnd.moc"
