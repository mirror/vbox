/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "VM network settings" dialog UI include (Qt Designer)
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

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/

/**
 *  QDialog class reimplementation to use for adding network interface.
 *  It has one line-edit field for entering network interface's name and
 *  common dialog's ok/cancel buttons.
 */
#if defined Q_WS_WIN
class VBoxAddNIDialog : public QDialog
{
    Q_OBJECT

public:

    VBoxAddNIDialog (QWidget *aParent, const QString &aIfaceName) :
        QDialog (aParent, "VBoxAddNIDialog", true /* modal */),
        mLeName (0)
    {
        setCaption (tr ("Add Host Interface"));
        QVBoxLayout *mainLayout = new QVBoxLayout (this, 10, 10, "mainLayout");

        /* Setup Input layout */
        QHBoxLayout *inputLayout = new QHBoxLayout (mainLayout, 10, "inputLayout");
        QLabel *lbName = new QLabel (tr ("Interface Name"), this);
        mLeName = new QLineEdit (aIfaceName, this);
        QWhatsThis::add (mLeName, tr ("Descriptive name of the new network interface"));
        inputLayout->addWidget (lbName);
        inputLayout->addWidget (mLeName);
        connect (mLeName, SIGNAL (textChanged (const QString &)),
                 this, SLOT (validate()));

        /* Setup Button layout */
        QHBoxLayout *buttonLayout = new QHBoxLayout (mainLayout, 10, "buttonLayout");
        mBtOk = new QPushButton (tr ("&OK"), this, "mBtOk");
        QSpacerItem *spacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
        QPushButton *btCancel = new QPushButton (tr ("Cancel"), this, "btCancel");
        connect (mBtOk, SIGNAL (clicked()), this, SLOT (accept()));
        connect (btCancel, SIGNAL (clicked()), this, SLOT (reject()));
        buttonLayout->addWidget (mBtOk);
        buttonLayout->addItem (spacer);
        buttonLayout->addWidget (btCancel);

        /* Validate interface name field */
        validate();
    }

    ~VBoxAddNIDialog() {}

    QString getName() { return mLeName->text(); }

private slots:

    void validate()
    {
        mBtOk->setEnabled (!mLeName->text().isEmpty());
    }

private:

    void showEvent (QShowEvent *aEvent)
    {
        setFixedHeight (height());
        QDialog::showEvent (aEvent);
    }

    QPushButton *mBtOk;
    QLineEdit *mLeName;
};
#endif


/**
 *  VBoxVMNetworkSettings class to use as network interface setup page.
 */
void VBoxVMNetworkSettings::init()
{
    mInterfaceNumber = 0;
    mNoInterfaces = tr ("<No suitable interfaces>");

    leMACAddress->setValidator (new QRegExpValidator (QRegExp ("[0-9,A-F]{12,12}"), this));

    cbNetworkAttachment->insertItem (vboxGlobal().toString (CEnums::NoNetworkAttachment));
    cbNetworkAttachment->insertItem (vboxGlobal().toString (CEnums::NATNetworkAttachment));
#ifndef Q_WS_MAC /* not yet on the Mac */
    cbNetworkAttachment->insertItem (vboxGlobal().toString (CEnums::HostInterfaceNetworkAttachment));
    cbNetworkAttachment->insertItem (vboxGlobal().toString (CEnums::InternalNetworkAttachment));
#endif

#if defined Q_WS_X11
    leTAPDescriptor->setValidator (new QIntValidator (-1, std::numeric_limits <LONG>::max(), this));
#else
    /* hide unavailable settings (TAP setup and terminate apps) */
    frmTAPSetupTerminate->setHidden (true);
    /* disable unused interface name UI */
    frmHostInterface_X11->setHidden (true);
#endif

#if defined Q_WS_WIN
    /* setup iconsets */
    pbHostAdd->setIconSet (VBoxGlobal::iconSet ("add_host_iface_16px.png",
                                                "add_host_iface_disabled_16px.png"));
    pbHostRemove->setIconSet (VBoxGlobal::iconSet ("remove_host_iface_16px.png",
                                                   "remove_host_iface_disabled_16px.png"));
    /* setup languages */
    QToolTip::add (pbHostAdd, tr ("Add"));
    QToolTip::add (pbHostRemove, tr ("Remove"));
    /* setup connections */
    connect (grbEnabled, SIGNAL (toggled (bool)),
             this, SLOT (grbEnabledToggled (bool)));
#else
    /* disable unused interface name UI */
    frmHostInterface_WIN->setHidden (true);
    /* setup iconsets */
    pbTAPSetup->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                                 "select_file_dis_16px.png"));
    pbTAPTerminate->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                                     "select_file_dis_16px.png"));
#endif

    /* the TAP file descriptor setting is always invisible -- currently not used
     * (remove the relative code at all? -- just leave for some time...) */
    frmTAPDescriptor->setHidden (true);

#if defined Q_WS_MAC
    /* no Host Interface Networking on the Mac yet */
    grbTAP->setHidden (true);
#endif
}

bool VBoxVMNetworkSettings::isPageValid (const QStringList &aList)
{
#if defined Q_WS_WIN
    CEnums::NetworkAttachmentType type =
        vboxGlobal().toNetworkAttachmentType (cbNetworkAttachment->currentText());

    return !(type == CEnums::HostInterfaceNetworkAttachment &&
             isInterfaceInvalid (aList, leHostInterfaceName->text()));
#else
    NOREF (aList);
    return true;
#endif
}

void VBoxVMNetworkSettings::loadList (const QStringList &aList,
                                      int aInterfaceNumber)
{
#if defined Q_WS_WIN
    mInterfaceNumber = aInterfaceNumber;
    /* save current list item name */
    QString currentListItemName = leHostInterfaceName->text();
    /* load current list items */
    lbHostInterface->clearFocus();
    lbHostInterface->clear();
    if (aList.count())
        lbHostInterface->insertStringList (aList);
    else
        lbHostInterface->insertItem (mNoInterfaces);
    selectListItem (currentListItemName);
    /* disable interface delete button */
    pbHostRemove->setEnabled (!aList.isEmpty());
#else
    NOREF (aList);
    NOREF (aInterfaceNumber);
#endif
}

void VBoxVMNetworkSettings::getFromAdapter (const CNetworkAdapter &adapter)
{
    cadapter = adapter;

    grbEnabled->setChecked (adapter.GetEnabled());

    CEnums::NetworkAttachmentType type = adapter.GetAttachmentType();
    cbNetworkAttachment->setCurrentItem (0);
    for (int i = 0; i < cbNetworkAttachment->count(); i ++)
        if (vboxGlobal().toNetworkAttachmentType (cbNetworkAttachment->text (i)) == type)
        {
            cbNetworkAttachment->setCurrentItem (i);
            cbNetworkAttachment_activated (cbNetworkAttachment->currentText());
            break;
        }

    leMACAddress->setText (adapter.GetMACAddress());

    chbCableConnected->setChecked (adapter.GetCableConnected());

#if defined Q_WS_WIN
    selectListItem (adapter.GetHostInterface());
#else
    leHostInterface->setText (adapter.GetHostInterface());
#endif

#if defined Q_WS_X11
    leTAPDescriptor->setText (QString::number (adapter.GetTAPFileDescriptor()));
    leTAPSetup->setText (adapter.GetTAPSetupApplication());
    leTAPTerminate->setText (adapter.GetTAPTerminateApplication());
#endif
}

void VBoxVMNetworkSettings::putBackToAdapter()
{
    cadapter.SetEnabled (grbEnabled->isChecked());

    CEnums::NetworkAttachmentType type =
        vboxGlobal().toNetworkAttachmentType (cbNetworkAttachment->currentText());
    switch (type)
    {
        case CEnums::NoNetworkAttachment:
            cadapter.Detach();
            break;
        case CEnums::NATNetworkAttachment:
            cadapter.AttachToNAT();
            break;
        case CEnums::HostInterfaceNetworkAttachment:
            cadapter.AttachToHostInterface();
            break;
        case CEnums::InternalNetworkAttachment:
            cadapter.AttachToInternalNetwork();
            break;
        default:
            AssertMsgFailed (("Invalid network attachment type: %d", type));
            break;
    }

    cadapter.SetMACAddress (leMACAddress->text());

    cadapter.SetCableConnected (chbCableConnected->isChecked());

    if (type == CEnums::HostInterfaceNetworkAttachment)
    {
#if defined Q_WS_WIN
        if (!lbHostInterface->currentText().isEmpty())
            cadapter.SetHostInterface (lbHostInterface->currentText());
#else
        QString iface = leHostInterface->text();
        cadapter.SetHostInterface (iface.isEmpty() ? QString::null : iface);
#endif
#if defined Q_WS_X11
        cadapter.SetTAPFileDescriptor (leTAPDescriptor->text().toLong());
        QString setup = leTAPSetup->text();
        cadapter.SetTAPSetupApplication (setup.isEmpty() ? QString::null : setup);
        QString term = leTAPTerminate->text();
        cadapter.SetTAPTerminateApplication (term.isEmpty() ? QString::null : term);
#endif
    }
}

void VBoxVMNetworkSettings::setValidator (QIWidgetValidator *aWalidator)
{
    mWalidator = aWalidator;
}

void VBoxVMNetworkSettings::revalidate()
{
    mWalidator->revalidate();
}

void VBoxVMNetworkSettings::grbEnabledToggled (bool aOn)
{
#if defined Q_WS_WIN
    if (!aOn)
    {
        lbHostInterface->clearFocus();
        cbNetworkAttachment->setCurrentItem (0);
        cbNetworkAttachment_activated (cbNetworkAttachment->currentText());
        if (lbHostInterface->selectedItem())
            lbHostInterface->setSelected (lbHostInterface->selectedItem(), false);
    }
    if (lbHostInterface->currentItem() != -1)
        lbHostInterface->setSelected (lbHostInterface->currentItem(), aOn);
#else
    NOREF (aOn);
#endif
}

void VBoxVMNetworkSettings::selectListItem (const QString &aItemName)
{
    if (!aItemName.isEmpty())
    {
#if defined Q_WS_WIN
        leHostInterfaceName->setText (aItemName);
        QListBoxItem* adapterNode = lbHostInterface->findItem (aItemName);
        if (adapterNode)
        {
            lbHostInterface->setCurrentItem (adapterNode);
            lbHostInterface->setSelected (adapterNode, true);
        }
#endif
    }
}

void VBoxVMNetworkSettings::cbNetworkAttachment_activated (const QString &aString)
{
#if defined Q_WS_WIN
    bool enableHostIf = vboxGlobal().toNetworkAttachmentType (aString) ==
                        CEnums::HostInterfaceNetworkAttachment;
    txHostInterface_WIN->setEnabled (enableHostIf);
    leHostInterfaceName->setEnabled (enableHostIf);
    lbHostInterface_highlighted (lbHostInterface->selectedItem());
#else
    NOREF (aString);
#endif
}

void VBoxVMNetworkSettings::lbHostInterface_highlighted (QListBoxItem *aItem)
{
    if (!aItem) return;
#if defined Q_WS_WIN
    leHostInterfaceName->setText (leHostInterfaceName->isEnabled() ?
                                  aItem->text() : QString::null);
    if (!lbHostInterface->isSelected (aItem))
        lbHostInterface->setSelected (aItem, true);
#endif
}

bool VBoxVMNetworkSettings::isInterfaceInvalid (const QStringList &aList,
                                                const QString &aIface)
{
#if defined Q_WS_WIN
    return aList.find (aIface) == aList.end();
#else
    NOREF (aList);
    NOREF (aIface);
    return false;
#endif
}

void VBoxVMNetworkSettings::pbGenerateMAC_clicked()
{
    cadapter.SetMACAddress (QString::null);
    leMACAddress->setText (cadapter.GetMACAddress());
}

void VBoxVMNetworkSettings::pbTAPSetup_clicked()
{
    QString selected = QFileDialog::getOpenFileName (
        "/",
        QString::null,
        this,
        NULL,
        tr ("Select TAP setup application"));

    if (selected)
        leTAPSetup->setText (selected);
}

void VBoxVMNetworkSettings::pbTAPTerminate_clicked()
{
    QString selected = QFileDialog::getOpenFileName (
        "/",
        QString::null,
        this,
        NULL,
        tr ("Select TAP terminate application"));

    if (selected)
        leTAPTerminate->setText (selected);
}

void VBoxVMNetworkSettings::hostInterfaceAdd()
{
#if defined Q_WS_WIN

    /* allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);

    /* creating add host interface dialog */
    VBoxAddNIDialog dlg (this, lbHostInterface->currentItem() != -1 ?
                         tr ("VirtualBox Host Interface %1").arg (mInterfaceNumber) :
                         leHostInterfaceName->text());
    if (dlg.exec() != QDialog::Accepted)
        return;
    QString iName = dlg.getName();

    /* create interface */
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostNetworkInterface iFace;
    CProgress progress = host.CreateHostNetworkInterface (iName, iFace);
    if (host.isOk())
    {
        vboxProblem().showModalProgressDialog (progress, iName, this);
        if (progress.GetResultCode() == 0)
        {
            ++ mInterfaceNumber;
            /* add&select newly created created interface */
            delete lbHostInterface->findItem (mNoInterfaces);
            lbHostInterface->insertItem (iName);
            selectListItem (iName);
            emit listChanged (this);
        }
        else
            vboxProblem().cannotCreateHostInterface (progress, iName, this);
    }
    else
        vboxProblem().cannotCreateHostInterface (host, iName, this);

    /* allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);

#endif
}

void VBoxVMNetworkSettings::hostInterfaceRemove()
{
#if defined Q_WS_WIN

    /* allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);

    /* check interface name */
    QString iName = lbHostInterface->currentText();
    if (iName.isEmpty())
        return;

    /* asking user about deleting selected network interface */
    int delNetIface = vboxProblem().message (this, VBoxProblemReporter::Question,
        tr ("<p>Do you want to remove the selected host network interface "
            "<nobr><b>%1</b>?</nobr></p>"
            "<p><b>Note:</b> This interface may be in use by one or more "
            "network adapters of this or another VM. After it is removed, these "
            "adapters will no longer work until you correct their settings by "
            "either choosing a differnet interface name or a different adapter "
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
        /* delete interface */
        CProgress progress = host.RemoveHostNetworkInterface (iFace.GetId(), iFace);
        if (host.isOk())
        {
            vboxProblem().showModalProgressDialog (progress, iName, this);
            if (progress.GetResultCode() == 0)
            {
                if (lbHostInterface->count() == 1)
                    lbHostInterface->insertItem (mNoInterfaces);
                delete lbHostInterface->findItem (iName);
                emit listChanged (this);
            }
            else
                vboxProblem().cannotRemoveHostInterface (progress, iFace, this);
        }
    }

    if (!host.isOk())
        vboxProblem().cannotRemoveHostInterface (host, iFace, this);
#endif
}

#if defined Q_WS_WIN
#include "VBoxVMNetworkSettings.ui.moc"
#endif
