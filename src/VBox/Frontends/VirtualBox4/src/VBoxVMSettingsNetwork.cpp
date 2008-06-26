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

/* Common Includes */
#include "VBoxVMSettingsNetwork.h"
#include "VBoxVMSettingsDlg.h"
#include "QIWidgetValidator.h"
#include "VBoxGlobal.h"
#ifdef Q_WS_WIN
#include "VBoxToolBar.h"
#include "VBoxVMSettingsUtils.h"
#include "VBoxProblemReporter.h"
#endif

/* Qt Includes */
#ifdef Q_WS_X11
#include <QFileDialog>
#endif
#ifdef Q_WS_WIN
#include <QTreeWidget>
#endif

/* Common Stuff */
#ifdef Q_WS_WIN
static QTreeWidgetItem* findItem (QTreeWidget *aList,
                                  const QString &aMatch)
{
    QList<QTreeWidgetItem*> list =
        aList->findItems (aMatch, Qt::MatchExactly, 0);
    return list.count() ? list [0] : 0;
}
#endif


/* VBoxVMSettingsNetwork Stuff */
VBoxVMSettingsNetwork::VBoxVMSettingsNetwork()
    : QIWithRetranslateUI<QWidget> (0)
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

#ifdef Q_WS_WIN
    mInterfaceName = aAdapter.GetHostInterface().isEmpty() ?
                     QString::null : aAdapter.GetHostInterface();
#endif
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
#ifdef Q_WS_WIN
        mAdapter.SetHostInterface (mInterfaceName);
#endif
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

QString VBoxVMSettingsNetwork::pageTitle() const
{
    QString pageTitle;
    if (!mAdapter.isNull())
    {
        pageTitle = VBoxGlobal::tr ("Adapter %1", "network")
            .arg (mAdapter.GetSlot());
    }
    return pageTitle;
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
    connect (mPbMAC, SIGNAL (clicked()), this, SLOT (genMACClicked()));
#ifdef Q_WS_X11
    connect (mTbSetup_x11, SIGNAL (clicked()), this, SLOT (tapSetupClicked()));
    connect (mTbTerminate_x11, SIGNAL (clicked()), this, SLOT (tapTerminateClicked()));
#endif

    mValidator->revalidate();
}

void VBoxVMSettingsNetwork::setNetworksList (const QStringList &aList)
{
    QString curText = mCbNetwork->currentText();
    mCbNetwork->clear();
    mCbNetwork->clearEditText();
    mCbNetwork->insertItems (0, aList);
    mCbNetwork->setCurrentIndex (mCbNetwork->findText (curText));
    if (mValidator)
        mValidator->revalidate();
}

#ifdef Q_WS_WIN
void VBoxVMSettingsNetwork::setInterfaceName (const QString &aName)
{
    mInterfaceName = aName;
    if (mValidator)
        mValidator->revalidate();
}

QString VBoxVMSettingsNetwork::interfaceName() const
{
    return mInterfaceName;
}
#endif

void VBoxVMSettingsNetwork::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsNetwork::retranslateUi (this);

    prepareComboboxes();
}

void VBoxVMSettingsNetwork::adapterToggled (bool aOn)
{
    if (!aOn)
    {
        mCbNAType->setCurrentIndex (0);
        naTypeChanged (mCbNAType->currentText());
    }
    if (mValidator)
        mValidator->revalidate();
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
    if (mValidator)
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


/* VBoxNIList Stuff */
#ifdef Q_WS_WIN
class VBoxNIListItem : public QTreeWidgetItem
{
public:

    enum { typeId = QTreeWidgetItem::UserType + 1 };

    VBoxNIListItem (QTreeWidget *aParent, const QString &aName, bool aWrong = false)
        : QTreeWidgetItem (aParent, QStringList (aName), typeId)
        , mWrong (aWrong)
    {
        init();
    }

    VBoxNIListItem (const QString &aName, bool aWrong = false)
        : QTreeWidgetItem (QStringList (aName), typeId)
        , mWrong (aWrong)
    {
        init();
    }

    void init()
    {
        /* Force the fake item's font to italic */
        if (mWrong)
        {
            QFont fnt = font (0);
            fnt.setItalic (true);
            setFont (0, fnt);
        }
    }

    bool isWrong() { return mWrong; }

private:

    bool mWrong;
};

VBoxNIList::VBoxNIList (QWidget *aParent)
    : QIWithRetranslateUI<QGroupBox> (aParent)
{
    /* Creating List Widget */
    QHBoxLayout *layout = new QHBoxLayout (this);
    mList = new QTreeWidget (this);
    mList->setColumnCount (1);
    mList->header()->hide();
    mList->setContextMenuPolicy (Qt::ActionsContextMenu);
    mList->setRootIsDecorated (false);
    layout->addWidget (mList);

    /* Prepare actions */
    mAddAction = new QAction (mList);
    mDelAction = new QAction (mList);
    mList->addAction (mAddAction);
    mList->addAction (mDelAction);
    mAddAction->setShortcut (QKeySequence ("Ins"));
    mDelAction->setShortcut (QKeySequence ("Del"));
    mAddAction->setIcon (VBoxGlobal::iconSet (":/add_host_iface_16px.png",
                                              ":/add_host_iface_disabled_16px.png"));
    mDelAction->setIcon (VBoxGlobal::iconSet (":/remove_host_iface_16px.png",
                                              ":/remove_host_iface_disabled_16px.png"));

    /* Prepare toolbar */
    VBoxToolBar *toolBar = new VBoxToolBar (this);
    toolBar->setUsesTextLabel (false);
    toolBar->setUsesBigPixmaps (false);
    toolBar->setOrientation (Qt::Vertical);
    toolBar->addAction (mAddAction);
    toolBar->addAction (mDelAction);
    layout->addWidget (toolBar);

    /* Setup connections */
    connect (mList, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (onCurrentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)));
    connect (mAddAction, SIGNAL (triggered (bool)),
             this, SLOT (addHostInterface()));
    connect (mDelAction, SIGNAL (triggered (bool)),
             this, SLOT (delHostInterface()));

    /* Populating interface list */
    populateInterfacesList();

    /* Applying language settings */
    retranslateUi();
}

bool VBoxNIList::isWrongInterface() const
{
    return !mList->currentItem();
}

void VBoxNIList::setCurrentInterface (const QString &aName)
{
    /* Remove wrong item if present */
    for (int i = 0; i < mList->topLevelItemCount(); ++ i)
    {
        VBoxNIListItem *item =
            mList->topLevelItem (i)->type() == VBoxNIListItem::typeId ?
            static_cast<VBoxNIListItem*> (mList->topLevelItem (i)) : 0;
        Assert (item);
        if (item && item->isWrong())
            delete item;
    }

    if (aName.isEmpty())
    {
        /* Make sure no one of items selected in the list currently */
        mList->setCurrentItem (0);
        mList->clearSelection();
    }
    else
    {
        /* Search for the required item */
        QTreeWidgetItem *querryItem = findItem (mList, aName);

        /* Create fake required item if real is not present */
        if (!querryItem)
            querryItem = new VBoxNIListItem (mList, aName, true);

        /* Set the required item to be the current */
        mList->setCurrentItem (querryItem);
    }
}

void VBoxNIList::onCurrentItemChanged (QTreeWidgetItem *aCurrent,
                                       QTreeWidgetItem *)
{
    VBoxNIListItem *item = aCurrent &&
        aCurrent->type() == VBoxNIListItem::typeId ?
        static_cast<VBoxNIListItem*> (aCurrent) : 0;
    mDelAction->setEnabled (item && !item->isWrong());

    emit currentInterfaceChanged (mList->currentItem() ?
        mList->currentItem()->text (0) : QString::null);
}

void VBoxNIList::addHostInterface()
{
    /* Allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);

    /* Search for the max available interface index */
    int ifaceNumber = 0;
    QString ifaceName = tr ("VirtualBox Host Interface %1");
    QRegExp regExp (QString ("^") + ifaceName.arg ("([0-9]+)") + QString ("$"));
    for (int index = 0; index < mList->topLevelItemCount(); ++ index)
    {
        QString iface = mList->topLevelItem (index)->text (0);
        int pos = regExp.indexIn (iface);
        if (pos != -1)
            ifaceNumber = regExp.cap (1).toInt() > ifaceNumber ?
                          regExp.cap (1).toInt() : ifaceNumber;
    }

    /* Creating add host interface dialog */
    VBoxAddNIDialog dlg (this, ifaceName.arg (++ ifaceNumber));
    if (dlg.exec() != QDialog::Accepted)
        return;
    QString iName = dlg.getName();

    /* Create interface */
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostNetworkInterface iFace;
    CProgress progress = host.CreateHostNetworkInterface (iName, iFace);
    if (host.isOk())
    {
        vboxProblem().showModalProgressDialog (progress, iName, this);
        /* Add&Select newly created interface */
        if (progress.GetResultCode() == 0)
            new VBoxNIListItem (mList, iName);
        else
            vboxProblem().cannotCreateHostInterface (progress, iName, this);
    }
    else
        vboxProblem().cannotCreateHostInterface (host, iName, this);

    emit listChanged();

    /* Allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);
}

void VBoxNIList::delHostInterface()
{
    Assert (mList->currentItem());

    /* Allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);

    /* Check interface name */
    QString iName = mList->currentItem()->text (0);
    if (iName.isEmpty())
        return;

    /* Asking user about deleting selected network interface */
    int delNetIface = vboxProblem().message (this, VBoxProblemReporter::Question,
        tr ("<p>Do you want to remove the selected host network interface "
            "<nobr><b>%1</b>?</nobr></p>"
            "<p><b>Note:</b> This interface may be in use by one or more "
            "network adapters of this or another VM. After it is removed, these "
            "adapters will no longer work until you correct their settings by "
            "either choosing a different interface name or a different adapter "
            "attachment type.</p>").arg (iName),
        0, /* autoConfirmId */
        QIMessageBox::Ok | QIMessageBox::Default,
        QIMessageBox::Cancel | QIMessageBox::Escape);
    if (delNetIface == QIMessageBox::Cancel)
        return;

    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostNetworkInterface iFace = host.GetNetworkInterfaces().FindByName (iName);
    if (host.isOk())
    {
        /* Delete interface */
        CProgress progress = host.RemoveHostNetworkInterface (iFace.GetId(), iFace);
        if (host.isOk())
        {
            vboxProblem().showModalProgressDialog (progress, iName, this);
            if (progress.GetResultCode() == 0)
                delete findItem (mList, iName);
            else
                vboxProblem().cannotRemoveHostInterface (progress, iFace, this);
        }
    }

    if (!host.isOk())
        vboxProblem().cannotRemoveHostInterface (host, iFace, this);

    emit listChanged();
}

void VBoxNIList::retranslateUi()
{
    setTitle (tr ("Host &Interfaces"));

    mList->setWhatsThis (tr ("Lists all available host interfaces."));

    mAddAction->setText (tr ("A&dd New Host Interface"));
    mDelAction->setText (tr ("&Remove Selected Host Interface"));
    mAddAction->setWhatsThis (tr ("Adds a new host interface."));
    mDelAction->setWhatsThis (tr ("Removes the selected host interface."));
    mAddAction->setToolTip (mAddAction->text().remove ('&') +
        QString (" (%1)").arg (mAddAction->shortcut().toString()));
    mDelAction->setToolTip (mDelAction->text().remove ('&') +
        QString (" (%1)").arg (mDelAction->shortcut().toString()));
}

void VBoxNIList::populateInterfacesList()
{
    /* Load current inner list */
    QList<QTreeWidgetItem*> itemsList;
    CHostNetworkInterfaceEnumerator en =
        vboxGlobal().virtualBox().GetHost().GetNetworkInterfaces().Enumerate();
    while (en.HasMore())
        itemsList << new VBoxNIListItem (en.GetNext().GetName());

    /* Save current list item name */
    QString currentListItemName = mList->currentItem() ?
        mList->currentItem()->text (0) : QString::null;

    /* Load current list items */
    mList->clear();
    if (!itemsList.isEmpty())
        mList->insertTopLevelItems (0, itemsList);

    /* Select current list item */
    QTreeWidgetItem *item = findItem (mList, currentListItemName);
    mList->setCurrentItem (item ? item : mList->topLevelItem (0));

    /* Update item's state */
    onCurrentItemChanged (mList->currentItem());
}
#endif


/* VBoxVMSettingsNetworkPage Stuff */
VBoxVMSettingsNetworkPage* VBoxVMSettingsNetworkPage::mSettings = 0;

void VBoxVMSettingsNetworkPage::getFromMachine (const CMachine &aMachine,
                                                QWidget *aPage,
                                                VBoxVMSettingsDlg *aDlg,
                                                const QString &aPath)
{
    mSettings = new VBoxVMSettingsNetworkPage (aPage);
    QVBoxLayout *layout = new QVBoxLayout (aPage);
    layout->setContentsMargins (0, 5, 0, 0);
    layout->addWidget (mSettings);
    mSettings->getFrom (aMachine, aDlg, aPath);
}

void VBoxVMSettingsNetworkPage::putBackToMachine()
{
    mSettings->putBackTo();
}

bool VBoxVMSettingsNetworkPage::revalidate (QString &aWarning,
                                            QString &aTitle)
{
    return mSettings->validate (aWarning, aTitle);
}

VBoxVMSettingsNetworkPage::VBoxVMSettingsNetworkPage (QWidget *aParent)
    : QIWithRetranslateUI<QWidget> (aParent)
    , mLockNetworkListUpdate (true)
{
    QVBoxLayout *layout = new QVBoxLayout (this);
    layout->setContentsMargins (0, 0, 0, 0);

    /* Creating Tab Widget */
    mTwAdapters = new QTabWidget (aParent);
    layout->addWidget (mTwAdapters);
    /* Prepare Networks Lists */
    populateNetworksList();

#ifdef Q_WS_WIN
    /* Creating Interfaces List */
    mNIList = new VBoxNIList (aParent);
    layout->addWidget (mNIList);
#else
    layout->addStretch();
#endif
}

void VBoxVMSettingsNetworkPage::getFrom (const CMachine &aMachine,
                                         VBoxVMSettingsDlg *aDlg,
                                         const QString &aPath)
{
    /* Creating Tab Pages */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    ulong count = vbox.GetSystemProperties().GetNetworkAdapterCount();
    for (ulong slot = 0; slot < count; ++ slot)
    {
        /* Get Adapter */
        CNetworkAdapter adapter = aMachine.GetNetworkAdapter (slot);

        /* Creating Adapter's page */
        VBoxVMSettingsNetwork *page = new VBoxVMSettingsNetwork();

        /* Loading Adapter's data into page */
        page->setNetworksList (mListNetworks);
        page->getFromAdapter (adapter);

        /* Attach Adapter's page to Tab Widget */
        mTwAdapters->addTab (page, page->pageTitle());

        /* Setup validation */
        QIWidgetValidator *wval =
            new QIWidgetValidator (QString ("%1: %2").arg (aPath, page->pageTitle()),
                                   (QWidget*)parent(), aDlg);
        connect (wval, SIGNAL (validityChanged (const QIWidgetValidator *)),
                 aDlg, SLOT (enableOk (const QIWidgetValidator*)));
        connect (wval, SIGNAL (isValidRequested (QIWidgetValidator *)),
                 aDlg, SLOT (revalidate (QIWidgetValidator*)));
        connect (page->mCbNetwork, SIGNAL (editTextChanged (const QString&)),
                 this, SLOT (updateNetworksList()));
#ifdef Q_WS_WIN
        connect (mNIList, SIGNAL (listChanged()), wval, SLOT (revalidate()));
#endif
        page->setValidator (wval);
    }

#ifdef Q_WS_WIN
    connect (mTwAdapters, SIGNAL (currentChanged (int)),
             this, SLOT (onCurrentPageChanged (int)));
    connect (mNIList, SIGNAL (currentInterfaceChanged (const QString &)),
             this, SLOT (onCurrentInterfaceChanged (const QString &)));
    onCurrentPageChanged (mTwAdapters->currentIndex());
#endif

    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsNetworkPage::putBackTo()
{
    for (int i = 0; i < mTwAdapters->count(); ++ i)
    {
        VBoxVMSettingsNetwork *page = static_cast<VBoxVMSettingsNetwork*>
            (mTwAdapters->widget (i));
        if (page)
            page->putBackToAdapter();
    }
}

bool VBoxVMSettingsNetworkPage::validate (QString &aWarning,
                                          QString &aTitle)
{
    bool valid = true;

    for (int i = 0; i < mTwAdapters->count(); ++ i)
    {
        VBoxVMSettingsNetwork *page = static_cast<VBoxVMSettingsNetwork*>
            (mTwAdapters->widget (i));
        Assert (page);

        KNetworkAttachmentType type =
            vboxGlobal().toNetworkAttachmentType (page->mCbNAType->currentText());

#ifdef Q_WS_WIN
        if (type == KNetworkAttachmentType_HostInterface &&
            page->interfaceName().isNull())
        {
            valid = false;
            aWarning = tr ("No host network interface is selected");
        } else
#endif
        if (type == KNetworkAttachmentType_Internal &&
            page->mCbNetwork->currentText().isEmpty())
        {
            valid = false;
            aWarning = tr ("Internal network name is not set");
        }

        if (!valid)
        {
            aTitle += ": " + page->pageTitle();
            break;
        }
    }

    return valid;
}

void VBoxVMSettingsNetworkPage::retranslateUi()
{
    for (int i = 0; i < mTwAdapters->count(); ++ i)
    {
        VBoxVMSettingsNetwork *page =
            static_cast<VBoxVMSettingsNetwork*> (mTwAdapters->widget (i));
        mTwAdapters->setTabText (i, page->pageTitle());
    }
}

void VBoxVMSettingsNetworkPage::updateNetworksList()
{
    /* Lock to prevent few simultaneously update */
    if (mLockNetworkListUpdate)
        return;
    mLockNetworkListUpdate = true;

    /* Compose latest list from current + new on all pages */
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

    /* Load latest list to all pages */
    for (int index = 0; index < mTwAdapters->count(); ++ index)
    {
        VBoxVMSettingsNetwork *page =
            static_cast<VBoxVMSettingsNetwork*> (mTwAdapters->widget (index));
        if (page)
            page->setNetworksList (curList);
    }

    /* Allow further Network List update */
    mLockNetworkListUpdate = false;
}

#ifdef Q_WS_WIN
void VBoxVMSettingsNetworkPage::onCurrentPageChanged (int aIndex)
{
    VBoxVMSettingsNetwork *page =
        static_cast<VBoxVMSettingsNetwork*> (mTwAdapters->widget (aIndex));
    Assert (page);
    mNIList->setCurrentInterface (page->interfaceName());
}

void VBoxVMSettingsNetworkPage::onCurrentInterfaceChanged (const QString &aName)
{
    VBoxVMSettingsNetwork *page =
        static_cast<VBoxVMSettingsNetwork*> (mTwAdapters->currentWidget());
    Assert (page);
    page->setInterfaceName (aName);
}
#endif

void VBoxVMSettingsNetworkPage::populateNetworksList()
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

