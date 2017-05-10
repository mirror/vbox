/* $Id$ */
/** @file
 * VBox Qt GUI - UIHostNetworkDetailsDialog class declaration.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIHostNetworkDetailsDialog_h___
#define ___UIHostNetworkDetailsDialog_h___

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QCheckBox;
class QLabel;
class QRadioButton;
class QILineEdit;
class QITabWidget;


/** Host Network Manager: Host Network Interface data structure. */
struct UIDataHostNetworkInterface
{
    /** Constructs data. */
    UIDataHostNetworkInterface()
        : m_strName(QString())
        , m_fDHCPEnabled(false)
        , m_strAddress(QString())
        , m_strMask(QString())
        , m_fSupportedIPv6(false)
        , m_strAddress6(QString())
        , m_strMaskLength6(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataHostNetworkInterface &other) const
    {
        return true
               && (m_strName == other.m_strName)
               && (m_fDHCPEnabled == other.m_fDHCPEnabled)
               && (m_strAddress == other.m_strAddress)
               && (m_strMask == other.m_strMask)
               && (m_fSupportedIPv6 == other.m_fSupportedIPv6)
               && (m_strAddress6 == other.m_strAddress6)
               && (m_strMaskLength6 == other.m_strMaskLength6)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataHostNetworkInterface &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataHostNetworkInterface &other) const { return !equal(other); }

    /** Holds interface name. */
    QString m_strName;
    /** Holds whether DHCP is enabled for that interface. */
    bool m_fDHCPEnabled;
    /** Holds IPv4 interface address. */
    QString m_strAddress;
    /** Holds IPv4 interface mask. */
    QString m_strMask;
    /** Holds whether IPv6 protocol supported. */
    bool m_fSupportedIPv6;
    /** Holds IPv6 interface address. */
    QString m_strAddress6;
    /** Holds IPv6 interface mask length. */
    QString m_strMaskLength6;
};


/** Host Network Manager: DHCP Server data structure. */
struct UIDataDHCPServer
{
    /** Constructs data. */
    UIDataDHCPServer()
        : m_fEnabled(false)
        , m_strAddress(QString())
        , m_strMask(QString())
        , m_strLowerAddress(QString())
        , m_strUpperAddress(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataDHCPServer &other) const
    {
        return true
               && (m_fEnabled == other.m_fEnabled)
               && (m_strAddress == other.m_strAddress)
               && (m_strMask == other.m_strMask)
               && (m_strLowerAddress == other.m_strLowerAddress)
               && (m_strUpperAddress == other.m_strUpperAddress)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataDHCPServer &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataDHCPServer &other) const { return !equal(other); }

    /** Holds whether DHCP server enabled. */
    bool m_fEnabled;
    /** Holds DHCP server address. */
    QString m_strAddress;
    /** Holds DHCP server mask. */
    QString m_strMask;
    /** Holds DHCP server lower address. */
    QString m_strLowerAddress;
    /** Holds DHCP server upper address. */
    QString m_strUpperAddress;
};


/** Host Network Manager: Host network data structure. */
struct UIDataHostNetwork
{
    /** Constructs data. */
    UIDataHostNetwork()
        : m_interface(UIDataHostNetworkInterface())
        , m_dhcpserver(UIDataDHCPServer())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataHostNetwork &other) const
    {
        return true
               && (m_interface == other.m_interface)
               && (m_dhcpserver == other.m_dhcpserver)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataHostNetwork &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataHostNetwork &other) const { return !equal(other); }

    /** Holds the interface data. */
    UIDataHostNetworkInterface m_interface;
    /** Holds the DHCP server data. */
    UIDataDHCPServer m_dhcpserver;
};


/** Host Network Manager: Host network details widget. */
class UIHostNetworkDetailsDialog : public QIWithRetranslateUI2<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about data changed and whether it @a fDiffers. */
    void sigDataChanged(bool fDiffers);

public:

    /** Constructs host network details dialog for the passed @a pParent and @a data. */
    UIHostNetworkDetailsDialog(QWidget *pParent = 0);

    /** Returns the host network data. */
    const UIDataHostNetwork &data() const { return m_newData; }
    /** Defines the host network @a data. */
    void setData(const UIDataHostNetwork &data);
    /** Clears the host network data. */
    void clearData();

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** @name Change handling stuff.
     * @{ */
        /** Handles interface automatic configuration choice change. */
        void sltToggledButtonAutomatic(bool fChecked);
        /** Handles interface manual configuration choice change. */
        void sltToggledButtonManual(bool fChecked);
        /** Handles interface IPv4 text change. */
        void sltTextChangedIPv4(const QString &strText);
        /** Handles interface NMv4 text change. */
        void sltTextChangedNMv4(const QString &strText);
        /** Handles interface IPv6 text change. */
        void sltTextChangedIPv6(const QString &strText);
        /** Handles interface NMv6 text change. */
        void sltTextChangedNMv6(const QString &strText);

        /** Handles DHCP server status change. */
        void sltStatusChangedServer(int iChecked);
        /** Handles DHCP server address text change. */
        void sltTextChangedAddress(const QString &strText);
        /** Handles DHCP server mask text change. */
        void sltTextChangedMask(const QString &strText);
        /** Handles DHCP server lower address text change. */
        void sltTextChangedLowerAddress(const QString &strText);
        /** Handles DHCP server upper address text change. */
        void sltTextChangedUpperAddress(const QString &strText);
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
        /** Prepares 'Interface' tab. */
        void prepareTabInterface();
        /** Prepares 'DHCP server' tab. */
        void prepareTabDHCPServer();
    /** @} */

    /** @name Loading stuff.
     * @{ */
        /** Loads interface data. */
        void loadDataForInterface();
        /** Loads server data. */
        void loadDataForDHCPServer();
    /** @} */

    /** @name Change handling stuff.
     * @{ */
        /** Revalidates changes for passed @a pWidget. */
        void revalidate(QWidget *pWidget = 0);
        /** Retranslates validation for passed @a pWidget. */
        void retranslateValidation(QWidget *pWidget = 0);
        /** Notifies listeners about data changed or not. */
        void notify();
    /** @} */

    /** @name General variables.
     * @{ */
        /** Holds the old data copy. */
        UIDataHostNetwork  m_oldData;
        /** Holds the new data copy. */
        UIDataHostNetwork  m_newData;
        /** Holds the tab-widget. */
        QITabWidget       *m_pTabWidget;
    /** @} */

    /** @name Interface variables.
     * @{ */
        /** Holds the automatic interface configuration button. */
        QRadioButton *m_pButtonAutomatic;
        /** Holds the automatic interface configuration error pane. */
        QLabel       *m_pErrorPaneAutomatic;

        /** Holds the manual interface configuration button. */
        QRadioButton *m_pButtonManual;

        /** Holds the IPv4 address label. */
        QLabel       *m_pLabelIPv4;
        /** Holds the IPv4 address editor. */
        QILineEdit   *m_pEditorIPv4;
        /** Holds the IPv4 address error pane. */
        QLabel       *m_pErrorPaneIPv4;

        /** Holds the IPv4 network mask label. */
        QLabel       *m_pLabelNMv4;
        /** Holds the IPv4 network mask editor. */
        QILineEdit   *m_pEditorNMv4;
        /** Holds the IPv4 network mask error pane. */
        QLabel       *m_pErrorPaneNMv4;

        /** Holds the IPv6 address label. */
        QLabel       *m_pLabelIPv6;
        /** Holds the IPv6 address editor. */
        QILineEdit   *m_pEditorIPv6;
        /** Holds the IPv6 address error pane. */
        QLabel       *m_pErrorPaneIPv6;

        /** Holds the IPv6 network mask label. */
        QLabel       *m_pLabelNMv6;
        /** Holds the IPv6 network mask editor. */
        QILineEdit   *m_pEditorNMv6;
        /** Holds the IPv6 network mask error pane. */
        QLabel       *m_pErrorPaneNMv6;
    /** @} */

    /** @name DHCP server variables.
     * @{ */
        /** Holds the DHCP server status chack-box. */
        QCheckBox  *m_pCheckBoxDHCP;

        /** Holds the DHCP address label. */
        QLabel     *m_pLabelDHCPAddress;
        /** Holds the DHCP address editor. */
        QILineEdit *m_pEditorDHCPAddress;
        /** Holds the DHCP address error pane. */
        QLabel     *m_pErrorPaneDHCPAddress;

        /** Holds the DHCP network mask label. */
        QLabel     *m_pLabelDHCPMask;
        /** Holds the DHCP network mask editor. */
        QILineEdit *m_pEditorDHCPMask;
        /** Holds the DHCP network mask error pane. */
        QLabel     *m_pErrorPaneDHCPMask;

        /** Holds the DHCP lower address label. */
        QLabel     *m_pLabelDHCPLowerAddress;
        /** Holds the DHCP lower address editor. */
        QILineEdit *m_pEditorDHCPLowerAddress;
        /** Holds the DHCP lower address error pane. */
        QLabel     *m_pErrorPaneDHCPLowerAddress;

        /** Holds the DHCP upper address label. */
        QLabel     *m_pLabelDHCPUpperAddress;
        /** Holds the DHCP upper address editor. */
        QILineEdit *m_pEditorDHCPUpperAddress;
        /** Holds the DHCP upper address error pane. */
        QLabel     *m_pErrorPaneDHCPUpperAddress;
    /** @} */
};

#endif /* !___UIHostNetworkDetailsDialog_h___ */

