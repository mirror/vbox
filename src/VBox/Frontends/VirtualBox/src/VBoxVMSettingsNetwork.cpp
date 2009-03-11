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
#include "QIWidgetValidator.h"
#include "VBoxGlobal.h"
#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
#include "VBoxToolBar.h"
#include "VBoxSettingsUtils.h"
#include "VBoxProblemReporter.h"
#endif

/* Qt Includes */
#if defined (Q_WS_X11) && !defined (VBOX_WITH_NETFLT)
#include <QFileDialog>
#endif
#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
#include <QTreeWidget>
#endif

/* Common Stuff */
#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
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
#if !defined (Q_WS_X11) || defined (VBOX_WITH_NETFLT)
    setTapVisible (false);
#endif

    /* Applying language settings */
    retranslateUi();

#ifdef Q_WS_MAC
    /* Prevent this widgets to go in the Small/Mini size state which is
     * available on Mac OS X. Not sure why this happens but this seems to help
     * against. */
    QList<QWidget*> list = findChildren<QWidget*>();
    foreach (QWidget *w, list)
        if (w->parent() == this)
            w->setFixedHeight (w->sizeHint().height());
#endif /* Q_WS_MAC */
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

#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
    mInterfaceName = aAdapter.GetHostInterface().isEmpty() ?
                     QString::null : aAdapter.GetHostInterface();
#endif
#if defined (Q_WS_X11) && !defined (VBOX_WITH_NETFLT)
    mLeInterface_x11->setText (aAdapter.GetHostInterface());
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
        case KNetworkAttachmentType_Bridged:
            mAdapter.AttachToBridgedInterface();
            break;
        case KNetworkAttachmentType_Internal:
            mAdapter.AttachToInternalNetwork();
            break;
        case KNetworkAttachmentType_HostOnly:
            mAdapter.AttachToHostOnlyInterface();
            break;
        default:
            AssertMsgFailed (("Invalid network attachment type: %d", type));
            break;
    }

    mAdapter.SetMACAddress (mLeMAC->text());

    mAdapter.SetCableConnected (mCbCable->isChecked());

    if (type == KNetworkAttachmentType_Bridged
#if defined (Q_WS_WIN) && defined (VBOX_WITH_NETFLT)
            || type == KNetworkAttachmentType_HostOnly
#endif
            )
    {
#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
        mAdapter.SetHostInterface (mInterfaceName);
#endif
#if defined (Q_WS_X11) && !defined (VBOX_WITH_NETFLT)
        QString iface = mLeInterface_x11->text();
        mAdapter.SetHostInterface (iface.isEmpty() ? QString::null : iface);
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
            .arg (QString ("&%1").arg (mAdapter.GetSlot() + 1));
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

    mValidator->revalidate();
}

QWidget* VBoxVMSettingsNetwork::setOrderAfter (QWidget *aAfter)
{
    setTabOrder (aAfter, mGbAdapter);
    setTabOrder (mGbAdapter, mCbAType);
    setTabOrder (mCbAType, mCbNAType);
    setTabOrder (mCbNAType, mCbNetwork);
    setTabOrder (mCbNetwork, mLeMAC);
    setTabOrder (mLeMAC, mPbMAC);
    setTabOrder (mPbMAC, mCbCable);
    setTabOrder (mCbCable, mLeInterface_x11);
    return mLeInterface_x11;
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

#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
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
    mLbNetwork->setEnabled (enableIntNet);
    mCbNetwork->setEnabled (enableIntNet);
#if defined (Q_WS_X11) && !defined (VBOX_WITH_NETFLT)
    bool enableHostIf = vboxGlobal().toNetworkAttachmentType (aString) ==
                        KNetworkAttachmentType_Bridged;
    setTapEnabled (enableHostIf);
#endif
    if (mValidator)
        mValidator->revalidate();
}

void VBoxVMSettingsNetwork::genMACClicked()
{
    mAdapter.SetMACAddress (QString::null);
    mLeMAC->setText (mAdapter.GetMACAddress());
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
    mCbAType->setItemData (0,
        mCbAType->itemText(0), Qt::ToolTipRole);
    mCbAType->insertItem (1,
        vboxGlobal().toString (KNetworkAdapterType_Am79C973));
    mCbAType->setItemData (1,
        mCbAType->itemText(1), Qt::ToolTipRole);
#ifdef VBOX_WITH_E1000
    mCbAType->insertItem (2,
        vboxGlobal().toString (KNetworkAdapterType_I82540EM));
    mCbAType->setItemData (2,
        mCbAType->itemText(2), Qt::ToolTipRole);
    mCbAType->insertItem (3,
        vboxGlobal().toString (KNetworkAdapterType_I82543GC));
    mCbAType->setItemData (3,
        mCbAType->itemText(3), Qt::ToolTipRole);
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
    mCbNAType->setItemData (0,
        mCbNAType->itemText(0), Qt::ToolTipRole);
    mCbNAType->insertItem (1,
        vboxGlobal().toString (KNetworkAttachmentType_NAT));
    mCbNAType->setItemData (1,
        mCbNAType->itemText(1), Qt::ToolTipRole);
    mCbNAType->insertItem (2,
        vboxGlobal().toString (KNetworkAttachmentType_Bridged));
    mCbNAType->setItemData (2,
        mCbNAType->itemText(2), Qt::ToolTipRole);
    mCbNAType->insertItem (3,
        vboxGlobal().toString (KNetworkAttachmentType_Internal));
    mCbNAType->setItemData (3,
        mCbNAType->itemText(3), Qt::ToolTipRole);
#if defined(RT_OS_LINUX) || defined (RT_OS_WINDOWS)
    mCbNAType->insertItem (4,
        vboxGlobal().toString (KNetworkAttachmentType_HostOnly));
    mCbNAType->setItemData (4,
        mCbNAType->itemText(4), Qt::ToolTipRole);
#endif /* RT_OS_LINUX */
    /* Set the old value */
    mCbNAType->setCurrentIndex (currentAttachment);
}

void VBoxVMSettingsNetwork::setTapEnabled (bool aEnabled)
{
    mGbTap->setEnabled (aEnabled);
    mLbInterface_x11->setEnabled (aEnabled);
    mLeInterface_x11->setEnabled (aEnabled);
}

void VBoxVMSettingsNetwork::setTapVisible (bool aVisible)
{
    mGbTap->setVisible (aVisible);
    mLbInterface_x11->setVisible (aVisible);
    mLeInterface_x11->setVisible (aVisible);
#ifdef Q_WS_MAC
    /* Make sure the layout is recalculated (Important on the mac). */
    layout()->activate();
#endif
}

/* VBoxNIList Stuff */
#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
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
    : QIWithRetranslateUI<QWidget> (aParent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout (this);
    VBoxGlobal::setLayoutMargin (mainLayout, 0);
    mLbTitle = new QILabelSeparator();
    mainLayout->addWidget (mLbTitle);
//    mainLayout->addItem (new QSpacerItem (8, 16, QSizePolicy::Fixed, QSizePolicy::Minimum), 1, 0);
    /* Creating List Widget */
    QHBoxLayout *layout = new QHBoxLayout ();
    mainLayout->addLayout (layout);
    mList = new QTreeWidget (this);
    setFocusProxy (mList);
    mLbTitle->setBuddy (mList);
    mList->setColumnCount (1);
    mList->header()->hide();
    mList->setContextMenuPolicy (Qt::ActionsContextMenu);
    mList->setRootIsDecorated (false);
    layout->addWidget (mList);

    /* Prepare actions */
# if defined (Q_WS_WIN)
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
# endif /* Q_WS_WIN */

# if defined (Q_WS_WIN)
    /* Prepare toolbar */
    VBoxToolBar *toolBar = new VBoxToolBar (this);
    toolBar->setUsesTextLabel (false);
    toolBar->setIconSize (QSize (16, 16));
    toolBar->setOrientation (Qt::Vertical);
    toolBar->addAction (mAddAction);
    toolBar->addAction (mDelAction);
    layout->addWidget (toolBar);
# endif /* Q_WS_WIN */

    /* Setup connections */
    connect (mList, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (onCurrentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)));
# if defined (Q_WS_WIN)
    connect (mAddAction, SIGNAL (triggered (bool)),
             this, SLOT (addHostInterface()));
    connect (mDelAction, SIGNAL (triggered (bool)),
             this, SLOT (delHostInterface()));
# endif /* Q_WS_WIN */

#if !defined(Q_WS_WIN) || !defined(VBOX_WITH_NETFLT)
    /* Populating interface list */
    populateInterfacesList();
#endif

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
#ifdef VBOX_WITH_NETFLT
        /* Always select the first item to have an initial value. */
        QTreeWidgetItem *item = mList->topLevelItem (0);
        if (item)
        {
            item->setSelected (true);
            mList->setCurrentItem (item);
            onCurrentItemChanged (mList->currentItem());
        }
#else
        /* Make sure no one of items selected in the list currently */
        mList->setCurrentItem (NULL);
        mList->clearSelection();
#endif
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
# if defined (Q_WS_WIN)
#  if defined (VBOX_WITH_NETFLT)
    if(mEnmAttachmentType == KNetworkAttachmentType_HostOnly)
#  endif
    {
        VBoxNIListItem *item = aCurrent &&
            aCurrent->type() == VBoxNIListItem::typeId ?
            static_cast<VBoxNIListItem*> (aCurrent) : 0;
        mDelAction->setEnabled (item && !item->isWrong());
    }
#  if defined (VBOX_WITH_NETFLT)
    else
    {
        mDelAction->setEnabled (false);
    }
#  endif
# else
    NOREF (aCurrent);
# endif

    emit currentInterfaceChanged (mList->currentItem() ?
        mList->currentItem()->text (0) : QString::null);
}

void VBoxNIList::addHostInterface()
{
# if defined (Q_WS_WIN)
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

    /* Create interface */
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostNetworkInterface iFace;
    CProgress progress = host.CreateHostOnlyNetworkInterface (iFace);
    if (host.isOk())
    {
        QString iName = iFace.GetName();

        vboxProblem().showModalProgressDialog (progress, iName, this);
        /* Add&Select newly created interface */
        if (progress.GetResultCode() == 0)
            new VBoxNIListItem (mList, iName);
        else
            vboxProblem().cannotCreateHostInterface (progress, iName, this);
    }
    else
        vboxProblem().cannotCreateHostInterface (host, this);

    emit listChanged();

    /* Allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);
# endif /* Q_WS_WIN */
}

void VBoxNIList::delHostInterface()
{
    Assert (mList->currentItem());

# if defined (Q_WS_WIN)
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
    CHostNetworkInterfaceVector interfaces =
        host.GetNetworkInterfaces();
    CHostNetworkInterface iFace;
    for (CHostNetworkInterfaceVector::ConstIterator it = interfaces.begin();
         it != interfaces.end(); ++it)
        if (it->GetName() == iName)
            iFace = *it;
    if (!iFace.isNull())
    {
        /* Delete interface */
        CProgress progress = host.RemoveHostOnlyNetworkInterface (iFace.GetId(), iFace);
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
# endif /* Q_WS_WIN */
}

void VBoxNIList::retranslateUi()
{
    mLbTitle->setText (tr ("Host &Interfaces"));

    mList->setWhatsThis (tr ("Lists all available host interfaces."));

# if defined (Q_WS_WIN)
    mAddAction->setText (tr ("A&dd New Host Interface"));
    mDelAction->setText (tr ("&Remove Selected Host Interface"));
    mAddAction->setWhatsThis (tr ("Adds a new host interface."));
    mDelAction->setWhatsThis (tr ("Removes the selected host interface."));
    mAddAction->setToolTip (mAddAction->text().remove ('&') +
        QString (" (%1)").arg (mAddAction->shortcut().toString()));
    mDelAction->setToolTip (mDelAction->text().remove ('&') +
        QString (" (%1)").arg (mDelAction->shortcut().toString()));
# endif /* Q_WS_WIN */
}

#if defined(Q_WS_WIN) && defined(VBOX_WITH_NETFLT)
void VBoxNIList::updateInterfacesList(KNetworkAttachmentType enmAttachmentType)
{
    bool bHostOnly = enmAttachmentType == KNetworkAttachmentType_HostOnly;
    mEnmAttachmentType = enmAttachmentType;
    mAddAction->setEnabled(bHostOnly);
    mDelAction->setEnabled(bHostOnly);
    populateInterfacesList(enmAttachmentType);
}

void VBoxNIList::populateInterfacesList(KNetworkAttachmentType enmAttachmentType)
#else
void VBoxNIList::populateInterfacesList()
#endif
{
    /* Load current inner list */
    QList<QTreeWidgetItem*> itemsList;
    CHostNetworkInterfaceVector interfaces = vboxGlobal().virtualBox().GetHost().GetNetworkInterfaces();
    for (CHostNetworkInterfaceVector::ConstIterator it = interfaces.begin();
         it != interfaces.end(); ++it)
    {
#if defined(Q_WS_WIN) && defined(VBOX_WITH_NETFLT)
        /* display real for not host-only and viceversa */
        if((enmAttachmentType == KNetworkAttachmentType_HostOnly)
                == (it->GetInterfaceType() == KHostNetworkInterfaceType_HostOnly))
#endif
            itemsList << new VBoxNIListItem (it->GetName());
    }

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
#endif /* Q_WS_WIN || VBOX_WITH_NETFLT */


/* VBoxVMSettingsNetworkPage Stuff */
VBoxVMSettingsNetworkPage::VBoxVMSettingsNetworkPage()
    : mValidator (0)
    , mLockNetworkListUpdate (true)
{
    QVBoxLayout *layout = new QVBoxLayout (this);
    layout->setContentsMargins (0, 5, 0, 5);

    /* Creating Tab Widget */
    mTwAdapters = new QTabWidget (this);
    layout->addWidget (mTwAdapters);
    /* Prepare Networks Lists */
    populateNetworksList();

#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
    /* Creating Interfaces List */
    mNIList = new VBoxNIList (this);
    layout->addWidget (mNIList);
#else
//    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
#endif
//    layout->activate();
}

void VBoxVMSettingsNetworkPage::getFrom (const CMachine &aMachine)
{
    Assert (mFirstWidget);
    setTabOrder (mFirstWidget, mTwAdapters->focusProxy());
    QWidget *lastFocusWidget = mTwAdapters->focusProxy();

    /* Creating Tab Pages */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    ulong count = qMin ((ULONG) 4, vbox.GetSystemProperties().GetNetworkAdapterCount());
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
        page->setValidator (mValidator);

        /* Setup tab order */
        lastFocusWidget = page->setOrderAfter (lastFocusWidget);

        /* Setup connections */
#if defined (VBOX_WITH_NETFLT)
        connect (page->mCbNAType, SIGNAL (currentIndexChanged (int)),
                 this, SLOT (updateInterfaceList()));
#endif
        connect (page->mCbNetwork, SIGNAL (editTextChanged (const QString&)),
                 this, SLOT (updateNetworksList()));
    }

#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
    setTabOrder (lastFocusWidget, mNIList->focusProxy());
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

void VBoxVMSettingsNetworkPage::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
    connect (mNIList, SIGNAL (listChanged()), mValidator, SLOT (revalidate()));
#endif
}

bool VBoxVMSettingsNetworkPage::revalidate (QString &aWarning,
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

#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
        if ((type == KNetworkAttachmentType_Bridged
#if defined (Q_WS_WIN) && defined (VBOX_WITH_NETFLT)
                || type == KNetworkAttachmentType_HostOnly
#endif
                ) && page->interfaceName().isNull())
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
            aTitle += ": " + vboxGlobal().removeAccelMark (page->pageTitle());
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

#if defined (VBOX_WITH_NETFLT)
void VBoxVMSettingsNetworkPage::updateInterfaceList()
{
    VBoxVMSettingsNetwork *page =
        static_cast <VBoxVMSettingsNetwork*> (mTwAdapters->currentWidget());
    KNetworkAttachmentType enmType = vboxGlobal().toNetworkAttachmentType (page->mCbNAType->currentText());

    bool isHostInterfaceAttached = enmType == KNetworkAttachmentType_Bridged;

    mNIList->setEnabled (isHostInterfaceAttached);
# ifdef Q_WS_WIN
    if(!isHostInterfaceAttached)
    {
        mNIList->setEnabled (enmType == KNetworkAttachmentType_HostOnly);
    }
    mNIList->updateInterfacesList(enmType);
# endif
}
#endif

#if defined (Q_WS_WIN) || defined (VBOX_WITH_NETFLT)
void VBoxVMSettingsNetworkPage::onCurrentPageChanged (int aIndex)
{
    VBoxVMSettingsNetwork *page =
        static_cast<VBoxVMSettingsNetwork*> (mTwAdapters->widget (aIndex));
    Assert (page);
    mNIList->setCurrentInterface (page->interfaceName());

#if defined (VBOX_WITH_NETFLT)
    updateInterfaceList();
#endif
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
    ulong count = qMin ((ULONG) 4, vbox.GetSystemProperties().GetNetworkAdapterCount());
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

