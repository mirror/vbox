/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsDisplay class implementation
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#include "QIWidgetValidator.h"
#include "VBoxVMSettingsDisplay.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

/**
 *  Calculates a suitable page step size for the given max value. The returned
 *  size is so that there will be no more than 32 pages. The minimum returned
 *  page size is 4.
 */
static int calcPageStep (int aMax)
{
    /* reasonable max. number of page steps is 32 */
    uint page = ((uint) aMax + 31) / 32;
    /* make it a power of 2 */
    uint p = page, p2 = 0x1;
    while ((p >>= 1))
        p2 <<= 1;
    if (page != p2)
        p2 <<= 1;
    if (p2 < 4)
        p2 = 4;
    return (int) p2;
}

VBoxVMSettingsDisplay::VBoxVMSettingsDisplay()
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsDisplay::setupUi (this);

    /* Setup constants */
    CSystemProperties sys = vboxGlobal().virtualBox().GetSystemProperties();
    const uint MinVRAM = sys.GetMinGuestVRAM();
    const uint MaxVRAM = sys.GetMaxGuestVRAM();

    /* Setup validators */
    mLeMemory->setValidator (new QIntValidator (MinVRAM, MaxVRAM, this));
    mLeVRDPPort->setValidator (new QIntValidator (0, 0xFFFF, this));
    mLeVRDPTimeout->setValidator (new QIntValidator (this));

    /* Setup connections */
    connect (mSlMemory, SIGNAL (valueChanged (int)),
             this, SLOT (valueChangedVRAM (int)));
    connect (mLeMemory, SIGNAL (textChanged (const QString&)),
             this, SLOT (textChangedVRAM (const QString&)));

    /* Setup initial values */
    mSlMemory->setPageStep (calcPageStep (MaxVRAM));
    mSlMemory->setSingleStep (mSlMemory->pageStep() / 4);
    mSlMemory->setTickInterval (mSlMemory->pageStep());
    /* Setup the scale so that ticks are at page step boundaries */
    mSlMemory->setMinimum ((MinVRAM / mSlMemory->pageStep()) * mSlMemory->pageStep());
    mSlMemory->setMaximum (MaxVRAM);
    /* Limit min/max. size of QLineEdit */
    mLeMemory->setFixedWidthByText (QString().fill ('8', 4));
    /* Ensure mLeMemory value and validation is updated */
    valueChangedVRAM (mSlMemory->value());
    /* Setup VRDP widget */
    mCbVRDPMethod->insertItem (0, ""); /* KVRDPAuthType_Null */
    mCbVRDPMethod->insertItem (1, ""); /* KVRDPAuthType_External */
    mCbVRDPMethod->insertItem (2, ""); /* KVRDPAuthType_Guest */
    /* Initially disabled */
    mCbVRDP->setChecked (false);

#ifdef QT_MAC_USE_COCOA
    /* No OpenGL on Snow Leopard 64-bit yet */
    mCb3D->setEnabled (false);
#endif /* QT_MAC_USE_COCOA */

    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsDisplay::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;

    /* Memory Size */
    mSlMemory->setValue (mMachine.GetVRAMSize());

    /* 3D Acceleration */
    mCb3D->setChecked (mMachine.GetAccelerate3DEnabled());

    /* VRDP Settings */
    CVRDPServer vrdp = mMachine.GetVRDPServer();
    if (!vrdp.isNull())
    {
        mCbVRDP->setChecked (vrdp.GetEnabled());
        mLeVRDPPort->setText (QString::number (vrdp.GetPort()));
        mCbVRDPMethod->setCurrentIndex (mCbVRDPMethod->
                                        findText (vboxGlobal().toString (vrdp.GetAuthType())));
        mLeVRDPTimeout->setText (QString::number (vrdp.GetAuthTimeout()));
    }
    else
    {
        vboxProblem().cannotLoadMachineSettings (mMachine, false /* strict */);
        mTwDisplay->setTabEnabled (1, false);
    }
}

void VBoxVMSettingsDisplay::putBackTo()
{
    /* Memory Size */
    mMachine.SetVRAMSize (mSlMemory->value());

    /* 3D Acceleration */
    mMachine.SetAccelerate3DEnabled (mCb3D->isChecked());

    /* VRDP Settings */
    CVRDPServer vrdp = mMachine.GetVRDPServer();
    if (!vrdp.isNull())
    {
        vrdp.SetEnabled (mCbVRDP->isChecked());
        vrdp.SetPort (mLeVRDPPort->text().toULong());
        vrdp.SetAuthType (vboxGlobal().toVRDPAuthType (mCbVRDPMethod->currentText()));
        vrdp.SetAuthTimeout (mLeVRDPTimeout->text().toULong());
    }
}

void VBoxVMSettingsDisplay::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
    connect (mCb3D, SIGNAL (stateChanged (int)),
             mValidator, SLOT (revalidate()));
    connect (mCbVRDP, SIGNAL (toggled (bool)),
             mValidator, SLOT (revalidate()));
    connect (mLeVRDPPort, SIGNAL (textChanged (const QString&)),
             mValidator, SLOT (revalidate()));
    connect (mLeVRDPTimeout, SIGNAL (textChanged (const QString&)),
             mValidator, SLOT (revalidate()));
}

bool VBoxVMSettingsDisplay::revalidate (QString &aWarning, QString & /* aTitle */)
{
    /* Video RAM amount test */
    quint64 needBytes = VBoxGlobal::requiredVideoMemory (&mMachine);
    if ((quint64) mSlMemory->value() * _1M < needBytes)
    {
        aWarning = tr (
            "you have assigned less than <b>%1</b> for video memory which is "
            "the minimum amount required to switch the virtual machine to "
            "fullscreen or seamless mode.")
            .arg (vboxGlobal().formatSize (needBytes, 0, VBoxDefs::FormatSize_RoundUp));
        return true;
    }

    /* 3D Acceleration support test */
    // TODO : W8 for NaN //

    return true;
}

void VBoxVMSettingsDisplay::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mTwDisplay->focusProxy());
    setTabOrder (mTwDisplay->focusProxy(), mSlMemory);
    setTabOrder (mSlMemory, mLeMemory);
    setTabOrder (mLeMemory, mCb3D);

    setTabOrder (mCb3D, mCbVRDP);
    setTabOrder (mCbVRDP, mLeVRDPPort);
    setTabOrder (mLeVRDPPort, mCbVRDPMethod);
    setTabOrder (mCbVRDPMethod, mLeVRDPTimeout);
}

void VBoxVMSettingsDisplay::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsDisplay::retranslateUi (this);

    CSystemProperties sys = vboxGlobal().virtualBox().GetSystemProperties();
    mLbMemoryMin->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (sys.GetMinGuestVRAM()));
    mLbMemoryMax->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (sys.GetMaxGuestVRAM()));

    mCbVRDPMethod->setItemText (0,
        vboxGlobal().toString (KVRDPAuthType_Null));
    mCbVRDPMethod->setItemText (1,
        vboxGlobal().toString (KVRDPAuthType_External));
    mCbVRDPMethod->setItemText (2,
        vboxGlobal().toString (KVRDPAuthType_Guest));
}

void VBoxVMSettingsDisplay::valueChangedVRAM (int aVal)
{
    mLeMemory->setText (QString().setNum (aVal));
}

void VBoxVMSettingsDisplay::textChangedVRAM (const QString &aText)
{
    mSlMemory->setValue (aText.toInt());
}

