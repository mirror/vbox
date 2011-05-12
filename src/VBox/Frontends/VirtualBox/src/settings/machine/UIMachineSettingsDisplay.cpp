/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsDisplay class implementation
 */

/*
 * Copyright (C) 2008-2011 Oracle Corporation
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
#include "UIMachineSettingsDisplay.h"
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

UIMachineSettingsDisplay::UIMachineSettingsDisplay()
    : mValidator(0)
    , m_minVRAM(0)
    , m_maxVRAM(0)
    , m_maxVRAMVisible(0)
    , m_initialVRAM(0)
#ifdef VBOX_WITH_CRHGSMI
    , m_bWddmMode(false)
#endif
{
    /* Apply UI decorations */
    Ui::UIMachineSettingsDisplay::setupUi (this);

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
    quint64 needMBytes = VBoxGlobal::requiredVideoMemory (&m_machine) / _1M;
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
bool UIMachineSettingsDisplay::isAcceleration2DVideoSelected() const
{
    return mCb2DVideo->isChecked();
}
#endif

#ifdef VBOX_WITH_CRHGSMI
void UIMachineSettingsDisplay::setWddmMode(bool bWddm)
{
    if (bWddm == m_bWddmMode)
        return;

    m_bWddmMode = bWddm;
    checkVRAMRequirements();
}
#endif

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsDisplay::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_cache.clear();

    /* Prepare display data: */
    UIDataSettingsMachineDisplay displayData;

    /* Gather display data: */
    displayData.m_iCurrentVRAM = m_machine.GetVRAMSize();
    displayData.m_cMonitorCount = m_machine.GetMonitorCount();
    displayData.m_f3dAccelerationEnabled = m_machine.GetAccelerate3DEnabled();
#ifdef VBOX_WITH_VIDEOHWACCEL
    displayData.m_f2dAccelerationEnabled = m_machine.GetAccelerate2DVideoEnabled();
#endif /* VBOX_WITH_VIDEOHWACCEL */
    /* Check if VRDE server is valid: */
    CVRDEServer vrdeServer = m_machine.GetVRDEServer();
    displayData.m_fVRDEServerSupported = !vrdeServer.isNull();
    if (!vrdeServer.isNull())
    {
        displayData.m_fVRDEServerEnabled = vrdeServer.GetEnabled();
        displayData.m_strVRDEPort = vrdeServer.GetVRDEProperty("TCP/Ports");
        displayData.m_VRDEAuthType = vrdeServer.GetAuthType();
        displayData.m_uVRDETimeout = vrdeServer.GetAuthTimeout();
        displayData.m_fMultipleConnectionsAllowed = vrdeServer.GetAllowMultiConnection();
    }

    /* Initialize other variables: */
    m_initialVRAM = RT_MIN(displayData.m_iCurrentVRAM, m_maxVRAM);

    /* Cache display data: */
    m_cache.cacheInitialData(displayData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsDisplay::getFromCache()
{
    /* Get display data from cache: */
    const UIDataSettingsMachineDisplay &displayData = m_cache.base();

    /* Load display data to page: */
    mSlMonitors->setValue(displayData.m_cMonitorCount);
    mCb3D->setEnabled(vboxGlobal().virtualBox().GetHost().GetAcceleration3DAvailable());
    mCb3D->setChecked(displayData.m_f3dAccelerationEnabled);
#ifdef VBOX_WITH_VIDEOHWACCEL
    mCb2DVideo->setEnabled(VBoxGlobal::isAcceleration2DVideoAvailable());
    mCb2DVideo->setChecked(displayData.m_f2dAccelerationEnabled);
#endif /* VBOX_WITH_VIDEOHWACCEL */
    checkVRAMRequirements();
    mSlMemory->setValue(displayData.m_iCurrentVRAM);
    if (displayData.m_fVRDEServerSupported)
    {
        mCbVRDE->setChecked(displayData.m_fVRDEServerEnabled);
        mLeVRDEPort->setText(displayData.m_strVRDEPort);
        mCbVRDEMethod->setCurrentIndex(mCbVRDEMethod->findText(vboxGlobal().toString(displayData.m_VRDEAuthType)));
        mLeVRDETimeout->setText(QString::number(displayData.m_uVRDETimeout));
        mCbMultipleConn->setChecked(displayData.m_fMultipleConnectionsAllowed);
    }
    else
        mTwDisplay->removeTab(1);

    /* Revalidate if possible: */
    if (mValidator) mValidator->revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsDisplay::putToCache()
{
    /* Prepare audio data: */
    UIDataSettingsMachineDisplay displayData = m_cache.base();

    /* Gather display data: */
    displayData.m_iCurrentVRAM = mSlMemory->value();
    displayData.m_cMonitorCount = mSlMonitors->value();
    displayData.m_f3dAccelerationEnabled = mCb3D->isChecked();
#ifdef VBOX_WITH_VIDEOHWACCEL
    displayData.m_f2dAccelerationEnabled = mCb2DVideo->isChecked();
#endif /* VBOX_WITH_VIDEOHWACCEL */
    if (displayData.m_fVRDEServerSupported)
    {
        displayData.m_fVRDEServerEnabled = mCbVRDE->isChecked();
        displayData.m_strVRDEPort = mLeVRDEPort->text();
        displayData.m_VRDEAuthType = vboxGlobal().toAuthType(mCbVRDEMethod->currentText());
        displayData.m_uVRDETimeout = mLeVRDETimeout->text().toULong();
        displayData.m_fMultipleConnectionsAllowed = mCbMultipleConn->isChecked();
    }

    /* Cache display data: */
    m_cache.cacheCurrentData(displayData);
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsDisplay::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Check if display data was changed: */
    if (m_cache.wasChanged())
    {
        /* Get display data from cache: */
        const UIDataSettingsMachineDisplay &displayData = m_cache.data();

        /* Store display data: */
        if (isMachineOffline())
        {
            /* Video tab: */
            m_machine.SetVRAMSize(displayData.m_iCurrentVRAM);
            m_machine.SetMonitorCount(displayData.m_cMonitorCount);
            m_machine.SetAccelerate3DEnabled(displayData.m_f3dAccelerationEnabled);
#ifdef VBOX_WITH_VIDEOHWACCEL
            m_machine.SetAccelerate2DVideoEnabled(displayData.m_f2dAccelerationEnabled);
#endif /* VBOX_WITH_VIDEOHWACCEL */
        }
        /* Check if VRDE server still valid: */
        CVRDEServer vrdeServer = m_machine.GetVRDEServer();
        if (!vrdeServer.isNull())
        {
            /* Store VRDE data: */
            if (isMachineInValidMode())
            {
                vrdeServer.SetEnabled(displayData.m_fVRDEServerEnabled);
                vrdeServer.SetVRDEProperty("TCP/Ports", displayData.m_strVRDEPort);
                vrdeServer.SetAuthType(displayData.m_VRDEAuthType);
                vrdeServer.SetAuthTimeout(displayData.m_uVRDETimeout);
            }
            if (isMachineOffline() || isMachineSaved())
            {
                vrdeServer.SetAllowMultiConnection(displayData.m_fMultipleConnectionsAllowed);
            }
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsDisplay::setValidator (QIWidgetValidator *aVal)
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

bool UIMachineSettingsDisplay::revalidate (QString &aWarning, QString & /* aTitle */)
{
    /* Video RAM amount test */
    quint64 needBytes = VBoxGlobal::requiredVideoMemory (&m_machine, mSlMonitors->value());
    if (   vboxGlobal().shouldWarnAboutToLowVRAM(&m_machine)
        && (quint64)mSlMemory->value() * _1M < needBytes)
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
    else if (false) // m_bWddmMode && mCb3D->isChecked())
    {
        int cVal = mSlMonitors->value();
        needBytes += VBoxGlobal::required3DWddmOffscreenVideoMemory(&m_machine, cVal);
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
    return true;
}

void UIMachineSettingsDisplay::setOrderAfter (QWidget *aWidget)
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

void UIMachineSettingsDisplay::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIMachineSettingsDisplay::retranslateUi (this);

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

void UIMachineSettingsDisplay::valueChangedVRAM (int aVal)
{
    mLeMemory->setText (QString().setNum (aVal));
}

void UIMachineSettingsDisplay::textChangedVRAM (const QString &aText)
{
    mSlMemory->setValue (aText.toInt());
}

void UIMachineSettingsDisplay::valueChangedMonitors (int aVal)
{
    mLeMonitors->setText (QString().setNum (aVal));
    checkVRAMRequirements();
}

void UIMachineSettingsDisplay::textChangedMonitors (const QString &aText)
{
    mSlMonitors->setValue (aText.toInt());
}

void UIMachineSettingsDisplay::checkVRAMRequirements()
{
    int cVal = mSlMonitors->value();
    /* The memory requirements have changed too. */
    quint64 needMBytes = VBoxGlobal::requiredVideoMemory (&m_machine, cVal) / _1M;
    /* Limit the maximum memory to save careless users from setting useless big values */
#ifdef VBOX_WITH_VIDEOHWACCEL
    if (mCb2DVideo->isChecked())
    {
        needMBytes += VBoxGlobal::required2DOffscreenVideoMemory() / _1M;
    }
#endif
#if 0 // def VBOX_WITH_CRHGSMI
    if (m_bWddmMode && mCb3D->isChecked())
    {
        needMBytes += VBoxGlobal::required3DWddmOffscreenVideoMemory(&m_machine, cVal) / _1M;
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

void UIMachineSettingsDisplay::polishPage()
{
    /* Video tab: */
    mLbMemory->setEnabled(isMachineOffline());
    mLbMemoryMin->setEnabled(isMachineOffline());
    mLbMemoryMax->setEnabled(isMachineOffline());
    mLbMemoryUnit->setEnabled(isMachineOffline());
    mSlMemory->setEnabled(isMachineOffline());
    mLeMemory->setEnabled(isMachineOffline());
    mLbMonitors->setEnabled(isMachineOffline());
    mLbMonitorsMin->setEnabled(isMachineOffline());
    mLbMonitorsMax->setEnabled(isMachineOffline());
    mLbMonitorsUnit->setEnabled(isMachineOffline());
    mSlMonitors->setEnabled(isMachineOffline());
    mLeMonitors->setEnabled(isMachineOffline());
    mLbOptions->setEnabled(isMachineOffline());
    mCb3D->setEnabled(isMachineOffline());
#ifdef VBOX_WITH_VIDEOHWACCEL
    mCb2DVideo->setEnabled(isMachineOffline());
#endif /* VBOX_WITH_VIDEOHWACCEL */
    /* VRDE tab: */
    mCbVRDE->setEnabled(isMachineInValidMode());
    mLbVRDPPort->setEnabled(isMachineInValidMode());
    mLeVRDEPort->setEnabled(isMachineInValidMode());
    mLbVRDPMethod->setEnabled(isMachineInValidMode());
    mCbVRDEMethod->setEnabled(isMachineInValidMode());
    mLbVRDPTimeout->setEnabled(isMachineInValidMode());
    mLeVRDETimeout->setEnabled(isMachineInValidMode());
    mLbOptions2->setEnabled(isMachineOffline() || isMachineSaved());
    mCbMultipleConn->setEnabled(isMachineOffline() || isMachineSaved());
}

