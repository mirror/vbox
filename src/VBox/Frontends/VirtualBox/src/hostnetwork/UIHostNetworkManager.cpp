/* $Id$ */
/** @file
 * VBox Qt GUI - UIHostNetworkManager class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QHeaderView>
# include <QMenuBar>
# include <QPushButton>

/* GUI includes: */
# include "QIDialogButtonBox.h"
# include "QITreeWidget.h"
# include "UIIconPool.h"
# include "UIMessageCenter.h"
# include "UIHostNetworkDetailsDialog.h"
# include "UIHostNetworkManager.h"
# include "UIToolBar.h"
# ifdef VBOX_WS_MAC
#  include "UIWindowMenuManager.h"
# endif /* VBOX_WS_MAC */
# include "VBoxGlobal.h"

/* COM includes: */
# include "CDHCPServer.h"
# include "CHost.h"
# include "CHostNetworkInterface.h"

/* Other VBox includes: */
# include <iprt/cidr.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Host Network Manager: Tree-widget item. */
class UIItemHostNetwork : public QITreeWidgetItem, public UIDataHostNetwork
{
public:

    /** Updates item fields from data. */
    void updateFields();

    /** Returns item name. */
    QString name() const { return m_interface.m_strName; }
};


/*********************************************************************************************************************************
*   Class UIItemHostNetwork implementation.                                                                                      *
*********************************************************************************************************************************/

void UIItemHostNetwork::updateFields()
{
    /* Compose item name/tool-tip: */
    setText(0, m_interface.m_strName);
    const QString strTable("<table cellspacing=5>%1</table>");
    const QString strHeader("<tr><td><nobr>%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    const QString strSubHeader("<tr><td><nobr>&nbsp;&nbsp;%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    QString strToolTip;

    /* Interface information: */
    strToolTip += strHeader.arg(UIHostNetworkManager::tr("Adapter"))
                           .arg(UIHostNetworkManager::tr("Manually configured", "interface"));
    strToolTip += strSubHeader.arg(UIHostNetworkManager::tr("IPv4 Address"))
                              .arg(m_interface.m_strInterfaceAddress.isEmpty() ?
                                   UIHostNetworkManager::tr ("Not set", "address") :
                                   m_interface.m_strInterfaceAddress) +
                  strSubHeader.arg(UIHostNetworkManager::tr("IPv4 Network Mask"))
                              .arg(m_interface.m_strInterfaceMask.isEmpty() ?
                                   UIHostNetworkManager::tr ("Not set", "mask") :
                                   m_interface.m_strInterfaceMask);
    if (m_interface.m_fIpv6Supported)
    {
        strToolTip += strSubHeader.arg(UIHostNetworkManager::tr("IPv6 Address"))
                                  .arg(m_interface.m_strInterfaceAddress6.isEmpty() ?
                                       UIHostNetworkManager::tr("Not set", "address") :
                                       m_interface.m_strInterfaceAddress6) +
                      strSubHeader.arg(UIHostNetworkManager::tr("IPv6 Network Mask Length"))
                                  .arg(m_interface.m_strInterfaceMaskLength6.isEmpty() ?
                                       UIHostNetworkManager::tr("Not set", "length") :
                                       m_interface.m_strInterfaceMaskLength6);
    }

    /* DHCP server information: */
    strToolTip += strHeader.arg(UIHostNetworkManager::tr("DHCP Server"))
                           .arg(m_dhcpserver.m_fDhcpServerEnabled ?
                                UIHostNetworkManager::tr("Enabled", "server") :
                                UIHostNetworkManager::tr("Disabled", "server"));
    if (m_dhcpserver.m_fDhcpServerEnabled)
    {
        strToolTip += strSubHeader.arg(UIHostNetworkManager::tr("Address"))
                                  .arg(m_dhcpserver.m_strDhcpServerAddress.isEmpty() ?
                                       UIHostNetworkManager::tr("Not set", "address") :
                                       m_dhcpserver.m_strDhcpServerAddress) +
                      strSubHeader.arg(UIHostNetworkManager::tr("Network Mask"))
                                  .arg(m_dhcpserver.m_strDhcpServerMask.isEmpty() ?
                                       UIHostNetworkManager::tr("Not set", "mask") :
                                       m_dhcpserver.m_strDhcpServerMask) +
                      strSubHeader.arg(UIHostNetworkManager::tr("Lower Bound"))
                                  .arg(m_dhcpserver.m_strDhcpLowerAddress.isEmpty() ?
                                       UIHostNetworkManager::tr("Not set", "bound") :
                                       m_dhcpserver.m_strDhcpLowerAddress) +
                      strSubHeader.arg(UIHostNetworkManager::tr("Upper Bound"))
                                  .arg(m_dhcpserver.m_strDhcpUpperAddress.isEmpty() ?
                                       UIHostNetworkManager::tr("Not set", "bound") :
                                       m_dhcpserver.m_strDhcpUpperAddress);
    }

    /* Assign tool-tip finally: */
    setToolTip(0, strTable.arg(strToolTip));
}


/*********************************************************************************************************************************
*   Class UIHostNetworkManager implementation.                                                                                   *
*********************************************************************************************************************************/

/* static */
UIHostNetworkManager *UIHostNetworkManager::s_pInstance = 0;
UIHostNetworkManager *UIHostNetworkManager::instance() { return s_pInstance; }

UIHostNetworkManager::UIHostNetworkManager(QWidget *pCenterWidget)
    : m_pPseudoParentWidget(pCenterWidget)
    , m_pToolBar(0)
    , m_pMenu(0)
    , m_pActionAdd(0)
    , m_pActionEdit(0)
    , m_pActionRemove(0)
    , m_pTreeWidget(0)
    , m_pButtonBox(0)
{
    /* Prepare instance: */
    s_pInstance = this;

    /* Prepare: */
    prepare();
}

UIHostNetworkManager::~UIHostNetworkManager()
{
    /* Cleanup: */
    cleanup();

    /* Cleanup instance: */
    s_pInstance = 0;
}

/* static */
void UIHostNetworkManager::showModeless(QWidget *pCenterWidget /* = 0 */)
{
    /* Create instance if not yet created: */
    if (!s_pInstance)
        new UIHostNetworkManager(pCenterWidget);

    /* Show instance: */
    s_pInstance->show();
    s_pInstance->setWindowState(s_pInstance->windowState() & ~Qt::WindowMinimized);
    s_pInstance->activateWindow();
}

void UIHostNetworkManager::retranslateUi()
{
    /* Translate window title: */
    setWindowTitle(tr("Host Network Manager"));

    /* Translate menu: */
    if (m_pMenu)
        m_pMenu->setTitle(QApplication::translate("UIActionPool", "&File"));

    /* Translate actions: */
    if (m_pActionAdd)
    {
        m_pActionAdd->setText(tr("&Create"));
        m_pActionAdd->setToolTip(tr("Create Host-only Network (%1)").arg(m_pActionAdd->shortcut().toString()));
        m_pActionAdd->setStatusTip(tr("Creates new host-only network."));
    }
    if (m_pActionEdit)
    {
        m_pActionEdit->setText(tr("&Modify..."));
        m_pActionEdit->setToolTip(tr("Modify Host-only Network (%1)").arg(m_pActionEdit->shortcut().toString()));
        m_pActionEdit->setStatusTip(tr("Modifies selected host-only network."));
    }
    if (m_pActionRemove)
    {
        m_pActionRemove->setText(tr("&Remove..."));
        m_pActionRemove->setToolTip(tr("Remove Host-only Network (%1)").arg(m_pActionRemove->shortcut().toString()));
        m_pActionRemove->setStatusTip(tr("Removes selected host-only network."));
    }

    /* Adjust tool-bar: */
#ifdef VBOX_WS_MAC
    // WORKAROUND:
    // There is a bug in Qt Cocoa which result in showing a "more arrow" when
    // the necessary size of the toolbar is increased. Also for some languages
    // the with doesn't match if the text increase. So manually adjust the size
    // after changing the text.
    if (m_pToolBar)
        m_pToolBar->updateLayout();
#endif
}

void UIHostNetworkManager::sltAddHostNetwork()
{
    /* Get host for further activities: */
    CHost comHost = vboxGlobal().host();

    /* Create interface: */
    CHostNetworkInterface comInterface;
    CProgress progress = comHost.CreateHostOnlyNetworkInterface(comInterface);

    /* Show error message if necessary: */
    if (!comHost.isOk() || progress.isNull())
        msgCenter().cannotCreateHostNetworkInterface(comHost, this);
    else
    {
        /* Show interface creation progress: */
        msgCenter().showModalProgressDialog(progress, tr("Networking"), ":/progress_network_interface_90px.png", this, 0);

        /* Show error message if necessary: */
        if (!progress.isOk() || progress.GetResultCode() != 0)
            msgCenter().cannotCreateHostNetworkInterface(progress, this);
        else
        {
            /* Get network name for further activities: */
            const QString strNetworkName = comInterface.GetNetworkName();

            /* Show error message if necessary: */
            if (!comInterface.isOk())
                msgCenter().cannotAcquireHostNetworkInterfaceParameter(comInterface, this);
            else
            {
                /* Get VBox for further activities: */
                CVirtualBox comVBox = vboxGlobal().virtualBox();

                /* Find corresponding DHCP server (create if necessary): */
                CDHCPServer comServer = comVBox.FindDHCPServerByNetworkName(strNetworkName);
                if (!comVBox.isOk() || comServer.isNull())
                    comServer = comVBox.CreateDHCPServer(strNetworkName);

                /* Show error message if necessary: */
                if (!comVBox.isOk() || comServer.isNull())
                    msgCenter().cannotCreateDHCPServer(comVBox, strNetworkName, this);
            }

            /* Add interface to the tree: */
            UIDataHostNetwork data;
            loadHostNetwork(comInterface, data);
            createItemForNetworkHost(data, true);

            /* Sort list by the 1st column: */
            m_pTreeWidget->sortByColumn(0, Qt::AscendingOrder);
        }
    }
}

void UIHostNetworkManager::sltEditHostNetwork()
{
    /* Get network item: */
    UIItemHostNetwork *pItem = static_cast<UIItemHostNetwork*>(m_pTreeWidget->currentItem());
    AssertMsgReturnVoid(pItem, ("Current item must not be null!\n"));

    /* Get item data: */
    UIDataHostNetwork oldData = *pItem;
    UIDataHostNetwork newData = oldData;

    /* Show details dialog: */
    UIHostNetworkDetailsDialog details(this, newData);
    if (details.exec() != QDialog::Accepted)
        return;

    /* Get host for further activities: */
    CHost comHost = vboxGlobal().host();

    /* Find corresponding interface: */
    CHostNetworkInterface comInterface = comHost.FindHostNetworkInterfaceByName(oldData.m_interface.m_strName);

    /* Show error message if necessary: */
    if (!comHost.isOk() || comInterface.isNull())
        msgCenter().cannotFindHostNetworkInterface(comHost, oldData.m_interface.m_strName, this);
    else
    {
        /* Save IPv4 interface configuration: */
        if (   comInterface.isOk()
            && (   newData.m_interface.m_strInterfaceAddress != oldData.m_interface.m_strInterfaceAddress
                || newData.m_interface.m_strInterfaceMask != oldData.m_interface.m_strInterfaceMask))
            comInterface.EnableStaticIPConfig(newData.m_interface.m_strInterfaceAddress, newData.m_interface.m_strInterfaceMask);
        /* Save IPv6 interface configuration: */
        if (   comInterface.isOk()
            && newData.m_interface.m_fIpv6Supported
            && (   newData.m_interface.m_strInterfaceAddress6 != oldData.m_interface.m_strInterfaceAddress6
                || newData.m_interface.m_strInterfaceMaskLength6 != oldData.m_interface.m_strInterfaceMaskLength6))
            comInterface.EnableStaticIPConfigV6(newData.m_interface.m_strInterfaceAddress6, newData.m_interface.m_strInterfaceMaskLength6.toULong());

        /* Show error message if necessary: */
        if (!comInterface.isOk())
            msgCenter().cannotSaveHostNetworkInterfaceParameter(comInterface, this);
        else
        {
            /* Get network name for further activities: */
            const QString strNetworkName = comInterface.GetNetworkName();

            /* Show error message if necessary: */
            if (!comInterface.isOk())
                msgCenter().cannotAcquireHostNetworkInterfaceParameter(comInterface, this);
            else
            {
                /* Get VBox for further activities: */
                CVirtualBox comVBox = vboxGlobal().virtualBox();

                /* Find corresponding DHCP server (create if necessary): */
                CDHCPServer comServer = comVBox.FindDHCPServerByNetworkName(strNetworkName);
                if (!comVBox.isOk() || comServer.isNull())
                    comServer = comVBox.CreateDHCPServer(strNetworkName);

                /* Show error message if necessary: */
                if (!comVBox.isOk() || comServer.isNull())
                    msgCenter().cannotCreateDHCPServer(comVBox, strNetworkName, this);
                else
                {
                    /* Save whether DHCP server is enabled: */
                    if (   comServer.isOk()
                        && newData.m_dhcpserver.m_fDhcpServerEnabled != oldData.m_dhcpserver.m_fDhcpServerEnabled)
                        comServer.SetEnabled(newData.m_dhcpserver.m_fDhcpServerEnabled);
                    /* Save DHCP server configuration: */
                    if (   comServer.isOk()
                        && newData.m_dhcpserver.m_fDhcpServerEnabled
                        && (   newData.m_dhcpserver.m_strDhcpServerAddress != oldData.m_dhcpserver.m_strDhcpServerAddress
                            || newData.m_dhcpserver.m_strDhcpServerMask != oldData.m_dhcpserver.m_strDhcpServerMask
                            || newData.m_dhcpserver.m_strDhcpLowerAddress != oldData.m_dhcpserver.m_strDhcpLowerAddress
                            || newData.m_dhcpserver.m_strDhcpUpperAddress != oldData.m_dhcpserver.m_strDhcpUpperAddress))
                        comServer.SetConfiguration(newData.m_dhcpserver.m_strDhcpServerAddress, newData.m_dhcpserver.m_strDhcpServerMask,
                                                   newData.m_dhcpserver.m_strDhcpLowerAddress, newData.m_dhcpserver.m_strDhcpUpperAddress);

                    /* Show error message if necessary: */
                    if (!comServer.isOk())
                        msgCenter().cannotSaveDHCPServerParameter(comServer, this);
                }
            }
        }

        /* Update interface in the tree: */
        UIDataHostNetwork data;
        loadHostNetwork(comInterface, data);
        updateItemForNetworkHost(data, true, pItem);
    }
}

void UIHostNetworkManager::sltRemoveHostNetwork()
{
    /* Get network item: */
    UIItemHostNetwork *pItem = static_cast<UIItemHostNetwork*>(m_pTreeWidget->currentItem());
    AssertMsgReturnVoid(pItem, ("Current item must not be null!\n"));

    /* Get interface name: */
    const QString strInterfaceName(pItem->name());

    /* Confirm host network removal: */
    if (!msgCenter().confirmHostOnlyInterfaceRemoval(strInterfaceName, this))
        return;

    /* Get host for further activities: */
    CHost comHost = vboxGlobal().host();

    /* Find corresponding interface: */
    const CHostNetworkInterface &comInterface = comHost.FindHostNetworkInterfaceByName(strInterfaceName);

    /* Show error message if necessary: */
    if (!comHost.isOk() || comInterface.isNull())
        msgCenter().cannotFindHostNetworkInterface(comHost, strInterfaceName, this);
    else
    {
        /* Get network name for further activities: */
        QString strNetworkName;
        if (comInterface.isOk())
            strNetworkName = comInterface.GetNetworkName();
        /* Get interface id for further activities: */
        QString strInterfaceId;
        if (comInterface.isOk())
            strInterfaceId = comInterface.GetId();

        /* Show error message if necessary: */
        if (!comInterface.isOk())
            msgCenter().cannotAcquireHostNetworkInterfaceParameter(comInterface, this);
        else
        {
            /* Get VBox for further activities: */
            CVirtualBox comVBox = vboxGlobal().virtualBox();

            /* Find corresponding DHCP server: */
            const CDHCPServer &comServer = comVBox.FindDHCPServerByNetworkName(strNetworkName);
            if (comVBox.isOk() && comServer.isNotNull())
            {
                /* Remove server if any: */
                comVBox.RemoveDHCPServer(comServer);

                /* Show error message if necessary: */
                if (!comVBox.isOk())
                    msgCenter().cannotRemoveDHCPServer(comVBox, strInterfaceName, this);
            }

            /* Remove interface finally: */
            CProgress progress = comHost.RemoveHostOnlyNetworkInterface(strInterfaceId);

            /* Show error message if necessary: */
            if (!comHost.isOk() || progress.isNull())
                msgCenter().cannotRemoveHostNetworkInterface(comHost, strInterfaceName, this);
            else
            {
                /* Show interface removal progress: */
                msgCenter().showModalProgressDialog(progress, tr("Networking"), ":/progress_network_interface_90px.png", this, 0);

                /* Show error message if necessary: */
                if (!progress.isOk() || progress.GetResultCode() != 0)
                    return msgCenter().cannotRemoveHostNetworkInterface(progress, strInterfaceName, this);
                else
                {
                    /* Remove interface from the tree: */
                    delete pItem;
                }
            }
        }
    }
}

void UIHostNetworkManager::sltHandleCurrentItemChange()
{
    /* Get network item: */
    UIItemHostNetwork *pItem = static_cast<UIItemHostNetwork*>(m_pTreeWidget->currentItem());

    /* Update actions availability: */
    m_pActionRemove->setEnabled(pItem);
    m_pActionEdit->setEnabled(pItem);
}

void UIHostNetworkManager::sltHandleContextMenuRequest(const QPoint &pos)
{
    /* Compose temporary context-menu: */
    QMenu menu;
    if (m_pTreeWidget->itemAt(pos))
    {
        menu.addAction(m_pActionEdit);
        menu.addAction(m_pActionRemove);
    }
    else
    {
        menu.addAction(m_pActionAdd);
    }
    /* And show it: */
    menu.exec(m_pTreeWidget->mapToGlobal(pos));
}

void UIHostNetworkManager::prepare()
{
    /* Prepare this: */
    prepareThis();

    /* Apply language settings: */
    retranslateUi();

    /* Center according pseudo-parent widget: */
    VBoxGlobal::centerWidget(this, m_pPseudoParentWidget, false);

    /* Load host networks: */
    loadHostNetworks();
}

void UIHostNetworkManager::prepareThis()
{
    /* Initial size: */
    resize(620, 460);

    /* Dialog should delete itself on 'close': */
    setAttribute(Qt::WA_DeleteOnClose);
    /* And no need to count it as important for application.
     * This way it will NOT be taken into account
     * when other top-level windows will be closed: */
    setAttribute(Qt::WA_QuitOnClose, false);

    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/host_iface_manager_32px.png", ":/host_iface_manager_16px.png"));

    /* Prepare actions: */
    prepareActions();
    /* Prepare central-widget: */
    prepareCentralWidget();
}

void UIHostNetworkManager::prepareActions()
{
    /* Create 'Add' action: */
    m_pActionAdd = new QAction(this);
    AssertPtrReturnVoid(m_pActionAdd);
    {
        /* Configure 'Add' action: */
        m_pActionAdd->setShortcut(QKeySequence("Ctrl+A"));
        m_pActionAdd->setIcon(UIIconPool::iconSetFull(":/add_host_iface_22px.png",
                                                      ":/add_host_iface_16px.png",
                                                      ":/add_host_iface_disabled_22px.png",
                                                      ":/add_host_iface_disabled_16px.png"));
        connect(m_pActionAdd, SIGNAL(triggered()), this, SLOT(sltAddHostNetwork()));
    }

    /* Create 'Edit' action: */
    m_pActionEdit = new QAction(this);
    AssertPtrReturnVoid(m_pActionEdit);
    {
        /* Configure 'Edit' action: */
        m_pActionEdit->setShortcut(QKeySequence("Ctrl+Space"));
        m_pActionEdit->setIcon(UIIconPool::iconSetFull(":/edit_host_iface_22px.png",
                                                       ":/edit_host_iface_16px.png",
                                                       ":/edit_host_iface_disabled_22px.png",
                                                       ":/edit_host_iface_disabled_16px.png"));
        connect(m_pActionEdit, SIGNAL(triggered()), this, SLOT(sltEditHostNetwork()));
    }

    /* Create 'Remove' action: */
    m_pActionRemove = new QAction(this);
    AssertPtrReturnVoid(m_pActionRemove);
    {
        /* Configure 'Remove' action: */
        m_pActionRemove->setShortcut(QKeySequence("Del"));
        m_pActionRemove->setIcon(UIIconPool::iconSetFull(":/remove_host_iface_22px.png",
                                                         ":/remove_host_iface_16px.png",
                                                         ":/remove_host_iface_disabled_22px.png",
                                                         ":/remove_host_iface_disabled_16px.png"));
        connect(m_pActionRemove, SIGNAL(triggered()), this, SLOT(sltRemoveHostNetwork()));
    }

    /* Prepare menu-bar: */
    prepareMenuBar();
}

void UIHostNetworkManager::prepareMenuBar()
{
    /* Create 'File' menu: */
    m_pMenu = menuBar()->addMenu(QString());
    AssertPtrReturnVoid(m_pMenu);
    {
        /* Configure 'File' menu: */
        m_pMenu->addAction(m_pActionAdd);
        m_pMenu->addAction(m_pActionEdit);
        m_pMenu->addAction(m_pActionRemove);
    }

#ifdef VBOX_WS_MAC
    /* Prepare 'Window' menu: */
    AssertPtrReturnVoid(gpWindowMenuManager);
    menuBar()->addMenu(gpWindowMenuManager->createMenu(this));
    gpWindowMenuManager->addWindow(this);
#endif
}

void UIHostNetworkManager::prepareCentralWidget()
{
    /* Create central-widget: */
    setCentralWidget(new QWidget);
    AssertPtrReturnVoid(centralWidget());
    {
        /* Create main-layout: */
        new QVBoxLayout(centralWidget());
        AssertPtrReturnVoid(centralWidget()->layout());
        {
            /* Prepare tool-bar: */
            prepareToolBar();
            /* Prepare tree-widget: */
            prepareTreeWidget();
            /* Prepare button-box: */
            prepareButtonBox();
        }
    }
}

void UIHostNetworkManager::prepareToolBar()
{
    /* Create tool-bar: */
    m_pToolBar = new UIToolBar;
    AssertPtrReturnVoid(m_pToolBar);
    {
        /* Configure tool-bar: */
        const QStyle *pStyle = QApplication::style();
        const int iIconMetric = (int)(pStyle->pixelMetric(QStyle::PM_SmallIconSize) * 1.375);
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        m_pToolBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        /* Add tool-bar actions: */
        if (m_pActionAdd)
            m_pToolBar->addAction(m_pActionAdd);
        if (m_pActionEdit)
            m_pToolBar->addAction(m_pActionEdit);
        if (m_pActionRemove)
            m_pToolBar->addAction(m_pActionRemove);
        /* Integrate tool-bar into dialog: */
        QVBoxLayout *pMainLayout = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
#ifdef VBOX_WS_MAC
        /* Enable unified tool-bars on macOS: */
        addToolBar(m_pToolBar);
        m_pToolBar->enableMacToolbar();
        /* Special spacing on macOS: */
        pMainLayout->setSpacing(10);
#else
        /* Add tool-bar into layout: */
        pMainLayout->insertWidget(0, m_pToolBar);
        /* Set spacing/margin like in the selector window: */
        pMainLayout->setSpacing(5);
        pMainLayout->setContentsMargins(5, 5, 5, 5);
#endif
    }
}

void UIHostNetworkManager::prepareTreeWidget()
{
    /* Create tree-widget: */
    m_pTreeWidget = new QITreeWidget;
    AssertPtrReturnVoid(m_pTreeWidget);
    {
        /* Configure tree-widget: */
        m_pTreeWidget->header()->hide();
        m_pTreeWidget->setRootIsDecorated(false);
        m_pTreeWidget->setAlternatingRowColors(true);
        m_pTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        m_pTreeWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_pTreeWidget->sortItems(0, Qt::AscendingOrder);
        connect(m_pTreeWidget, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
                this, SLOT(sltHandleCurrentItemChange()));
        connect(m_pTreeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
                this, SLOT(sltEditHostNetwork()));
        connect(m_pTreeWidget, SIGNAL(customContextMenuRequested(const QPoint &)),
                this, SLOT(sltHandleContextMenuRequest(const QPoint &)));
        /* Add tree-widget into layout: */
        QVBoxLayout *pMainLayout = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
        pMainLayout->addWidget(m_pTreeWidget);
    }
}

void UIHostNetworkManager::prepareButtonBox()
{
    /* Create button-box: */
    m_pButtonBox = new QIDialogButtonBox;
    AssertPtrReturnVoid(m_pButtonBox);
    {
        /* Configure button-box: */
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Help | QDialogButtonBox::Close);
        m_pButtonBox->button(QDialogButtonBox::Close)->setShortcut(Qt::Key_Escape);
        connect(m_pButtonBox, SIGNAL(helpRequested()), &msgCenter(), SLOT(sltShowHelpHelpDialog()));
        connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(close()));
        /* Add button-box into layout: */
        QVBoxLayout *pMainLayout = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
        pMainLayout->addWidget(m_pButtonBox);
    }
}

void UIHostNetworkManager::cleanupMenuBar()
{
#ifdef VBOX_WS_MAC
    /* Cleanup 'Window' menu: */
    AssertPtrReturnVoid(gpWindowMenuManager);
    gpWindowMenuManager->removeWindow(this);
    gpWindowMenuManager->destroyMenu(this);
#endif
}

void UIHostNetworkManager::cleanup()
{
    /* Cleanup menu-bar: */
    cleanupMenuBar();
}

void UIHostNetworkManager::loadHostNetworks()
{
    /* Clear tree first of all: */
    m_pTreeWidget->clear();

    /* Get host for further activities: */
    const CHost comHost = vboxGlobal().host();

    /* Get interfaces for further activities: */
    const QVector<CHostNetworkInterface> interfaces = comHost.GetNetworkInterfaces();

    /* Show error message if necessary: */
    if (!comHost.isOk())
        msgCenter().cannotAcquireHostNetworkInterfaces(comHost, this);
    else
    {
        /* For each host-only interface => load it to the tree: */
        foreach (const CHostNetworkInterface &comInterface, interfaces)
            if (comInterface.GetInterfaceType() == KHostNetworkInterfaceType_HostOnly)
            {
                UIDataHostNetwork data;
                loadHostNetwork(comInterface, data);
                createItemForNetworkHost(data, false);
            }

        /* Sort list by the 1st column: */
        m_pTreeWidget->sortByColumn(0, Qt::AscendingOrder);
        /* Choose the 1st item as current initially: */
        m_pTreeWidget->setCurrentItem(m_pTreeWidget->topLevelItem(0));
        sltHandleCurrentItemChange();
    }
}

void UIHostNetworkManager::loadHostNetwork(const CHostNetworkInterface &comInterface, UIDataHostNetwork &data)
{
    /* Gather interface settings: */
    if (comInterface.isOk())
        data.m_interface.m_strName = comInterface.GetName();
    if (comInterface.isOk())
        data.m_interface.m_strInterfaceAddress = comInterface.GetIPAddress();
    if (comInterface.isOk())
        data.m_interface.m_strInterfaceMask = comInterface.GetNetworkMask();
    if (comInterface.isOk())
        data.m_interface.m_fIpv6Supported = comInterface.GetIPV6Supported();
    if (comInterface.isOk())
        data.m_interface.m_strInterfaceAddress6 = comInterface.GetIPV6Address();
    if (comInterface.isOk())
        data.m_interface.m_strInterfaceMaskLength6 = QString::number(comInterface.GetIPV6NetworkMaskPrefixLength());

    /* Get host interface network name for further activities: */
    QString strNetworkName;
    if (comInterface.isOk())
        strNetworkName = comInterface.GetNetworkName();

    /* Show error message if necessary: */
    if (!comInterface.isOk())
        msgCenter().cannotAcquireHostNetworkInterfaceParameter(comInterface, this);

    /* Get VBox for further activities: */
    CVirtualBox comVBox = vboxGlobal().virtualBox();

    /* Find corresponding DHCP server (create if necessary): */
    CDHCPServer comServer = comVBox.FindDHCPServerByNetworkName(strNetworkName);
    if (!comVBox.isOk() || comServer.isNull())
        comServer = comVBox.CreateDHCPServer(strNetworkName);

    /* Show error message if necessary: */
    if (!comVBox.isOk() || comServer.isNull())
        msgCenter().cannotCreateDHCPServer(comVBox, strNetworkName, this);
    else
    {
        /* Gather DHCP server settings: */
        if (comServer.isOk())
            data.m_dhcpserver.m_fDhcpServerEnabled = comServer.GetEnabled();
        if (comServer.isOk())
            data.m_dhcpserver.m_strDhcpServerAddress = comServer.GetIPAddress();
        if (comServer.isOk())
            data.m_dhcpserver.m_strDhcpServerMask = comServer.GetNetworkMask();
        if (comServer.isOk())
            data.m_dhcpserver.m_strDhcpLowerAddress = comServer.GetLowerIP();
        if (comServer.isOk())
            data.m_dhcpserver.m_strDhcpUpperAddress = comServer.GetUpperIP();

        /* Show error message if necessary: */
        if (!comServer.isOk())
            return msgCenter().cannotAcquireDHCPServerParameter(comServer, this);
    }
}

void UIHostNetworkManager::createItemForNetworkHost(const UIDataHostNetwork &data, bool fChooseItem)
{
    /* Create new item: */
    UIItemHostNetwork *pItem = new UIItemHostNetwork;
    AssertPtrReturnVoid(pItem);
    {
        /* Configure item: */
        pItem->UIDataHostNetwork::operator=(data);
        pItem->updateFields();
        /* Add item to the tree: */
        m_pTreeWidget->addTopLevelItem(pItem);
        /* And choose it as current if necessary: */
        if (fChooseItem)
            m_pTreeWidget->setCurrentItem(pItem);
    }
}

void UIHostNetworkManager::updateItemForNetworkHost(const UIDataHostNetwork &data, bool fChooseItem, UIItemHostNetwork *pItem)
{
    /* Update passed item: */
    AssertPtrReturnVoid(pItem);
    {
        /* Configure item: */
        pItem->UIDataHostNetwork::operator=(data);
        pItem->updateFields();
        /* And choose it as current if necessary: */
        if (fChooseItem)
            m_pTreeWidget->setCurrentItem(pItem);
    }
}

