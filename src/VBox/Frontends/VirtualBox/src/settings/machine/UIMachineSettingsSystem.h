/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsSystem class declaration
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

#ifndef __UIMachineSettingsSystem_h__
#define __UIMachineSettingsSystem_h__

#include "COMDefs.h"
#include "UISettingsPage.h"
#include "UIMachineSettingsSystem.gen.h"

/* Machine settings / System page / Boot item: */
struct UIBootItemData
{
    KDeviceType m_type;
    bool m_fEnabled;
};

/* Machine settings / System page / Cache: */
struct UISettingsCacheMachineSystem
{
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
};

/* Machine settings / System page: */
class UIMachineSettingsSystem : public UISettingsPageMachine,
                             public Ui::UIMachineSettingsSystem
{
    Q_OBJECT;

public:

    UIMachineSettingsSystem();

    bool isHWVirtExEnabled() const;
    int cpuCount() const;
    bool isHIDEnabled() const;
    KChipsetType chipsetType() const;

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

private:

    bool eventFilter (QObject *aObject, QEvent *aEvent);

    void adjustBootOrderTWSize();

    QIWidgetValidator *mValidator;

    uint mMinGuestCPU;
    uint mMaxGuestCPU;

    QList<KDeviceType> m_possibleBootItems;

    /* Cache: */
    UISettingsCacheMachineSystem m_cache;
};

#endif // __UIMachineSettingsSystem_h__

