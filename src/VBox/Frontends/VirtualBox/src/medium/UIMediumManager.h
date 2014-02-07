/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMediumManager class declaration
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMediumManager_h__
#define __UIMediumManager_h__

/* GUI includes: */
#include "UIMediumManager.gen.h"
#include "QIWithRetranslateUI.h"
#include "QIMainDialog.h"
#include "UIMediumDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"
#include "CVirtualBox.h"

/* Forward declarations: */
class UIToolBar;
class UIMediumItem;
class UIEnumerationProgressBar;
class UIMedium;

/** Functor interface allowing to check if passed UIMediumItem is suitable. */
class CheckIfSuitableBy
{
public:
    /** Determines whether passed UIMediumItem is suitable. */
    virtual bool isItSuitable(UIMediumItem *pItem) const = 0;
};

/* Medium Manager Dialog: */
class UIMediumManager : public QIWithRetranslateUI2<QIMainDialog>, public Ui::UIMediumManager
{
    Q_OBJECT;

    /* Enumerators: */
    enum TabIndex { HDTab = 0, CDTab, FDTab };
    enum ItemAction { ItemAction_Added, ItemAction_Updated, ItemAction_Removed };
    enum Action { Action_Edit, Action_Copy, Action_Modify, Action_Remove, Action_Release };

    /* Constructor/destructor: */
    UIMediumManager(QWidget *pCenterWidget, bool fRefresh = true);
    ~UIMediumManager();

public:

    /* Static API: Singleton stuff: */
    static UIMediumManager* instance();
    static void showModeless(QWidget *pCenterWidget, bool fRefresh = true);

private slots:

    /** Fully refreshes medium manager contents. */
    void sltRefreshAll();

    /* Handlers: Medium-processing stuff: */
    void sltHandleMediumCreated(const QString &strMediumID);
    void sltHandleMediumDeleted(const QString &strMediumID);

    /* Handlers: Medium-enumeration stuff: */
    void sltHandleMediumEnumerationStart();
    void sltHandleMediumEnumerated(const QString &strMediumID);
    void sltHandleMediumEnumerationFinish();

    /* Handlers: Medium-modification stuff: */
    void sltCopyMedium();
    void sltModifyMedium();
    void sltRemoveMedium();
    void sltReleaseMedium();

    /* Handlers: Navigation stuff: */
    void sltHandleCurrentTabChanged();
    void sltHandleCurrentItemChanged();
    void sltHandleDoubleClick();
    void sltHandleContextMenuCall(const QPoint &position);

    /* Handlers: Geometry stuff:  */
    void sltMakeRequestForTableAdjustment();
    void sltPerformTablesAdjustment();

private:

    /* Helpers: Prepare stuff: */
    void prepare();
    void prepareThis();
    void prepareActions();
    void prepareMenuBar();
    void prepareToolBar();
    void prepareContextMenu();
    void prepareTabWidget();
    void prepareTreeWidgets();
    void prepareTreeWidgetHD();
    void prepareTreeWidgetCD();
    void prepareTreeWidgetFD();
    void prepareInformationPanes();
    void prepareButtonBox();
    void prepareProgressBar();
#ifdef Q_WS_MAC
    void prepareMacWindowMenu();
#endif /* Q_WS_MAC */

    /** Repopulates tree-widgets content. */
    void repopulateTreeWidgets();

    /** Updates details according latest changes in current medium-item of predefined @a type. */
    void refetchCurrentMediumItem(UIMediumType type);
    /** Updates details according latest changes in current medium-item of chosen type. */
    void refetchCurrentChosenMediumItem();
    /** Updates details according latest changes in all current medium-items. */
    void refetchCurrentMediumItems();

    /** Update actions according currently chosen medium-item. */
    void updateActions();
    /** Update tab icons according last @a action happened with @a pItem. */
    void updateTabIcons(UIMediumItem *pItem, ItemAction action);
    /** Update information pane of passed medium @a type. */
    void updateInformationPanes(UIMediumType type = UIMediumType_Invalid);
    /** Update information pane for hard-drive tab. */
    void updateInformationPanesHD();
    /** Update information pane for optical-disk tab. */
    void updateInformationPanesCD();
    /** Update information pane for floppy-disk tab. */
    void updateInformationPanesFD();

    /* Helpers: Cleanup stuff: */
#ifdef Q_WS_MAC
    void cleanupMacWindowMenu();
#endif /* Q_WS_MAC */
    void cleanup();

    /* Handler: Translation stuff: */
    void retranslateUi();

    /** Creates UIMediumItem for corresponding @a medium. */
    void createMediumItem(const UIMedium &medium);
    /** Updates UIMediumItem for corresponding @a medium. */
    void updateMediumItem(const UIMedium &medium);
    /** Deletes UIMediumItem for corresponding @a strMediumID. */
    void deleteMediumItem(const QString &strMediumID);

    /* Helpers: Medium-modification stuff: */
    bool releaseMediumFrom(const UIMedium &medium, const QString &strMachineId);
    bool releaseHardDiskFrom(const UIMedium &medium, CMachine &machine);
    bool releaseOpticalDiskFrom(const UIMedium &medium, CMachine &machine);
    bool releaseFloppyDiskFrom(const UIMedium &medium, CMachine &machine);

    /** Determines medium type for passed @a pTreeWidget. */
    UIMediumType mediumType(QTreeWidget *pTreeWidget) const;

    /** Returns current medium type. */
    UIMediumType currentMediumType() const;

    /** Returns tree-widget for passed medium @a type. */
    QTreeWidget* treeWidget(UIMediumType type) const;
    /** Returns current tree-widget. */
    QTreeWidget* currentTreeWidget() const;

    /** Returns medium-item for passed medium @a type. */
    UIMediumItem* mediumItem(UIMediumType type) const;
    /** Returns current medium-item. */
    UIMediumItem* currentMediumItem() const;

    /** Defines <i>current-item</i> for passed @a pTreeWidget as @a pItem. */
    void setCurrentItem(QTreeWidget *pTreeWidget, QTreeWidgetItem *pItem);

    UIMediumItem* searchItem(QTreeWidget *pTree, const CheckIfSuitableBy &functor) const;
    UIMediumItem* searchItem(QTreeWidgetItem *pParentItem, const CheckIfSuitableBy &functor) const;
    UIMediumItem* createHardDiskItem(QTreeWidget *pTree, const UIMedium &medium) const;

    /* Helper: Other stuff: */
    bool checkMediumFor(UIMediumItem *pItem, Action action);

    /** Casts passed QTreeWidgetItem @a pItem to UIMediumItem if possible. */
    static UIMediumItem* toMediumItem(QTreeWidgetItem *pItem);

    /* Static helper: Formatting stuff: */
    static QString formatPaneText(const QString &strText, bool fCompact = true, const QString &strElipsis = "middle");

    /* Static helper: Enumeration stuff: */
    static bool isMediumAttachedToHiddenMachinesOnly(const UIMedium &medium);

    /* Variable: Singleton instance: */
    static UIMediumManager *m_spInstance;

    /* Variables: General stuff: */
    CVirtualBox m_vbox;
    QWidget *m_pCenterWidget;
    bool m_fRefresh;

    /* Variables: Tab-widget stuff: */
    bool m_fInaccessibleHD;
    bool m_fInaccessibleCD;
    bool m_fInaccessibleFD;
    const QIcon m_iconHD;
    const QIcon m_iconCD;
    const QIcon m_iconFD;
    QString m_strSelectedIdHD;
    QString m_strSelectedIdCD;
    QString m_strSelectedIdFD;

    /* Variables: Menu & Toolbar stuff: */
    UIToolBar *m_pToolBar;
    QMenu     *m_pContextMenu;
    QMenu     *m_pMenu;
    QAction   *m_pActionCopy;
    QAction   *m_pActionModify;
    QAction   *m_pActionRemove;
    QAction   *m_pActionRelease;
    QAction   *m_pActionRefresh;

    /* Variable: Progress-bar stuff: */
    UIEnumerationProgressBar *m_pProgressBar;
};

#endif /* __UIMediumManager_h__ */
