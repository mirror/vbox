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

void VBoxVMNetworkSettings::init()
{
    /* set initial values
     * -------------------------------------------------------------------- */

    leMACAddress->setValidator (new QRegExpValidator (QRegExp ("[0-9,A-F,a-f]{12,12}"), this));

#if defined Q_WS_X11
    leTAPDescriptor->setValidator (new QIntValidator (-1, std::numeric_limits <LONG>::max(), this));
#endif

    /* set initial values
     * -------------------------------------------------------------------- */

    NoSuitableIfaces = tr ("<No suitable interfaces>");

#if defined Q_WS_WIN
    updateInterfaceList();
#endif
    
    cbNetworkAttachment->insertItem (vboxGlobal().toString (CEnums::NoNetworkAttachment));
    cbNetworkAttachment->insertItem (vboxGlobal().toString (CEnums::NATNetworkAttachment));
    cbNetworkAttachment->insertItem (vboxGlobal().toString (CEnums::HostInterfaceNetworkAttachment));

    grbTAP->setEnabled (false); /* initially disabled */

#if defined Q_WS_WIN
    /* disable unused interface name UI */
    frmHostInterface_X11->setHidden (true);
    /* setup iconsets -- qdesigner is not capable... */
    pbHostAdd->setIconSet (VBoxGlobal::iconSet ("add_host_iface_16px.png",
                                                "add_host_iface_disabled_16px.png"));
    pbHostRemove->setIconSet (VBoxGlobal::iconSet ("remove_host_iface_16px.png",
                                                   "remove_host_iface_disabled_16px.png"));
#else
    /* disable unused interface name UI */
    frmHostInterface_WIN->setHidden (true);
    /* setup iconsets -- qdesigner is not capable... */
    pbTAPSetup->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                                 "select_file_dis_16px.png"));
    pbTAPTerminate->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                                     "select_file_dis_16px.png"));
#endif

#if !defined Q_WS_X11
    /* hide unavailable settings (TAP setup and terminate apps) */
    frmTAPSetupTerminate->setHidden (true);
#endif

    /* the TAP file descriptor setting is always invisible -- currently not used
     * (remove the relative code at all? -- just leave for some time...) */
    frmTAPDescriptor->setHidden (true);
}

void VBoxVMNetworkSettings::updateInterfaceList()
{
#if defined Q_WS_WIN
    /* clear lists */
    networkInterfaceList.clear();
    lbHostInterface->clear();
    /* read a QStringList of interface names */
    CHostNetworkInterfaceEnumerator en =
        vboxGlobal().virtualBox().GetHost().GetNetworkInterfaces().Enumerate();
    while (en.HasMore())
        networkInterfaceList += en.GetNext().GetName();
    /* setup a list of interface names */
    if (networkInterfaceList.count())
        lbHostInterface->insertStringList (networkInterfaceList);
    else
        lbHostInterface->insertItem (NoSuitableIfaces);
    /* disable interface delete button */
    pbHostRemove->setEnabled (!networkInterfaceList.isEmpty());
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
    if (adapter.GetHostInterface().isEmpty())
        lbHostInterface->setCurrentItem (0);
    else
    {
        QListBoxItem* adapterNode = lbHostInterface->findItem(adapter.GetHostInterface());
        if (adapterNode)
            lbHostInterface->setCurrentItem(adapterNode);
    }
#else
    leHostInterface->setText (adapter.GetHostInterface());
#endif

#if defined Q_WS_X11
    leTAPDescriptor->setText (QString::number(adapter.GetTAPFileDescriptor()));
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

void VBoxVMNetworkSettings::cbNetworkAttachment_activated (const QString &string)
{
    grbTAP->setEnabled (vboxGlobal().toNetworkAttachmentType (string) ==
                        CEnums::HostInterfaceNetworkAttachment);
}

void VBoxVMNetworkSettings::lbHostInterface_highlighted(QListBoxItem* /*item*/)
{
    leHostInterfaceName->clear();
}

bool VBoxVMNetworkSettings::checkNetworkInterface (QString ni)
{
#if defined Q_WS_WIN
    return networkInterfaceList.find (ni) != networkInterfaceList.end() ? 1 : 0;
#else
    NOREF(ni);
    return true;
#endif
}

void VBoxVMNetworkSettings::pbGenerateMAC_clicked()
{
    cadapter.SetMACAddress (QString::null);
    leMACAddress->setText (cadapter.GetMACAddress());
}

void VBoxVMNetworkSettings::pbTAPSetup_clicked()
{
//    QFileDialog dlg ("/", QString::null, this);
//    dlg.setMode (QFileDialog::ExistingFile);
//    dlg.setViewMode (QFileDialog::List);
//    dlg.setCaption (tr ("Select TAP setup application"));
//    if (dlg.exec() == QDialog::Accepted)
//        leTAPSetup->setText (dlg.selectedFile());

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
//    QFileDialog dlg ("/", QString::null, this);
//    dlg.setMode (QFileDialog::ExistingFile);
//    dlg.setViewMode (QFileDialog::List);
//    dlg.setCaption (tr ("Select TAP terminate application"));
//    if (dlg.exec() == QDialog::Accepted)
//        leTAPTerminate->setText (dlg.selectedFile());

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

    /* check interface name */
    QString iName = leHostInterfaceName->text();
    if (iName.isEmpty() || iName == NoSuitableIfaces)
    {
        vboxProblem().message (this, VBoxProblemReporter::Error, 
            tr ("Host network interface name cannot be empty"));
        return;
    }

    /* create interface */
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostNetworkInterface iFace;
    CProgress progress = host.CreateHostNetworkInterface (iName, iFace);
    if (host.isOk())
    {
        vboxProblem().showModalProgressDialog (progress, iName, this);
        if (progress.GetResultCode() == 0)
        {
            updateInterfaceList();
            /* move the selection to the new created item */
            QListBoxItem *createdNode =
                lbHostInterface->findItem (leHostInterfaceName->text());
            if (createdNode)
                lbHostInterface->setCurrentItem (createdNode);
            else
                lbHostInterface->setCurrentItem (0);
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
                updateInterfaceList();
                /* move the selection to the start of the list */
                lbHostInterface->setCurrentItem(0);
                lbHostInterface->setSelected(0, true);
            }
            else
                vboxProblem().cannotRemoveHostInterface (progress, iFace, this);
        }
    }

    if (!host.isOk())
        vboxProblem().cannotRemoveHostInterface (host, iFace, this);
#endif
}
