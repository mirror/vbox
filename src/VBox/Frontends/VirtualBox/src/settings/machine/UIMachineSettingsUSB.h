/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsUSB class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsUSB.gen.h"

/* Forward declarations: */
class VBoxUSBMenu;
class UIToolBar;

/* Common settings / USB page / USB filter data: */
struct UIDataSettingsMachineUSBFilter
{
    /* Default constructor: */
    UIDataSettingsMachineUSBFilter()
        : m_fActive(false)
        , m_strName(QString())
        , m_strVendorId(QString())
        , m_strProductId(QString())
        , m_strRevision(QString())
        , m_strManufacturer(QString())
        , m_strProduct(QString())
        , m_strSerialNumber(QString())
        , m_strPort(QString())
        , m_strRemote(QString())
        , m_action(KUSBDeviceFilterAction_Null)
        , m_hostUSBDeviceState(KUSBDeviceState_NotSupported) {}
    /* Functions: */
    bool equal(const UIDataSettingsMachineUSBFilter &other) const
    {
        return (m_fActive == other.m_fActive) &&
               (m_strName == other.m_strName) &&
               (m_strVendorId == other.m_strVendorId) &&
               (m_strProductId == other.m_strProductId) &&
               (m_strRevision == other.m_strRevision) &&
               (m_strManufacturer == other.m_strManufacturer) &&
               (m_strProduct == other.m_strProduct) &&
               (m_strSerialNumber == other.m_strSerialNumber) &&
               (m_strPort == other.m_strPort) &&
               (m_strRemote == other.m_strRemote) &&
               (m_action == other.m_action) &&
               (m_hostUSBDeviceState == other.m_hostUSBDeviceState);
    }
    /* Operators: */
    bool operator==(const UIDataSettingsMachineUSBFilter &other) const { return equal(other); }
    bool operator!=(const UIDataSettingsMachineUSBFilter &other) const { return !equal(other); }
    /* Common variables: */
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
    /* Host only variables: */
    KUSBDeviceFilterAction m_action;
    bool m_fHostUSBDevice;
    KUSBDeviceState m_hostUSBDeviceState;
};
typedef UISettingsCache<UIDataSettingsMachineUSBFilter> UICacheSettingsMachineUSBFilter;

/* Common settings / USB page / USB data: */
struct UIDataSettingsMachineUSB
{
    /* Default constructor: */
    UIDataSettingsMachineUSB()
        : m_fUSBEnabled(false)
        , m_fEHCIEnabled(false) {}
    /* Functions: */
    bool equal(const UIDataSettingsMachineUSB &other) const
    {
        return (m_fUSBEnabled == other.m_fUSBEnabled) &&
               (m_fEHCIEnabled == other.m_fEHCIEnabled);
    }
    /* Operators: */
    bool operator==(const UIDataSettingsMachineUSB &other) const { return equal(other); }
    bool operator!=(const UIDataSettingsMachineUSB &other) const { return !equal(other); }
    /* Variables: */
    bool m_fUSBEnabled;
    bool m_fEHCIEnabled;
};
typedef UISettingsCachePool<UIDataSettingsMachineUSB, UICacheSettingsMachineUSBFilter> UICacheSettingsMachineUSB;

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

    /* Load data to cache from corresponding external object(s),
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
    bool revalidate(QString &strWarningText, QString &strTitle);

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private slots:

    void usbAdapterToggled(bool fEnabled);
    void currentChanged (QTreeWidgetItem *aItem = 0);

    void newClicked();
    void addClicked();
    void edtClicked();
    void addConfirmed (QAction *aAction);
    void delClicked();
    void mupClicked();
    void mdnClicked();
    void showContextMenu(const QPoint &pos);
    void sltUpdateActivityState(QTreeWidgetItem *pChangedItem);

private:

    void addUSBFilter(const UIDataSettingsMachineUSBFilter &usbFilterData, bool fIsNew);

    /* Fetch data to m_properties, m_settings or m_machine: */
    void fetchData(const QVariant &data);

    /* Upload m_properties, m_settings or m_machine to data: */
    void uploadData(QVariant &data) const;

    /* Returns the multi-line description of the given USB filter: */
    static QString toolTipFor(const UIDataSettingsMachineUSBFilter &data);

    void polishPage();

    /* Global data source: */
    CSystemProperties m_properties;
    VBoxGlobalSettings m_settings;

    /* Machine data source: */
    CMachine m_machine;
    CConsole m_console;

    /* Other variables: */
    QIWidgetValidator *mValidator;
    UIToolBar *m_pToolBar;
    QAction *mNewAction;
    QAction *mAddAction;
    QAction *mEdtAction;
    QAction *mDelAction;
    QAction *mMupAction;
    QAction *mMdnAction;
    VBoxUSBMenu *mUSBDevicesMenu;
    QString mUSBFilterName;
    QList<UIDataSettingsMachineUSBFilter> m_filters;

    /* Cache: */
    UICacheSettingsMachineUSB m_cache;
};

#endif // __UIMachineSettingsUSB_h__

