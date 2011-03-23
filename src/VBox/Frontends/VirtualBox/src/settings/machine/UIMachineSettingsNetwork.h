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
struct UINetworkAdapterData
{
    int m_iSlot;
    /* CNetworkAdapter used only for Generate MAC ability! */
    CNetworkAdapter m_adapter;
    bool m_fAdapterEnabled;
    KNetworkAdapterType m_adapterType;
    KNetworkAttachmentType m_attachmentType;
    KNetworkAdapterPromiscModePolicy m_promiscuousMode;
    QString m_strBridgedAdapterName;
    QString m_strInternalNetworkName;
    QString m_strHostInterfaceName;
#ifdef VBOX_WITH_VDE
    QString m_strVDENetworkName;
#endif /* VBOX_WITH_VDE */
    QString m_strMACAddress;
    bool m_fCableConnected;
    UIPortForwardingDataList m_redirects;
};

/* Machine settings / Network page / Cache: */
struct UISettingsCacheMachineNetwork
{
    QList<UINetworkAdapterData> m_items;
};

/* Machine settings / Network page / Adapter tab: */
class UIMachineSettingsNetwork : public QIWithRetranslateUI<QWidget>,
                                 public Ui::UIMachineSettingsNetwork
{
    Q_OBJECT;

public:

    UIMachineSettingsNetwork(UIMachineSettingsNetworkPage *pParent, bool fDisableStaticControls = false);

    void polishTab();

    void fetchAdapterData(const UINetworkAdapterData &data);
    void uploadAdapterData(UINetworkAdapterData &data);

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
    CNetworkAdapter m_adapter;

    QString m_strBrgName;
    QString m_strIntName;
    QString m_strHoiName;
#ifdef VBOX_WITH_VDE
    QString mVDEName;
#endif /* VBOX_WITH_VDE */

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

    void setValidator(QIWidgetValidator *pValidator);
    bool revalidate(QString &strWarning, QString &strTitle);

    void retranslateUi();

private slots:

    void updatePages();

private:

    void polishPage();

    QIWidgetValidator *m_pValidator;
    QITabWidget *m_pTwAdapters;

    QStringList m_brgList;
    QStringList m_intList;
    QStringList m_hoiList;

    /* Cache: */
    UISettingsCacheMachineNetwork m_cache;
};

#endif // __UIMachineSettingsNetwork_h__

