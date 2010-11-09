/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsUSB class declaration
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMachineSettingsUSB_h__
#define __UIMachineSettingsUSB_h__

#include "UISettingsPage.h"
#include "UIMachineSettingsUSB.gen.h"
#include "COMDefs.h"

class VBoxUSBMenu;

/* Common settings / USB page / Filter data: */
struct UIUSBFilterData
{
    /* Common: */
    bool m_fActive;
    QString m_strName;
    QString m_strVendorId;
    QString m_strProductId;
    QString m_strRevision;
    QString m_strManufacturer;
    QString m_strProduct;
    QString m_strSerialNumber;
    QString m_strPort;
    QString m_strRemote;

    /* Host only: */
    KUSBDeviceFilterAction m_action;
    bool m_fHostUSBDevice;
    KUSBDeviceState m_hostUSBDeviceState;
};

/* Common settings / USB page / Cache: */
struct UISettingsCacheCommonUSB
{
    bool m_fUSBEnabled;
    bool m_fEHCIEnabled;
    QList<UIUSBFilterData> m_items;
};

/* Common settings / USB page: */
class UIMachineSettingsUSB : public UISettingsPage,
                          public Ui::UIMachineSettingsUSB
{
    Q_OBJECT;

public:

    enum RemoteMode
    {
        ModeAny = 0,
        ModeOn,
        ModeOff
    };

    UIMachineSettingsUSB(UISettingsPageType type);

    bool isOHCIEnabled() const;

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

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private slots:

    void usbAdapterToggled (bool aOn);
    void currentChanged (QTreeWidgetItem *aItem = 0,
                         QTreeWidgetItem *aPrev = 0);

    void newClicked();
    void addClicked();
    void edtClicked();
    void addConfirmed (QAction *aAction);
    void delClicked();
    void mupClicked();
    void mdnClicked();
    void showContextMenu (const QPoint &aPos);
    void sltUpdateActivityState(QTreeWidgetItem *pChangedItem);
    void markSettingsChanged();

private:

    void addUSBFilter(const UIUSBFilterData &data, bool fIsNew);

    /* Fetch data to m_properties, m_settings or m_machine: */
    void fetchData(const QVariant &data);

    /* Upload m_properties, m_settings or m_machine to data: */
    void uploadData(QVariant &data) const;

    /* Returns the multi-line description of the given USB filter: */
    static QString toolTipFor(const UIUSBFilterData &data);

    /* Global data source: */
    CSystemProperties m_properties;
    VBoxGlobalSettings m_settings;

    /* Machine data source: */
    CMachine m_machine;

    /* Other variables: */
    QIWidgetValidator *mValidator;
    QAction *mNewAction;
    QAction *mAddAction;
    QAction *mEdtAction;
    QAction *mDelAction;
    QAction *mMupAction;
    QAction *mMdnAction;
    QMenu *mMenu;
    VBoxUSBMenu *mUSBDevicesMenu;
    bool mUSBFilterListModified;
    QString mUSBFilterName;

    /* Cache: */
    UISettingsCacheCommonUSB m_cache;
};

#endif // __UIMachineSettingsUSB_h__

