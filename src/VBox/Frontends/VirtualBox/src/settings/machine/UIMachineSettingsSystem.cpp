/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSystem class implementation.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QHeaderView>

/* GUI includes: */
# include "QIWidgetValidator.h"
# include "UIConverter.h"
# include "UIIconPool.h"
# include "UIMachineSettingsSystem.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CBIOSSettings.h"

/* Other VBox includes: */
# include <iprt/cdefs.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Machine settings: System Boot data structure. */
struct UIBootItemData
{
    /** Constructs data. */
    UIBootItemData()
        : m_type(KDeviceType_Null)
        , m_fEnabled(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIBootItemData &other) const
    {
        return true
               && (m_type == other.m_type)
               && (m_fEnabled == other.m_fEnabled)
               ;
    }

    /** Holds the boot device type. */
    KDeviceType m_type;
    /** Holds whether the boot device enabled. */
    bool m_fEnabled;
};


/** Machine settings: System page data structure. */
struct UIDataSettingsMachineSystem
{
    /** Constructs data. */
    UIDataSettingsMachineSystem()
        /* Support flags: */
        : m_fSupportedPAE(false)
        , m_fSupportedHwVirtEx(false)
        /* Motherboard data: */
        , m_iMemorySize(-1)
        , m_bootItems(QList<UIBootItemData>())
        , m_chipsetType(KChipsetType_Null)
        , m_pointingHIDType(KPointingHIDType_None)
        , m_fEnabledIoApic(false)
        , m_fEnabledEFI(false)
        , m_fEnabledUTC(false)
        /* CPU data: */
        , m_cCPUCount(-1)
        , m_iCPUExecCap(-1)
        , m_fEnabledPAE(false)
        /* Acceleration data: */
        , m_paravirtProvider(KParavirtProvider_None)
        , m_fEnabledHwVirtEx(false)
        , m_fEnabledNestedPaging(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineSystem &other) const
    {
        return true
               /* Support flags: */
               && (m_fSupportedPAE == other.m_fSupportedPAE)
               && (m_fSupportedHwVirtEx == other.m_fSupportedHwVirtEx)
               /* Motherboard data: */
               && (m_iMemorySize == other.m_iMemorySize)
               && (m_bootItems == other.m_bootItems)
               && (m_chipsetType == other.m_chipsetType)
               && (m_pointingHIDType == other.m_pointingHIDType)
               && (m_fEnabledIoApic == other.m_fEnabledIoApic)
               && (m_fEnabledEFI == other.m_fEnabledEFI)
               && (m_fEnabledUTC == other.m_fEnabledUTC)
               /* CPU data: */
               && (m_cCPUCount == other.m_cCPUCount)
               && (m_iCPUExecCap == other.m_iCPUExecCap)
               && (m_fEnabledPAE == other.m_fEnabledPAE)
               /* Acceleration data: */
               && (m_paravirtProvider == other.m_paravirtProvider)
               && (m_fEnabledHwVirtEx == other.m_fEnabledHwVirtEx)
               && (m_fEnabledNestedPaging == other.m_fEnabledNestedPaging)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineSystem &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineSystem &other) const { return !equal(other); }

    /** Holds whether the PAE is supported. */
    bool  m_fSupportedPAE;
    /** Holds whether the HW Virt Ex is supported. */
    bool  m_fSupportedHwVirtEx;

    /** Holds the RAM size. */
    int                    m_iMemorySize;
    /** Holds the boot items. */
    QList<UIBootItemData>  m_bootItems;
    /** Holds the chipset type. */
    KChipsetType           m_chipsetType;
    /** Holds the pointing HID type. */
    KPointingHIDType       m_pointingHIDType;
    /** Holds whether the IO APIC is enabled. */
    bool                   m_fEnabledIoApic;
    /** Holds whether the EFI is enabled. */
    bool                   m_fEnabledEFI;
    /** Holds whether the UTC is enabled. */
    bool                   m_fEnabledUTC;

    /** Holds the CPU count. */
    int   m_cCPUCount;
    /** Holds the CPU execution cap. */
    int   m_iCPUExecCap;
    /** Holds whether the PAE is enabled. */
    bool  m_fEnabledPAE;

    /** Holds the paravirtualization provider. */
    KParavirtProvider  m_paravirtProvider;
    /** Holds whether the HW Virt Ex is enabled. */
    bool               m_fEnabledHwVirtEx;
    /** Holds whether the Nested Paging is enabled. */
    bool               m_fEnabledNestedPaging;
};


UIMachineSettingsSystem::UIMachineSettingsSystem()
    : m_uMinGuestCPU(0), m_uMaxGuestCPU(0)
    , m_uMinGuestCPUExecCap(0), m_uMedGuestCPUExecCap(0), m_uMaxGuestCPUExecCap(0)
    , m_fIsUSBEnabled(false)
    , m_pCache(new UISettingsCacheMachineSystem)
{
    /* Prepare: */
    prepare();
}

UIMachineSettingsSystem::~UIMachineSettingsSystem()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

bool UIMachineSettingsSystem::isHWVirtExEnabled() const
{
    return m_pCheckBoxVirtualization->isChecked();
}

bool UIMachineSettingsSystem::isHIDEnabled() const
{
    return (KPointingHIDType)m_pComboPointingHIDType->itemData(m_pComboPointingHIDType->currentIndex()).toInt() != KPointingHIDType_PS2Mouse;
}

KChipsetType UIMachineSettingsSystem::chipsetType() const
{
    return (KChipsetType)m_pComboChipsetType->itemData(m_pComboChipsetType->currentIndex()).toInt();
}

void UIMachineSettingsSystem::setUSBEnabled(bool fEnabled)
{
    /* Make sure USB status has changed: */
    if (m_fIsUSBEnabled == fEnabled)
        return;

    /* Update USB status value: */
    m_fIsUSBEnabled = fEnabled;

    /* Revalidate: */
    revalidate();
}

bool UIMachineSettingsSystem::changed() const
{
    return m_pCache->wasChanged();
}

void UIMachineSettingsSystem::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare system data: */
    UIDataSettingsMachineSystem systemData;

    /* Load support flags: */
    systemData.m_fSupportedPAE = vboxGlobal().host().GetProcessorFeature(KProcessorFeature_PAE);
    systemData.m_fSupportedHwVirtEx = vboxGlobal().host().GetProcessorFeature(KProcessorFeature_HWVirtEx);

    /* Load motherboard data: */
    systemData.m_iMemorySize = m_machine.GetMemorySize();
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
    systemData.m_pointingHIDType = m_machine.GetPointingHIDType();
    systemData.m_fEnabledIoApic = m_machine.GetBIOSSettings().GetIOAPICEnabled();
    systemData.m_fEnabledEFI = m_machine.GetFirmwareType() >= KFirmwareType_EFI && m_machine.GetFirmwareType() <= KFirmwareType_EFIDUAL;
    systemData.m_fEnabledUTC = m_machine.GetRTCUseUTC();

    /* Load CPU data: */
    systemData.m_cCPUCount = systemData.m_fSupportedHwVirtEx ? m_machine.GetCPUCount() : 1;
    systemData.m_iCPUExecCap = m_machine.GetCPUExecutionCap();
    systemData.m_fEnabledPAE = m_machine.GetCPUProperty(KCPUPropertyType_PAE);

    /* Load acceleration data: */
    systemData.m_paravirtProvider = m_machine.GetParavirtProvider();
    systemData.m_fEnabledHwVirtEx = m_machine.GetHWVirtExProperty(KHWVirtExPropertyType_Enabled);
    systemData.m_fEnabledNestedPaging = m_machine.GetHWVirtExProperty(KHWVirtExPropertyType_NestedPaging);

    /* Cache system data: */
    m_pCache->cacheInitialData(systemData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsSystem::getFromCache()
{
    /* Get system data from cache: */
    const UIDataSettingsMachineSystem &systemData = m_pCache->base();

    /* Repopulate 'pointing HID type' combo.
     * We are doing that *now* because it has dynamical content
     * which depends on recashed value: */
    repopulateComboPointingHIDType();

    /* Load motherboard data to page: */
    m_pSliderMemorySize->setValue(systemData.m_iMemorySize);
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
    int iChipsetTypePosition = m_pComboChipsetType->findData(systemData.m_chipsetType);
    m_pComboChipsetType->setCurrentIndex(iChipsetTypePosition == -1 ? 0 : iChipsetTypePosition);
    int iHIDTypePosition = m_pComboPointingHIDType->findData(systemData.m_pointingHIDType);
    m_pComboPointingHIDType->setCurrentIndex(iHIDTypePosition == -1 ? 0 : iHIDTypePosition);
    m_pCheckBoxApic->setChecked(systemData.m_fEnabledIoApic);
    m_pCheckBoxEFI->setChecked(systemData.m_fEnabledEFI);
    m_pCheckBoxUseUTC->setChecked(systemData.m_fEnabledUTC);

    /* Load CPU data to page: */
    m_pSliderCPUCount->setValue(systemData.m_cCPUCount);
    m_pSliderCPUExecCap->setValue(systemData.m_iCPUExecCap);
    m_pCheckBoxPAE->setChecked(systemData.m_fEnabledPAE);

    /* Load acceleration data to page: */
    int iParavirtProviderPosition = m_pComboParavirtProvider->findData(systemData.m_paravirtProvider);
    m_pComboParavirtProvider->setCurrentIndex(iParavirtProviderPosition == -1 ? 0 : iParavirtProviderPosition);
    m_pCheckBoxVirtualization->setChecked(systemData.m_fEnabledHwVirtEx);
    m_pCheckBoxNestedPaging->setChecked(systemData.m_fEnabledNestedPaging);

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::putToCache()
{
    /* Prepare system data: */
    UIDataSettingsMachineSystem systemData = m_pCache->base();

    /* Gather motherboard data: */
    systemData.m_iMemorySize = m_pSliderMemorySize->value();
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
    systemData.m_chipsetType = (KChipsetType)m_pComboChipsetType->itemData(m_pComboChipsetType->currentIndex()).toInt();
    systemData.m_pointingHIDType = (KPointingHIDType)m_pComboPointingHIDType->itemData(m_pComboPointingHIDType->currentIndex()).toInt();
    systemData.m_fEnabledIoApic = m_pCheckBoxApic->isChecked() || m_pSliderCPUCount->value() > 1 ||
                                  (KChipsetType)m_pComboChipsetType->itemData(m_pComboChipsetType->currentIndex()).toInt() == KChipsetType_ICH9;
    systemData.m_fEnabledEFI = m_pCheckBoxEFI->isChecked();
    systemData.m_fEnabledUTC = m_pCheckBoxUseUTC->isChecked();

    /* Gather CPU data: */
    systemData.m_cCPUCount = m_pSliderCPUCount->value();
    systemData.m_iCPUExecCap = m_pSliderCPUExecCap->value();
    systemData.m_fEnabledPAE = m_pCheckBoxPAE->isChecked();

    /* Gather acceleration data: */
    systemData.m_paravirtProvider = (KParavirtProvider)m_pComboParavirtProvider->itemData(m_pComboParavirtProvider->currentIndex()).toInt();
    systemData.m_fEnabledHwVirtEx = m_pCheckBoxVirtualization->checkState() == Qt::Checked || m_pSliderCPUCount->value() > 1;
    systemData.m_fEnabledNestedPaging = m_pCheckBoxNestedPaging->isChecked();

    /* Cache system data: */
    m_pCache->cacheCurrentData(systemData);
}

void UIMachineSettingsSystem::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Check if system data was changed: */
    if (m_pCache->wasChanged())
    {
        /* Get system data from cache: */
        const UIDataSettingsMachineSystem &systemData = m_pCache->data();

        /* Store system data: */
        if (isMachineOffline())
        {
            /* Motherboard tab: */
            m_machine.SetMemorySize(systemData.m_iMemorySize);
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
            m_machine.SetPointingHIDType(systemData.m_pointingHIDType);
            m_machine.GetBIOSSettings().SetIOAPICEnabled(systemData.m_fEnabledIoApic);
            m_machine.SetFirmwareType(systemData.m_fEnabledEFI ? KFirmwareType_EFI : KFirmwareType_BIOS);
            m_machine.SetRTCUseUTC(systemData.m_fEnabledUTC);

            /* Processor tab: */
            m_machine.SetCPUCount(systemData.m_cCPUCount);
            m_machine.SetCPUProperty(KCPUPropertyType_PAE, systemData.m_fEnabledPAE);

            /* Acceleration tab: */
            m_machine.SetParavirtProvider(systemData.m_paravirtProvider);
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

bool UIMachineSettingsSystem::validate(QList<UIValidationMessage> &messages)
{
    /* Pass by default: */
    bool fPass = true;

    /* Motherboard tab: */
    {
        /* Prepare message: */
        UIValidationMessage message;
        message.first = VBoxGlobal::removeAccelMark(m_pTabWidgetSystem->tabText(0));

        /* RAM amount test: */
        const ulong uFullSize = vboxGlobal().host().GetMemorySize();
        if (m_pSliderMemorySize->value() > (int)m_pSliderMemorySize->maxRAMAlw())
        {
            message.second << tr(
                "More than <b>%1%</b> of the host computer's memory (<b>%2</b>) is assigned to the virtual machine. "
                "Not enough memory is left for the host operating system. Please select a smaller amount.")
                .arg((unsigned)qRound((double)m_pSliderMemorySize->maxRAMAlw() / uFullSize * 100.0))
                .arg(vboxGlobal().formatSize((uint64_t)uFullSize * _1M));
            fPass = false;
        }
        else if (m_pSliderMemorySize->value() > (int)m_pSliderMemorySize->maxRAMOpt())
        {
            message.second << tr(
                "More than <b>%1%</b> of the host computer's memory (<b>%2</b>) is assigned to the virtual machine. "
                "There might not be enough memory left for the host operating system. Please consider selecting a smaller amount.")
                .arg((unsigned)qRound((double)m_pSliderMemorySize->maxRAMOpt() / uFullSize * 100.0))
                .arg(vboxGlobal().formatSize((uint64_t)uFullSize * _1M));
        }

        /* Chipset type vs IO-APIC test: */
        if ((KChipsetType)m_pComboChipsetType->itemData(m_pComboChipsetType->currentIndex()).toInt() == KChipsetType_ICH9 && !m_pCheckBoxApic->isChecked())
        {
            message.second << tr(
                "The I/O APIC feature is not currently enabled in the Motherboard section of the System page. "
                "This is needed in order to support a chip set of type ICH9 you have enabled for this VM. "
                "It will be done automatically if you confirm your changes.");
        }

        /* HID vs USB test: */
        if (isHIDEnabled() && !m_fIsUSBEnabled)
        {
            message.second << tr(
                "USB controller emulation is not currently enabled on the USB page. "
                "This is needed to support an emulated USB input device you have enabled for this VM. "
                "It will be done automatically if you confirm your changes.");
        }

        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;
    }

    /* CPU tab: */
    {
        /* Prepare message: */
        UIValidationMessage message;
        message.first = VBoxGlobal::removeAccelMark(m_pTabWidgetSystem->tabText(1));

        /* VCPU amount test: */
        const int cTotalCPUs = vboxGlobal().host().GetProcessorOnlineCoreCount();
        if (m_pSliderCPUCount->value() > 2 * cTotalCPUs)
        {
            message.second << tr(
                "For performance reasons, the number of virtual CPUs attached to the virtual machine may not be more than twice the number "
                "of physical CPUs on the host (<b>%1</b>). Please reduce the number of virtual CPUs.")
                .arg(cTotalCPUs);
            fPass = false;
        }
        else if (m_pSliderCPUCount->value() > cTotalCPUs)
        {
            message.second << tr(
                "More virtual CPUs are assigned to the virtual machine than the number of physical CPUs on the host system (<b>%1</b>). "
                "This is likely to degrade the performance of your virtual machine. Please consider reducing the number of virtual CPUs.")
                .arg(cTotalCPUs);
        }

        /* VCPU vs IO-APIC test: */
        if (m_pSliderCPUCount->value() > 1 && !m_pCheckBoxApic->isChecked())
        {
            message.second << tr(
                "The I/O APIC feature is not currently enabled in the Motherboard section of the System page. "
                "This is needed in order to support more than one virtual processor you have chosen for this VM. "
                "It will be done automatically if you confirm your changes.");
        }

        /* VCPU vs VT-x/AMD-V test: */
        if (m_pSliderCPUCount->value() > 1 && !m_pCheckBoxVirtualization->isChecked())
        {
            message.second << tr(
                "Hardware virtualization is not currently enabled in the Acceleration section of the System page. "
                "This is needed in order to support more than one virtual processor you have chosen for this VM. "
                "It will be done automatically if you confirm your changes.");
        }

        /* CPU execution cap test: */
        if (m_pSliderCPUExecCap->value() < (int)m_uMedGuestCPUExecCap)
        {
            message.second << tr(
                "The processor execution cap is set to a low value. This may make the machine feel slow to respond.");
        }

        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;
    }

    /* Return result: */
    return fPass;
}

void UIMachineSettingsSystem::setOrderAfter(QWidget *pWidget)
{
    /* Configure navigation for 'motherboard' tab: */
    setTabOrder(pWidget, m_pTabWidgetSystem->focusProxy());
    setTabOrder(m_pTabWidgetSystem->focusProxy(), m_pSliderMemorySize);
    setTabOrder(m_pSliderMemorySize, m_pEditorMemorySize);
    setTabOrder(m_pEditorMemorySize, mTwBootOrder);
    setTabOrder(mTwBootOrder, mTbBootItemUp);
    setTabOrder(mTbBootItemUp, mTbBootItemDown);
    setTabOrder(mTbBootItemDown, m_pComboChipsetType);
    setTabOrder(m_pComboChipsetType, m_pComboPointingHIDType);
    setTabOrder(m_pComboPointingHIDType, m_pCheckBoxApic);
    setTabOrder(m_pCheckBoxApic, m_pCheckBoxEFI);
    setTabOrder(m_pCheckBoxEFI, m_pCheckBoxUseUTC);

    /* Configure navigation for 'processor' tab: */
    setTabOrder(m_pCheckBoxUseUTC, m_pSliderCPUCount);
    setTabOrder(m_pSliderCPUCount, m_pEditorCPUCount);
    setTabOrder(m_pEditorCPUCount, m_pSliderCPUExecCap);
    setTabOrder(m_pSliderCPUExecCap, m_pEditorCPUExecCap);
    setTabOrder(m_pEditorCPUExecCap, m_pComboParavirtProvider);

    /* Configure navigation for 'acceleration' tab: */
    setTabOrder(m_pComboParavirtProvider, m_pCheckBoxPAE);
    setTabOrder(m_pCheckBoxPAE, m_pCheckBoxVirtualization);
    setTabOrder(m_pCheckBoxVirtualization, m_pCheckBoxNestedPaging);
}

void UIMachineSettingsSystem::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsSystem::retranslateUi(this);

    /* Readjust the tree widget items size: */
    adjustBootOrderTWSize();

    /* Retranslate the memory slider legend: */
    m_pEditorMemorySize->setSuffix(QString(" %1").arg(tr("MB")));
    m_pLabelMemoryMin->setText(tr("%1 MB").arg(m_pSliderMemorySize->minRAM()));
    m_pLabelMemoryMax->setText(tr("%1 MB").arg(m_pSliderMemorySize->maxRAM()));

    /* Retranslate the cpu slider legend: */
    m_pLabelCPUMin->setText(tr("%1 CPU", "%1 is 1 for now").arg(m_uMinGuestCPU));
    m_pLabelCPUMax->setText(tr("%1 CPUs", "%1 is host cpu count * 2 for now").arg(m_uMaxGuestCPU));

    /* Retranslate the cpu cap slider legend: */
    m_pLabelCPUExecCapMin->setText(tr("%1%").arg(m_uMinGuestCPUExecCap));
    m_pLabelCPUExecCapMax->setText(tr("%1%").arg(m_uMaxGuestCPUExecCap));

    /* Retranslate combo-boxes: */
    retranslateComboChipsetType();
    retranslateComboPointingHIDType();
    retranslateComboParavirtProvider();
}

void UIMachineSettingsSystem::polishPage()
{
    /* Get system data from cache: */
    const UIDataSettingsMachineSystem &systemData = m_pCache->base();

    /* Motherboard tab: */
    m_pLabelMemorySize->setEnabled(isMachineOffline());
    m_pLabelMemoryMin->setEnabled(isMachineOffline());
    m_pLabelMemoryMax->setEnabled(isMachineOffline());
    m_pSliderMemorySize->setEnabled(isMachineOffline());
    m_pEditorMemorySize->setEnabled(isMachineOffline());
    m_pLabelBootOrder->setEnabled(isMachineOffline());
    mTwBootOrder->setEnabled(isMachineOffline());
    mTbBootItemUp->setEnabled(isMachineOffline() && mTwBootOrder->hasFocus() && mTwBootOrder->currentRow() > 0);
    mTbBootItemDown->setEnabled(isMachineOffline() && mTwBootOrder->hasFocus() && (mTwBootOrder->currentRow() < mTwBootOrder->count() - 1));
    m_pLabelChipsetType->setEnabled(isMachineOffline());
    m_pComboChipsetType->setEnabled(isMachineOffline());
    m_pLabelPointingHIDType->setEnabled(isMachineOffline());
    m_pComboPointingHIDType->setEnabled(isMachineOffline());
    m_pLabelMotherboardExtended->setEnabled(isMachineOffline());
    m_pCheckBoxApic->setEnabled(isMachineOffline());
    m_pCheckBoxEFI->setEnabled(isMachineOffline());
    m_pCheckBoxUseUTC->setEnabled(isMachineOffline());

    /* Processor tab: */
    m_pLabelCPUCount->setEnabled(isMachineOffline());
    m_pLabelCPUMin->setEnabled(isMachineOffline());
    m_pLabelCPUMax->setEnabled(isMachineOffline());
    m_pSliderCPUCount->setEnabled(isMachineOffline() && systemData.m_fSupportedHwVirtEx);
    m_pEditorCPUCount->setEnabled(isMachineOffline() && systemData.m_fSupportedHwVirtEx);
    m_pLabelCPUExecCap->setEnabled(isMachineInValidMode());
    m_pLabelCPUExecCapMin->setEnabled(isMachineInValidMode());
    m_pLabelCPUExecCapMax->setEnabled(isMachineInValidMode());
    m_pSliderCPUExecCap->setEnabled(isMachineInValidMode());
    m_pEditorCPUExecCap->setEnabled(isMachineInValidMode());
    m_pLabelCPUExtended->setEnabled(isMachineOffline());
    m_pCheckBoxPAE->setEnabled(isMachineOffline() && systemData.m_fSupportedPAE);

    /* Acceleration tab: */
    m_pTabWidgetSystem->setTabEnabled(2, systemData.m_fSupportedHwVirtEx);
    m_pLabelParavirtProvider->setEnabled(isMachineOffline());
    m_pComboParavirtProvider->setEnabled(isMachineOffline());
    m_pLabelVirtualization->setEnabled(isMachineOffline());
    m_pCheckBoxVirtualization->setEnabled(isMachineOffline());
    m_pCheckBoxNestedPaging->setEnabled(isMachineOffline() && m_pCheckBoxVirtualization->isChecked());
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
                    sltHandleCurrentBootItemChange(mTwBootOrder->currentRow());
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

    /* Call to base-class: */
    return QWidget::eventFilter(pObject, pEvent);
}

void UIMachineSettingsSystem::sltHandleMemorySizeSliderChange()
{
    /* Apply new memory-size value: */
    m_pEditorMemorySize->blockSignals(true);
    m_pEditorMemorySize->setValue(m_pSliderMemorySize->value());
    m_pEditorMemorySize->blockSignals(false);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::sltHandleMemorySizeEditorChange()
{
    /* Apply new memory-size value: */
    m_pSliderMemorySize->blockSignals(true);
    m_pSliderMemorySize->setValue(m_pEditorMemorySize->value());
    m_pSliderMemorySize->blockSignals(false);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::sltHandleCurrentBootItemChange(int iCurrentItem)
{
    /* Update boot-order tool-buttons: */
    const bool fEnabledUP = iCurrentItem > 0;
    const bool fEnabledDOWN = iCurrentItem < mTwBootOrder->count() - 1;
    if ((mTbBootItemUp->hasFocus() && !fEnabledUP) ||
        (mTbBootItemDown->hasFocus() && !fEnabledDOWN))
        mTwBootOrder->setFocus();
    mTbBootItemUp->setEnabled(fEnabledUP);
    mTbBootItemDown->setEnabled(fEnabledDOWN);
}

void UIMachineSettingsSystem::sltHandleCPUCountSliderChange()
{
    /* Apply new memory-size value: */
    m_pEditorCPUCount->blockSignals(true);
    m_pEditorCPUCount->setValue(m_pSliderCPUCount->value());
    m_pEditorCPUCount->blockSignals(false);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::sltHandleCPUCountEditorChange()
{
    /* Apply new memory-size value: */
    m_pSliderCPUCount->blockSignals(true);
    m_pSliderCPUCount->setValue(m_pEditorCPUCount->value());
    m_pSliderCPUCount->blockSignals(false);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::sltHandleCPUExecCapSliderChange()
{
    /* Apply new memory-size value: */
    m_pEditorCPUExecCap->blockSignals(true);
    m_pEditorCPUExecCap->setValue(m_pSliderCPUExecCap->value());
    m_pEditorCPUExecCap->blockSignals(false);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::sltHandleCPUExecCapEditorChange()
{
    /* Apply new memory-size value: */
    m_pSliderCPUExecCap->blockSignals(true);
    m_pSliderCPUExecCap->setValue(m_pEditorCPUExecCap->value());
    m_pSliderCPUExecCap->blockSignals(false);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::prepare()
{
    /* Apply UI decorations: */
    Ui::UIMachineSettingsSystem::setupUi(this);

    /* Prepare tabs: */
    prepareTabMotherboard();
    prepareTabProcessor();
    prepareTabAcceleration();

    /* Retranslate finally: */
    retranslateUi();
}

void UIMachineSettingsSystem::prepareTabMotherboard()
{
    /* Load configuration: */
    const CSystemProperties properties = vboxGlobal().virtualBox().GetSystemProperties();

    /* Preconfigure memory-size editor: */
    m_pEditorMemorySize->setMinimum(m_pSliderMemorySize->minRAM());
    m_pEditorMemorySize->setMaximum(m_pSliderMemorySize->maxRAM());
    vboxGlobal().setMinimumWidthAccordingSymbolCount(m_pEditorMemorySize, 5);

    /* Preconfigure boot-table widgets: */
    mTbBootItemUp->setIcon(UIIconPool::iconSet(":/list_moveup_16px.png", ":/list_moveup_disabled_16px.png"));
    mTbBootItemDown->setIcon(UIIconPool::iconSet(":/list_movedown_16px.png", ":/list_movedown_disabled_16px.png"));
#ifdef VBOX_WS_MAC
    /* We need a little space for the focus rect: */
    m_pLayoutBootOrder->setContentsMargins(3, 3, 3, 3);
    m_pLayoutBootOrder->setSpacing(3);
#endif /* VBOX_WS_MAC */
    /* Install global event filter
     * to handle boot-table focus in/out events: */
    /// @todo Get rid of that *crap*!
    qApp->installEventFilter(this);

    /* Populate possible boot items list.
     * Currently, it seems, we are supporting only 4 possible boot device types:
     * 1. Floppy, 2. DVD-ROM, 3. Hard Disk, 4. Network.
     * But maximum boot devices count supported by machine
     * should be retrieved through the ISystemProperties getter.
     * Moreover, possible boot device types are not listed in some separate Main vector,
     * so we should get them (randomely?) from the list of all device types.
     * Until there will be separate Main getter for list of supported boot device types,
     * this list will be hard-coded here... */
    const int iPossibleBootListSize = qMin((ULONG)4, properties.GetMaxBootPosition());
    for (int iBootPosition = 1; iBootPosition <= iPossibleBootListSize; ++iBootPosition)
    {
        switch (iBootPosition)
        {
            case 1: m_possibleBootItems << KDeviceType_Floppy; break;
            case 2: m_possibleBootItems << KDeviceType_DVD; break;
            case 3: m_possibleBootItems << KDeviceType_HardDisk; break;
            case 4: m_possibleBootItems << KDeviceType_Network; break;
            default: break;
        }
    }
    /* Add all available devices types, so we could initially calculate the right size: */
    for (int i = 0; i < m_possibleBootItems.size(); ++i)
    {
        QListWidgetItem *pItem = new UIBootTableItem(m_possibleBootItems[i]);
        mTwBootOrder->addItem(pItem);
    }

    /* Populate 'chipset type' combo: */
    m_pComboChipsetType->addItem(gpConverter->toString(KChipsetType_PIIX3), QVariant(KChipsetType_PIIX3));
    m_pComboChipsetType->addItem(gpConverter->toString(KChipsetType_ICH9), QVariant(KChipsetType_ICH9));
    connect(m_pComboChipsetType, SIGNAL(currentIndexChanged(int)), this, SLOT(revalidate()));

    /* Preconfigure 'pointing HID type' combo: */
    m_pComboPointingHIDType->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(m_pComboPointingHIDType, SIGNAL(currentIndexChanged(int)), this, SLOT(revalidate()));

    /* Install memory-size widget connections: */
    connect(m_pSliderMemorySize, SIGNAL(valueChanged(int)), this, SLOT(sltHandleMemorySizeSliderChange()));
    connect(m_pEditorMemorySize, SIGNAL(valueChanged(int)), this, SLOT(sltHandleMemorySizeEditorChange()));

    /* Install boot-table connections: */
    connect(mTbBootItemUp, SIGNAL(clicked()), mTwBootOrder, SLOT(sltMoveItemUp()));
    connect(mTbBootItemDown, SIGNAL(clicked()), mTwBootOrder, SLOT(sltMoveItemDown()));
    connect(mTwBootOrder, SIGNAL(sigRowChanged(int)), this, SLOT(sltHandleCurrentBootItemChange(int)));

    /* Advanced options: */
    connect(m_pCheckBoxApic, SIGNAL(stateChanged(int)), this, SLOT(revalidate()));
}

void UIMachineSettingsSystem::prepareTabProcessor()
{
    /* Load configuration: */
    const CSystemProperties properties = vboxGlobal().virtualBox().GetSystemProperties();
    const uint hostCPUs = vboxGlobal().host().GetProcessorOnlineCoreCount();
    m_uMinGuestCPU = properties.GetMinGuestCPUCount();
    m_uMaxGuestCPU = qMin(2 * hostCPUs, (uint)properties.GetMaxGuestCPUCount());
    m_uMinGuestCPUExecCap = 1;
    m_uMedGuestCPUExecCap = 40;
    m_uMaxGuestCPUExecCap = 100;

    /* Preconfigure CPU-count slider: */
    m_pSliderCPUCount->setPageStep(1);
    m_pSliderCPUCount->setSingleStep(1);
    m_pSliderCPUCount->setTickInterval(1);
    m_pSliderCPUCount->setMinimum(m_uMinGuestCPU);
    m_pSliderCPUCount->setMaximum(m_uMaxGuestCPU);
    m_pSliderCPUCount->setOptimalHint(1, hostCPUs);
    m_pSliderCPUCount->setWarningHint(hostCPUs, m_uMaxGuestCPU);

    /* Preconfigure CPU-count editor: */
    m_pEditorCPUCount->setMinimum(m_uMinGuestCPU);
    m_pEditorCPUCount->setMaximum(m_uMaxGuestCPU);
    vboxGlobal().setMinimumWidthAccordingSymbolCount(m_pEditorCPUCount, 4);

    /* Preconfigure CPU-execution-cap slider: */
    m_pSliderCPUExecCap->setPageStep(10);
    m_pSliderCPUExecCap->setSingleStep(1);
    m_pSliderCPUExecCap->setTickInterval(10);
    /* Setup the scale so that ticks are at page step boundaries: */
    m_pSliderCPUExecCap->setMinimum(m_uMinGuestCPUExecCap);
    m_pSliderCPUExecCap->setMaximum(m_uMaxGuestCPUExecCap);
    m_pSliderCPUExecCap->setWarningHint(m_uMinGuestCPUExecCap, m_uMedGuestCPUExecCap);
    m_pSliderCPUExecCap->setOptimalHint(m_uMedGuestCPUExecCap, m_uMaxGuestCPUExecCap);

    /* Preconfigure CPU-execution-cap editor: */
    m_pEditorCPUExecCap->setMinimum(m_uMinGuestCPUExecCap);
    m_pEditorCPUExecCap->setMaximum(m_uMaxGuestCPUExecCap);
    vboxGlobal().setMinimumWidthAccordingSymbolCount(m_pEditorCPUExecCap, 4);

    /* Install CPU widget connections: */
    connect(m_pSliderCPUCount, SIGNAL(valueChanged(int)), this, SLOT(sltHandleCPUCountSliderChange()));
    connect(m_pEditorCPUCount, SIGNAL(valueChanged(int)), this, SLOT(sltHandleCPUCountEditorChange()));
    connect(m_pSliderCPUExecCap, SIGNAL(valueChanged(int)), this, SLOT(sltHandleCPUExecCapSliderChange()));
    connect(m_pEditorCPUExecCap, SIGNAL(valueChanged(int)), this, SLOT(sltHandleCPUExecCapEditorChange()));
}

void UIMachineSettingsSystem::prepareTabAcceleration()
{
    /* Populate 'paravirt provider' combo: */
    m_pComboParavirtProvider->addItem(gpConverter->toString(KParavirtProvider_None), QVariant(KParavirtProvider_None));
    m_pComboParavirtProvider->addItem(gpConverter->toString(KParavirtProvider_Default), QVariant(KParavirtProvider_Default));
    m_pComboParavirtProvider->addItem(gpConverter->toString(KParavirtProvider_Legacy), QVariant(KParavirtProvider_Legacy));
    m_pComboParavirtProvider->addItem(gpConverter->toString(KParavirtProvider_Minimal), QVariant(KParavirtProvider_Minimal));
    m_pComboParavirtProvider->addItem(gpConverter->toString(KParavirtProvider_HyperV), QVariant(KParavirtProvider_HyperV));
    m_pComboParavirtProvider->addItem(gpConverter->toString(KParavirtProvider_KVM), QVariant(KParavirtProvider_KVM));

    /* Hide VT-x/AMD-V checkbox when raw-mode is not supported: */
#ifndef VBOX_WITH_RAW_MODE
    m_pWidgetPlaceholder->setVisible(false);
    m_pCheckBoxVirtualization->setVisible(false);
#endif /* !VBOX_WITH_RAW_MODE */

    /* Advanced options: */
    connect(m_pCheckBoxVirtualization, SIGNAL(stateChanged(int)), this, SLOT(revalidate()));
}

void UIMachineSettingsSystem::repopulateComboPointingHIDType()
{
    /* Is there any value currently present/selected? */
    KPointingHIDType enmCurrentValue = KPointingHIDType_None;
    {
        const int iCurrentIndex = m_pComboPointingHIDType->currentIndex();
        if (iCurrentIndex != -1)
            enmCurrentValue = (KPointingHIDType)m_pComboPointingHIDType->itemData(iCurrentIndex).toInt();
    }

    /* Clear combo: */
    m_pComboPointingHIDType->clear();

    /* Repopulate combo taking into account currently cached value: */
    const KPointingHIDType enmCachedValue = m_pCache->base().m_pointingHIDType;
    {
        /* "PS/2 Mouse" value is always here: */
        m_pComboPointingHIDType->addItem(gpConverter->toString(KPointingHIDType_PS2Mouse), (int)KPointingHIDType_PS2Mouse);

        /* "USB Mouse" value is here only if it is currently selected: */
        if (enmCachedValue == KPointingHIDType_USBMouse)
            m_pComboPointingHIDType->addItem(gpConverter->toString(KPointingHIDType_USBMouse), (int)KPointingHIDType_USBMouse);

        /* "USB Mouse/Tablet" value is always here: */
        m_pComboPointingHIDType->addItem(gpConverter->toString(KPointingHIDType_USBTablet), (int)KPointingHIDType_USBTablet);

        /* "PS/2 and USB Mouse" value is here only if it is currently selected: */
        if (enmCachedValue == KPointingHIDType_ComboMouse)
            m_pComboPointingHIDType->addItem(gpConverter->toString(KPointingHIDType_ComboMouse), (int)KPointingHIDType_ComboMouse);

        /* "USB Multi-Touch Mouse/Tablet" value is always here: */
        m_pComboPointingHIDType->addItem(gpConverter->toString(KPointingHIDType_USBMultiTouch), (int)KPointingHIDType_USBMultiTouch);
    }

    /* Was there any value previously present/selected? */
    if (enmCurrentValue != KPointingHIDType_None)
    {
        int iPreviousIndex = m_pComboPointingHIDType->findData((int)enmCurrentValue);
        if (iPreviousIndex != -1)
            m_pComboPointingHIDType->setCurrentIndex(iPreviousIndex);
    }
}

void UIMachineSettingsSystem::retranslateComboChipsetType()
{
    /* For each the element in KChipsetType enum: */
    for (int iIndex = (int)KChipsetType_Null; iIndex < (int)KChipsetType_Max; ++iIndex)
    {
        /* Cast to the corresponding type: */
        const KChipsetType enmType = (KChipsetType)iIndex;
        /* Look for the corresponding item: */
        const int iCorrespondingIndex = m_pComboChipsetType->findData((int)enmType);
        /* Re-translate if corresponding item was found: */
        if (iCorrespondingIndex != -1)
            m_pComboChipsetType->setItemText(iCorrespondingIndex, gpConverter->toString(enmType));
    }
}

void UIMachineSettingsSystem::retranslateComboPointingHIDType()
{
    /* For each the element in KPointingHIDType enum: */
    for (int iIndex = (int)KPointingHIDType_None; iIndex < (int)KPointingHIDType_Max; ++iIndex)
    {
        /* Cast to the corresponding type: */
        const KPointingHIDType enmType = (KPointingHIDType)iIndex;
        /* Look for the corresponding item: */
        const int iCorrespondingIndex = m_pComboPointingHIDType->findData((int)enmType);
        /* Re-translate if corresponding item was found: */
        if (iCorrespondingIndex != -1)
            m_pComboPointingHIDType->setItemText(iCorrespondingIndex, gpConverter->toString(enmType));
    }
}

void UIMachineSettingsSystem::retranslateComboParavirtProvider()
{
    /* For each the element in KParavirtProvider enum: */
    for (int iIndex = (int)KParavirtProvider_None; iIndex < (int)KParavirtProvider_Max; ++iIndex)
    {
        /* Cast to the corresponding type: */
        const KParavirtProvider enmType = (KParavirtProvider)iIndex;
        /* Look for the corresponding item: */
        const int iCorrespondingIndex = m_pComboParavirtProvider->findData((int)enmType);
        /* Re-translate if corresponding item was found: */
        if (iCorrespondingIndex != -1)
            m_pComboParavirtProvider->setItemText(iCorrespondingIndex, gpConverter->toString(enmType));
    }
}

void UIMachineSettingsSystem::adjustBootOrderTWSize()
{
    /* Adjust boot-table size: */
    mTwBootOrder->adjustSizeToFitContent();
    /* Update the layout system */
    if (m_pTabMotherboard->layout())
    {
        m_pTabMotherboard->layout()->activate();
        m_pTabMotherboard->layout()->update();
    }
}

