/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsDisplay class implementation
 */

/*
 * Copyright (C) 2008-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QDesktopWidget>

/* GUI includes: */
#include "QIWidgetValidator.h"
#include "UIMachineSettingsDisplay.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIConverter.h"

/* COM includes: */
#include "CVRDEServer.h"

UIMachineSettingsDisplay::UIMachineSettingsDisplay()
    : m_pValidator(0)
    , m_iMinVRAM(0)
    , m_iMaxVRAM(0)
    , m_iMaxVRAMVisible(0)
    , m_iInitialVRAM(0)
#ifdef VBOX_WITH_VIDEOHWACCEL
    , m_f2DVideoAccelerationSupported(false)
#endif /* VBOX_WITH_VIDEOHWACCEL */
#ifdef VBOX_WITH_CRHGSMI
    , m_fWddmModeSupported(false)
#endif /* VBOX_WITH_CRHGSMI */
{
    /* Prepare: */
    prepare();
}

void UIMachineSettingsDisplay::setGuestOSType(CGuestOSType guestOSType)
{
    /* Check if guest os type changed: */
    if (m_guestOSType == guestOSType)
        return;

    /* Remember new guest os type: */
    m_guestOSType = guestOSType;

#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Check if 2D video acceleration supported by the guest OS type: */
    QString strguestOSTypeFamily = m_guestOSType.GetFamilyId();
    m_f2DVideoAccelerationSupported = strguestOSTypeFamily == "Windows";
#endif /* VBOX_WITH_VIDEOHWACCEL */
#ifdef VBOX_WITH_CRHGSMI
    /* Check if WDDM mode supported by the guest OS type: */
    QString strguestOSTypeId = m_guestOSType.GetId();
    m_fWddmModeSupported = VBoxGlobal::isWddmCompatibleOsType(strguestOSTypeId);
#endif /* VBOX_WITH_CRHGSMI */

    /* Recheck video RAM requirement: */
    checkVRAMRequirements();
}

#ifdef VBOX_WITH_VIDEOHWACCEL
bool UIMachineSettingsDisplay::isAcceleration2DVideoSelected() const
{
    return mCb2DVideo->isChecked();
}
#endif /* VBOX_WITH_VIDEOHWACCEL */

/* Load data to cache from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsDisplay::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_cache.clear();

    /* Prepare display data: */
    UIDataSettingsMachineDisplay displayData;

    /* Cache Video data: */
    displayData.m_iCurrentVRAM = m_machine.GetVRAMSize();
    displayData.m_cGuestScreenCount = m_machine.GetMonitorCount();
    displayData.m_f3dAccelerationEnabled = m_machine.GetAccelerate3DEnabled();
#ifdef VBOX_WITH_VIDEOHWACCEL
    displayData.m_f2dAccelerationEnabled = m_machine.GetAccelerate2DVideoEnabled();
#endif /* VBOX_WITH_VIDEOHWACCEL */

    /* Check if Remote Display server is valid: */
    CVRDEServer remoteDisplayServer = m_machine.GetVRDEServer();
    displayData.m_fRemoteDisplayServerSupported = !remoteDisplayServer.isNull();
    if (!remoteDisplayServer.isNull())
    {
        /* Cache Remote Display data: */
        displayData.m_fRemoteDisplayServerEnabled = remoteDisplayServer.GetEnabled();
        displayData.m_strRemoteDisplayPort = remoteDisplayServer.GetVRDEProperty("TCP/Ports");
        displayData.m_remoteDisplayAuthType = remoteDisplayServer.GetAuthType();
        displayData.m_uRemoteDisplayTimeout = remoteDisplayServer.GetAuthTimeout();
        displayData.m_fRemoteDisplayMultiConnAllowed = remoteDisplayServer.GetAllowMultiConnection();
    }

    /* Cache Video Capture data: */
    displayData.m_fVideoCaptureEnabled = m_machine.GetVideoCaptureEnabled();
    displayData.m_strVideoCaptureFolder = QFileInfo(m_machine.GetSettingsFilePath()).absolutePath();
    displayData.m_strVideoCaptureFilePath = m_machine.GetVideoCaptureFile();
    displayData.m_iVideoCaptureFrameWidth = m_machine.GetVideoCaptureWidth();
    displayData.m_iVideoCaptureFrameHeight = m_machine.GetVideoCaptureHeight();
    displayData.m_iVideoCaptureFrameRate = m_machine.GetVideoCaptureFps();
    displayData.m_iVideoCaptureBitRate = m_machine.GetVideoCaptureRate();

    /* Initialize other variables: */
    m_iInitialVRAM = RT_MIN(displayData.m_iCurrentVRAM, m_iMaxVRAM);

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

    /* Load Video data to page: */
    m_pSliderScreeens->setValue(displayData.m_cGuestScreenCount);
    mCb3D->setChecked(displayData.m_f3dAccelerationEnabled);
#ifdef VBOX_WITH_VIDEOHWACCEL
    mCb2DVideo->setChecked(displayData.m_f2dAccelerationEnabled);
#endif /* VBOX_WITH_VIDEOHWACCEL */
    checkVRAMRequirements();
    m_pSliderMemory->setValue(displayData.m_iCurrentVRAM);

    /* If Remote Display server is supported: */
    if (displayData.m_fRemoteDisplayServerSupported)
    {
        /* Load Remote Display data to page: */
        m_pCheckboxRemoteDisplay->setChecked(displayData.m_fRemoteDisplayServerEnabled);
        m_pEditorRemoteDisplayPort->setText(displayData.m_strRemoteDisplayPort);
        m_pComboRemoteDisplayAuthMethod->setCurrentIndex(m_pComboRemoteDisplayAuthMethod->findText(gpConverter->toString(displayData.m_remoteDisplayAuthType)));
        m_pEditorRemoteDisplayTimeout->setText(QString::number(displayData.m_uRemoteDisplayTimeout));
        m_pCheckboxMultipleConn->setChecked(displayData.m_fRemoteDisplayMultiConnAllowed);
    }

    /* Load Video Capture data to page: */
    m_pCheckboxVideoCapture->setChecked(displayData.m_fVideoCaptureEnabled);
    m_pEditorVideoCapturePath->setHomeDir(displayData.m_strVideoCaptureFolder);
    m_pEditorVideoCapturePath->setPath(displayData.m_strVideoCaptureFilePath);
    m_pEditorVideoCaptureWidth->setValue(displayData.m_iVideoCaptureFrameWidth);
    m_pEditorVideoCaptureHeight->setValue(displayData.m_iVideoCaptureFrameHeight);
    m_pEditorVideoCaptureFrameRate->setValue(displayData.m_iVideoCaptureFrameRate);
    m_pEditorVideoCaptureBitRate->setValue(displayData.m_iVideoCaptureBitRate);

    /* Polish page finally: */
    polishPage();

    /* Revalidate if possible: */
    if (m_pValidator)
        m_pValidator->revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsDisplay::putToCache()
{
    /* Prepare audio data: */
    UIDataSettingsMachineDisplay displayData = m_cache.base();

    /* Gather Video data from page: */
    displayData.m_iCurrentVRAM = m_pSliderMemory->value();
    displayData.m_cGuestScreenCount = m_pSliderScreeens->value();
    displayData.m_f3dAccelerationEnabled = mCb3D->isChecked();
#ifdef VBOX_WITH_VIDEOHWACCEL
    displayData.m_f2dAccelerationEnabled = mCb2DVideo->isChecked();
#endif /* VBOX_WITH_VIDEOHWACCEL */

    /* If Remote Display server is supported: */
    if (displayData.m_fRemoteDisplayServerSupported)
    {
        /* Gather Remote Display data from page: */
        displayData.m_fRemoteDisplayServerEnabled = m_pCheckboxRemoteDisplay->isChecked();
        displayData.m_strRemoteDisplayPort = m_pEditorRemoteDisplayPort->text();
        displayData.m_remoteDisplayAuthType = gpConverter->fromString<KAuthType>(m_pComboRemoteDisplayAuthMethod->currentText());
        displayData.m_uRemoteDisplayTimeout = m_pEditorRemoteDisplayTimeout->text().toULong();
        displayData.m_fRemoteDisplayMultiConnAllowed = m_pCheckboxMultipleConn->isChecked();
    }

    /* Gather Video Capture data from page: */
    displayData.m_fVideoCaptureEnabled = m_pCheckboxVideoCapture->isChecked();
    displayData.m_strVideoCaptureFilePath = m_pEditorVideoCapturePath->path();
    displayData.m_iVideoCaptureFrameWidth = m_pEditorVideoCaptureWidth->value();
    displayData.m_iVideoCaptureFrameHeight = m_pEditorVideoCaptureHeight->value();
    displayData.m_iVideoCaptureFrameRate = m_pEditorVideoCaptureFrameRate->value();
    displayData.m_iVideoCaptureBitRate = m_pEditorVideoCaptureBitRate->value();

    /* Cache display data: */
    m_cache.cacheCurrentData(displayData);
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsDisplay::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Make sure machine is in valid mode & display data was changed: */
    if (isMachineInValidMode() && m_cache.wasChanged())
    {
        /* Get display data from cache: */
        const UIDataSettingsMachineDisplay &displayData = m_cache.data();

        /* Make sure machine is 'offline': */
        if (isMachineOffline())
        {
            /* Store Video data: */
            m_machine.SetVRAMSize(displayData.m_iCurrentVRAM);
            m_machine.SetMonitorCount(displayData.m_cGuestScreenCount);
            m_machine.SetAccelerate3DEnabled(displayData.m_f3dAccelerationEnabled);
#ifdef VBOX_WITH_VIDEOHWACCEL
            m_machine.SetAccelerate2DVideoEnabled(displayData.m_f2dAccelerationEnabled);
#endif /* VBOX_WITH_VIDEOHWACCEL */
        }

        /* Check if Remote Display server still valid: */
        CVRDEServer remoteDisplayServer = m_machine.GetVRDEServer();
        if (!remoteDisplayServer.isNull())
        {
            /* Store Remote Display data: */
            remoteDisplayServer.SetEnabled(displayData.m_fRemoteDisplayServerEnabled);
            remoteDisplayServer.SetVRDEProperty("TCP/Ports", displayData.m_strRemoteDisplayPort);
            remoteDisplayServer.SetAuthType(displayData.m_remoteDisplayAuthType);
            remoteDisplayServer.SetAuthTimeout(displayData.m_uRemoteDisplayTimeout);
            /* Make sure machine is 'offline' or 'saved': */
            if (isMachineOffline() || isMachineSaved())
                remoteDisplayServer.SetAllowMultiConnection(displayData.m_fRemoteDisplayMultiConnAllowed);
        }

        /* Store Video Capture data: */
        m_machine.SetVideoCaptureEnabled(displayData.m_fVideoCaptureEnabled);
        /* Make sure machine is 'offline': */
        if (isMachineOffline()) // TODO: Ask about isMachineSaved()
        {
            m_machine.SetVideoCaptureFile(displayData.m_strVideoCaptureFilePath);
            m_machine.SetVideoCaptureWidth(displayData.m_iVideoCaptureFrameWidth);
            m_machine.SetVideoCaptureHeight(displayData.m_iVideoCaptureFrameHeight);
            m_machine.SetVideoCaptureFps(displayData.m_iVideoCaptureFrameRate);
            m_machine.SetVideoCaptureRate(displayData.m_iVideoCaptureBitRate);
            QVector<BOOL> screens(m_machine.GetMonitorCount(), true);
            m_machine.SetVideoCaptureScreens(screens);
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsDisplay::setValidator(QIWidgetValidator *pValidator)
{
    m_pValidator = pValidator;
    connect(mCb3D, SIGNAL(stateChanged(int)), m_pValidator, SLOT(revalidate()));
#ifdef VBOX_WITH_VIDEOHWACCEL
    connect(mCb2DVideo, SIGNAL(stateChanged(int)), m_pValidator, SLOT(revalidate()));
#endif /* VBOX_WITH_VIDEOHWACCEL */
    connect(m_pCheckboxRemoteDisplay, SIGNAL(toggled(bool)), m_pValidator, SLOT(revalidate()));
    connect(m_pEditorRemoteDisplayPort, SIGNAL(textChanged(const QString&)), m_pValidator, SLOT(revalidate()));
    connect(m_pEditorRemoteDisplayTimeout, SIGNAL(textChanged(const QString&)), m_pValidator, SLOT(revalidate()));
}

bool UIMachineSettingsDisplay::revalidate(QString &strWarning, QString& /* strTitle */)
{
    /* Check if video RAM requirement changed first: */
    checkVRAMRequirements();

    if (mCb3D->isChecked() && !vboxGlobal().is3DAvailable())
    {
        strWarning = tr("you enabled 3D acceleration. However, 3D acceleration is not "
                        "working on the current host setup so you will not be able to "
                        "start the VM.");
        return true;
    }

    /* Video RAM amount test: */
    if (shouldWeWarnAboutLowVideoMemory() && !m_guestOSType.isNull())
    {
        quint64 uNeedBytes = VBoxGlobal::requiredVideoMemory(m_guestOSType.GetId(), m_pSliderScreeens->value());

        /* Basic video RAM amount test: */
        if ((quint64)m_pSliderMemory->value() * _1M < uNeedBytes)
        {
            strWarning = tr("you have assigned less than <b>%1</b> of video memory which is "
                            "the minimum amount required to switch the virtual machine to "
                            "fullscreen or seamless mode.")
                            .arg(vboxGlobal().formatSize(uNeedBytes, 0, FormatSize_RoundUp));
            return true;
        }
#ifdef VBOX_WITH_VIDEOHWACCEL
        /* 2D acceleration video RAM amount test: */
        if (mCb2DVideo->isChecked() && m_f2DVideoAccelerationSupported)
        {
            uNeedBytes += VBoxGlobal::required2DOffscreenVideoMemory();
            if ((quint64)m_pSliderMemory->value() * _1M < uNeedBytes)
            {
                strWarning = tr("you have assigned less than <b>%1</b> of video memory which is "
                                "the minimum amount required for HD Video to be played efficiently.")
                                .arg(vboxGlobal().formatSize(uNeedBytes, 0, FormatSize_RoundUp));
                return true;
            }
        }
#endif /* VBOX_WITH_VIDEOHWACCEL */
#if 0
# ifdef VBOX_WITH_CRHGSMI
        if (mCb3D->isChecked() && m_fWddmModeSupported)
        {
            int cGuestScreenCount = m_pSliderScreeens->value();
            uNeedBytes += VBoxGlobal::required3DWddmOffscreenVideoMemory(m_guestOSType.GetId(), cGuestScreenCount);
            uNeedBytes = RT_MAX(uNeedBytes, 128 * _1M);
            uNeedBytes = RT_MIN(uNeedBytes, 256 * _1M);
            if ((quint64)m_pSliderMemory->value() * _1M < uNeedBytes)
            {
                strWarning = tr("you have 3D Acceleration enabled for a operation system which uses the WDDM video driver. "
                                "For maximal performance set the guest VRAM to at least <b>%1</b>.")
                                .arg(vboxGlobal().formatSize(uNeedBytes, 0, FormatSize_RoundUp));
                return true;
            }
        }
# endif /* VBOX_WITH_CRHGSMI */
#endif /* 0 */
    }

    /* Check mode availability: */
#ifdef VBOX_WITH_VIDEOHWACCEL
    /* 2D video acceleration is available for Windows guests only: */
    if (mCb2DVideo->isChecked() && !m_f2DVideoAccelerationSupported)
    {
        strWarning = tr("you have 2D Video Acceleration enabled. As 2D Video Acceleration "
                        "is supported for Windows guests only, this feature will be disabled.");
        return true;
    }
#endif /* VBOX_WITH_VIDEOHWACCEL */

    return true;
}

void UIMachineSettingsDisplay::setOrderAfter(QWidget *pWidget)
{
    /* Video tab-order */
    setTabOrder(pWidget, m_pTabWidget->focusProxy());
    setTabOrder(m_pTabWidget->focusProxy(), m_pSliderMemory);
    setTabOrder(m_pSliderMemory, m_pEditorMemory);
    setTabOrder(m_pEditorMemory, m_pSliderScreeens);
    setTabOrder(m_pSliderScreeens, m_pEditorScreens);
    setTabOrder(m_pEditorScreens, mCb3D);
#ifdef VBOX_WITH_VIDEOHWACCEL
    setTabOrder(mCb3D, mCb2DVideo);
    setTabOrder(mCb2DVideo, m_pCheckboxRemoteDisplay);
#else /* VBOX_WITH_VIDEOHWACCEL */
    setTabOrder(mCb3D, m_pCheckboxRemoteDisplay);
#endif /* !VBOX_WITH_VIDEOHWACCEL */

    /* Remote Display tab-order: */
    setTabOrder(m_pCheckboxRemoteDisplay, m_pEditorRemoteDisplayPort);
    setTabOrder(m_pEditorRemoteDisplayPort, m_pComboRemoteDisplayAuthMethod);
    setTabOrder(m_pComboRemoteDisplayAuthMethod, m_pEditorRemoteDisplayTimeout);
    setTabOrder(m_pEditorRemoteDisplayTimeout, m_pCheckboxMultipleConn);

    /* Video Capture tab-order: */
    setTabOrder(m_pCheckboxMultipleConn, m_pCheckboxVideoCapture);
    setTabOrder(m_pCheckboxVideoCapture, m_pEditorVideoCapturePath);
    setTabOrder(m_pEditorVideoCapturePath, m_pComboVideoCaptureSize);
    setTabOrder(m_pComboVideoCaptureSize, m_pEditorVideoCaptureWidth);
    setTabOrder(m_pEditorVideoCaptureWidth, m_pEditorVideoCaptureHeight);
    setTabOrder(m_pEditorVideoCaptureHeight, m_pComboVideoCaptureFrameRate);
    setTabOrder(m_pComboVideoCaptureFrameRate, m_pEditorVideoCaptureFrameRate);
    setTabOrder(m_pEditorVideoCaptureFrameRate, m_pComboVideoCaptureBitRate);
    setTabOrder(m_pComboVideoCaptureBitRate, m_pEditorVideoCaptureBitRate);
}

void UIMachineSettingsDisplay::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIMachineSettingsDisplay::retranslateUi(this);

    /* Video stuff: */
    CSystemProperties sys = vboxGlobal().virtualBox().GetSystemProperties();
    m_pLabelMemoryMin->setText(tr("<qt>%1&nbsp;MB</qt>").arg(m_iMinVRAM));
    m_pLabelMemoryMin->setText(tr("<qt>%1&nbsp;MB</qt>").arg(m_iMaxVRAMVisible));
    m_pLabelScreensMin->setText(tr("<qt>%1</qt>").arg(1));
    m_pLabelScreensMax->setText(tr("<qt>%1</qt>").arg(sys.GetMaxGuestMonitors()));

    /* Remote Display stuff: */
    m_pComboRemoteDisplayAuthMethod->setItemText(0, gpConverter->toString(KAuthType_Null));
    m_pComboRemoteDisplayAuthMethod->setItemText(1, gpConverter->toString(KAuthType_External));
    m_pComboRemoteDisplayAuthMethod->setItemText(2, gpConverter->toString(KAuthType_Guest));

    /* Video Capture stuff: */
    m_pComboVideoCaptureSize->setItemText(0, tr("User Defined"));
    m_pComboVideoCaptureFrameRate->setItemText(0, tr("User Defined"));
    m_pComboVideoCaptureBitRate->setItemText(0, tr("User Defined"));
}

void UIMachineSettingsDisplay::polishPage()
{
    /* Get system data from cache: */
    const UIDataSettingsMachineDisplay &displayData = m_cache.base();

    /* Video tab: */
    m_pContainerVideo->setEnabled(isMachineOffline());
#ifdef VBOX_WITH_VIDEOHWACCEL
    mCb2DVideo->setEnabled(VBoxGlobal::isAcceleration2DVideoAvailable());
#endif /* VBOX_WITH_VIDEOHWACCEL */

    /* Remote Display tab: */
    m_pTabWidget->setTabEnabled(1, displayData.m_fRemoteDisplayServerSupported);
    m_pContainerRemoteDisplay->setEnabled(isMachineInValidMode());
    m_pContainerRemoteDisplayOptions->setEnabled(m_pCheckboxRemoteDisplay->isChecked());
    m_pLabelRemoteDisplayOptions->setEnabled(isMachineOffline() || isMachineSaved());
    m_pCheckboxMultipleConn->setEnabled(isMachineOffline() || isMachineSaved());

    /* Video Capture tab: */
    m_pContainerVideoCapture->setEnabled(isMachineOffline()); // TODO: Ask about isMachineSaved()
    m_pContainerVideoCaptureOptions->setEnabled(m_pCheckboxVideoCapture->isChecked());
}

void UIMachineSettingsDisplay::sltValueChangedVRAM(int iValue)
{
    m_pEditorMemory->setText(QString::number(iValue));
}

void UIMachineSettingsDisplay::sltTextChangedVRAM(const QString &strText)
{
    m_pSliderMemory->setValue(strText.toInt());
}

void UIMachineSettingsDisplay::sltValueChangedScreens(int iValue)
{
    m_pEditorScreens->setText(QString::number(iValue));
    checkVRAMRequirements();
}

void UIMachineSettingsDisplay::sltTextChangedScreens(const QString &strText)
{
    m_pSliderScreeens->setValue(strText.toInt());
}

void UIMachineSettingsDisplay::sltHandleVideoCaptureSizeChange(int iCurrentIndex)
{
    /* Get the proposed size: */
    QSize videoCaptureSize = m_pComboVideoCaptureSize->itemData(iCurrentIndex).toSize();

    /* Make sure its valid: */
    if (!videoCaptureSize.isValid())
        return;

    /* Apply proposed size: */
    m_pEditorVideoCaptureWidth->setValue(videoCaptureSize.width());
    m_pEditorVideoCaptureHeight->setValue(videoCaptureSize.height());
}

void UIMachineSettingsDisplay::sltHandleVideoCaptureFrameRateChange(int iCurrentIndex)
{
    /* Get the proposed frame-rate: */
    int iVideoCaptureFrameRate = m_pComboVideoCaptureFrameRate->itemData(iCurrentIndex).toInt();

    /* Make sure its valid: */
    if (iVideoCaptureFrameRate == 0)
        return;

    /* Apply proposed frame-rate: */
    m_pEditorVideoCaptureFrameRate->setValue(iVideoCaptureFrameRate);
}

void UIMachineSettingsDisplay::sltHandleVideoCaptureBitRateChange(int iCurrentIndex)
{
    /* Get the proposed bit-rate: */
    int iVideoCaptureBitRate = m_pComboVideoCaptureBitRate->itemData(iCurrentIndex).toInt();

    /* Make sure its valid: */
    if (iVideoCaptureBitRate == 0)
        return;

    /* Apply proposed frame-rate: */
    m_pEditorVideoCaptureBitRate->setValue(iVideoCaptureBitRate);
}

void UIMachineSettingsDisplay::sltHandleVideoCaptureWidthChange()
{
    /* Look for preset: */
    lookForCorrespondingSizePreset();
}

void UIMachineSettingsDisplay::sltHandleVideoCaptureHeightChange()
{
    /* Look for preset: */
    lookForCorrespondingSizePreset();
}

void UIMachineSettingsDisplay::sltHandleVideoCaptureFrameRateChange()
{
    /* Look for preset: */
    lookForCorrespondingFrameRatePreset();
}

void UIMachineSettingsDisplay::sltHandleVideoCaptureBitRateChange()
{
    /* Look for preset: */
    lookForCorrespondingBitRatePreset();
}

void UIMachineSettingsDisplay::prepare()
{
    /* Apply UI decorations: */
    Ui::UIMachineSettingsDisplay::setupUi(this);

    /* Prepare tabs: */
    prepareVideoTab();
    prepareRemoteDisplayTab();
    prepareVideoCaptureTab();

    /* Translate finally: */
    retranslateUi();
}

void UIMachineSettingsDisplay::prepareVideoTab()
{
    /* Prepare variables: */
    CSystemProperties sys = vboxGlobal().virtualBox().GetSystemProperties();
    m_iMinVRAM = sys.GetMinGuestVRAM();
    m_iMaxVRAM = sys.GetMaxGuestVRAM();
    m_iMaxVRAMVisible = m_iMaxVRAM;
#if (QT_VERSION >= 0x040600)
    const uint cHostScreens = QApplication::desktop()->screenCount();
#else /* (QT_VERSION >= 0x040600) */
    const uint cHostScreens = QApplication::desktop()->numScreens();
#endif /* !(QT_VERSION >= 0x040600) */
    const uint cMinGuestScreens = 1;
    const uint cMaxGuestScreens = sys.GetMaxGuestMonitors();

    /* Setup validators: */
    m_pEditorMemory->setValidator(new QIntValidator(m_iMinVRAM, m_iMaxVRAMVisible, this));
    m_pEditorScreens->setValidator(new QIntValidator(cMinGuestScreens, cMaxGuestScreens, this));
    m_pEditorRemoteDisplayPort->setValidator(new QRegExpValidator(QRegExp("(([0-9]{1,5}(\\-[0-9]{1,5}){0,1}),)*([0-9]{1,5}(\\-[0-9]{1,5}){0,1})"), this));
    m_pEditorRemoteDisplayTimeout->setValidator(new QIntValidator(this));

    /* Setup connections: */
    connect(m_pSliderMemory, SIGNAL(valueChanged(int)), this, SLOT(sltValueChangedVRAM(int)));
    connect(m_pEditorMemory, SIGNAL(textChanged(const QString&)), this, SLOT(sltTextChangedVRAM(const QString&)));
    connect(m_pSliderScreeens, SIGNAL(valueChanged(int)), this, SLOT(sltValueChangedScreens(int)));
    connect(m_pEditorScreens, SIGNAL(textChanged(const QString&)), this, SLOT(sltTextChangedScreens(const QString&)));

    /* Setup widgets: */
    m_pSliderMemory->setPageStep(calcPageStep(m_iMaxVRAMVisible));
    m_pSliderMemory->setSingleStep(m_pSliderMemory->pageStep() / 4);
    m_pSliderMemory->setTickInterval(m_pSliderMemory->pageStep());
    m_pSliderScreeens->setPageStep(1);
    m_pSliderScreeens->setSingleStep(1);
    m_pSliderScreeens->setTickInterval(1);

    /* Setup the scale so that ticks are at page step boundaries: */
    m_pSliderMemory->setMinimum((m_iMinVRAM / m_pSliderMemory->pageStep()) * m_pSliderMemory->pageStep());
    m_pSliderMemory->setMaximum(m_iMaxVRAMVisible);
    m_pSliderMemory->setSnappingEnabled(true);
    m_pSliderMemory->setErrorHint(0, 1);
    m_pSliderScreeens->setMinimum(cMinGuestScreens);
    m_pSliderScreeens->setMaximum(cMaxGuestScreens);
    m_pSliderScreeens->setErrorHint(0, cMinGuestScreens);
    m_pSliderScreeens->setOptimalHint(cMinGuestScreens, cHostScreens);
    m_pSliderScreeens->setWarningHint(cHostScreens, cMaxGuestScreens);

    /* Limit min/max. size of QLineEdit: */
    m_pEditorMemory->setFixedWidthByText(QString().fill('8', 4));
    m_pEditorScreens->setFixedWidthByText(QString().fill('8', 4));

    /* Ensure value and validation is updated: */
    sltValueChangedVRAM(m_pSliderMemory->value());
    sltValueChangedScreens(m_pSliderScreeens->value());

#ifndef VBOX_WITH_VIDEOHWACCEL
    /* Hide check-box if not supported: */
    mCb2DVideo->setVisible(false);
#endif /* VBOX_WITH_VIDEOHWACCEL */
}

void UIMachineSettingsDisplay::prepareRemoteDisplayTab()
{
    /* Prepare auth-method combo: */
    m_pComboRemoteDisplayAuthMethod->insertItem(0, ""); /* KAuthType_Null */
    m_pComboRemoteDisplayAuthMethod->insertItem(1, ""); /* KAuthType_External */
    m_pComboRemoteDisplayAuthMethod->insertItem(2, ""); /* KAuthType_Guest */
}

void UIMachineSettingsDisplay::prepareVideoCaptureTab()
{
    /* Prepare filepath selector: */
    m_pEditorVideoCapturePath->setMode(VBoxFilePathSelectorWidget::Mode_File_Save);

    /* Prepare frame-size combo-box: */
    m_pComboVideoCaptureSize->addItem(""); /* User Defined */
    m_pComboVideoCaptureSize->addItem("320 x 200 (16:10)",   QVariant(QSize(320, 200)));
    m_pComboVideoCaptureSize->addItem("640 x 480 (4:3)",     QVariant(QSize(640, 480)));
    m_pComboVideoCaptureSize->addItem("720 x 480 (3:2)",     QVariant(QSize(720, 480)));
    m_pComboVideoCaptureSize->addItem("800 x 600 (4:3)",     QVariant(QSize(800, 600)));
    m_pComboVideoCaptureSize->addItem("1024 x 768 (4:3)",    QVariant(QSize(1024, 768)));
    m_pComboVideoCaptureSize->addItem("1280 x 720 (16:9)",   QVariant(QSize(1280, 720)));
    m_pComboVideoCaptureSize->addItem("1280 x 800 (16:10)",  QVariant(QSize(1280, 800)));
    m_pComboVideoCaptureSize->addItem("1366 x 768 (16:9)",   QVariant(QSize(1366, 768)));
    m_pComboVideoCaptureSize->addItem("1440 x 900 (16:10)",  QVariant(QSize(1440, 900)));
    m_pComboVideoCaptureSize->addItem("1600 x 900 (16:9)",   QVariant(QSize(1600, 900)));
    m_pComboVideoCaptureSize->addItem("1680 x 1050 (16:10)", QVariant(QSize(1680, 1050)));
    m_pComboVideoCaptureSize->addItem("1920 x 1080 (16:9)",  QVariant(QSize(1920, 1080)));
    m_pComboVideoCaptureSize->addItem("1920 x 1200 (16:10)", QVariant(QSize(1920, 1200)));
    connect(m_pComboVideoCaptureSize, SIGNAL(currentIndexChanged(int)), this, SLOT(sltHandleVideoCaptureSizeChange(int)));

    /* Prepare frame-width/height spin-boxes: */
    vboxGlobal().setMinimumWidthAccordingSymbolCount(m_pEditorVideoCaptureWidth, 5);
    vboxGlobal().setMinimumWidthAccordingSymbolCount(m_pEditorVideoCaptureHeight, 5);
    m_pEditorVideoCaptureWidth->setMinimum(16);
    m_pEditorVideoCaptureWidth->setMaximum(1920);
    m_pEditorVideoCaptureHeight->setMinimum(16);
    m_pEditorVideoCaptureHeight->setMaximum(1200);
    connect(m_pEditorVideoCaptureWidth, SIGNAL(valueChanged(int)), this, SLOT(sltHandleVideoCaptureWidthChange()));
    connect(m_pEditorVideoCaptureHeight, SIGNAL(valueChanged(int)), this, SLOT(sltHandleVideoCaptureHeightChange()));

    /* Prepare frame-rate combo-box: */
    m_pComboVideoCaptureFrameRate->addItem(""); /* User Defined */
    m_pComboVideoCaptureFrameRate->addItem("1",  QVariant(1));
    m_pComboVideoCaptureFrameRate->addItem("2",  QVariant(2));
    m_pComboVideoCaptureFrameRate->addItem("5",  QVariant(5));
    m_pComboVideoCaptureFrameRate->addItem("10", QVariant(10));
    m_pComboVideoCaptureFrameRate->addItem("15", QVariant(15));
    m_pComboVideoCaptureFrameRate->addItem("20", QVariant(20));
    m_pComboVideoCaptureFrameRate->addItem("25", QVariant(25));
    m_pComboVideoCaptureFrameRate->addItem("30", QVariant(30));
    connect(m_pComboVideoCaptureFrameRate, SIGNAL(currentIndexChanged(int)), this, SLOT(sltHandleVideoCaptureFrameRateChange(int)));

    /* Prepare frame-rate spin-box: */
    vboxGlobal().setMinimumWidthAccordingSymbolCount(m_pEditorVideoCaptureFrameRate, 3);
    m_pEditorVideoCaptureFrameRate->setMinimum(1);
    m_pEditorVideoCaptureFrameRate->setMaximum(30);
    connect(m_pEditorVideoCaptureFrameRate, SIGNAL(valueChanged(int)), this, SLOT(sltHandleVideoCaptureFrameRateChange()));

    /* Prepare bit-rate combo-box: */
    m_pComboVideoCaptureBitRate->addItem(""); /* User Defined */
    m_pComboVideoCaptureBitRate->addItem("32",   QVariant(32));
    m_pComboVideoCaptureBitRate->addItem("64",   QVariant(64));
    m_pComboVideoCaptureBitRate->addItem("128",  QVariant(128));
    m_pComboVideoCaptureBitRate->addItem("160",  QVariant(160));
    m_pComboVideoCaptureBitRate->addItem("256",  QVariant(256));
    m_pComboVideoCaptureBitRate->addItem("320",  QVariant(320));
    m_pComboVideoCaptureBitRate->addItem("512",  QVariant(512));
    m_pComboVideoCaptureBitRate->addItem("1024", QVariant(1024));
    m_pComboVideoCaptureBitRate->addItem("2048", QVariant(2048));
    connect(m_pComboVideoCaptureBitRate, SIGNAL(currentIndexChanged(int)), this, SLOT(sltHandleVideoCaptureBitRateChange(int)));

    /* Prepare bit-rate spin-box: */
    vboxGlobal().setMinimumWidthAccordingSymbolCount(m_pEditorVideoCaptureBitRate, 5);
    m_pEditorVideoCaptureBitRate->setMinimum(32);
    m_pEditorVideoCaptureBitRate->setMaximum(2048);
    connect(m_pEditorVideoCaptureBitRate, SIGNAL(valueChanged(int)), this, SLOT(sltHandleVideoCaptureBitRateChange()));
}

void UIMachineSettingsDisplay::checkVRAMRequirements()
{
    /* Make sure guest OS type is set: */
    if (m_guestOSType.isNull())
        return;

    /* Get monitors count and base video memory requirements: */
    int cGuestScreenCount = m_pSliderScreeens->value();
    quint64 uNeedMBytes = VBoxGlobal::requiredVideoMemory(m_guestOSType.GetId(), cGuestScreenCount) / _1M;

    /* Initial values: */
    m_iMaxVRAMVisible = cGuestScreenCount * 32;
    if (m_iMaxVRAMVisible < 128)
        m_iMaxVRAMVisible = 128;
    if (m_iMaxVRAMVisible < m_iInitialVRAM)
        m_iMaxVRAMVisible = m_iInitialVRAM;

#ifdef VBOX_WITH_VIDEOHWACCEL
    if (mCb2DVideo->isChecked() && m_f2DVideoAccelerationSupported)
    {
        uNeedMBytes += VBoxGlobal::required2DOffscreenVideoMemory() / _1M;
    }
#endif /* VBOX_WITH_VIDEOHWACCEL */
#ifdef VBOX_WITH_CRHGSMI
    if (mCb3D->isChecked() && m_fWddmModeSupported)
    {
# if 0
        uNeedMBytes += VBoxGlobal::required3DWddmOffscreenVideoMemory(m_guestOSType.GetId(), cGuestScreenCount) / _1M;
        uNeedMBytes = RT_MAX(uNeedMBytes, 128);
        uNeedMBytes = RT_MIN(uNeedMBytes, 256);
# endif
        m_iMaxVRAMVisible = 256;
    }
#endif /* VBOX_WITH_CRHGSMI */

    m_pSliderMemory->setWarningHint(1, uNeedMBytes);
    m_pSliderMemory->setPageStep(calcPageStep(m_iMaxVRAMVisible));
    m_pSliderMemory->setMaximum(m_iMaxVRAMVisible);
    m_pSliderMemory->setOptimalHint(uNeedMBytes, m_iMaxVRAMVisible);
    m_pEditorMemory->setValidator(new QIntValidator(m_iMinVRAM, m_iMaxVRAMVisible, this));
    m_pLabelMemoryMin->setText(tr("<qt>%1&nbsp;MB</qt>").arg(m_iMaxVRAMVisible));
}

bool UIMachineSettingsDisplay::shouldWeWarnAboutLowVideoMemory()
{
    bool fResult = true;

    QStringList excludingOSList = QStringList()
        << "Other" << "DOS" << "Netware" << "L4" << "QNX" << "JRockitVE";
    if (excludingOSList.contains(m_guestOSType.GetId()))
        fResult = false;

    return fResult;
}

/* static */
int UIMachineSettingsDisplay::calcPageStep(int iMax)
{
    /* Reasonable max. number of page steps is 32. */
    uint page = ((uint)iMax + 31) / 32;
    /* Make it a power of 2: */
    uint p = page, p2 = 0x1;
    while ((p >>= 1))
        p2 <<= 1;
    if (page != p2)
        p2 <<= 1;
    if (p2 < 4)
        p2 = 4;
    return (int)p2;
}

void UIMachineSettingsDisplay::lookForCorrespondingSizePreset()
{
    /* Look for video-capture size preset: */
    lookForCorrespondingPreset(m_pComboVideoCaptureSize,
                               QSize(m_pEditorVideoCaptureWidth->value(),
                                     m_pEditorVideoCaptureHeight->value()));
}

void UIMachineSettingsDisplay::lookForCorrespondingFrameRatePreset()
{
    /* Look for video-capture frame-rate preset: */
    lookForCorrespondingPreset(m_pComboVideoCaptureFrameRate,
                               m_pEditorVideoCaptureFrameRate->value());
}

void UIMachineSettingsDisplay::lookForCorrespondingBitRatePreset()
{
    /* Look for video-capture bit-rate preset: */
    lookForCorrespondingPreset(m_pComboVideoCaptureBitRate,
                               m_pEditorVideoCaptureBitRate->value());
}

/* static */
void UIMachineSettingsDisplay::lookForCorrespondingPreset(QComboBox *pWhere, const QVariant &whichData)
{
    /* Use passed iterator to look for corresponding preset of passed combo-box: */
    int iLookupResult = pWhere->findData(whichData);
    if (iLookupResult != -1 && pWhere->currentIndex() != iLookupResult)
        pWhere->setCurrentIndex(iLookupResult);
    else if (iLookupResult == -1 && pWhere->currentIndex() != 0)
        pWhere->setCurrentIndex(0);
}

