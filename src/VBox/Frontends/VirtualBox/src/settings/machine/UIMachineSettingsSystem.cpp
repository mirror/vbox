/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsSystem class implementation
 */

/*
 * Copyright (C) 2008-2012 Oracle Corporation
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
#include <QHeaderView>

/* GUI includes: */
#include "QIWidgetValidator.h"
#include "UIIconPool.h"
#include "VBoxGlobal.h"
#include "UIMachineSettingsSystem.h"
#include "UIConverter.h"

/* COM includes: */
#include "CBIOSSettings.h"

/* Other VBox includes: */
#include <iprt/cdefs.h>

UIMachineSettingsSystem::UIMachineSettingsSystem()
    : m_pValidator(0)
    , m_uMinGuestCPU(0), m_uMaxGuestCPU(0)
    , m_uMinGuestCPUExecCap(0), m_uMedGuestCPUExecCap(0), m_uMaxGuestCPUExecCap(0)
    , m_fOHCIEnabled(false)
{
    /* Apply UI decorations: */
    Ui::UIMachineSettingsSystem::setupUi(this);

    /* Setup constants: */
    CSystemProperties properties = vboxGlobal().virtualBox().GetSystemProperties();
    uint hostCPUs = vboxGlobal().host().GetProcessorCount();
    m_uMinGuestCPU = properties.GetMinGuestCPUCount();
    m_uMaxGuestCPU = RT_MIN (2 * hostCPUs, properties.GetMaxGuestCPUCount());
    m_uMinGuestCPUExecCap = 1;
    m_uMedGuestCPUExecCap = 40;
    m_uMaxGuestCPUExecCap = 100;

    /* Populate possible boot items list.
     * Currently, it seems, we are supporting only 4 possible boot device types:
     * 1. Floppy, 2. DVD-ROM, 3. Hard Disk, 4. Network.
     * But maximum boot devices count supported by machine
     * should be retrieved through the ISystemProperties getter.
     * Moreover, possible boot device types are not listed in some separate Main vector,
     * so we should get them (randomely?) from the list of all device types.
     * Until there will be separate Main getter for list of supported boot device types,
     * this list will be hard-coded here... */
    int iPossibleBootListSize = qMin((ULONG)4, properties.GetMaxBootPosition());
    for (int iBootPosition = 1; iBootPosition <= iPossibleBootListSize; ++iBootPosition)
    {
        switch (iBootPosition)
        {
            case 1:
                m_possibleBootItems << KDeviceType_Floppy;
                break;
            case 2:
                m_possibleBootItems << KDeviceType_DVD;
                break;
            case 3:
                m_possibleBootItems << KDeviceType_HardDisk;
                break;
            case 4:
                m_possibleBootItems << KDeviceType_Network;
                break;
            default:
                break;
        }
    }

    /* Add all available devices types, so we could initially calculate the right size: */
    for (int i = 0; i < m_possibleBootItems.size(); ++i)
    {
        QListWidgetItem *pItem = new UIBootTableItem(m_possibleBootItems[i]);
        mTwBootOrder->addItem(pItem);
    }

    /* Setup validators: */
    mLeMemory->setValidator(new QIntValidator(mSlMemory->minRAM(), mSlMemory->maxRAM(), this));
    mLeCPU->setValidator(new QIntValidator(m_uMinGuestCPU, m_uMaxGuestCPU, this));
    mLeCPUExecCap->setValidator(new QIntValidator(m_uMinGuestCPUExecCap, m_uMaxGuestCPUExecCap, this));

    /* Setup RAM connections: */
    connect(mSlMemory, SIGNAL(valueChanged(int)), this, SLOT(valueChangedRAM(int)));
    connect(mLeMemory, SIGNAL(textChanged(const QString&)), this, SLOT(textChangedRAM(const QString&)));

    /* Setup boot-table connections: */
    connect(mTbBootItemUp, SIGNAL(clicked()), mTwBootOrder, SLOT(sltMoveItemUp()));
    connect(mTbBootItemDown, SIGNAL(clicked()), mTwBootOrder, SLOT(sltMoveItemDown()));
    connect(mTwBootOrder, SIGNAL(sigRowChanged(int)), this, SLOT(onCurrentBootItemChanged(int)));

    /* Setup CPU connections: */
    connect(mSlCPU, SIGNAL(valueChanged(int)), this, SLOT(valueChangedCPU(int)));
    connect(mLeCPU, SIGNAL(textChanged(const QString&)), this, SLOT(textChangedCPU(const QString&)));
    connect(mSlCPUExecCap, SIGNAL(valueChanged(int)), this, SLOT(sltValueChangedCPUExecCap(int)));
    connect(mLeCPUExecCap, SIGNAL(textChanged(const QString&)), this, SLOT(sltTextChangedCPUExecCap(const QString&)));

    /* Setup boot-table iconsets: */
    mTbBootItemUp->setIcon(UIIconPool::iconSet(":/list_moveup_16px.png", ":/list_moveup_disabled_16px.png"));
    mTbBootItemDown->setIcon(UIIconPool::iconSet(":/list_movedown_16px.png", ":/list_movedown_disabled_16px.png"));

#ifdef Q_WS_MAC
    /* We need a little space for the focus rect: */
    mLtBootOrder->setContentsMargins (3, 3, 3, 3);
    mLtBootOrder->setSpacing (3);
#endif /* Q_WS_MAC */

    /* Limit min/max size of QLineEdit: */
    mLeMemory->setFixedWidthByText(QString().fill('8', 5));
    /* Ensure mLeMemory value and validation is updated: */
    valueChangedRAM(mSlMemory->value());

    /* Setup cpu slider: */
    mSlCPU->setPageStep(1);
    mSlCPU->setSingleStep(1);
    mSlCPU->setTickInterval(1);
    /* Setup the scale so that ticks are at page step boundaries: */
    mSlCPU->setMinimum(m_uMinGuestCPU);
    mSlCPU->setMaximum(m_uMaxGuestCPU);
    mSlCPU->setOptimalHint(1, hostCPUs);
    mSlCPU->setWarningHint(hostCPUs, m_uMaxGuestCPU);
    /* Limit min/max. size of QLineEdit: */
    mLeCPU->setFixedWidthByText(QString().fill('8', 4));
    /* Ensure mLeMemory value and validation is updated: */
    valueChangedCPU(mSlCPU->value());

    /* Setup cpu cap slider: */
    mSlCPUExecCap->setPageStep(10);
    mSlCPUExecCap->setSingleStep(1);
    mSlCPUExecCap->setTickInterval(10);
    /* Setup the scale so that ticks are at page step boundaries: */
    mSlCPUExecCap->setMinimum(m_uMinGuestCPUExecCap);
    mSlCPUExecCap->setMaximum(m_uMaxGuestCPUExecCap);
    mSlCPUExecCap->setWarningHint(m_uMinGuestCPUExecCap, m_uMedGuestCPUExecCap);
    mSlCPUExecCap->setOptimalHint(m_uMedGuestCPUExecCap, m_uMaxGuestCPUExecCap);
    /* Limit min/max. size of QLineEdit: */
    mLeCPUExecCap->setFixedWidthByText(QString().fill('8', 4));
    /* Ensure mLeMemory value and validation is updated: */
    sltValueChangedCPUExecCap(mSlCPUExecCap->value());

    /* Populate chipset combo: */
    mCbChipset->addItem(gpConverter->toString(KChipsetType_PIIX3), QVariant(KChipsetType_PIIX3));
    mCbChipset->addItem(gpConverter->toString(KChipsetType_ICH9), QVariant(KChipsetType_ICH9));

    /* Install global event filter: */
    qApp->installEventFilter(this);

    /* Retranslate finally: */
    retranslateUi();
}

bool UIMachineSettingsSystem::isHWVirtExEnabled() const
{
    return mCbVirt->isChecked();
}

bool UIMachineSettingsSystem::isHIDEnabled() const
{
    return mCbUseAbsHID->isChecked();
}

KChipsetType UIMachineSettingsSystem::chipsetType() const
{
    return (KChipsetType)mCbChipset->itemData(mCbChipset->currentIndex()).toInt();
}

void UIMachineSettingsSystem::setOHCIEnabled(bool fEnabled)
{
    m_fOHCIEnabled = fEnabled;
}

/* Load data to cache from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsSystem::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_cache.clear();

    /* Prepare system data: */
    UIDataSettingsMachineSystem systemData;

    /* Load support flags: */
    systemData.m_fSupportedPAE = vboxGlobal().host().GetProcessorFeature(KProcessorFeature_PAE);
    systemData.m_fSupportedHwVirtEx = vboxGlobal().host().GetProcessorFeature(KProcessorFeature_HWVirtEx);

    /* Load motherboard data: */
    systemData.m_iRAMSize = m_machine.GetMemorySize();
    /* Load boot-items of current VM: */
    QList<KDeviceType> usedBootItems;
    for (int i = 1; i <= m_possibleBootItems.size(); ++i)
    {
        KDeviceType type = m_machine.GetBootOrder(i);
        if (type != KDeviceType_Null)
        {
            usedBootItems << type;
            UIBootItemData data;
            data.m_type = type;
            data.m_fEnabled = true;
            systemData.m_bootItems << data;
        }
    }
    /* Load other unique boot-items: */
    for (int i = 0; i < m_possibleBootItems.size(); ++i)
    {
        KDeviceType type = m_possibleBootItems[i];
        if (!usedBootItems.contains(type))
        {
            UIBootItemData data;
            data.m_type = type;
            data.m_fEnabled = false;
            systemData.m_bootItems << data;
        }
    }
    /* Load other motherboard data: */
    systemData.m_chipsetType = m_machine.GetChipsetType();
    systemData.m_fEnabledIoApic = m_machine.GetBIOSSettings().GetIOAPICEnabled();
    systemData.m_fEnabledEFI = m_machine.GetFirmwareType() >= KFirmwareType_EFI && m_machine.GetFirmwareType() <= KFirmwareType_EFIDUAL;
    systemData.m_fEnabledUTC = m_machine.GetRTCUseUTC();
    systemData.m_fEnabledAbsoluteHID = m_machine.GetPointingHIDType() == KPointingHIDType_USBTablet;

    /* Load CPU data: */
    systemData.m_cCPUCount = systemData.m_fSupportedHwVirtEx ? m_machine.GetCPUCount() : 1;
    systemData.m_iCPUExecCap = m_machine.GetCPUExecutionCap();
    systemData.m_fEnabledPAE = m_machine.GetCPUProperty(KCPUPropertyType_PAE);

    /* Load acceleration data: */
    systemData.m_fEnabledHwVirtEx = m_machine.GetHWVirtExProperty(KHWVirtExPropertyType_Enabled);
    systemData.m_fEnabledNestedPaging = m_machine.GetHWVirtExProperty(KHWVirtExPropertyType_NestedPaging);

    /* Cache system data: */
    m_cache.cacheInitialData(systemData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsSystem::getFromCache()
{
    /* Get system data from cache: */
    const UIDataSettingsMachineSystem &systemData = m_cache.base();

    /* Load motherboard data to page: */
    mSlMemory->setValue(systemData.m_iRAMSize);
    /* Remove any old data in the boot view: */
    QAbstractItemView *iv = qobject_cast <QAbstractItemView*> (mTwBootOrder);
    iv->model()->removeRows(0, iv->model()->rowCount());
    /* Apply internal variables data to QWidget(s): */
    for (int i = 0; i < systemData.m_bootItems.size(); ++i)
    {
        UIBootItemData data = systemData.m_bootItems[i];
        QListWidgetItem *pItem = new UIBootTableItem(data.m_type);
        pItem->setCheckState(data.m_fEnabled ? Qt::Checked : Qt::Unchecked);
        mTwBootOrder->addItem(pItem);
    }
    /* Load other motherboard data to page: */
    int iChipsetPosition = mCbChipset->findData(systemData.m_chipsetType);
    mCbChipset->setCurrentIndex(iChipsetPosition == -1 ? 0 : iChipsetPosition);
    mCbApic->setChecked(systemData.m_fEnabledIoApic);
    mCbEFI->setChecked(systemData.m_fEnabledEFI);
    mCbTCUseUTC->setChecked(systemData.m_fEnabledUTC);
    mCbUseAbsHID->setChecked(systemData.m_fEnabledAbsoluteHID);

    /* Load CPU data to page: */
    mSlCPU->setValue(systemData.m_cCPUCount);
    mSlCPUExecCap->setValue(systemData.m_iCPUExecCap);
    mCbPae->setChecked(systemData.m_fEnabledPAE);

    /* Load acceleration data to page: */
	mCbVirt->setChecked(systemData.m_fEnabledHwVirtEx);
    mCbNestedPaging->setChecked(systemData.m_fEnabledNestedPaging);

    /* Polish page finally: */
    polishPage();

    /* Revalidate if possible: */
    if (m_pValidator)
        m_pValidator->revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsSystem::putToCache()
{
    /* Prepare system data: */
    UIDataSettingsMachineSystem systemData = m_cache.base();

    /* Gather motherboard data: */
    systemData.m_iRAMSize = mSlMemory->value();
    /* Gather boot-table data: */
    systemData.m_bootItems.clear();
    for (int i = 0; i < mTwBootOrder->count(); ++i)
    {
        QListWidgetItem *pItem = mTwBootOrder->item(i);
        UIBootItemData data;
        data.m_type = static_cast<UIBootTableItem*>(pItem)->type();
        data.m_fEnabled = pItem->checkState() == Qt::Checked;
        systemData.m_bootItems << data;
    }
    /* Gather other motherboard data: */
    systemData.m_chipsetType = (KChipsetType)mCbChipset->itemData(mCbChipset->currentIndex()).toInt();
    systemData.m_fEnabledIoApic = mCbApic->isChecked() || mSlCPU->value() > 1 ||
                                  (KChipsetType)mCbChipset->itemData(mCbChipset->currentIndex()).toInt() == KChipsetType_ICH9;
    systemData.m_fEnabledEFI = mCbEFI->isChecked();
    systemData.m_fEnabledUTC = mCbTCUseUTC->isChecked();
    systemData.m_fEnabledAbsoluteHID = mCbUseAbsHID->isChecked();

    /* Gather CPU data: */
    systemData.m_cCPUCount = mSlCPU->value();
    systemData.m_iCPUExecCap = mSlCPUExecCap->value();
    systemData.m_fEnabledPAE = mCbPae->isChecked();

    /* Gather acceleration data: */
    systemData.m_fEnabledHwVirtEx = mCbVirt->checkState() == Qt::Checked || mSlCPU->value() > 1;
    systemData.m_fEnabledNestedPaging = mCbNestedPaging->isChecked();

    /* Cache system data: */
    m_cache.cacheCurrentData(systemData);
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsSystem::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Check if system data was changed: */
    if (m_cache.wasChanged())
    {
        /* Get system data from cache: */
        const UIDataSettingsMachineSystem &systemData = m_cache.data();

        /* Store system data: */
        if (isMachineOffline())
        {
            /* Motherboard tab: */
            m_machine.SetMemorySize(systemData.m_iRAMSize);
            /* Save boot-items of current VM: */
            int iBootIndex = 0;
            for (int i = 0; i < systemData.m_bootItems.size(); ++i)
            {
                if (systemData.m_bootItems[i].m_fEnabled)
                    m_machine.SetBootOrder(++iBootIndex, systemData.m_bootItems[i].m_type);
            }
            /* Save other unique boot-items: */
            for (int i = 0; i < systemData.m_bootItems.size(); ++i)
            {
                if (!systemData.m_bootItems[i].m_fEnabled)
                    m_machine.SetBootOrder(++iBootIndex, KDeviceType_Null);
            }
            m_machine.SetChipsetType(systemData.m_chipsetType);
            m_machine.GetBIOSSettings().SetIOAPICEnabled(systemData.m_fEnabledIoApic);
            m_machine.SetFirmwareType(systemData.m_fEnabledEFI ? KFirmwareType_EFI : KFirmwareType_BIOS);
            m_machine.SetRTCUseUTC(systemData.m_fEnabledUTC);
            m_machine.SetPointingHIDType(systemData.m_fEnabledAbsoluteHID ? KPointingHIDType_USBTablet : KPointingHIDType_PS2Mouse);

            /* Processor tab: */
            m_machine.SetCPUCount(systemData.m_cCPUCount);
            m_machine.SetCPUProperty(KCPUPropertyType_PAE, systemData.m_fEnabledPAE);

            /* Acceleration tab: */
            m_machine.SetHWVirtExProperty(KHWVirtExPropertyType_Enabled, systemData.m_fEnabledHwVirtEx);
            m_machine.SetHWVirtExProperty(KHWVirtExPropertyType_NestedPaging, systemData.m_fEnabledNestedPaging);
        }
        if (isMachineInValidMode())
        {
            /* Processor tab: */
            m_machine.SetCPUExecutionCap(systemData.m_iCPUExecCap);
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsSystem::setValidator(QIWidgetValidator *pValidator)
{
    /* Configure validation: */
    m_pValidator = pValidator;
    connect(mCbChipset, SIGNAL(currentIndexChanged(int)), m_pValidator, SLOT(revalidate()));
    connect(mCbApic, SIGNAL(stateChanged(int)), m_pValidator, SLOT(revalidate()));
    connect(mCbVirt, SIGNAL(stateChanged(int)), m_pValidator, SLOT(revalidate()));
    connect(mCbUseAbsHID, SIGNAL(stateChanged(int)), m_pValidator, SLOT(revalidate()));
}

bool UIMachineSettingsSystem::revalidate(QString &strWarning, QString& /* strTitle */)
{
    /* RAM amount test: */
    ulong uFullSize = vboxGlobal().host().GetMemorySize();
    if (mSlMemory->value() > (int)mSlMemory->maxRAMAlw())
    {
        strWarning = tr(
            "you have assigned more than <b>%1%</b> of your computer's memory "
            "(<b>%2</b>) to the virtual machine. Not enough memory is left "
            "for your host operating system. Please select a smaller amount.")
            .arg((unsigned)qRound((double)mSlMemory->maxRAMAlw() / uFullSize * 100.0))
            .arg(vboxGlobal().formatSize((uint64_t)uFullSize * _1M));
        return false;
    }
    if (mSlMemory->value() > (int)mSlMemory->maxRAMOpt())
    {
        strWarning = tr(
            "you have assigned more than <b>%1%</b> of your computer's memory "
            "(<b>%2</b>) to the virtual machine. There might not be enough memory "
            "left for your host operating system. Continue at your own risk.")
            .arg((unsigned)qRound((double)mSlMemory->maxRAMOpt() / uFullSize * 100.0))
            .arg(vboxGlobal().formatSize((uint64_t)uFullSize * _1M));
        return true;
    }

    /* VCPU amount test: */
    int cTotalCPUs = vboxGlobal().host().GetProcessorOnlineCount();
    if (mSlCPU->value() > 2 * cTotalCPUs)
    {
        strWarning = tr(
            "for performance reasons, the number of virtual CPUs attached to the "
            "virtual machine may not be more than twice the number of physical "
            "CPUs on the host (<b>%1</b>). Please reduce the number of virtual CPUs.")
            .arg(cTotalCPUs);
        return false;
    }
    if (mSlCPU->value() > cTotalCPUs)
    {
        strWarning = tr(
            "you have assigned more virtual CPUs to the virtual machine than "
            "the number of physical CPUs on your host system (<b>%1</b>). "
            "This is likely to degrade the performance of your virtual machine. "
            "Please consider reducing the number of virtual CPUs.")
            .arg(cTotalCPUs);
        return true;
    }

    /* VCPU IO-APIC test: */
    if (mSlCPU->value() > 1 && !mCbApic->isChecked())
    {
        strWarning = tr(
            "you have assigned more than one virtual CPU to this VM. "
            "This will not work unless the IO-APIC feature is also enabled. "
            "This will be done automatically when you accept the VM Settings "
            "by pressing the OK button.");
        return true;
    }

    /* VCPU VT-x/AMD-V test: */
    if (mSlCPU->value() > 1 && !mCbVirt->isChecked())
    {
        strWarning = tr(
            "you have assigned more than one virtual CPU to this VM. "
            "This will not work unless hardware virtualization (VT-x/AMD-V) is also enabled. "
            "This will be done automatically when you accept the VM Settings "
            "by pressing the OK button.");
        return true;
    }

    /* CPU execution cap is low: */
    if (mSlCPUExecCap->value() < (int)m_uMedGuestCPUExecCap)
    {
        strWarning = tr(
            "you have set the processor execution cap to a low value. "
            "This can make the machine feel slow to respond.");
        return true;
    }

    /* Chipset type & IO-APIC test: */
    if ((KChipsetType)mCbChipset->itemData(mCbChipset->currentIndex()).toInt() == KChipsetType_ICH9 && !mCbApic->isChecked())
    {
        strWarning = tr(
            "you have assigned ICH9 chipset type to this VM. "
            "It will not work properly unless the IO-APIC feature is also enabled. "
            "This will be done automatically when you accept the VM Settings "
            "by pressing the OK button.");
        return true;
    }

    /* HID dependency from OHCI feature: */
    if (mCbUseAbsHID->isChecked() && !m_fOHCIEnabled)
    {
        strWarning = tr(
            "you have enabled a USB HID (Human Interface Device). "
            "This will not work unless USB emulation is also enabled. "
            "This will be done automatically when you accept the VM Settings "
            "by pressing the OK button.");
        return true;
    }

    return true;
}

void UIMachineSettingsSystem::setOrderAfter(QWidget *pWidget)
{
    /* Motherboard tab-order: */
    setTabOrder(pWidget, mTwSystem->focusProxy());
    setTabOrder(mTwSystem->focusProxy(), mSlMemory);
    setTabOrder(mSlMemory, mLeMemory);
    setTabOrder(mLeMemory, mTwBootOrder);
    setTabOrder(mTwBootOrder, mTbBootItemUp);
    setTabOrder(mTbBootItemUp, mTbBootItemDown);
    setTabOrder(mTbBootItemDown, mCbChipset);
    setTabOrder(mCbChipset, mCbApic);
    setTabOrder(mCbApic, mCbEFI);
    setTabOrder(mCbEFI, mCbTCUseUTC);
    setTabOrder(mCbTCUseUTC, mCbUseAbsHID);

    /* Processor tab-order: */
    setTabOrder(mCbUseAbsHID, mSlCPU);
    setTabOrder(mSlCPU, mLeCPU);
    setTabOrder(mLeCPU, mSlCPUExecCap);
    setTabOrder(mSlCPUExecCap, mLeCPUExecCap);
    setTabOrder(mLeCPUExecCap, mCbPae);

    /* Acceleration tab-order: */
    setTabOrder(mCbPae, mCbVirt);
    setTabOrder(mCbVirt, mCbNestedPaging);
}

void UIMachineSettingsSystem::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsSystem::retranslateUi(this);

    /* Readjust the tree widget items size: */
    adjustBootOrderTWSize();

    /* Retranslate the memory slider legend: */
    mLbMemoryMin->setText(tr("<qt>%1&nbsp;MB</qt>").arg(mSlMemory->minRAM()));
    mLbMemoryMax->setText(tr("<qt>%1&nbsp;MB</qt>").arg(mSlMemory->maxRAM()));

    /* Retranslate the cpu slider legend: */
    mLbCPUMin->setText(tr("<qt>%1&nbsp;CPU</qt>", "%1 is 1 for now").arg(m_uMinGuestCPU));
    mLbCPUMax->setText(tr("<qt>%1&nbsp;CPUs</qt>", "%1 is host cpu count * 2 for now").arg(m_uMaxGuestCPU));

    /* Retranslate the cpu cap slider legend: */
    mLbCPUExecCapMin->setText(tr("<qt>%1%</qt>", "Min CPU execution cap in %").arg(m_uMinGuestCPUExecCap));
    mLbCPUExecCapMax->setText(tr("<qt>%1%</qt>", "Max CPU execution cap in %").arg(m_uMaxGuestCPUExecCap));

    /* Retranslate combo-box: */
    retranslateComboPointingChipsetType();
}

void UIMachineSettingsSystem::valueChangedRAM(int iValue)
{
    mLeMemory->setText(QString::number(iValue));
}

void UIMachineSettingsSystem::textChangedRAM(const QString &strText)
{
    mSlMemory->setValue(strText.toInt());
}

void UIMachineSettingsSystem::onCurrentBootItemChanged(int iCurrentItem)
{
    /* Update boot-order tool-buttons: */
    bool fEnabledUP = iCurrentItem > 0;
    bool fEnabledDOWN = iCurrentItem < mTwBootOrder->count() - 1;
    if ((mTbBootItemUp->hasFocus() && !fEnabledUP) ||
        (mTbBootItemDown->hasFocus() && !fEnabledDOWN))
        mTwBootOrder->setFocus();
    mTbBootItemUp->setEnabled(fEnabledUP);
    mTbBootItemDown->setEnabled(fEnabledDOWN);
}

void UIMachineSettingsSystem::adjustBootOrderTWSize()
{
    /* Adjust boot-table size: */
    mTwBootOrder->adjustSizeToFitContent();
    /* Update the layout system */
    if (mTabMotherboard->layout())
    {
        mTabMotherboard->layout()->activate();
        mTabMotherboard->layout()->update();
    }
}

void UIMachineSettingsSystem::valueChangedCPU(int iValue)
{
    mLeCPU->setText(QString::number(iValue));
}

void UIMachineSettingsSystem::textChangedCPU(const QString &strText)
{
    mSlCPU->setValue(strText.toInt());
}

void UIMachineSettingsSystem::sltValueChangedCPUExecCap(int iValue)
{
    mLeCPUExecCap->setText(QString::number(iValue));
}

void UIMachineSettingsSystem::sltTextChangedCPUExecCap(const QString &strText)
{
    mSlCPUExecCap->setValue(strText.toInt());
}

void UIMachineSettingsSystem::retranslateComboPointingChipsetType()
{
    /* For each the element in KPointingHIDType enum: */
    for (int iIndex = (int)KChipsetType_Null; iIndex < (int)KChipsetType_Max; ++iIndex)
    {
        /* Cast to the corresponding type: */
        KChipsetType type = (KChipsetType)iIndex;
        /* Look for the corresponding item: */
        int iCorrespondingIndex = mCbChipset->findData((int)type);
        /* Re-translate corresponding item if it was found: */
        if (iCorrespondingIndex != -1)
            mCbChipset->setItemText(iCorrespondingIndex, gpConverter->toString(type));
    }
}

void UIMachineSettingsSystem::polishPage()
{
    /* Get system data from cache: */
    const UIDataSettingsMachineSystem &systemData = m_cache.base();

    /* Motherboard tab: */
    mLbMemory->setEnabled(isMachineOffline());
    mLbMemoryMin->setEnabled(isMachineOffline());
    mLbMemoryMax->setEnabled(isMachineOffline());
    mLbMemoryUnit->setEnabled(isMachineOffline());
    mSlMemory->setEnabled(isMachineOffline());
    mLeMemory->setEnabled(isMachineOffline());
    mLbBootOrder->setEnabled(isMachineOffline());
    mTwBootOrder->setEnabled(isMachineOffline());
    mTbBootItemUp->setEnabled(isMachineOffline() && mTwBootOrder->hasFocus() && mTwBootOrder->currentRow() > 0);
    mTbBootItemDown->setEnabled(isMachineOffline() && mTwBootOrder->hasFocus() && (mTwBootOrder->currentRow() < mTwBootOrder->count() - 1));
    mLbChipset->setEnabled(isMachineOffline());
    mCbChipset->setEnabled(isMachineOffline());
    mLbMotherboardExtended->setEnabled(isMachineOffline());
    mCbApic->setEnabled(isMachineOffline());
    mCbEFI->setEnabled(isMachineOffline());
    mCbTCUseUTC->setEnabled(isMachineOffline());
    mCbUseAbsHID->setEnabled(isMachineOffline());

    /* Processor tab: */
    mLbCPU->setEnabled(isMachineOffline());
    mLbCPUMin->setEnabled(isMachineOffline());
    mLbCPUMax->setEnabled(isMachineOffline());
    mSlCPU->setEnabled(isMachineOffline() && systemData.m_fSupportedHwVirtEx);
    mLeCPU->setEnabled(isMachineOffline() && systemData.m_fSupportedHwVirtEx);
    mLbCPUExecCap->setEnabled(isMachineInValidMode());
    mLbCPUExecCapMin->setEnabled(isMachineInValidMode());
    mLbCPUExecCapMax->setEnabled(isMachineInValidMode());
    mSlCPUExecCap->setEnabled(isMachineInValidMode());
    mLeCPUExecCap->setEnabled(isMachineInValidMode());
    mLbProcessorExtended->setEnabled(isMachineOffline());
    mCbPae->setEnabled(isMachineOffline() && systemData.m_fSupportedPAE);

    /* Acceleration tab: */
    mTwSystem->setTabEnabled(2, systemData.m_fSupportedHwVirtEx);
    mLbVirt->setEnabled(isMachineOffline());
    mCbVirt->setEnabled(isMachineOffline());
    mCbNestedPaging->setEnabled(isMachineOffline() && mCbVirt->isChecked());
}

bool UIMachineSettingsSystem::eventFilter(QObject *pObject, QEvent *pEvent)
{
    if (!pObject->isWidgetType())
        return QWidget::eventFilter(pObject, pEvent);

    QWidget *pWidget = static_cast<QWidget*>(pObject);
    if (pWidget->window() != window())
        return QWidget::eventFilter(pObject, pEvent);

    switch (pEvent->type())
    {
        case QEvent::FocusIn:
        {
            /* Boot Table: */
            if (pWidget == mTwBootOrder)
            {
                if (!mTwBootOrder->currentItem())
                    mTwBootOrder->setCurrentItem(mTwBootOrder->item(0));
                else
                    onCurrentBootItemChanged(mTwBootOrder->currentRow());
                mTwBootOrder->currentItem()->setSelected(true);
            }
            else if (pWidget != mTbBootItemUp && pWidget != mTbBootItemDown)
            {
                if (mTwBootOrder->currentItem())
                {
                    mTwBootOrder->currentItem()->setSelected(false);
                    mTbBootItemUp->setEnabled(false);
                    mTbBootItemDown->setEnabled(false);
                }
            }
            break;
        }
        default:
            break;
    }

    return QWidget::eventFilter(pObject, pEvent);
}

