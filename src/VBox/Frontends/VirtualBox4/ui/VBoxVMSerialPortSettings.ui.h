/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "VM serial port settings" dialog UI include (Qt Designer)
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

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/


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

    mPortNumCombo->insertStringList (vboxGlobal().COMPortNames());
    mPortNumCombo->insertItem (vboxGlobal().toCOMPortName (0, 0));

    mHostModeCombo->insertItem (vboxGlobal().toString (KPortMode_Disconnected));
    mHostModeCombo->insertItem (vboxGlobal().toString (KPortMode_HostPipe));
    mHostModeCombo->insertItem (vboxGlobal().toString (KPortMode_HostDevice));
}

void VBoxVMSerialPortSettings::getFromPort (const CSerialPort &aPort)
{
    mPort = aPort;

    mSerialPortBox->setChecked (mPort.GetEnabled());

    ulong IRQ = mPort.GetIRQ();
    ulong IOBase = mPort.GetIOBase();
    mPortNumCombo->setCurrentText (vboxGlobal().toCOMPortName (IRQ, IOBase));
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

    mPort.SetPath (QDir::convertSeparators (mPortPathLine->text()));

    mPort.SetHostMode (vboxGlobal().toPortMode (mHostModeCombo->currentText()));
    mPort.SetServer (mServerCheck->isChecked());
}

bool VBoxVMSerialPortSettings::isUserDefined()
{
    ulong a, b;
    return !vboxGlobal().toCOMPortNumbers (mPortNumCombo->currentText(), a, b);
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
    ulong IRQ, IOBase;
    bool std = vboxGlobal().toCOMPortNumbers (aText, IRQ, IOBase);

    mIRQLine->setEnabled (!std);
    mIOPortLine->setEnabled (!std);
    if (std)
    {
        mIRQLine->setText (QString::number (IRQ));
        mIOPortLine->setText ("0x" + QString::number (IOBase, 16).upper());
    }
}

void VBoxVMSerialPortSettings::mHostModeCombo_activated (const QString &aText)
{
    KPortMode mode = vboxGlobal().toPortMode (aText);
    mServerCheck->setEnabled (mode == KPortMode_HostPipe);
    mPortPathLine->setEnabled (mode != KPortMode_Disconnected);
}
