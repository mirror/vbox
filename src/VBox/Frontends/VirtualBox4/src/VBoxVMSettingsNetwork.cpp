/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsNetwork class implementation
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

#include "VBoxVMSettingsNetwork.h"
#include "VBoxVMSettingsDlg.h"
#include "QIWidgetValidator.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QFileDialog>

// #ifdef Q_WS_WIN
// static QListWidgetItem* findItem (QListWidget *aList,
//                                   const QString &aMatch)
// {
//     QList<QListWidgetItem*> list =
//         aList->findItems (aMatch, Qt::MatchExactly);
//
//     return list.count() ? list [0] : 0;
// }
//#endif

/* Initialization of variables common for all pages */
QTabWidget* VBoxVMSettingsNetwork::mTwAdapters = 0;
// #ifdef Q_WS_WIN
// QGroupBox* VBoxVMSettingsNetwork::mGbInterfaces = 0;
// QListWidget* VBoxVMSettingsNetwork::mLwInterfaces = 0;
// QAction* VBoxVMSettingsNetwork::mAddAction = 0;
// QAction* VBoxVMSettingsNetwork::mDelAction = 0;
// #endif
bool VBoxVMSettingsNetwork::mLockNetworkListUpdate = true;
QStringList VBoxVMSettingsNetwork::mListNetworks = QStringList();
// #ifdef Q_WS_WIN
// QStringList VBoxVMSettingsNetwork::mListInterface = QStringList();
// QString VBoxVMSettingsNetwork::mNoInterfaces = tr ("<No suitable interfaces>");
// #endif

VBoxVMSettingsNetwork::VBoxVMSettingsNetwork(QWidget *aParent /* = NULL */)
    : QIWithRetranslateUI<QWidget> (aParent)
    , mValidator (0)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsNetwork::setupUi (this);

    /* Setup common validators */
    mLeMAC->setValidator (new QRegExpValidator
        (QRegExp ("[0-9A-Fa-f][02468ACEace][0-9A-Fa-f]{10}"), this));

    /* Setup dialog for current platform */

#ifndef Q_WS_X11
    mGbTAP->setHidden (true);
#endif

#ifdef Q_WS_MAC
    /* No Host Interface Networking on the Mac yet */
    mGbTAP->setHidden (true);
#endif

#ifdef Q_WS_X11
    /* Setup iconsets */
    mTbSetup_x11->setIcon (VBoxGlobal::iconSet (":/select_file_16px.png",
                                                ":/select_file_dis_16px.png"));
    mTbTerminate_x11->setIcon (VBoxGlobal::iconSet (":/select_file_16px.png",
                                                    ":/select_file_dis_16px.png"));
#endif

    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsNetwork::getFromMachine (const CMachine &aMachine,
                                            QWidget *aPage,
                                            VBoxVMSettingsDlg *aDlg,
                                            const QString &aPath)
{
    /* Creating Page Layout */
    QVBoxLayout *layout = new QVBoxLayout (aPage);
    layout->setContentsMargins (0, 5, 0, 0);

    /* Creating Tab Widget */
    mTwAdapters = new QTabWidget (aPage);
    layout->addWidget (mTwAdapters);

    /* Prepare networks & interfaces lists */
    prepareListNetworks();
// #ifdef Q_WS_WIN
//     prepareListInterfaces();
// #endif

// #ifdef Q_WS_WIN
//     /* Creating Tree Widget */
//     mGbInterfaces = new QGroupBox (aPage);
//     layout->addWidget (mGbInterfaces);
//     QHBoxLayout *iLayout = new QHBoxLayout (mGbInterfaces);
//     mLwInterfaces = new QListWidget (mGbInterfaces);
//     mLwInterfaces->setContextMenuPolicy (Qt::ActionsContextMenu);
//     iLayout->addWidget (mLwInterfaces);
//
//     /* Prepare actions */
//     mAddAction = new QAction (mLwInterfaces);
//     mDelAction = new QAction (mLwInterfaces);
//     mAddAction->setText (tr ("A&dd New Host Interface"));
//     mDelAction->setText (tr ("&Remove Selected Host Interface"));
//     mAddAction->setShortcut (QKeySequence ("Ins"));
//     mDelAction->setShortcut (QKeySequence ("Del"));
//     mAddAction->setToolTip (mAddAction->text().remove ('&') +
//         QString (" (%1)").arg (mAddAction->shortcut().toString()));
//     mDelAction->setToolTip (mDelAction->text().remove ('&') +
//         QString (" (%1)").arg (mDelAction->shortcut().toString()));
//     mAddAction->setWhatsThis (tr ("Adds a new host interface."));
//     mDelAction->setWhatsThis (tr ("Removes the selected host interface."));
//     mAddAction->setIcon (VBoxGlobal::iconSet (":/add_host_iface_16px.png",
//                                               ":/add_host_iface_disabled_16px.png"));
//     mDelAction->setIcon (VBoxGlobal::iconSet (":/remove_host_iface_16px.png",
//                                               ":/remove_host_iface_disabled_16px.png"));
//     connect (mAddAction, SIGNAL (clicked()), this, SLOT (addHostInterface()));
//     connect (mDelAction, SIGNAL (clicked()), this, SLOT (delHostInterface()));
//
//     /* Prepare toolbar */
//     VBoxToolBar *toolBar = new VBoxToolBar (mGbInterfaces);
//     toolBar->setUsesTextLabel (false);
//     toolBar->setUsesBigPixmaps (false);
//     toolBar->setOrientation (Qt::Vertical);
//     toolBar->addAction (mAddAction);
//     toolBar->addAction (mDelAction);
//     iLayout->addWidget (toolBar);
// #endif

    /* Creating Tab Pages */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    ulong count = vbox.GetSystemProperties().GetNetworkAdapterCount();
    for (ulong slot = 0; slot < count; ++ slot)
    {
        /* Get Adapter */
        CNetworkAdapter adapter = aMachine.GetNetworkAdapter (slot);

        /* Creating Adapter's page */
        VBoxVMSettingsNetwork *page = new VBoxVMSettingsNetwork();

        /* Set the mAdapter member which is necessary for next pageTitle call. */
        page->mAdapter = adapter;
        /* Setup validation */
        QIWidgetValidator *wval =
            new QIWidgetValidator (QString ("%1: %2").arg (aPath, page->pageTitle()),
                                   aPage, aDlg);
        connect (wval, SIGNAL (validityChanged (const QIWidgetValidator *)),
                 aDlg, SLOT (enableOk (const QIWidgetValidator*)));
        connect (wval, SIGNAL (isValidRequested (QIWidgetValidator *)),
                 aDlg, SLOT (revalidate (QIWidgetValidator*)));

        page->setValidator (wval);

        /* Loading Adapter's data into page */
        page->loadListNetworks (mListNetworks);
        page->getFromAdapter (adapter);

        /* Attach Adapter's page to Tab Widget */
        mTwAdapters->addTab (page, page->pageTitle());
    }

    layout->addStretch();
}

void VBoxVMSettingsNetwork::putBackToMachine()
{
    for (int index = 0; index < mTwAdapters->count(); ++ index)
    {
        VBoxVMSettingsNetwork *page =
            static_cast<VBoxVMSettingsNetwork*> (mTwAdapters->widget (index));
        if (page)
            page->putBackToAdapter();
    }
}

bool VBoxVMSettingsNetwork::revalidate (QString &aWarning, QString &aTitle)
{
    bool valid = true;

    for (int index = 0; index < mTwAdapters->count(); ++ index)
    {
        VBoxVMSettingsNetwork *page =
            static_cast<VBoxVMSettingsNetwork*> (mTwAdapters->widget (index));
        Assert (page);

        KNetworkAttachmentType type =
            vboxGlobal().toNetworkAttachmentType (page->mCbNAType->currentText());

// #ifdef Q_WS_WIN
//         if (type == KNetworkAttachmentType_HostInterface &&
//             mListInterface.indexOf (mLwInterfaces->currentItem()->text()) == -1)
//         {
//             valid = false;
//             aWarning = tr ("Incorrect host network interface is selected");
//         }
//         else
// #endif
        if (type == KNetworkAttachmentType_Internal &&
            page->mCbNetwork->currentText().isEmpty())
        {
            valid = false;
            aWarning = tr ("Internal network name is not set");
        }

        if (!valid)
        {
            aTitle += ": " +
                mTwAdapters->tabText (mTwAdapters->indexOf (page));
            break;
        }
    }

    return valid;
}

void VBoxVMSettingsNetwork::loadListNetworks (const QStringList &aList)
{
    QString curText = mCbNetwork->currentText();
    mCbNetwork->clear();
    mCbNetwork->clearEditText();
    mCbNetwork->insertItems (0, aList);
    mCbNetwork->setCurrentIndex (mCbNetwork->findText (curText));
    mValidator->revalidate();
}

void VBoxVMSettingsNetwork::getFromAdapter (const CNetworkAdapter &aAdapter)
{
    mAdapter = aAdapter;

    mGbAdapter->setChecked (aAdapter.GetEnabled());

    int aPos = mCbAType->findText (
        vboxGlobal().toString (aAdapter.GetAdapterType()));
    mCbAType->setCurrentIndex (aPos == -1 ? 0 : aPos);

    int naPos = mCbNAType->findText (
        vboxGlobal().toString (aAdapter.GetAttachmentType()));
    mCbNAType->setCurrentIndex (naPos == -1 ? 0 : naPos);
    naTypeChanged (mCbNAType->currentText());

    mLeMAC->setText (aAdapter.GetMACAddress());

    mCbCable->setChecked (aAdapter.GetCableConnected());

    int inPos = mCbNetwork->findText (aAdapter.GetInternalNetwork());
    mCbNetwork->setCurrentIndex (inPos == -1 ? 0 : inPos);

// #ifdef Q_WS_WIN
//     mInterface_win = aAdapter.GetHostInterface();
// #endif
#ifdef Q_WS_X11
    mLeInterface_x11->setText (aAdapter.GetHostInterface());
    mLeSetup_x11->setText (aAdapter.GetTAPSetupApplication());
    mLeTerminate_x11->setText (aAdapter.GetTAPTerminateApplication());
#endif
}

void VBoxVMSettingsNetwork::putBackToAdapter()
{
    mAdapter.SetEnabled (mGbAdapter->isChecked());

    mAdapter.SetAdapterType (vboxGlobal().
        toNetworkAdapterType (mCbAType->currentText()));

    KNetworkAttachmentType type =
        vboxGlobal().toNetworkAttachmentType (mCbNAType->currentText());
    switch (type)
    {
        case KNetworkAttachmentType_Null:
            mAdapter.Detach();
            break;
        case KNetworkAttachmentType_NAT:
            mAdapter.AttachToNAT();
            break;
        case KNetworkAttachmentType_HostInterface:
            mAdapter.AttachToHostInterface();
            break;
        case KNetworkAttachmentType_Internal:
            mAdapter.AttachToInternalNetwork();
            break;
        default:
            AssertMsgFailed (("Invalid network attachment type: %d", type));
            break;
    }

    mAdapter.SetMACAddress (mLeMAC->text());

    mAdapter.SetCableConnected (mCbCable->isChecked());

    if (type == KNetworkAttachmentType_HostInterface)
    {
// #ifdef Q_WS_WIN
//         mAdapter.SetHostInterface (mInterface_win);
// #endif
#ifdef Q_WS_X11
        QString iface = mLeInterface_x11->text();
        mAdapter.SetHostInterface (iface.isEmpty() ? QString::null : iface);
        QString setup = mLeSetup_x11->text();
        mAdapter.SetTAPSetupApplication (setup.isEmpty() ? QString::null : setup);
        QString term = mLeTerminate_x11->text();
        mAdapter.SetTAPTerminateApplication (term.isEmpty() ? QString::null : term);
#endif
    }
    else if (type == KNetworkAttachmentType_Internal)
        mAdapter.SetInternalNetwork (mCbNetwork->currentText());
}

void VBoxVMSettingsNetwork::setValidator (QIWidgetValidator *aValidator)
{
    mValidator = aValidator;

    connect (mGbAdapter, SIGNAL (toggled (bool)),
             this, SLOT (adapterToggled (bool)));
    connect (mCbNAType, SIGNAL (activated (const QString&)),
             this, SLOT (naTypeChanged (const QString&)));
    connect (mCbNetwork, SIGNAL (activated (const QString&)),
             mValidator, SLOT (revalidate()));
    connect (mCbNetwork, SIGNAL (editTextChanged (const QString&)),
             this, SLOT (updateListNetworks()));
    connect (mPbMAC, SIGNAL (clicked()), this, SLOT (genMACClicked()));
#ifdef Q_WS_X11
    connect (mTbSetup_x11, SIGNAL (clicked()), this, SLOT (tapSetupClicked()));
    connect (mTbTerminate_x11, SIGNAL (clicked()), this, SLOT (tapTerminateClicked()));
#endif

    mValidator->revalidate();
}


void VBoxVMSettingsNetwork::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsNetwork::retranslateUi (this);

    mTwAdapters->setTabText (mTwAdapters->indexOf (this), pageTitle());

    prepareComboboxes();
}


void VBoxVMSettingsNetwork::naTypeChanged (const QString &aString)
{
    bool enableIntNet = vboxGlobal().toNetworkAttachmentType (aString) ==
                        KNetworkAttachmentType_Internal;
    bool enableHostIf = vboxGlobal().toNetworkAttachmentType (aString) ==
                        KNetworkAttachmentType_HostInterface;
    mLbNetwork->setEnabled (enableIntNet);
    mCbNetwork->setEnabled (enableIntNet);
#ifdef Q_WS_X11
    mGbTAP->setEnabled (enableHostIf);
#endif
    mValidator->revalidate();
}

void VBoxVMSettingsNetwork::genMACClicked()
{
    mAdapter.SetMACAddress (QString::null);
    mLeMAC->setText (mAdapter.GetMACAddress());
}

#ifdef Q_WS_X11
void VBoxVMSettingsNetwork::tapSetupClicked()
{
    QString selected = QFileDialog::getOpenFileName (
        this, tr ("Select TAP setup application"), "/");

    if (!selected.isEmpty())
        mLeSetup_x11->setText (selected);
}

void VBoxVMSettingsNetwork::tapTerminateClicked()
{
    QString selected = QFileDialog::getOpenFileName (
        this, tr ("Select TAP terminate application"), "/");

    if (!selected.isEmpty())
        mLeTerminate_x11->setText (selected);
}
#endif

void VBoxVMSettingsNetwork::adapterToggled (bool aOn)
{
    if (!aOn)
    {
        mCbNAType->setCurrentIndex (0);
        naTypeChanged (mCbNAType->currentText());
    }
    mValidator->revalidate();
}

void VBoxVMSettingsNetwork::updateListNetworks()
{
    if (mLockNetworkListUpdate)
        return;
    mLockNetworkListUpdate = true;

    QStringList curList (mListNetworks);
    for (int index = 0; index < mTwAdapters->count(); ++ index)
    {
        VBoxVMSettingsNetwork *page =
            static_cast<VBoxVMSettingsNetwork*> (mTwAdapters->widget (index));
        if (page)
        {
            QString curText = page->mCbNetwork->currentText();
            if (!curText.isEmpty() && !curList.contains (curText))
                curList << curText;
        }
    }

    for (int index = 0; index < mTwAdapters->count(); ++ index)
    {
        VBoxVMSettingsNetwork *page =
            static_cast<VBoxVMSettingsNetwork*> (mTwAdapters->widget (index));
        if (page)
            page->loadListNetworks (curList);
    }

    mLockNetworkListUpdate = false;
}

// #ifdef Q_WS_WIN
// void VBoxVMSettingsNetwork::addHostInterface()
// {
//     /* Allow the started helper process to make itself the foreground window */
//     AllowSetForegroundWindow (ASFW_ANY);
//
//     /* Search for the max available interface index */
//     int ifaceNumber = 0;
//     QString ifaceName = tr ("VirtualBox Host Interface %1");
//     QRegExp regExp (QString ("^") + ifaceName.arg ("([0-9]+)") + QString ("$"));
//     for (int index = 0; index < mLwInterfaces->count(); ++ index)
//     {
//         QString iface = mLwInterfaces->item (index)->text();
//         int pos = regExp.indexIn (iface);
//         if (pos != -1)
//             ifaceNumber = regExp.cap (1).toInt() > ifaceNumber ?
//                           regExp.cap (1).toInt() : ifaceNumber;
//     }
//
//     /* Creating add host interface dialog */
//     VBoxAddNIDialog dlg (this, ifaceName.arg (++ ifaceNumber));
//     if (dlg.exec() != QDialog::Accepted)
//         return;
//     QString iName = dlg.getName();
//
//     /* Create interface */
//     CHost host = vboxGlobal().virtualBox().GetHost();
//     CHostNetworkInterface iFace;
//     CProgress progress = host.CreateHostNetworkInterface (iName, iFace);
//     if (host.isOk())
//     {
//         vboxProblem().showModalProgressDialog (progress, iName, this);
//         if (progress.GetResultCode() == 0)
//         {
//             /* Add&Select newly created interface */
//             delete findItem (mLwInterfaces, mNoInterfaces);
//             mLwInterfaces->addItem (iName);
//             mListInterface += iName;
//             mLwInterfaces->setCurrentItem (mLwInterfaces->count() - 1);
//             /* Enable interface delete button */
//             mTbRemInterface->setEnabled (true);
//         }
//         else
//             vboxProblem().cannotCreateHostInterface (progress, iName, this);
//     }
//     else
//         vboxProblem().cannotCreateHostInterface (host, iName, this);
//
//     /* Allow the started helper process to make itself the foreground window */
//     AllowSetForegroundWindow (ASFW_ANY);
// }
//
// void VBoxVMSettingsNetwork::delHostInterface()
// {
//     /* Allow the started helper process to make itself the foreground window */
//     AllowSetForegroundWindow (ASFW_ANY);
//
//     /* Check interface name */
//     QString iName = mLwInterfaces->currentItem()->text();
//     if (iName.isEmpty())
//         return;
//
//     /* Asking user about deleting selected network interface */
//     int delNetIface = vboxProblem().message (this, VBoxProblemReporter::Question,
//         tr ("<p>Do you want to remove the selected host network interface "
//             "<nobr><b>%1</b>?</nobr></p>"
//             "<p><b>Note:</b> This interface may be in use by one or more "
//             "network adapters of this or another VM. After it is removed, these "
//             "adapters will no longer work until you correct their settings by "
//             "either choosing a different interface name or a different adapter "
//             "attachment type.</p>").arg (iName),
//         0, /* autoConfirmId */
//         QIMessageBox::Ok | QIMessageBox::Default,
//         QIMessageBox::Cancel | QIMessageBox::Escape);
//     if (delNetIface == QIMessageBox::Cancel)
//         return;
//
//     CHost host = vboxGlobal().virtualBox().GetHost();
//     CHostNetworkInterface iFace = host.GetNetworkInterfaces().FindByName (iName);
//     if (host.isOk())
//     {
//         /* Delete interface */
//         CProgress progress = host.RemoveHostNetworkInterface (iFace.GetId(), iFace);
//         if (host.isOk())
//         {
//             vboxProblem().showModalProgressDialog (progress, iName, this);
//             if (progress.GetResultCode() == 0)
//             {
//                 if (mLwInterfaces->count() == 1)
//                 {
//                     mLwInterfaces->addItem (mNoInterfaces);
//                     /* Disable interface delete button */
//                     mTbRemInterface->setEnabled (false);
//                 }
//                 delete findItem (mLwInterfaces, iName);
//                 mLwInterfaces->currentItem()->setSelected (true);
//                 mListInterface.erase (mListInterface.find (iName));
//             }
//             else
//                 vboxProblem().cannotRemoveHostInterface (progress, iFace, this);
//         }
//     }
//
//     if (!host.isOk())
//         vboxProblem().cannotRemoveHostInterface (host, iFace, this);
// }
// #endif

QString VBoxVMSettingsNetwork::pageTitle() const
{
    QString pageTitle;
    if (!mAdapter.isNull())
    {
        pageTitle = QString (tr ("Adapter %1", "network"))
            .arg (mAdapter.GetSlot());
    }
    return pageTitle;
}

void VBoxVMSettingsNetwork::prepareComboboxes()
{
    /* Save the current selected value */
    int currentAdapter = mCbAType->currentIndex();
    /* Clear the driver box */
    mCbAType->clear();
    /* Refill them */
    mCbAType->insertItem (0,
        vboxGlobal().toString (KNetworkAdapterType_Am79C970A));
    mCbAType->insertItem (1,
        vboxGlobal().toString (KNetworkAdapterType_Am79C973));
#ifdef VBOX_WITH_E1000
    mCbAType->insertItem (2,
        vboxGlobal().toString (KNetworkAdapterType_I82540EM));
    mCbAType->insertItem (3,
        vboxGlobal().toString (KNetworkAdapterType_I82543GC));
#endif
    /* Set the old value */
    mCbAType->setCurrentIndex (currentAdapter);

    /* Save the current selected value */
    int currentAttachment = mCbNAType->currentIndex();
    /* Clear the driver box */
    mCbNAType->clear();
    /* Refill them */
    mCbNAType->insertItem (0,
        vboxGlobal().toString (KNetworkAttachmentType_Null));
    mCbNAType->insertItem (1,
        vboxGlobal().toString (KNetworkAttachmentType_NAT));
#ifndef Q_WS_MAC /* Not yet on the Mac */
    mCbNAType->insertItem (2,
        vboxGlobal().toString (KNetworkAttachmentType_HostInterface));
    mCbNAType->insertItem (3,
        vboxGlobal().toString (KNetworkAttachmentType_Internal));
#endif
    /* Set the old value */
    mCbNAType->setCurrentIndex (currentAttachment);
}

void VBoxVMSettingsNetwork::prepareListNetworks()
{
    /* Clear inner list */
    mListNetworks.clear();

    /* Load internal network list */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    ulong count = vbox.GetSystemProperties().GetNetworkAdapterCount();
    CMachineVector vec =  vbox.GetMachines2();
    for (CMachineVector::ConstIterator m = vec.begin();
         m != vec.end(); ++ m)
    {
        if (m->GetAccessible())
        {
            for (ulong slot = 0; slot < count; ++ slot)
            {
                CNetworkAdapter adapter = m->GetNetworkAdapter (slot);
                if (adapter.GetAttachmentType() == KNetworkAttachmentType_Internal &&
                    !mListNetworks.contains (adapter.GetInternalNetwork()))
                    mListNetworks << adapter.GetInternalNetwork();
            }
        }
    }

    /* Allow further Network List update */
    mLockNetworkListUpdate = false;
}

// #ifdef Q_WS_WIN
// void VBoxVMSettingsNetwork::prepareListInterfaces()
// {
//     /* Clear inner list */
//     mListInterface.clear();
//
//     /* Load current inner list */
//     CHostNetworkInterfaceEnumerator en =
//         vboxGlobal().virtualBox().GetHost().GetNetworkInterfaces().Enumerate();
//     while (en.HasMore())
//         mListInterface += en.GetNext().GetName();
//
//     /* Save current list item name */
//     QString currentListItemName = mLwInterfaces->currentText();
//
//     /* Load current list items */
//     mLwInterfaces->clear();
//     if (mListInterface.count())
//         mLwInterfaces->insertItems (0, mListInterface);
//     else
//         mLwInterfaces->insertItem (0, mNoInterfaces);
//
//     /* Select current list item */
//     QListWidgetItem *item = findItem (mLwInterfaces, currentListItemName);
//     mLwInterfaces->setCurrentItem (item ? item : mLwInterfaces->item (0));
//
//     /* Enable/Disable interface delete button */
//     mDelAction->setEnabled (!mListInterface.isEmpty());
// }
// #endif

