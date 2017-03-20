/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsUSB class declaration.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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
        , m_fHostUSBDevice(false)
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
typedef UISettingsCache<UIDataSettingsMachineUSBFilter> UISettingsCacheMachineUSBFilter;

/* Common settings / USB page / USB data: */
struct UIDataSettingsMachineUSB
{
    /* Default constructor: */
    UIDataSettingsMachineUSB()
        : m_fUSBEnabled(false)
        , m_USBControllerType(KUSBControllerType_Null) {}
    /* Functions: */
    bool equal(const UIDataSettingsMachineUSB &other) const
    {
        return (m_fUSBEnabled == other.m_fUSBEnabled) &&
               (m_USBControllerType == other.m_USBControllerType);
    }
    /* Operators: */
    bool operator==(const UIDataSettingsMachineUSB &other) const { return equal(other); }
    bool operator!=(const UIDataSettingsMachineUSB &other) const { return !equal(other); }
    /* Variables: */
    bool m_fUSBEnabled;
    KUSBControllerType m_USBControllerType;
};
typedef UISettingsCachePool<UIDataSettingsMachineUSB, UISettingsCacheMachineUSBFilter> UISettingsCacheMachineUSB;

/* Common settings / USB page: */
class UIMachineSettingsUSB : public UISettingsPageMachine,
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

    UIMachineSettingsUSB();
    ~UIMachineSettingsUSB();

    bool isUSBEnabled() const;

protected:

    /** Loads data into the cache from corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void loadToCacheFrom(QVariant &data);
    /** Loads data into corresponding widgets from the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void getFromCache();

    /** Saves data from corresponding widgets to the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void putToCache();
    /** Saves data from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void saveFromCacheTo(QVariant &data);

    /** Returns whether the page content was changed. */
    bool changed() const { return m_cache.wasChanged(); }

    /** Performs validation, updates @a messages list if something is wrong. */
    bool validate(QList<UIValidationMessage> &messages);

    /** Defines TAB order. */
    void setOrderAfter(QWidget *pWidget);

    /** Handles translation event. */
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

    /* Helper: Prepare stuff: */
    void prepareValidation();

    void addUSBFilter(const UIDataSettingsMachineUSBFilter &usbFilterData, bool fIsNew);

    /* Returns the multi-line description of the given USB filter: */
    static QString toolTipFor(const UIDataSettingsMachineUSBFilter &data);

    void polishPage();

    /* Other variables: */
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
    UISettingsCacheMachineUSB m_cache;
};

#endif // __UIMachineSettingsUSB_h__

