/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "VM parallel port settings" dialog UI include (Qt Designer)
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


void VBoxVMParallelPortSettings::init()
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

    mPortNumCombo->insertStringList (vboxGlobal().LPTPortNames());
    mPortNumCombo->insertItem (vboxGlobal().toLPTPortName (0, 0));
}

void VBoxVMParallelPortSettings::getFromPort (const CParallelPort &aPort)
{
    mPort = aPort;

    mParallelPortBox->setChecked (mPort.GetEnabled());

    ulong IRQ = mPort.GetIRQ();
    ulong IOBase = mPort.GetIOBase();
    mPortNumCombo->setCurrentText (vboxGlobal().toLPTPortName (IRQ, IOBase));
    mIRQLine->setText (QString::number (IRQ));
    mIOPortLine->setText ("0x" + QString::number (IOBase, 16).upper());

    mPortPathLine->setText (mPort.GetPath());

    /* ensure everything is up-to-date */
    mParallelPortBox_toggled (mParallelPortBox->isChecked());
}

void VBoxVMParallelPortSettings::putBackToPort()
{
    mPort.SetEnabled (mParallelPortBox->isChecked());

    mPort.SetIRQ (mIRQLine->text().toULong (NULL, 0));
    mPort.SetIOBase (mIOPortLine->text().toULong (NULL, 0));

    mPort.SetPath (QDir::convertSeparators (mPortPathLine->text()));
}

bool VBoxVMParallelPortSettings::isUserDefined()
{
    ulong a, b;
    return !vboxGlobal().toLPTPortNumbers (mPortNumCombo->currentText(), a, b);
}

void VBoxVMParallelPortSettings::mParallelPortBox_toggled (bool aOn)
{
    if (aOn)
        mPortNumCombo_activated (mPortNumCombo->currentText());
}

void VBoxVMParallelPortSettings::mPortNumCombo_activated (const QString &aText)
{
    ulong IRQ, IOBase;
    bool std = vboxGlobal().toLPTPortNumbers (aText, IRQ, IOBase);

    mIRQLine->setEnabled (!std);
    mIOPortLine->setEnabled (!std);
    if (std)
    {
        mIRQLine->setText (QString::number (IRQ));
        mIOPortLine->setText ("0x" + QString::number (IOBase, 16).upper());
    }
}
