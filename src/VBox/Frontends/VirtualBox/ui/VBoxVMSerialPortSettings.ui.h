/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "VM serial port settings" dialog UI include (Qt Designer)
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

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/


static const struct PortConfig
{
    const char *name;
    const ulong IRQ;
    const ulong IOBase;
}
kKnownPorts[] = 
{
    { "COM1", 4, 0x3F8 },
    { "COM2", 3, 0x2F8 },
    { "COM3", 4, 0x3E8 },
    { "COM4", 3, 0x2E8 },
    /* must not contain an element with IRQ=0 and IOBase=0 used to cause
     * portNumbers2Name() to return the "User-defined" string. */
};

static const PortConfig *findByPortNumbers (ulong aIRQ, ulong aIOBase)
{
    for (size_t i = 0; i < ELEMENTS (kKnownPorts); ++ i)
        if (kKnownPorts [i].IRQ == aIRQ &&
            kKnownPorts [i].IOBase == aIOBase)
            return &kKnownPorts [i];
    return NULL;
}

static const PortConfig *findByPortName (const char *aName)
{
    for (size_t i = 0; i < ELEMENTS (kKnownPorts); ++ i)
        if (strcmp (kKnownPorts [i].name, aName) == 0)
            return &kKnownPorts [i];
    return NULL;
}

static QString portNumbers2Name (ulong aIRQ, ulong aIOBase)
{
    const PortConfig *config = findByPortNumbers (aIRQ, aIOBase);
    if (config)
        return config->name;

    return VBoxVMSerialPortSettings::tr ("User-defined");
}


void VBoxVMSerialPortSettings::init()
{
    /* setup validation */

    mIRQLine->setValidator (new QIULongValidator (0, 255, this));
    mIOPortLine->setValidator (new QIULongValidator (0, 0xFFFF, this));

    mPortPathLine->setValidator (new QRegExpValidator (QRegExp (".+"), this));

    /* setup constraints */

    mIRQLine->setMaximumWidth (mIRQLine->fontMetrics().width ("888888")
                               + mIRQLine->frameWidth() * 2);
    mIRQLine->setMinimumWidth (mIRQLine->minimumWidth());

    mIOPortLine->setMaximumWidth (mIOPortLine->fontMetrics().width ("8888888")
                                   + mIOPortLine->frameWidth() * 2);
    mIOPortLine->setMinimumWidth (mIOPortLine->minimumWidth());

    /* set initial values */

    for (size_t i = 0; i < ELEMENTS (kKnownPorts); ++ i)
        mPortNumCombo->insertItem (kKnownPorts [i].name);
    mPortNumCombo->insertItem (portNumbers2Name (0, 0));

    mHostModeCombo->insertItem (vboxGlobal().toString (CEnums::DisconnectedPort));
    mHostModeCombo->insertItem (vboxGlobal().toString (CEnums::HostPipePort));
    mHostModeCombo->insertItem (vboxGlobal().toString (CEnums::HostDevicePort));
}

void VBoxVMSerialPortSettings::getFromPort (const CSerialPort &aPort)
{
    mPort = aPort;

    mSerialPortBox->setChecked (mPort.GetEnabled());

    ulong IRQ = mPort.GetIRQ();
    ulong IOBase = mPort.GetIOBase();
    mPortNumCombo->setCurrentText (portNumbers2Name (IRQ, IOBase));
    mIRQLine->setText (QString::number (IRQ));
    mIOPortLine->setText ("0x" + QString::number (IOBase, 16).upper());

    mHostModeCombo->setCurrentText (vboxGlobal().toString (mPort.GetHostMode()));
    mServerCheck->setChecked (mPort.GetServer());
    mPortPathLine->setText (mPort.GetPath());

    /* ensure everything is up-to-date */
    mSerialPortBox_toggled (mSerialPortBox->isChecked());
}

void VBoxVMSerialPortSettings::putBackToPort()
{
    mPort.SetEnabled (mSerialPortBox->isChecked());

    mPort.SetIRQ (mIRQLine->text().toULong (NULL, 0));
    mPort.SetIOBase (mIOPortLine->text().toULong (NULL, 0));

    mPort.SetHostMode (vboxGlobal().toPortMode (mHostModeCombo->currentText()));
    mPort.SetServer (mServerCheck->isChecked());
    mPort.SetPath (QDir::convertSeparators (mPortPathLine->text()));
}

bool VBoxVMSerialPortSettings::isUserDefined()
{
    return findByPortName (mPortNumCombo->currentText().utf8().data()) == NULL;
}

void VBoxVMSerialPortSettings::mSerialPortBox_toggled (bool aOn)
{
    if (aOn)
    {
        mPortNumCombo_activated (mPortNumCombo->currentText());
        mHostModeCombo_activated (mHostModeCombo->currentText());
    }
}

void VBoxVMSerialPortSettings::mPortNumCombo_activated (const QString &aText)
{
    const PortConfig *config = findByPortName (aText.utf8().data());
    mIRQLine->setEnabled (config == NULL);
    mIOPortLine->setEnabled (config == NULL);
    if (config != NULL)
    {
        mIRQLine->setText (QString::number (config->IRQ));
        mIOPortLine->setText ("0x" + QString::number (config->IOBase, 16).upper());
    }
}

void VBoxVMSerialPortSettings::mHostModeCombo_activated (const QString &aText)
{
    CEnums::PortMode mode = vboxGlobal().toPortMode (aText);
    mServerCheck->setEnabled (mode == CEnums::HostPipePort);
    mPortPathLine->setEnabled (mode != CEnums::DisconnectedPort);
}
