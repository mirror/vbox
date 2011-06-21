/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsNetwork class declaration
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

#ifndef __UIMachineSettingsNetwork_h__
#define __UIMachineSettingsNetwork_h__

/* Local includes */
#include "COMDefs.h"
#include "UISettingsPage.h"
#include "UIMachineSettingsNetwork.gen.h"
#include "UIMachineSettingsPortForwardingDlg.h"

/* Forward declarations */
class UIMachineSettingsNetworkPage;
class QITabWidget;

/* Machine settings / Network page / Adapter data: */
struct UIDataSettingsMachineNetworkAdapter
{
    /* Default constructor: */
    UIDataSettingsMachineNetworkAdapter()
        : m_iSlot(0)
        , m_fAdapterEnabled(false)
        , m_adapterType(KNetworkAdapterType_Null)
        , m_attachmentType(KNetworkAttachmentType_Null)
        , m_promiscuousMode(KNetworkAdapterPromiscModePolicy_Deny)
        , m_strBridgedAdapterName(QString())
        , m_strInternalNetworkName(QString())
        , m_strHostInterfaceName(QString())
        , m_strGenericDriver(QString())
        , m_strGenericProperties(QString())
        , m_strMACAddress(QString())
        , m_fCableConnected(false)
        , m_redirects(UIPortForwardingDataList()) {}
    /* Functions: */
    bool equal(const UIDataSettingsMachineNetworkAdapter &other) const
    {
        return (m_iSlot == other.m_iSlot) &&
               (m_fAdapterEnabled == other.m_fAdapterEnabled) &&
               (m_adapterType == other.m_adapterType) &&
               (m_attachmentType == other.m_attachmentType) &&
               (m_promiscuousMode == other.m_promiscuousMode) &&
               (m_strBridgedAdapterName == other.m_strBridgedAdapterName) &&
               (m_strInternalNetworkName == other.m_strInternalNetworkName) &&
               (m_strHostInterfaceName == other.m_strHostInterfaceName) &&
               (m_strGenericDriver == other.m_strGenericDriver) &&
               (m_strGenericProperties == other.m_strGenericProperties) &&
               (m_strMACAddress == other.m_strMACAddress) &&
               (m_fCableConnected == other.m_fCableConnected) &&
               (m_redirects == other.m_redirects);
    }
    /* Operators: */
    bool operator==(const UIDataSettingsMachineNetworkAdapter &other) const { return equal(other); }
    bool operator!=(const UIDataSettingsMachineNetworkAdapter &other) const { return !equal(other); }
    /* Variables: */
    int m_iSlot;
    bool m_fAdapterEnabled;
    KNetworkAdapterType m_adapterType;
    KNetworkAttachmentType m_attachmentType;
    KNetworkAdapterPromiscModePolicy m_promiscuousMode;
    QString m_strBridgedAdapterName;
    QString m_strInternalNetworkName;
    QString m_strHostInterfaceName;
    QString m_strGenericDriver;
    QString m_strGenericProperties;
    QString m_strMACAddress;
    bool m_fCableConnected;
    UIPortForwardingDataList m_redirects;
};
typedef UISettingsCache<UIDataSettingsMachineNetworkAdapter> UICacheSettingsMachineNetworkAdapter;

/* Machine settings / Network page / Network data: */
struct UIDataSettingsMachineNetwork
{
    /* Default constructor: */
    UIDataSettingsMachineNetwork() {}
    /* Operators: */
    bool operator==(const UIDataSettingsMachineNetwork& /* other */) const { return true; }
    bool operator!=(const UIDataSettingsMachineNetwork& /* other */) const { return false; }
};
typedef UISettingsCachePool<UIDataSettingsMachineNetwork, UICacheSettingsMachineNetworkAdapter> UICacheSettingsMachineNetwork;

/* Machine settings / Network page / Adapter tab: */
class UIMachineSettingsNetwork : public QIWithRetranslateUI<QWidget>,
                                 public Ui::UIMachineSettingsNetwork
{
    Q_OBJECT;

public:

    UIMachineSettingsNetwork(UIMachineSettingsNetworkPage *pParent);

    void polishTab();

    void fetchAdapterCache(const UICacheSettingsMachineNetworkAdapter &adapterCache);
    void uploadAdapterCache(UICacheSettingsMachineNetworkAdapter &adapterCache);

    void setValidator(QIWidgetValidator *pValidator);
    bool revalidate(QString &strWarning, QString &strTitle);

    QWidget* setOrderAfter(QWidget *pAfter);

    QString pageTitle() const;

    KNetworkAttachmentType attachmentType() const;
    QString alternativeName(int type = -1) const;

protected:

    void showEvent(QShowEvent *pEvent);

    void retranslateUi();

private slots:

    void sltUpdateAttachmentAlternative();
    void sltUpdateAlternativeName();
    void sltToggleAdvanced();
    void sltGenerateMac();
    void sltOpenPortForwardingDlg();

private:

    void populateComboboxes();

    UIMachineSettingsNetworkPage *m_pParent;
    QIWidgetValidator *m_pValidator;

    int m_iSlot;
    QString m_strBrgName;
    QString m_strIntName;
    QString m_strHoiName;
    QString m_strGenericDriver;

    bool m_fPolished;
    UIPortForwardingDataList m_portForwardingRules;
};

/* Machine settings / Network page: */
class UIMachineSettingsNetworkPage : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    UIMachineSettingsNetworkPage();

    QStringList brgList(bool aRefresh = false);
    QStringList intList(bool aRefresh = false);
    QStringList fullIntList(bool aRefresh = false);
    QStringList hoiList(bool aRefresh = false);

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

    void setValidator(QIWidgetValidator *pValidator);
    bool revalidate(QString &strWarning, QString &strTitle);

    void retranslateUi();

private slots:

    void updatePages();

private:

    void polishPage();

    static QString summarizeGenericProperties(const CNetworkAdapter &adapter);
    static void updateGenericProperties(CNetworkAdapter &adapter, const QString &strPropText);

    QIWidgetValidator *m_pValidator;
    QITabWidget *m_pTwAdapters;

    QStringList m_brgList;
    QStringList m_intList;
    QStringList m_hoiList;

    /* Cache: */
    UICacheSettingsMachineNetwork m_cache;
};

#endif // __UIMachineSettingsNetwork_h__

