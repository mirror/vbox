/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "VM network settings" dialog UI include (Qt Designer)
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/

void VBoxVMNetworkSettings::init()
{
    leMACAddress->setValidator (new QRegExpValidator
                                (QRegExp ("[0-9A-Fa-f][02468ACEace][0-9A-Fa-f]{10}"), this));

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
    /* disable unused interface name UI */
    grbTAP->setHidden (true);
    grbIntNet->setHidden (true);
    connect (grbEnabled, SIGNAL (toggled (bool)),
             this, SLOT (grbEnabledToggled (bool)));
#else
    /* disable unused interface name UI */
    txHostInterface_WIN->setHidden (true);
    cbHostInterfaceName->setHidden (true);
    txInternalNetwork_WIN->setHidden (true);
    cbInternalNetworkName_WIN->setHidden (true);
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

VBoxVMNetworkSettings::CheckPageResult VBoxVMNetworkSettings::checkPage (const QStringList &aList)
{
    CEnums::NetworkAttachmentType type =
        vboxGlobal().toNetworkAttachmentType (cbNetworkAttachment->currentText());
#if defined Q_WS_WIN
    if (type == CEnums::HostInterfaceNetworkAttachment &&
        isInterfaceInvalid (aList, cbHostInterfaceName->currentText()))
        return IncorrectInterface;
    else if (type == CEnums::InternalNetworkAttachment &&
             cbInternalNetworkName_WIN->currentText().isEmpty())
        return MissedNetworkName;
    else
        return NoErrors;
#else
    NOREF (aList);
    if (type == CEnums::InternalNetworkAttachment &&
        cbInternalNetworkName_X11->currentText().isEmpty())
        return MissedNetworkName;
    else
        return NoErrors;
#endif
}

void VBoxVMNetworkSettings::loadInterfaceList (const QStringList &aList,
                                               const QString &aNillItem)
{
#if defined Q_WS_WIN
    /* save current list item name */
    QString currentListItemName = cbHostInterfaceName->currentText();
    /* clear current list */
    cbHostInterfaceName->clear();
    /* load current list items */
    if (aList.count())
    {
        cbHostInterfaceName->insertStringList (aList);
        int index = aList.findIndex (currentListItemName);
        if (index != -1)
            cbHostInterfaceName->setCurrentItem (index);
    }
    else
    {
        cbHostInterfaceName->insertItem (aNillItem);
        cbHostInterfaceName->setCurrentItem (0);
    }
#else
    NOREF (aList);
    NOREF (aNillItem);
#endif
}

void VBoxVMNetworkSettings::loadNetworksList (const QStringList &aList)
{
    QComboBox *cbNetworkName = 0;
#if defined Q_WS_WIN
    cbNetworkName = cbInternalNetworkName_WIN;
#else
    cbNetworkName = cbInternalNetworkName_X11;
#endif
    Assert (cbNetworkName);
    QString curText = cbNetworkName->currentText();
    cbNetworkName->clear();
    cbNetworkName->clearEdit();
    cbNetworkName->insertStringList (aList);
    cbNetworkName->setCurrentText (curText);
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
    QString name = adapter.GetHostInterface();
    for (int index = 0; index < cbHostInterfaceName->count(); ++ index)
        if (cbHostInterfaceName->text (index) == name)
        {
            cbHostInterfaceName->setCurrentItem (index);
            break;
        }
    if (cbHostInterfaceName->currentItem() == -1)
        cbHostInterfaceName->setCurrentText (name);
    cbInternalNetworkName_WIN->setCurrentText (adapter.GetInternalNetwork());
#else
    leHostInterface->setText (adapter.GetHostInterface());
    cbInternalNetworkName_X11->setCurrentText (adapter.GetInternalNetwork());
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
        if (!cbHostInterfaceName->currentText().isEmpty())
            cadapter.SetHostInterface (cbHostInterfaceName->currentText());
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
    else if (type == CEnums::InternalNetworkAttachment)
    {
#if defined Q_WS_WIN
        if (!cbInternalNetworkName_WIN->currentText().isEmpty())
            cadapter.SetInternalNetwork (cbInternalNetworkName_WIN->currentText());
#else
        if (!cbInternalNetworkName_X11->currentText().isEmpty())
            cadapter.SetInternalNetwork (cbInternalNetworkName_X11->currentText());
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
        cbNetworkAttachment->setCurrentItem (0);
        cbNetworkAttachment_activated (cbNetworkAttachment->currentText());
    }
#else
    NOREF (aOn);
#endif
}

void VBoxVMNetworkSettings::cbNetworkAttachment_activated (const QString &aString)
{
    bool enableHostIf = vboxGlobal().toNetworkAttachmentType (aString) ==
                        CEnums::HostInterfaceNetworkAttachment;
    bool enableIntNet = vboxGlobal().toNetworkAttachmentType (aString) ==
                        CEnums::InternalNetworkAttachment;
#if defined Q_WS_WIN
    txHostInterface_WIN->setEnabled (enableHostIf);
    cbHostInterfaceName->setEnabled (enableHostIf);
    txInternalNetwork_WIN->setEnabled (enableIntNet);
    cbInternalNetworkName_WIN->setEnabled (enableIntNet);
#else
    grbTAP->setEnabled (enableHostIf);
    grbIntNet->setEnabled (enableIntNet);
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
