/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxSelectorWnd class implementation
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

#include <qlabel.h>
#include <qtextbrowser.h>
#include <qmenubar.h>
#include <qpopupmenu.h>
#include <qmessagebox.h>
#include <qwidgetlist.h>
#include <qtabwidget.h>
#include <qwidgetstack.h>
#include <qbutton.h>

#include <qlayout.h>
#include <qvbox.h>

#if defined (Q_WS_WIN32)
#include <Htmlhelp.h>
#endif


static const char *GUI_LastWindowPosition = "GUI/LastWindowPostion";
static const char *GUI_LastWindowPosition_Max = "max";

// VBoxVMDetailsView class
////////////////////////////////////////////////////////////////////////////////

/**
 *  Two-page widget stack to represent VM details: one page for normal details
 *  and another one for inaccessibility errors.
 */
class VBoxVMDetailsView : public QWidgetStack
{
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

    bool connectDetailsText (const char *aSignal, const QObject *aReceiver,
                             const char *aMember)
    {
        return connect (mDetailsText, aSignal, aReceiver, aMember);
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
{
    if (aSelf)
        *aSelf = this;

    statusBar();

    /* application icon */
    setIcon (QPixmap::fromMimeSource ("ico40x01.png"));

    /* actions */

    fileDiskMgrAction = new QAction( this, "fileDiskMgrAction" );
    fileDiskMgrAction->setIconSet (VBoxGlobal::iconSet ("diskim_16px.png"));
    fileSettingsAction = new QAction(this, "fileSettingsAction");
    fileSettingsAction->setIconSet (VBoxGlobal::iconSet ("global_settings_16px.png"));
    fileExitAction = new QAction (this, "fileExitAction");
    fileExitAction->setIconSet (VBoxGlobal::iconSet ("exit_16px.png"));

    vmNewAction = new QAction (this, "vmNewAction");
    vmNewAction->setIconSet (VBoxGlobal::iconSetEx ("vm_new.png", "new_16px.png"));
    vmConfigAction = new QAction (this, "vmConfigAction");
    vmConfigAction->setIconSet (VBoxGlobal::iconSetEx (
        "vm_settings.png", "settings_16px.png",
        "vm_settings_dis.png", "settings_dis_16px.png"));
    vmDeleteAction = new QAction( this, "vmDeleteAction" );
    vmDeleteAction->setIconSet (VBoxGlobal::iconSetEx (
        "vm_delete.png", "delete_16px.png",
        "vm_delete_dis.png", "delete_dis_16px.png"));
    vmStartAction = new QAction( this, "vmStartAction" );
    vmStartAction->setIconSet (VBoxGlobal::iconSetEx (
        "vm_start.png", "start_16px.png",
        "vm_start_dis.png", "start_dis_16px.png"));
    vmDiscardAction = new QAction (this, "vmDiscardAction");
    vmDiscardAction->setIconSet (VBoxGlobal::iconSetEx (
        "vm_discard.png", "discard_16px.png",
        "vm_discard_dis.png", "discard_dis_16px.png"));
    vmRefreshAction = new QAction (this, "vmRefreshAction");
    vmRefreshAction->setIconSet (VBoxGlobal::iconSet (
        "refresh_16px.png", "refresh_disabled_16px.png"));
    
    helpContentsAction = new QAction (this, "helpContentsAction");
    helpContentsAction->setIconSet (VBoxGlobal::iconSet ("help_16px.png"));
    helpWebAction = new QAction (this, "helpWebAction");
    helpWebAction->setIconSet (VBoxGlobal::iconSet ("site_16px.png"));
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
                         VBoxGlobal::iconSet ("take_snapshot_16px.png"),
                         QString::null);

    /* add actions to the toolbar */

    setUsesTextLabel (true);
    setUsesBigPixmaps (true);
    vmTools->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Preferred);

    vmNewAction->addTo (vmTools);
    vmConfigAction->addTo (vmTools);
    vmDeleteAction->addTo (vmTools);
    vmTools->addSeparator();
    vmStartAction->addTo (vmTools);
    vmDiscardAction->addTo (vmTools);

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
    vmConfigAction->addTo (vmMenu);
    vmDeleteAction->addTo (vmMenu);
    vmMenu->insertSeparator();
    vmStartAction->addTo (vmMenu);
    vmDiscardAction->addTo (vmMenu);
    vmMenu->insertSeparator();
    vmRefreshAction->addTo (vmMenu);

    menuBar()->insertItem (QString::null, vmMenu, 2);

    QPopupMenu *helpMenu = new QPopupMenu( this, "helpMenu" );
    helpContentsAction->addTo ( helpMenu );
    helpWebAction->addTo( helpMenu );
    helpMenu->insertSeparator();
    helpAboutAction->addTo( helpMenu );
    helpMenu->insertSeparator();
    helpResetMessagesAction->addTo (helpMenu);

    menuBar()->insertItem (QString::null, helpMenu, 3);
    
    languageChange();

    /* restore the position of the window */
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        QString winPos = vbox.GetExtraData (GUI_LastWindowPosition);

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
            max = winPos.section (',', 4, 4) == GUI_LastWindowPosition_Max;
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
            resize (QSize (700, 500).expandedTo (minimumSizeHint())
                .boundedTo (ar.size()));
        }
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
    connect (vmRefreshAction, SIGNAL (activated()), this, SLOT (vmRefresh()));

    connect (helpContentsAction, SIGNAL (activated()), this, SLOT(showHelpContents()));
    connect (helpWebAction, SIGNAL (activated()),
             &vboxProblem(), SLOT (showHelpWebDialog()));
    connect (helpAboutAction, SIGNAL (activated()),
             &vboxProblem(), SLOT (showHelpAboutDialog()));
    connect (helpResetMessagesAction, SIGNAL (activated()),
             &vboxProblem(), SLOT (resetSuppressedMessages()));

    connect (vmListBox, SIGNAL (currentChanged(QListBoxItem *)),
             this, SLOT (vmListBoxCurrentChanged()));
    connect (vmListBox, SIGNAL (selected (QListBoxItem *)),
             this, SLOT (vmStart()));

    vmDetailsView->connectDetailsText (
        SIGNAL (linkClicked (const QString &)),
        this, SLOT (vmSettings (const QString &)));

    /* listen to "media enumeration finished" signals */
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
    /* save the position of the window */
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        QString winPos = QString ("%1,%2,%3,%4")
                                 .arg (normal_pos.x()).arg (normal_pos.y())
                                 .arg (normal_size.width())
                                 .arg (normal_size.height());
        if (isMaximized())
            winPos += QString (",%1").arg (GUI_LastWindowPosition_Max);

        vbox.SetExtraData (GUI_LastWindowPosition, winPos);
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
    VMGlobalSettings settings = vboxGlobal().settings();
    CSystemProperties props = vboxGlobal().virtualBox().GetSystemProperties();

    VBoxGlobalSettingsDlg dlg (this, "VBoxGlobalSettingsDlg");
    dlg.getFrom (props, settings);

    if (dlg.exec() == QDialog::Accepted)
    {
        VMGlobalSettings s = settings;
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
 *  @param  category
 *      Category to select in the settings dialog. See
 *      VBoxVMSettingsDlg::setup().
 */
void VBoxSelectorWnd::vmSettings (const QString &category /* = QString::null */)
{
    VBoxVMListBoxItem *item = (VBoxVMListBoxItem *) vmListBox->selectedItem();

    AssertMsgReturn (item, ("Item must be always selected here"), (void) 0);

    // open a direct session to modify VM settings
    QUuid id = item->id();
    CSession session = vboxGlobal().openSession (id);
    if (session.isNull())
        return;

    CMachine m = session.GetMachine();
    AssertMsgReturn (!m.isNull(), ("Machine must not be null"), (void) 0);

    VBoxVMSettingsDlg dlg (this, "VBoxVMSettingsDlg");
    dlg.getFromMachine (m);
    dlg.setup (category);

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
                    machine.DetachHardDisk (att.GetController(), att.GetDeviceNumber());
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

    CMachine machine = item->machine();
    AssertMsg (machine.GetState() < CEnums::Running,
               ("Machine must be PoweredOff/Saved/Aborted"));

#if defined (VBOX_GUI_SEPARATE_VM_PROCESS)

    AssertMsg (!vboxGlobal().isVMConsoleProcess(),
               ("Must NOT be a VM console process"));

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

    CProgress progress = vbox.OpenRemoteSession (session, id, "gui");
    if (!vbox.isOk())
    {
        vboxProblem().cannotOpenSession (vbox, machine);
        return;
    }

    /* show the "VM spawning" progress dialog */
    vboxProblem().showModalProgressDialog (progress, machine.GetName(), this);

    if (progress.GetResultCode() != 0)
        vboxProblem().cannotOpenSession (vbox, machine, progress);

    session.Close();

#else // !VBOX_GUI_SEPARATE_VM_PROCESS

    if (!vboxGlobal().startMachine (machine.GetId()))
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

void VBoxSelectorWnd::vmRefresh()
{
    VBoxVMListBoxItem *item = (VBoxVMListBoxItem *) vmListBox->selectedItem();

    AssertMsgReturn (item, ("Item must be always selected here"), (void) 0);
    
    refreshVMItem (item->id(), true /* aDetails */, true /* aSnapshots */);
}

void VBoxSelectorWnd::refreshVMList()
{
    vmListBox->refresh();
    vmListBoxCurrentChanged();
}

void VBoxSelectorWnd::refreshVMItem (const QUuid &aID, bool aDetails,
                                                       bool aSnapshots)
{
    vmListBox->refresh (aID);
    VBoxVMListBoxItem *item = (VBoxVMListBoxItem *) vmListBox->selectedItem();
    if (item && item->id() == aID)
        vmListBoxCurrentChanged (aDetails, aSnapshots);
}

#if defined (Q_WS_X11)
DECLCALLBACK(int) showHelpContentsThread(RTTHREAD  /* ThreadSelf */, void *pvUser)
{
    char **papszArgs = reinterpret_cast<char **>(pvUser);
    int rc;
    RTPROCESS hProcess;

    rc = RTProcCreate(papszArgs[0], papszArgs, 0, 0, &hProcess);
    if (rc == VINF_SUCCESS)
    {
        RTPROCSTATUS ProcStatus;

        return RTProcWait(hProcess, RTPROCWAIT_FLAGS_BLOCK, &ProcStatus);
    }
    return rc;
}
#endif

void VBoxSelectorWnd::showHelpContents()
{
#if defined (Q_WS_WIN32)
    QString fullHelpFilePath = qApp->applicationDirPath() + "/VirtualBox.chm";

    HtmlHelp (GetDesktopWindow(), fullHelpFilePath.ucs2(),
              HH_DISPLAY_TOPIC, NULL);
#elif defined (Q_WS_X11)
# ifndef VBOX_OSE
    char szPathProg[RTPATH_MAX];
    if (RTPathProgram(szPathProg, sizeof(szPathProg)) == VINF_SUCCESS)
    {
        const char szViewer[] = "/kchmviewer";
        const char szHelpFile[] = "/VirtualBox.chm";
        char szViewerPath[RTPATH_MAX], szFilePath[RTPATH_MAX];
        char *papszArgs[3] = { szViewerPath, szFilePath, 0 };
        RTTHREAD hThread;
        int cPathLen;

        cPathLen = strlen(szPathProg);
        AssertReturn(cPathLen + sizeof(szHelpFile) < RTPATH_MAX, (void) 0);  /* is there a cleaner way? */
        strcpy(szViewerPath, szPathProg);
        strncpy(szViewerPath + cPathLen, szViewer, sizeof(szViewerPath) - cPathLen);
        szViewerPath[sizeof(szViewerPath) - 1] = 0;
        strcpy(szFilePath, szPathProg);
        strncpy(szFilePath + cPathLen, szHelpFile, sizeof(szFilePath) - cPathLen);
        szFilePath[sizeof(szFilePath) - 1] = 0;
        /* For now we just start the process and do not worry about anything else. */
        RTThreadCreate(&hThread, showHelpContentsThread, papszArgs, 0,
                       RTTHREADTYPE_INFREQUENT_POLLER, 0, "HelpViewer");
    }
# endif
#endif
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
    setCaption (tr ("InnoTek VirtualBox"));
#endif

    vmTabWidget->changeTab (vmDetailsView, tr ("&Details"));
    vmTabWidget->changeTab (vmSnapshotsWgt, tr ("&Snapshots"));

    /* ensure the details and screenshot view are updated */
    vmListBoxCurrentChanged();

    fileDiskMgrAction->setMenuText (tr ("Virtual &Disk Manager..."));
    fileDiskMgrAction->setAccel (tr ("Ctrl+D"));
    fileDiskMgrAction->setStatusTip (tr ("Display the Virtual Disk Manager dialog"));

    fileSettingsAction->setMenuText (tr ("&Global Settings..."));
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
    vmDeleteAction->setAccel( QString::null );
    vmDeleteAction->setStatusTip (tr ("Delete the selected virtual machine"));

    vmStartAction->setMenuText (tr ("S&tart"));
    vmStartAction->setText (tr ("Start"));
    vmStartAction->setAccel (QString::null);
    vmStartAction->setStatusTip (tr ("Start the selected virtual machine"));

    vmDiscardAction->setMenuText (tr ("D&iscard"));
    vmDiscardAction->setText (tr ("Discard"));
    vmDiscardAction->setAccel (QString::null);
    vmDiscardAction->setStatusTip (
        tr ("Discard the saved state of the selected virtual machine"));

    vmRefreshAction->setMenuText (tr ("&Refresh"));
    vmRefreshAction->setText (tr ("Refresh"));
    vmRefreshAction->setAccel (tr ("Ctrl+R"));
    vmRefreshAction->setStatusTip (
        tr ("Refresh the accessibility state of the selected virtual machine"));

    helpContentsAction->setMenuText (tr ("&Contents..."));
    helpContentsAction->setAccel (tr ("F1"));
    helpContentsAction->setStatusTip (tr ("Show the online help contents"));

    helpWebAction->setMenuText (tr ("&VirtualBox Web Site..."));
    helpWebAction->setAccel( QString::null );
    helpWebAction->setStatusTip (
        tr ("Open the browser and go to the VirtualBox product web site"));

    helpAboutAction->setMenuText (tr ("&About VirtualBox..."));
    helpAboutAction->setAccel( QString::null );
    helpAboutAction->setStatusTip (tr ("Show a dialog with product information"));

    helpResetMessagesAction->setMenuText (tr ("&Reset All Warnings"));
    helpResetMessagesAction->setAccel (QString::null);
    helpResetMessagesAction->setStatusTip (
        tr ("Cause all suppressed warnings and messages to be shown again"));

    if (menuBar()->findItem(1))
        menuBar()->findItem(1)->setText(tr("&File"));
    if (menuBar()->findItem(2))
        menuBar()->findItem(2)->setText (tr ("&VM"));
    if (menuBar()->findItem(3))
        menuBar()->findItem(3)->setText (tr ("&Help"));
    
    vmDetailsView->languageChange();
}

//
// Private slots
/////////////////////////////////////////////////////////////////////////////

void VBoxSelectorWnd::vmListBoxCurrentChanged (bool aRefreshDetails,
                                               bool aRefreshSnapshots)
{
    if ( !vmListBox->selectedItem() && vmListBox->currentItem() >= 0 )
    {
        /* selected always follows current */
        vmListBox->setSelected( vmListBox->currentItem(), true );
    }
    vmListBox->ensureCurrentVisible();
    VBoxVMListBoxItem *item = (VBoxVMListBoxItem *) vmListBox->selectedItem();

    if (item && item->accessible())
    {
        CMachine m = item->machine();
        if (aRefreshDetails)
        {
            vmDetailsView->setDetailsText (
                vboxGlobal().detailsReport (m, false /* isNewVM */,
                                               true /* withLinks */));
        }
        if (aRefreshSnapshots)
        {
            /* refresh snapshots widget */
            vmSnapshotsWgt->setMachine (m);
        }

        CEnums::MachineState state = m.GetState();
        bool running = m.GetSessionState() != CEnums::SessionClosed;
        bool modifyEnabled = !running && state != CEnums::Saved;

        /* enable/disable info panes */
        vmDetailsView->setEnabled (modifyEnabled);
        vmSnapshotsWgt->setEnabled (!running);

        /* enable/disable modify actions */
        vmConfigAction->setEnabled (modifyEnabled);
        vmDeleteAction->setEnabled (modifyEnabled);
        vmStartAction->setEnabled (!running);
        vmDiscardAction->setEnabled (state == CEnums::Saved && !running);

        vmRefreshAction->setEnabled (false);
    }
    else
    {
        if (item)
        {
            /* the VM is inaccessible */
            vmDetailsView->setErrorText (
                VBoxProblemReporter::formatErrorInfo (item->accessError()));

            vmRefreshAction->setEnabled (true);
        }
        else
        {
            vmDetailsView->setEmpty();
            vmRefreshAction->setEnabled (false);
        }

        vmDetailsView->setEnabled (true);
        
        /* empty and disable refresh snapshots widget */
        vmSnapshotsWgt->setMachine (CMachine());
        vmSnapshotsWgt->setEnabled (false);

        /* disable modify actions */
        vmConfigAction->setEnabled (false);
        vmDeleteAction->setEnabled (item != NULL);
        vmStartAction->setEnabled (false);
        vmDiscardAction->setEnabled (false);
    }
}

void VBoxSelectorWnd::mediaEnumFinished (const VBoxMediaList &list)
{
    /* refresh the current details to pick up hard disk sizes */
    vmListBoxCurrentChanged (true /* aRefreshDetails */);

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

    /* we react to the media enumeration event only once (after media
     * emumeration started from main() at startup), and then disconnect
     * to avoid annoying the user */
    disconnect (&vboxGlobal(), SIGNAL (mediaEnumFinished (const VBoxMediaList &)),
                this, SLOT (mediaEnumFinished (const VBoxMediaList &)));
}

void VBoxSelectorWnd::machineStateChanged (const VBoxMachineStateChangeEvent &e)
{
    refreshVMItem (e.id, false /* aDetails */, false /* aSnapshots */);
}

void VBoxSelectorWnd::machineDataChanged (const VBoxMachineDataChangeEvent &e)
{
    refreshVMItem (e.id, true /* aDetails */, false /* aSnapshots */);
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
    refreshVMItem (e.id, false /* aDetails */, false /* aSnapshots */);
}

void VBoxSelectorWnd::snapshotChanged (const VBoxSnapshotEvent &/* e */)
{
    /// @todo (dmik) nothing to do, should remove I guess
}
