/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "USB device filter" settings dialog UI include (Qt Designer)
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
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

void VBoxUSBFilterSettings::init()
{
    // by default, the widget is entirely disabled
    setEnabled (false);

    // set the dummy focus proxy to let others know which our child
    // is the last in the focus chain
    setFocusProxy (cbRemote);
    cbRemote->insertItem (tr ("Any", "remote")); // 0
    cbRemote->insertItem (tr ("Yes", "remote")); // 1
    cbRemote->insertItem (tr ("No", "remote")); // 2
}

void VBoxUSBFilterSettings::getFromFilter (const CUSBDeviceFilter &aFilter)
{
    mFilter = aFilter;

    leUSBFilterName->setValidator (new QRegExpValidator (QRegExp (".+"), this));

    leUSBFilterName->setText (aFilter.GetName());

    leUSBFilterVendorId->setText (aFilter.GetVendorId());
    leUSBFilterProductId->setText (aFilter.GetProductId());
    leUSBFilterRevision->setText (aFilter.GetRevision());
    leUSBFilterPort->setText (aFilter.GetPort());

    leUSBFilterManufacturer->setText (aFilter.GetManufacturer());
    leUSBFilterProduct->setText (aFilter.GetProduct());
    leUSBFilterSerial->setText (aFilter.GetSerialNumber());
    {
        QCString remote = aFilter.GetRemote().latin1();
        if (remote == "yes" || remote == "true" || remote == "1")
            cbRemote->setCurrentItem (1);
        else if (remote == "no" || remote == "false" || remote == "0")
            cbRemote->setCurrentItem (2);
        else
            cbRemote->setCurrentItem (0);
    }

    setEnabled (true);
}

inline static QString emptyToNull (const QString &str)
{
    return str.isEmpty() ? QString::null : str;
}

COMResult VBoxUSBFilterSettings::putBackToFilter()
{
    do
    {
        mFilter.SetName (leUSBFilterName->text());
        if (!mFilter.isOk())
            break;

        mFilter.SetVendorId (emptyToNull (leUSBFilterVendorId->text()));
        if (!mFilter.isOk())
            break;
        mFilter.SetProductId (emptyToNull (leUSBFilterProductId->text()));
        if (!mFilter.isOk())
            break;
        mFilter.SetRevision (emptyToNull (leUSBFilterRevision->text()));
        if (!mFilter.isOk())
            break;
        mFilter.SetPort (emptyToNull (leUSBFilterPort->text()));
        if (!mFilter.isOk())
            break;

        mFilter.SetManufacturer (emptyToNull (leUSBFilterManufacturer->text()));
        if (!mFilter.isOk())
            break;
        mFilter.SetProduct (emptyToNull (leUSBFilterProduct->text()));
        if (!mFilter.isOk())
            break;
        mFilter.SetSerialNumber (emptyToNull (leUSBFilterSerial->text()));
        if (!mFilter.isOk())
            break;
        switch (cbRemote->currentItem())
        {
            case 1: mFilter.SetRemote ("yes"); break;
            case 2: mFilter.SetRemote ("no"); break;
            default: AssertMsgFailed (("Invalid combo box index"));
            case 0: mFilter.SetRemote (QString::null); break;
        }
        if (!mFilter.isOk())
            break;
    }
    while (0);

    return !mFilter.isOk() ? COMResult (mFilter) : COMResult();
}

