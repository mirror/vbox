/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsNetwork class declaration.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
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

/* Local includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsNetwork.gen.h"
#include "UIMachineSettingsPortForwardingDlg.h"

/* Forward declarations: */
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
        , m_strGenericDriverName(QString())
        , m_strGenericProperties(QString())
        , m_strNATNetworkName(QString())
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
               (m_strGenericDriverName == other.m_strGenericDriverName) &&
               (m_strGenericProperties == other.m_strGenericProperties) &&
               (m_strNATNetworkName == other.m_strNATNetworkName) &&
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
    QString m_strGenericDriverName;
    QString m_strGenericProperties;
    QString m_strNATNetworkName;
    QString m_strMACAddress;
    bool m_fCableConnected;
    UIPortForwardingDataList m_redirects;
};
typedef UISettingsCache<UIDataSettingsMachineNetworkAdapter> UISettingsCacheMachineNetworkAdapter;

/* Machine settings / Network page / Network data: */
struct UIDataSettingsMachineNetwork
{
    /* Default constructor: */
    UIDataSettingsMachineNetwork() {}
    /* Operators: */
    bool operator==(const UIDataSettingsMachineNetwork& /* other */) const { return true; }
    bool operator!=(const UIDataSettingsMachineNetwork& /* other */) const { return false; }
};
typedef UISettingsCachePool<UIDataSettingsMachineNetwork, UISettingsCacheMachineNetworkAdapter> UISettingsCacheMachineNetwork;

/* Machine settings / Network page / Adapter tab: */
class UIMachineSettingsNetwork : public QIWithRetranslateUI<QWidget>, public Ui::UIMachineSettingsNetwork
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIMachineSettingsNetwork(UIMachineSettingsNetworkPage *pParent);

    /* Load / Save API: */
    void fetchAdapterCache(const UISettingsCacheMachineNetworkAdapter &adapterCache);
    void uploadAdapterCache(UISettingsCacheMachineNetworkAdapter &adapterCache);

    /** Performs validation, updates @a messages list if something is wrong. */
    bool validate(QList<UIValidationMessage> &messages);

    /* Navigation stuff: */
    QWidget* setOrderAfter(QWidget *pAfter);

    /* Other public stuff: */
    QString tabTitle() const;
    KNetworkAttachmentType attachmentType() const;
    QString alternativeName(int iType = -1) const;
    void polishTab();
    void reloadAlternative();

    /** Defines whether the advanced button is @a fExpanded. */
    void setAdvancedButtonState(bool fExpanded);

signals:

    /* Signal to notify listeners about tab content changed: */
    void sigTabUpdated();

    /** Notifies about the advanced button has @a fExpanded. */
    void sigNotifyAdvancedButtonStateChange(bool fExpanded);

protected:

    /** Handles translation event. */
    void retranslateUi();

private slots:

    /* Different handlers: */
    void sltHandleAdapterActivityChange();
    void sltHandleAttachmentTypeChange();
    void sltHandleAlternativeNameChange();
    void sltHandleAdvancedButtonStateChange();
    void sltGenerateMac();
    void sltOpenPortForwardingDlg();

private:

    /* Helper: Prepare stuff: */
    void prepareValidation();

    /* Helping stuff: */
    void populateComboboxes();
    void updateAlternativeList();
    void updateAlternativeName();

    /** Handles advanced button state change. */
    void handleAdvancedButtonStateChange();

    /* Various static stuff: */
    static int position(QComboBox *pComboBox, int iData);
    static int position(QComboBox *pComboBox, const QString &strText);

    /* Parent page: */
    UIMachineSettingsNetworkPage *m_pParent;

    /* Other variables: */
    int m_iSlot;
    QString m_strBridgedAdapterName;
    QString m_strInternalNetworkName;
    QString m_strHostInterfaceName;
    QString m_strGenericDriverName;
    QString m_strNATNetworkName;
    UIPortForwardingDataList m_portForwardingRules;
};

/* Machine settings / Network page: */
class UIMachineSettingsNetworkPage : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIMachineSettingsNetworkPage();

    /* Bridged adapter list: */
    const QStringList& bridgedAdapterList() const { return m_bridgedAdapterList; }
    /* Internal network list: */
    const QStringList& internalNetworkList() const { return m_internalNetworkList; }
    /* Host-only interface list: */
    const QStringList& hostInterfaceList() const { return m_hostInterfaceList; }
    /* Generic driver list: */
    const QStringList& genericDriverList() const { return m_genericDriverList; }
    /* NAT network list: */
    const QStringList& natNetworkList() const { return m_natNetworkList; }

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

    /** Handles translation event. */
    void retranslateUi();

private slots:

    /* Handles tab updates: */
    void sltHandleUpdatedTab();

    /** Handles advanced button state change to @a fExpanded. */
    void sltHandleAdvancedButtonStateChange(bool fExpanded);

private:

    /* Private helpers: */
    void polishPage();
    void refreshBridgedAdapterList();
    void refreshInternalNetworkList(bool fFullRefresh = false);
    void refreshHostInterfaceList();
    void refreshGenericDriverList(bool fFullRefresh = false);
    void refreshNATNetworkList();

    /* Various static stuff: */
    static QStringList otherInternalNetworkList();
    static QStringList otherGenericDriverList();
    static QString summarizeGenericProperties(const CNetworkAdapter &adapter);
    static void updateGenericProperties(CNetworkAdapter &adapter, const QString &strPropText);

    /* Tab holder: */
    QITabWidget *m_pTwAdapters;

    /* Alternative-name lists: */
    QStringList m_bridgedAdapterList;
    QStringList m_internalNetworkList;
    QStringList m_hostInterfaceList;
    QStringList m_genericDriverList;
    QStringList m_natNetworkList;

    /* Cache: */
    UISettingsCacheMachineNetwork m_cache;
};

#endif // __UIMachineSettingsNetwork_h__

