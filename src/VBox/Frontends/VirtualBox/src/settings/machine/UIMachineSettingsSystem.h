/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsSystem class declaration
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

#ifndef __UIMachineSettingsSystem_h__
#define __UIMachineSettingsSystem_h__

#include "COMDefs.h"
#include "UISettingsPage.h"
#include "UIMachineSettingsSystem.gen.h"

/* Machine settings / System page / Boot item: */
struct UIBootItemData
{
    /* Default constructor: */
    UIBootItemData() : m_type(KDeviceType_Null), m_fEnabled(false) {}
    /* Operator==: */
    bool operator==(const UIBootItemData &other) const
    {
        return (m_type == other.m_type) &&
               (m_fEnabled == other.m_fEnabled);
    }
    /* Variables: */
    KDeviceType m_type;
    bool m_fEnabled;
};

/* Machine settings / System page / Data: */
struct UIDataSettingsMachineSystem
{
    /* Default constructor: */
    UIDataSettingsMachineSystem()
        : m_bootItems(QList<UIBootItemData>())
        , m_chipsetType(KChipsetType_Null)
        , m_fPFHwVirtExSupported(false)
        , m_fPFPAESupported(false)
        , m_fIoApicEnabled(false)
        , m_fEFIEnabled(false)
        , m_fUTCEnabled(false)
        , m_fUseAbsHID(false)
        , m_fPAEEnabled(false)
        , m_fHwVirtExEnabled(false)
        , m_fNestedPagingEnabled(false)
        , m_iRAMSize(-1)
        , m_cCPUCount(-1)
        , m_cCPUExecCap(-1) {}
    /* Functions: */
    bool equal(const UIDataSettingsMachineSystem &other) const
    {
        return (m_bootItems == other.m_bootItems) &&
               (m_chipsetType == other.m_chipsetType) &&
               (m_fPFHwVirtExSupported == other.m_fPFHwVirtExSupported) &&
               (m_fPFPAESupported == other.m_fPFPAESupported) &&
               (m_fIoApicEnabled == other.m_fIoApicEnabled) &&
               (m_fEFIEnabled == other.m_fEFIEnabled) &&
               (m_fUTCEnabled == other.m_fUTCEnabled) &&
               (m_fUseAbsHID == other.m_fUseAbsHID) &&
               (m_fPAEEnabled == other.m_fPAEEnabled) &&
               (m_fHwVirtExEnabled == other.m_fHwVirtExEnabled) &&
               (m_fNestedPagingEnabled == other.m_fNestedPagingEnabled) &&
               (m_iRAMSize == other.m_iRAMSize) &&
               (m_cCPUCount == other.m_cCPUCount) &&
               (m_cCPUExecCap == other.m_cCPUExecCap);
    }
    /* Operators: */
    bool operator==(const UIDataSettingsMachineSystem &other) const { return equal(other); }
    bool operator!=(const UIDataSettingsMachineSystem &other) const { return !equal(other); }
    /* Variables: */
    QList<UIBootItemData> m_bootItems;
    KChipsetType m_chipsetType;
    bool m_fPFHwVirtExSupported;
    bool m_fPFPAESupported;
    bool m_fIoApicEnabled;
    bool m_fEFIEnabled;
    bool m_fUTCEnabled;
    bool m_fUseAbsHID;
    bool m_fPAEEnabled;
    bool m_fHwVirtExEnabled;
    bool m_fNestedPagingEnabled;
    int m_iRAMSize;
    int m_cCPUCount;
    int m_cCPUExecCap;
};
typedef UISettingsCache<UIDataSettingsMachineSystem> UICacheSettingsMachineSystem;

/* Machine settings / System page: */
class UIMachineSettingsSystem : public UISettingsPageMachine,
                             public Ui::UIMachineSettingsSystem
{
    Q_OBJECT;

public:

    UIMachineSettingsSystem();

    bool isHWVirtExEnabled() const;
    bool isHIDEnabled() const;
    KChipsetType chipsetType() const;
    void setOHCIEnabled(bool fEnabled);

signals:

    void tableChanged();

protected:

    /* Load data to cashe from corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void loadToCacheFrom(QVariant &data);
    /* Load data to corresponding widgets from cache,
     * this task SHOULD be performed in GUI thread only: */
    void getFromCache();

    /* Save data from corresponding widgets to cache,
     * this task SHOULD be performed in GUI thread only: */
    void putToCache();
    /* Save data from cache to corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void saveFromCacheTo(QVariant &data);

    /* Page changed: */
    bool changed() const { return m_cache.wasChanged(); }

    void setValidator (QIWidgetValidator *aVal);
    bool revalidate (QString &aWarning, QString &aTitle);

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private slots:

    void valueChangedRAM (int aVal);
    void textChangedRAM (const QString &aText);

    void onCurrentBootItemChanged (int);

    void valueChangedCPU (int aVal);
    void textChangedCPU (const QString &aText);
    void sltValueChangedCPUExecCap(int iValue);
    void sltTextChangedCPUExecCap(const QString &strText);

private:

    bool eventFilter (QObject *aObject, QEvent *aEvent);

    void adjustBootOrderTWSize();

    void polishPage();

    QIWidgetValidator *mValidator;

    uint mMinGuestCPU;
    uint mMaxGuestCPU;
    uint mMinGuestCPUExecCap;
    uint mMedGuestCPUExecCap;
    uint mMaxGuestCPUExecCap;

    QList<KDeviceType> m_possibleBootItems;

    bool m_fOHCIEnabled;

    /* Cache: */
    UICacheSettingsMachineSystem m_cache;
};

#endif // __UIMachineSettingsSystem_h__

