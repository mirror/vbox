/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudProfileManager class implementation.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
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
# include <QPushButton>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIDialogButtonBox.h"
# include "QITreeWidget.h"
# include "VBoxGlobal.h"
# include "UIActionPoolSelector.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
# include "UICloudProfileDetailsWidget.h"
# include "UICloudProfileManager.h"
# include "UIToolBar.h"
# ifdef VBOX_WS_MAC
#  include "UIWindowMenuManager.h"
# endif

/* COM includes: */
#include "CCloudProfile.h"
#include "CCloudProvider.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Tree-widget item types. */
enum CloudItemType
{
    CloudItemType_Provider = QTreeWidgetItem::UserType + 1,
    CloudItemType_Profile  = QTreeWidgetItem::UserType + 2
};
Q_DECLARE_METATYPE(CloudItemType);

/** Tree-widget data types. */
enum
{
    Data_ItemType   = Qt::UserRole + 1,
    Data_ProviderID = Qt::UserRole + 2,
};

/** Tree-widget column types. */
enum
{
    Column_Name,
    Column_Max
};


/** Cloud Profile Manager provider's tree-widget item. */
class UIItemCloudProvider : public QITreeWidgetItem, public UIDataCloudProvider
{
public:

    /** Constructs item. */
    UIItemCloudProvider();

    /** Updates item fields from base-class data. */
    void updateFields();

    /** Returns item name. */
    QString name() const { return m_strName; }
};

/** Cloud Profile Manager profile's tree-widget item. */
class UIItemCloudProfile : public QITreeWidgetItem, public UIDataCloudProfile
{
public:

    /** Constructs item. */
    UIItemCloudProfile();

    /** Updates item fields from base-class data. */
    void updateFields();

    /** Returns item name. */
    QString name() const { return m_strName; }
};


/*********************************************************************************************************************************
*   Class UIItemCloudProvider implementation.                                                                                    *
*********************************************************************************************************************************/

UIItemCloudProvider::UIItemCloudProvider()
{
    /* Assign item type: */
    setData(Column_Name, Data_ItemType, CloudItemType_Provider);
}

void UIItemCloudProvider::updateFields()
{
    /* Update item fields: */
    setText(Column_Name, m_strName);
    setData(Column_Name, Data_ProviderID, m_uuid);
}


/*********************************************************************************************************************************
*   Class UIItemCloudProfile implementation.                                                                                     *
*********************************************************************************************************************************/

UIItemCloudProfile::UIItemCloudProfile()
{
    /* Assign item type: */
    setData(Column_Name, Data_ItemType, CloudItemType_Profile);
}

void UIItemCloudProfile::updateFields()
{
    /* Update item fields: */
    setText(Column_Name, m_strName);
}


/*********************************************************************************************************************************
*   Class UICloudProfileManagerWidget implementation.                                                                            *
*********************************************************************************************************************************/

UICloudProfileManagerWidget::UICloudProfileManagerWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                                         bool fShowToolbar /* = true */, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_fShowToolbar(fShowToolbar)
    , m_pToolBar(0)
    , m_pTreeWidget(0)
    , m_pDetailsWidget(0)
{
    /* Prepare: */
    prepare();
}

QMenu *UICloudProfileManagerWidget::menu() const
{
    return m_pActionPool->action(UIActionIndexST_M_CloudWindow)->menu();
}

void UICloudProfileManagerWidget::retranslateUi()
{
    /* Adjust toolbar: */
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

void UICloudProfileManagerWidget::sltResetCloudProfileDetailsChanges()
{
    /* Just push the current item data there again: */
    sltHandleCurrentItemChange();
}

void UICloudProfileManagerWidget::sltApplyCloudProfileDetailsChanges()
{
    /* Get profile item: */
    UIItemCloudProfile *pItem = static_cast<UIItemCloudProfile*>(m_pTreeWidget->currentItem());
    AssertMsgReturnVoid(pItem, ("Current item must not be null!\n"));

    /// @todo apply cloud profile details!
}

void UICloudProfileManagerWidget::sltCreateCloudProfile()
{
    /// @todo create cloud profile!
}

void UICloudProfileManagerWidget::sltRemoveCloudProfile()
{
    /// @todo remove cloud profile!
}

void UICloudProfileManagerWidget::sltToggleCloudProfileDetailsVisibility(bool fVisible)
{
    /* Save the setting: */
    gEDataManager->setCloudProfileManagerDetailsExpanded(fVisible);
    /* Show/hide details area and Apply button: */
    m_pDetailsWidget->setVisible(fVisible);
    /* Notify external lsiteners: */
    emit sigCloudProfileDetailsVisibilityChanged(fVisible);
}

void UICloudProfileManagerWidget::sltRefreshCloudProfiles()
{
    /// @todo refresh cloud profiles!

    // Not implemented.
    AssertMsgFailed(("Not implemented!"));
}

void UICloudProfileManagerWidget::sltHandleItemChange(QTreeWidgetItem *pItem)
{
    /* Get profile item: */
    UIItemCloudProfile *pChangedItem = static_cast<UIItemCloudProfile*>(pItem);
    AssertMsgReturnVoid(pChangedItem, ("Changed item must not be null!\n"));

    /// @todo handle item change!
}

void UICloudProfileManagerWidget::sltHandleCurrentItemChange()
{
    /* Get items: */
    QTreeWidgetItem *pItem = m_pTreeWidget->currentItem();
    UIItemCloudProvider *pItemProvider = pItem && pItem->data(0, Data_ItemType).value<CloudItemType>() == CloudItemType_Provider
                                       ? static_cast<UIItemCloudProvider*>(pItem)
                                       : 0;
    UIItemCloudProfile  *pItemProfile  = pItem && pItem->data(0, Data_ItemType).value<CloudItemType>() == CloudItemType_Profile
                                       ? static_cast<UIItemCloudProfile*>(pItem)
                                       : 0;

    /* Update actions availability: */
    m_pActionPool->action(UIActionIndexST_M_Cloud_S_Create)->setEnabled(!pItem || pItemProvider);
    m_pActionPool->action(UIActionIndexST_M_Cloud_S_Remove)->setEnabled(pItemProfile);
    m_pActionPool->action(UIActionIndexST_M_Cloud_T_Details)->setEnabled(pItemProfile);

    /* If there is an item => update details data: */
    if (pItemProfile)
        m_pDetailsWidget->setData(*pItemProfile);
    /* Otherwise => clear details data: */
    else
        m_pDetailsWidget->setData(UIDataCloudProfile());

    /* Update details area visibility: */
    sltToggleCloudProfileDetailsVisibility(pItem && m_pActionPool->action(UIActionIndexST_M_Cloud_T_Details)->isChecked());
}

void UICloudProfileManagerWidget::sltHandleContextMenuRequest(const QPoint &position)
{
    /* Get items: */
    QTreeWidgetItem *pItem = m_pTreeWidget->itemAt(position);
    UIItemCloudProvider *pItemProvider = pItem && pItem->data(0, Data_ItemType).value<CloudItemType>() == CloudItemType_Provider
                                       ? static_cast<UIItemCloudProvider*>(pItem)
                                       : 0;
    UIItemCloudProfile  *pItemProfile  = pItem && pItem->data(0, Data_ItemType).value<CloudItemType>() == CloudItemType_Profile
                                       ? static_cast<UIItemCloudProfile*>(pItem)
                                       : 0;

    /* Compose temporary context-menu: */
    QMenu menu;
    if (pItemProfile)
    {
        menu.addAction(m_pActionPool->action(UIActionIndexST_M_Cloud_S_Remove));
        menu.addAction(m_pActionPool->action(UIActionIndexST_M_Cloud_T_Details));
    }
    else if (pItemProvider)
    {
        menu.addAction(m_pActionPool->action(UIActionIndexST_M_Cloud_S_Create));
//        menu.addAction(m_pActionPool->action(UIActionIndexST_M_Cloud_S_Refresh));
    }

    /* And show it: */
    menu.exec(m_pTreeWidget->mapToGlobal(position));
}

void UICloudProfileManagerWidget::prepare()
{
    /* Prepare actions: */
    prepareActions();
    /* Prepare widgets: */
    prepareWidgets();

    /* Load settings: */
    loadSettings();

    /* Apply language settings: */
    retranslateUi();

    /* Load cloud stuff: */
    loadCloudStuff();
}

void UICloudProfileManagerWidget::prepareActions()
{
    /* Connect actions: */
    connect(m_pActionPool->action(UIActionIndexST_M_Cloud_S_Create), &QAction::triggered,
            this, &UICloudProfileManagerWidget::sltCreateCloudProfile);
    connect(m_pActionPool->action(UIActionIndexST_M_Cloud_S_Remove), &QAction::triggered,
            this, &UICloudProfileManagerWidget::sltRemoveCloudProfile);
    connect(m_pActionPool->action(UIActionIndexST_M_Cloud_T_Details), &QAction::toggled,
            this, &UICloudProfileManagerWidget::sltToggleCloudProfileDetailsVisibility);
    connect(m_pActionPool->action(UIActionIndexST_M_Cloud_S_Refresh), &QAction::triggered,
            this, &UICloudProfileManagerWidget::sltRefreshCloudProfiles);
}

void UICloudProfileManagerWidget::prepareWidgets()
{
    /* Create main-layout: */
    new QVBoxLayout(this);
    if (layout())
    {
        /* Configure layout: */
        layout()->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
        layout()->setSpacing(10);
#else
        layout()->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2);
#endif

        /* Prepare toolbar, if requested: */
        if (m_fShowToolbar)
            prepareToolBar();
        /* Prepare tree-widget: */
        prepareTreeWidget();
        /* Prepare details-widget: */
        prepareDetailsWidget();
    }
}

void UICloudProfileManagerWidget::prepareToolBar()
{
    /* Create toolbar: */
    m_pToolBar = new UIToolBar(parentWidget());
    if (m_pToolBar)
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) * 1.375);
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        /* Add toolbar actions: */
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndexST_M_Cloud_S_Create));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndexST_M_Cloud_S_Remove));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndexST_M_Cloud_T_Details));
//        m_pToolBar->addSeparator();
//        m_pToolBar->addAction(m_pActionPool->action(UIActionIndexST_M_Cloud_S_Refresh));

#ifdef VBOX_WS_MAC
        /* Check whether we are embedded into a stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Add into layout: */
            layout()->addWidget(m_pToolBar);
        }
#else
        /* Add into layout: */
        layout()->addWidget(m_pToolBar);
#endif
    }
}

void UICloudProfileManagerWidget::prepareTreeWidget()
{
    /* Create tree-widget: */
    m_pTreeWidget = new QITreeWidget;
    if (m_pTreeWidget)
    {
        /* Configure tree-widget: */
        m_pTreeWidget->header()->hide();
        m_pTreeWidget->setRootIsDecorated(false);
        m_pTreeWidget->setAlternatingRowColors(true);
        m_pTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        m_pTreeWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_pTreeWidget->setColumnCount(Column_Max);
        m_pTreeWidget->setSortingEnabled(true);
        m_pTreeWidget->sortByColumn(Column_Name, Qt::AscendingOrder);
        m_pTreeWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        connect(m_pTreeWidget, &QITreeWidget::currentItemChanged,
                this, &UICloudProfileManagerWidget::sltHandleCurrentItemChange);
        connect(m_pTreeWidget, &QITreeWidget::customContextMenuRequested,
                this, &UICloudProfileManagerWidget::sltHandleContextMenuRequest);
        connect(m_pTreeWidget, &QITreeWidget::itemChanged,
                this, &UICloudProfileManagerWidget::sltHandleItemChange);
        connect(m_pTreeWidget, &QITreeWidget::itemDoubleClicked,
                m_pActionPool->action(UIActionIndexST_M_Cloud_T_Details), &QAction::setChecked);

        /* Add into layout: */
        layout()->addWidget(m_pTreeWidget);
    }
}

void UICloudProfileManagerWidget::prepareDetailsWidget()
{
    /* Create details-widget: */
    m_pDetailsWidget = new UICloudProfileDetailsWidget(m_enmEmbedding);
    if (m_pDetailsWidget)
    {
        /* Configure details-widget: */
        m_pDetailsWidget->setVisible(false);
        m_pDetailsWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        connect(m_pDetailsWidget, &UICloudProfileDetailsWidget::sigDataChanged,
                this, &UICloudProfileManagerWidget::sigCloudProfileDetailsDataChanged);
        connect(m_pDetailsWidget, &UICloudProfileDetailsWidget::sigDataChangeRejected,
                this, &UICloudProfileManagerWidget::sltResetCloudProfileDetailsChanges);
        connect(m_pDetailsWidget, &UICloudProfileDetailsWidget::sigDataChangeAccepted,
                this, &UICloudProfileManagerWidget::sltApplyCloudProfileDetailsChanges);

        /* Add into layout: */
        layout()->addWidget(m_pDetailsWidget);
    }
}

void UICloudProfileManagerWidget::loadSettings()
{
    /* Details action/widget: */
    m_pActionPool->action(UIActionIndexST_M_Cloud_T_Details)->setChecked(gEDataManager->cloudProfileManagerDetailsExpanded());
}

void UICloudProfileManagerWidget::loadCloudStuff()
{
    /* Clear tree first of all: */
    m_pTreeWidget->clear();

    /// @todo load cloud profiles!
}

void UICloudProfileManagerWidget::loadCloudProvider(const CCloudProvider &comProvider, UIDataCloudProvider &data)
{
    Q_UNUSED(comProvider);
    Q_UNUSED(data);

    /// @todo load cloud profile!
}

void UICloudProfileManagerWidget::loadCloudProfile(const CCloudProfile &comProfile, UIDataCloudProfile &data)
{
    Q_UNUSED(comProfile);
    Q_UNUSED(data);

    /// @todo load cloud profile!
}

void UICloudProfileManagerWidget::createItemForCloudProvider(const UIDataCloudProvider &data, bool fChooseItem)
{
    /* Create new provider item: */
    UIItemCloudProvider *pItem = new UIItemCloudProvider;
    if (pItem)
    {
        /* Configure item: */
        pItem->UIDataCloudProvider::operator=(data);
        pItem->updateFields();
        /* Add item to the tree: */
        m_pTreeWidget->addTopLevelItem(pItem);
        /* And choose it as current if necessary: */
        if (fChooseItem)
            m_pTreeWidget->setCurrentItem(pItem);
    }
}

void UICloudProfileManagerWidget::createItemForCloudProfile(QTreeWidgetItem *pParent, const UIDataCloudProfile &data, bool fChooseItem)
{
    /* Create new profile item: */
    UIItemCloudProfile *pItem = new UIItemCloudProfile;
    if (pItem)
    {
        /* Configure item: */
        pItem->setFlags(pItem->flags() | Qt::ItemIsEditable);
        pItem->UIDataCloudProfile::operator=(data);
        pItem->updateFields();
        /* Add item to the parent: */
        pParent->addChild(pItem);
        /* And choose it as current if necessary: */
        if (fChooseItem)
            m_pTreeWidget->setCurrentItem(pItem);
    }
}

void UICloudProfileManagerWidget::updateItemForCloudProfile(const UIDataCloudProfile &data, bool fChooseItem, UIItemCloudProfile *pItem)
{
    /* Update passed item: */
    if (pItem)
    {
        /* Configure item: */
        pItem->UIDataCloudProfile::operator=(data);
        pItem->updateFields();
        /* And choose it as current if necessary: */
        if (fChooseItem)
            m_pTreeWidget->setCurrentItem(pItem);
    }
}


/*********************************************************************************************************************************
*   Class UICloudProfileManagerFactory implementation.                                                                           *
*********************************************************************************************************************************/

UICloudProfileManagerFactory::UICloudProfileManagerFactory(UIActionPool *pActionPool /* = 0 */)
    : m_pActionPool(pActionPool)
{
}

void UICloudProfileManagerFactory::create(QIManagerDialog *&pDialog, QWidget *pCenterWidget)
{
    pDialog = new UICloudProfileManager(pCenterWidget, m_pActionPool);
}


/*********************************************************************************************************************************
*   Class UICloudProfileManager implementation.                                                                                  *
*********************************************************************************************************************************/

UICloudProfileManager::UICloudProfileManager(QWidget *pCenterWidget, UIActionPool *pActionPool)
    : QIWithRetranslateUI<QIManagerDialog>(pCenterWidget)
    , m_pActionPool(pActionPool)
{
}

void UICloudProfileManager::sltHandleButtonBoxClick(QAbstractButton *pButton)
{
    /* Disable buttons first of all: */
    button(ButtonType_Reset)->setEnabled(false);
    button(ButtonType_Apply)->setEnabled(false);

    /* Compare with known buttons: */
    if (pButton == button(ButtonType_Reset))
        emit sigDataChangeRejected();
    else
    if (pButton == button(ButtonType_Apply))
        emit sigDataChangeAccepted();
}

void UICloudProfileManager::retranslateUi()
{
    /* Translate window title: */
    setWindowTitle(tr("Cloud Profile Manager"));

    /* Translate buttons: */
    button(ButtonType_Reset)->setText(tr("Reset"));
    button(ButtonType_Apply)->setText(tr("Apply"));
    button(ButtonType_Close)->setText(tr("Close"));
    button(ButtonType_Reset)->setStatusTip(tr("Reset changes in current cloud profile details"));
    button(ButtonType_Apply)->setStatusTip(tr("Apply changes in current cloud profile details"));
    button(ButtonType_Close)->setStatusTip(tr("Close dialog without saving"));
    button(ButtonType_Reset)->setShortcut(QString("Ctrl+Backspace"));
    button(ButtonType_Apply)->setShortcut(QString("Ctrl+Return"));
    button(ButtonType_Close)->setShortcut(Qt::Key_Escape);
    button(ButtonType_Reset)->setToolTip(tr("Reset Changes (%1)").arg(button(ButtonType_Reset)->shortcut().toString()));
    button(ButtonType_Apply)->setToolTip(tr("Apply Changes (%1)").arg(button(ButtonType_Apply)->shortcut().toString()));
    button(ButtonType_Close)->setToolTip(tr("Close Window (%1)").arg(button(ButtonType_Close)->shortcut().toString()));
}

void UICloudProfileManager::configure()
{
    /* Apply window icons: */
    /// @todo apply proper cloud profile manager icons!
    setWindowIcon(UIIconPool::iconSetFull(":/host_iface_manager_32px.png", ":/host_iface_manager_16px.png"));
}

void UICloudProfileManager::configureCentralWidget()
{
    /* Create widget: */
    UICloudProfileManagerWidget *pWidget = new UICloudProfileManagerWidget(EmbedTo_Dialog, m_pActionPool, true, this);
    if (pWidget)
    {
        /* Configure widget: */
        setWidget(pWidget);
        setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        setWidgetToolbar(pWidget->toolbar());
#endif
        connect(this, &UICloudProfileManager::sigDataChangeRejected,
                pWidget, &UICloudProfileManagerWidget::sltResetCloudProfileDetailsChanges);
        connect(this, &UICloudProfileManager::sigDataChangeAccepted,
                pWidget, &UICloudProfileManagerWidget::sltApplyCloudProfileDetailsChanges);

        /* Add into layout: */
        centralWidget()->layout()->addWidget(pWidget);
    }
}

void UICloudProfileManager::configureButtonBox()
{
    /* Configure button-box: */
    connect(widget(), &UICloudProfileManagerWidget::sigCloudProfileDetailsVisibilityChanged,
            button(ButtonType_Apply), &QPushButton::setVisible);
    connect(widget(), &UICloudProfileManagerWidget::sigCloudProfileDetailsVisibilityChanged,
            button(ButtonType_Reset), &QPushButton::setVisible);
    connect(widget(), &UICloudProfileManagerWidget::sigCloudProfileDetailsDataChanged,
            button(ButtonType_Apply), &QPushButton::setEnabled);
    connect(widget(), &UICloudProfileManagerWidget::sigCloudProfileDetailsDataChanged,
            button(ButtonType_Reset), &QPushButton::setEnabled);
    connect(buttonBox(), &QIDialogButtonBox::clicked,
            this, &UICloudProfileManager::sltHandleButtonBoxClick);
    // WORKAROUND:
    // Since we connected signals later than extra-data loaded
    // for signals above, we should handle that stuff here again:
    button(ButtonType_Apply)->setVisible(gEDataManager->cloudProfileManagerDetailsExpanded());
    button(ButtonType_Reset)->setVisible(gEDataManager->cloudProfileManagerDetailsExpanded());
}

void UICloudProfileManager::finalize()
{
    /* Apply language settings: */
    retranslateUi();
}

UICloudProfileManagerWidget *UICloudProfileManager::widget()
{
    return qobject_cast<UICloudProfileManagerWidget*>(QIManagerDialog::widget());
}
