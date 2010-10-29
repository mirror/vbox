/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsDisplay class implementation
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "QIWidgetValidator.h"
#include "VBoxVMSettingsDisplay.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

#include <QDesktopWidget>

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
    m_minVRAM = sys.GetMinGuestVRAM();
    m_maxVRAM = sys.GetMaxGuestVRAM();
    m_maxVRAMVisible = m_maxVRAM;
    const uint MinMonitors = 1;
#if (QT_VERSION >= 0x040600)
    const uint cHostScreens = QApplication::desktop()->screenCount();
#else /* (QT_VERSION >= 0x040600) */
    const uint cHostScreens = QApplication::desktop()->numScreens();
#endif /* !(QT_VERSION >= 0x040600) */
    const uint MaxMonitors = sys.GetMaxGuestMonitors();

    /* Setup validators */
    mLeMemory->setValidator (new QIntValidator (m_minVRAM, m_maxVRAMVisible, this));
    mLeMonitors->setValidator (new QIntValidator (MinMonitors, MaxMonitors, this));
    mLeVRDEPort->setValidator (new QRegExpValidator (QRegExp ("(([0-9]{1,5}(\\-[0-9]{1,5}){0,1}),)*([0-9]{1,5}(\\-[0-9]{1,5}){0,1})"), this));
    mLeVRDETimeout->setValidator (new QIntValidator (this));

    /* Setup connections */
    connect (mSlMemory, SIGNAL (valueChanged (int)), this, SLOT (valueChangedVRAM (int)));
    connect (mLeMemory, SIGNAL (textChanged (const QString&)), this, SLOT (textChangedVRAM (const QString&)));
    connect (mSlMonitors, SIGNAL (valueChanged (int)), this, SLOT (valueChangedMonitors (int)));
    connect (mLeMonitors, SIGNAL (textChanged (const QString&)), this, SLOT (textChangedMonitors (const QString&)));

    /* Setup initial values */
    mSlMemory->setPageStep (calcPageStep (m_maxVRAMVisible));
    mSlMemory->setSingleStep (mSlMemory->pageStep() / 4);
    mSlMemory->setTickInterval (mSlMemory->pageStep());
    mSlMonitors->setPageStep (1);
    mSlMonitors->setSingleStep (1);
    mSlMonitors->setTickInterval (1);
    /* Setup the scale so that ticks are at page step boundaries */
    mSlMemory->setMinimum ((m_minVRAM / mSlMemory->pageStep()) * mSlMemory->pageStep());
    mSlMemory->setMaximum (m_maxVRAMVisible);
    mSlMemory->setSnappingEnabled (true);
    quint64 needMBytes = VBoxGlobal::requiredVideoMemory (&mMachine) / _1M;
    mSlMemory->setErrorHint (0, 1);
    mSlMemory->setWarningHint (1, needMBytes);
    mSlMemory->setOptimalHint (needMBytes, m_maxVRAMVisible);
    mSlMonitors->setMinimum (MinMonitors);
    mSlMonitors->setMaximum (MaxMonitors);
    mSlMonitors->setErrorHint (0, MinMonitors);
    mSlMonitors->setOptimalHint (MinMonitors, cHostScreens);
    mSlMonitors->setWarningHint (cHostScreens, MaxMonitors);
    /* Limit min/max. size of QLineEdit */
    mLeMemory->setFixedWidthByText (QString().fill ('8', 4));
    mLeMonitors->setFixedWidthByText (QString().fill ('8', 4));
    /* Ensure value and validation is updated */
    valueChangedVRAM (mSlMemory->value());
    valueChangedMonitors (mSlMonitors->value());
    /* Setup VRDE widget */
    mCbVRDEMethod->insertItem (0, ""); /* KAuthType_Null */
    mCbVRDEMethod->insertItem (1, ""); /* KAuthType_External */
    mCbVRDEMethod->insertItem (2, ""); /* KAuthType_Guest */
    /* Initially disabled */
    mCbVRDE->setChecked (false);

    mCb3D->setEnabled (false);

#ifndef VBOX_WITH_VIDEOHWACCEL
    mCb2DVideo->setVisible (false);
#endif

#ifdef VBOX_WITH_CRHGSMI
    m_bWddmMode = false;
#endif

    /* Applying language settings */
    retranslateUi();
}

#ifdef VBOX_WITH_VIDEOHWACCEL
bool VBoxVMSettingsDisplay::isAcceleration2DVideoSelected() const
{
    return mCb2DVideo->isChecked();
}
#endif

#ifdef VBOX_WITH_CRHGSMI
void VBoxVMSettingsDisplay::setWddmMode(bool bWddm)
{
    if (bWddm == m_bWddmMode)
        return;

    m_bWddmMode = bWddm;
    checkVRAMRequirements();
}
#endif

void VBoxVMSettingsDisplay::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;

    int currentSize = mMachine.GetVRAMSize();
    m_initialVRAM = RT_MIN(currentSize, m_maxVRAM);

    /* Memory Size */
    mSlMemory->setValue (currentSize);

    /* Monitors Count */
    mSlMonitors->setValue (mMachine.GetMonitorCount());

    /* 3D Acceleration */
    bool isAccelerationSupported = vboxGlobal().virtualBox().GetHost()
                                   .GetAcceleration3DAvailable();
    mCb3D->setEnabled (isAccelerationSupported);
    mCb3D->setChecked (mMachine.GetAccelerate3DEnabled());

    /* must come _before_ setting the initial memory value */
    checkVRAMRequirements();

#ifdef VBOX_WITH_VIDEOHWACCEL
    mCb2DVideo->setEnabled (VBoxGlobal::isAcceleration2DVideoAvailable());
    mCb2DVideo->setChecked (   mMachine.GetAccelerate2DVideoEnabled()
                            && VBoxGlobal::isAcceleration2DVideoAvailable());
#endif

    /* Remote Desktop Settings */
    CVRDEServer vrdeServer = mMachine.GetVRDEServer();
    if (!vrdeServer.isNull())
    {
        mCbVRDE->setChecked (vrdeServer.GetEnabled());
        mLeVRDEPort->setText (vrdeServer.GetVRDEProperty("TCP/Ports"));
        mCbVRDEMethod->setCurrentIndex (mCbVRDEMethod->
                                        findText (vboxGlobal().toString (vrdeServer.GetAuthType())));
        mLeVRDETimeout->setText (QString::number (vrdeServer.GetAuthTimeout()));
        mCbMultipleConn->setChecked(vrdeServer.GetAllowMultiConnection());
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

    /* Monitors Count */
    mMachine.SetMonitorCount (mSlMonitors->value());

    /* 3D Acceleration */
    mMachine.SetAccelerate3DEnabled (mCb3D->isChecked());

#ifdef VBOX_WITH_VIDEOHWACCEL
    /* 2D Video Acceleration */
    mMachine.SetAccelerate2DVideoEnabled (mCb2DVideo->isChecked());
#endif

    /* VRDE Settings */
    CVRDEServer vrdeServer = mMachine.GetVRDEServer();
    if (!vrdeServer.isNull())
    {
        vrdeServer.SetEnabled (mCbVRDE->isChecked());
        vrdeServer.SetVRDEProperty("TCP/Ports", mLeVRDEPort->text());
        vrdeServer.SetAuthType (vboxGlobal().toAuthType (mCbVRDEMethod->currentText()));
        vrdeServer.SetAuthTimeout (mLeVRDETimeout->text().toULong());
        vrdeServer.SetAllowMultiConnection(mCbMultipleConn->isChecked());
    }
}

void VBoxVMSettingsDisplay::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
    connect (mCb3D, SIGNAL (stateChanged (int)),
             mValidator, SLOT (revalidate()));
#ifdef VBOX_WITH_VIDEOHWACCEL
    connect (mCb2DVideo, SIGNAL (stateChanged (int)),
             mValidator, SLOT (revalidate()));
#endif
    connect (mCbVRDE, SIGNAL (toggled (bool)),
             mValidator, SLOT (revalidate()));
    connect (mLeVRDEPort, SIGNAL (textChanged (const QString&)),
             mValidator, SLOT (revalidate()));
    connect (mLeVRDETimeout, SIGNAL (textChanged (const QString&)),
             mValidator, SLOT (revalidate()));
}

bool VBoxVMSettingsDisplay::revalidate (QString &aWarning, QString & /* aTitle */)
{
    /* Video RAM amount test */
    quint64 needBytes = VBoxGlobal::requiredVideoMemory (&mMachine, mSlMonitors->value());
    if ((quint64) mSlMemory->value() * _1M < needBytes)
    {
        aWarning = tr (
            "you have assigned less than <b>%1</b> of video memory which is "
            "the minimum amount required to switch the virtual machine to "
            "fullscreen or seamless mode.")
            .arg (vboxGlobal().formatSize (needBytes, 0, VBoxDefs::FormatSize_RoundUp));
    }
#ifdef VBOX_WITH_VIDEOHWACCEL
    else if (mCb2DVideo->isChecked())
    {
        needBytes += VBoxGlobal::required2DOffscreenVideoMemory();
        if ((quint64) mSlMemory->value() * _1M < needBytes)
        {
            aWarning = tr (
                "you have assigned less than <b>%1</b> of video memory which is "
                "the minimum amount required for HD Video to be played efficiently.")
                .arg (vboxGlobal().formatSize (needBytes, 0, VBoxDefs::FormatSize_RoundUp));
        }
    }
#endif
#ifdef VBOX_WITH_CRHGSMI
    else if (m_bWddmMode && mCb3D->isChecked())
    {
        int cVal = mSlMonitors->value();
        needBytes += VBoxGlobal::required3DWddmOffscreenVideoMemory(&mMachine, cVal);
        needBytes = RT_MAX(needBytes, 128 * _1M);
        needBytes = RT_MIN(needBytes, 256 * _1M);
        if ((quint64) mSlMemory->value() * _1M < needBytes)
        {
            aWarning = tr(
                "You have 3D Acceleration enabled for a operation system which uses the WDDM video driver. "
                "For maximal performance set the guest VRAM to at least <b>%1</b>."
                ).arg (vboxGlobal().formatSize (needBytes, 0, VBoxDefs::FormatSize_RoundUp));
        }
    }
#endif
    checkVRAMRequirements();

    /* 3D Acceleration support test */
    // TODO : W8 for NaN //

    return true;
}

void VBoxVMSettingsDisplay::setOrderAfter (QWidget *aWidget)
{
    /* Video tab-order */
    setTabOrder (aWidget, mTwDisplay->focusProxy());
    setTabOrder (mTwDisplay->focusProxy(), mSlMemory);
    setTabOrder (mSlMemory, mLeMemory);
    setTabOrder (mLeMemory, mSlMonitors);
    setTabOrder (mSlMonitors, mLeMonitors);
    setTabOrder (mLeMonitors, mCb3D);
#ifdef VBOX_WITH_VIDEOHWACCEL
    setTabOrder (mCb3D, mCb2DVideo);
    setTabOrder (mCb2DVideo, mCbVRDE);
#else
    setTabOrder (mCb3D, mCbVRDE);
#endif

    /* Remote display tab-order */
    setTabOrder (mCbVRDE, mLeVRDEPort);
    setTabOrder (mLeVRDEPort, mCbVRDEMethod);
    setTabOrder (mCbVRDEMethod, mLeVRDETimeout);
    setTabOrder (mLeVRDETimeout, mCbMultipleConn);
}

void VBoxVMSettingsDisplay::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsDisplay::retranslateUi (this);

    CSystemProperties sys = vboxGlobal().virtualBox().GetSystemProperties();
    mLbMemoryMin->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (m_minVRAM));
    mLbMemoryMax->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (m_maxVRAMVisible));
    mLbMonitorsMin->setText (tr ("<qt>%1</qt>").arg (1));
    mLbMonitorsMax->setText (tr ("<qt>%1</qt>").arg (sys.GetMaxGuestMonitors()));

    mCbVRDEMethod->setItemText (0,
        vboxGlobal().toString (KAuthType_Null));
    mCbVRDEMethod->setItemText (1,
        vboxGlobal().toString (KAuthType_External));
    mCbVRDEMethod->setItemText (2,
        vboxGlobal().toString (KAuthType_Guest));
}

void VBoxVMSettingsDisplay::valueChangedVRAM (int aVal)
{
    mLeMemory->setText (QString().setNum (aVal));
}

void VBoxVMSettingsDisplay::textChangedVRAM (const QString &aText)
{
    mSlMemory->setValue (aText.toInt());
}

void VBoxVMSettingsDisplay::valueChangedMonitors (int aVal)
{
    mLeMonitors->setText (QString().setNum (aVal));
    checkVRAMRequirements();
}

void VBoxVMSettingsDisplay::textChangedMonitors (const QString &aText)
{
    mSlMonitors->setValue (aText.toInt());
}

void VBoxVMSettingsDisplay::checkVRAMRequirements()
{
    int cVal = mSlMonitors->value();
    /* The memory requirements have changed too. */
    quint64 needMBytes = VBoxGlobal::requiredVideoMemory (&mMachine, cVal) / _1M;
    /* Limit the maximum memory to save careless users from setting useless big values */
#ifdef VBOX_WITH_VIDEOHWACCEL
    if (mCb2DVideo->isChecked())
    {
        needMBytes += VBoxGlobal::required2DOffscreenVideoMemory() / _1M;
    }
#endif
#ifdef VBOX_WITH_CRHGSMI
    if (m_bWddmMode && mCb3D->isChecked())
    {
        needMBytes += VBoxGlobal::required3DWddmOffscreenVideoMemory(&mMachine, cVal) / _1M;
        needMBytes = RT_MAX(needMBytes, 128);
        needMBytes = RT_MIN(needMBytes, 256);
        m_maxVRAMVisible = 256;
    }
    else
#endif
    {
        m_maxVRAMVisible = cVal * 32;
        if (m_maxVRAMVisible < 128)
            m_maxVRAMVisible = 128;
        if (m_maxVRAMVisible < m_initialVRAM)
            m_maxVRAMVisible = m_initialVRAM;
    }
    mSlMemory->setWarningHint (1, needMBytes);
    mSlMemory->setPageStep (calcPageStep (m_maxVRAMVisible));
    mSlMemory->setMaximum (m_maxVRAMVisible);
    mSlMemory->setOptimalHint (needMBytes, m_maxVRAMVisible);
    mLeMemory->setValidator (new QIntValidator (m_minVRAM, m_maxVRAMVisible, this));
    mLbMemoryMax->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (m_maxVRAMVisible));
    /* ... or just call retranslateUi()? */
}

