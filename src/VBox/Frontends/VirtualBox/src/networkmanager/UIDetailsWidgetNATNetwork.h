/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsWidgetNATNetwork class declaration.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_networkmanager_UIDetailsWidgetNATNetwork_h
#define FEQT_INCLUDED_SRC_networkmanager_UIDetailsWidgetNATNetwork_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIPortForwardingTable.h"

/* Forward declarations: */
class QAbstractButton;
class QCheckBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QIDialogButtonBox;
class QILineEdit;
class QITabWidget;


/** Network Manager: NAT network data structure. */
struct UIDataNATNetwork
{
    /** Constructs data. */
    UIDataNATNetwork()
        : m_fExists(false)
        , m_fEnabled(false)
        , m_strName(QString())
        , m_strCIDR(QString())
        , m_fSupportsDHCP(false)
        , m_fSupportsIPv6(false)
        , m_fAdvertiseDefaultIPv6Route(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataNATNetwork &other) const
    {
        return true
               && (m_fExists == other.m_fExists)
               && (m_fEnabled == other.m_fEnabled)
               && (m_strName == other.m_strName)
               && (m_strCIDR == other.m_strCIDR)
               && (m_fSupportsDHCP == other.m_fSupportsDHCP)
               && (m_fSupportsIPv6 == other.m_fSupportsIPv6)
               && (m_fAdvertiseDefaultIPv6Route == other.m_fAdvertiseDefaultIPv6Route)
               && (m_rules4 == other.m_rules4)
               && (m_rules6 == other.m_rules6)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataNATNetwork &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataNATNetwork &other) const { return !equal(other); }

    /** Holds whether this network is not NULL. */
    bool                      m_fExists;
    /** Holds whether this network enabled. */
    bool                      m_fEnabled;
    /** Holds network name. */
    QString                   m_strName;
    /** Holds network CIDR. */
    QString                   m_strCIDR;
    /** Holds whether this network supports DHCP. */
    bool                      m_fSupportsDHCP;
    /** Holds whether this network supports IPv6. */
    bool                      m_fSupportsIPv6;
    /** Holds whether this network advertised as default IPv6 route. */
    bool                      m_fAdvertiseDefaultIPv6Route;
    /** Holds IPv4 port forwarding rules. */
    UIPortForwardingDataList  m_rules4;
    /** Holds IPv6 port forwarding rules. */
    UIPortForwardingDataList  m_rules6;
};


/** Network Manager: NAT network details-widget. */
class UIDetailsWidgetNATNetwork : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about data changed and whether it @a fDiffers. */
    void sigDataChanged(bool fDiffers);

    /** Notifies listeners about data change rejected and should be reseted. */
    void sigDataChangeRejected();
    /** Notifies listeners about data change accepted and should be applied. */
    void sigDataChangeAccepted();

public:

    /** Constructs medium details dialog passing @a pParent to the base-class.
      * @param  enmEmbedding  Brings embedding type. */
    UIDetailsWidgetNATNetwork(EmbedTo enmEmbedding, QWidget *pParent = 0);

    /** Returns the host network data. */
    const UIDataNATNetwork &data() const { return m_newData; }
    /** Defines the host network @a data.
      * @param  fHoldPosition  Holds whether we should try to keep
      *                        port forwarding rule position intact. */
    void setData(const UIDataNATNetwork &data, bool fHoldPosition = false);

    /** Revalidates changes. */
    bool revalidate();

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** @name Change handling stuff.
      * @{ */
        /** Handles network availability choice change. */
        void sltNetworkAvailabilityChanged(bool fChecked);
        /** Handles network name text change. */
        void sltNetworkNameChanged(const QString &strText);
        /** Handles network CIDR text change. */
        void sltNetworkCIDRChanged(const QString &strText);
        /** Handles network supports DHCP choice change. */
        void sltSupportsDHCPChanged(bool fChecked);
        /** Handles network supports IPv6 choice change. */
        void sltSupportsIPv6Changed(bool fChecked);
        /** Handles network advertised as default IPv6 route choice change. */
        void sltAdvertiseDefaultIPv6RouteChanged(bool fChecked);

        /** */
        void sltForwardingRulesIPv4Changed();
        /** */
        void sltForwardingRulesIPv6Changed();

        /** Handles button-box button click. */
        void sltHandleButtonBoxClick(QAbstractButton *pButton);
    /** @} */

private:

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares this. */
        void prepareThis();
        /** Prepares tab-widget. */
        void prepareTabWidget();
        /** Prepares 'Options' tab. */
        void prepareTabOptions();
        /** Prepares 'Forwarding' tab. */
        void prepareTabForwarding();
    /** @} */

    /** @name Loading stuff.
      * @{ */
        /** Loads 'Options' data. */
        void loadDataForOptions();
        /** Loads 'Forwarding' data. */
        void loadDataForForwarding();
    /** @} */

    /** @name Change handling stuff.
      * @{ */
        /** Updates button states. */
        void updateButtonStates();
    /** @} */

    /** @name General variables.
      * @{ */
        /** Holds the parent widget embedding type. */
        const EmbedTo m_enmEmbedding;

        /** Holds the old data copy. */
        UIDataNATNetwork  m_oldData;
        /** Holds the new data copy. */
        UIDataNATNetwork  m_newData;

        /** Holds the tab-widget. */
        QITabWidget *m_pTabWidget;
    /** @} */

    /** @name Network variables.
      * @{ */
        /** Holds the network availability check-box instance. */
        QCheckBox         *m_pCheckboxNetworkAvailable;
        /** Holds the network name label instance. */
        QLabel            *m_pLabelNetworkName;
        /** Holds the network name editor instance. */
        QLineEdit         *m_pEditorNetworkName;
        /** Holds the network CIDR label instance. */
        QLabel            *m_pLabelNetworkCIDR;
        /** Holds the network CIDR editor instance. */
        QLineEdit         *m_pEditorNetworkCIDR;
        /** Holds the extended label instance. */
        QLabel            *m_pLabelExtended;
        /** Holds the 'supports DHCP' check-box instance. */
        QCheckBox         *m_pCheckboxSupportsDHCP;
        /** Holds the 'supports IPv6' check-box instance. */
        QCheckBox         *m_pCheckboxSupportsIPv6;
        /** Holds the 'advertise default IPv6 route' check-box instance. */
        QCheckBox         *m_pCheckboxAdvertiseDefaultIPv6Route;
        /** Holds the 'Options' button-box instance. */
        QIDialogButtonBox *m_pButtonBoxOptions;
    /** @} */

    /** @name Forwarding variables.
      * @{ */
        /** */
        QITabWidget           *m_pTabWidgetForwarding;
        /** */
        UIPortForwardingTable *m_pForwardingTableIPv4;
        /** */
        UIPortForwardingTable *m_pForwardingTableIPv6;
        /** Holds the 'Forwarding' button-box instance. */
        QIDialogButtonBox     *m_pButtonBoxForwarding;
        /** Holds whether we should try to keep
          * port forwarding rule position intact. */
        bool                   m_fHoldPosition;
    /** @} */
};


#endif /* !FEQT_INCLUDED_SRC_networkmanager_UIDetailsWidgetNATNetwork_h */

