/** @file
 * VBox Qt GUI - UIMediumManager class declaration.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMediumManager_h___
#define ___UIMediumManager_h___

/* GUI includes: */
#include "UIMediumManager.gen.h"
#include "QIWithRetranslateUI.h"
#include "QIMainDialog.h"
#include "UIMediumDefs.h"

/* Forward declarations: */
class UIMedium;
class UIMediumItem;
class UIToolBar;
class UIEnumerationProgressBar;

/** Functor interface allowing to check if passed UIMediumItem is suitable. */
class CheckIfSuitableBy
{
public:
    /** Determines whether passed @a pItem is suitable. */
    virtual bool isItSuitable(UIMediumItem *pItem) const = 0;
};

/** Medium Manager dialog. */
class UIMediumManager : public QIWithRetranslateUI2<QIMainDialog>, public Ui::UIMediumManager
{
    Q_OBJECT;

    /** Tab index enumerator. */
    enum TabIndex { TabIndex_HD = 0, TabIndex_CD, TabIndex_FD };

    /** Item action enumerator. */
    enum Action { Action_Add, Action_Edit, Action_Copy, Action_Modify, Action_Remove, Action_Release };

    /** Constructor for UIMediumManager dialog. */
    UIMediumManager(QWidget *pCenterWidget, bool fRefresh = true);
    /** Destructor for UIMediumManager dialog. */
    ~UIMediumManager();

public:

    /** Returns UIMediumManager singleton instance. */
    static UIMediumManager* instance();
    /** Shows UIMediumManager singleton instance, creates new if necessary. */
    static void showModeless(QWidget *pCenterWidget, bool fRefresh = true);

private slots:

    /** Handles VBoxGlobal::sigMediumCreated signal. */
    void sltHandleMediumCreated(const QString &strMediumID);
    /** Handles VBoxGlobal::sigMediumDeleted signal. */
    void sltHandleMediumDeleted(const QString &strMediumID);

    /** Handles VBoxGlobal::sigMediumEnumerationStarted signal. */
    void sltHandleMediumEnumerationStart();
    /** Handles VBoxGlobal::sigMediumEnumerated signal. */
    void sltHandleMediumEnumerated(const QString &strMediumID);
    /** Handles VBoxGlobal::sigMediumEnumerationFinished signal. */
    void sltHandleMediumEnumerationFinish();

    /** Handles UIMediumManager::m_pActionCopy action triggering. */
    void sltCopyMedium();
    /** Handles UIMediumManager::m_pActionModify action triggering. */
    void sltModifyMedium();
    /** Handles UIMediumManager::m_pActionRemove action triggering. */
    void sltRemoveMedium();
    /** Handles UIMediumManager::m_pActionRelease action triggering. */
    void sltReleaseMedium();
    /** Handles UIMediumManager::m_pActionRefresh action triggering. */
    void sltRefreshAll();

    /** Handles tab change case. */
    void sltHandleCurrentTabChanged();
    /** Handles item change case. */
    void sltHandleCurrentItemChanged();
    /** Handles item double-click case. */
    void sltHandleDoubleClick();
    /** Handles item context-menu-call case. */
    void sltHandleContextMenuCall(const QPoint &position);

    /** Adjusts tree-widgets according content. */
    void sltPerformTablesAdjustment();

private:

    /** General prepare wrapper. */
    void prepare();
    /** Prepare dialog. */
    void prepareThis();
    /** Prepare actions. */
    void prepareActions();
    /** Prepare menu-bar. */
    void prepareMenuBar();
    /** Prepare tool-bar. */
    void prepareToolBar();
    /** Prepare context-menu. */
    void prepareContextMenu();
    /** Prepare tab-widget. */
    void prepareTabWidget();
    /** Prepare tree-widgets. */
    void prepareTreeWidgets();
    /** Prepare hard-drive tree-widget. */
    void prepareTreeWidgetHD();
    /** Prepare optical-disk tree-widget. */
    void prepareTreeWidgetCD();
    /** Prepare floppy-disk tree-widget. */
    void prepareTreeWidgetFD();
    /** Prepare information-panes. */
    void prepareInformationPanes();
    /** Prepare button-box. */
    void prepareButtonBox();
    /** Prepare progress-bar. */
    void prepareProgressBar();
#ifdef Q_WS_MAC
    /** Mac OS X: Prepare <i>Window</i> menu. */
    void prepareMacWindowMenu();
#endif /* Q_WS_MAC */

    /** Repopulates tree-widgets content. */
    void repopulateTreeWidgets();

    /** Updates details according latest changes in current item of passed @a type. */
    void refetchCurrentMediumItem(UIMediumType type);
    /** Updates details according latest changes in current item of chosen type. */
    void refetchCurrentChosenMediumItem();
    /** Updates details according latest changes in all current items. */
    void refetchCurrentMediumItems();

    /** Update actions according currently chosen item. */
    void updateActions();
    /** Update tab icons according last @a action happened with @a pItem. */
    void updateTabIcons(UIMediumItem *pItem, Action action);
    /** Update information pane of passed medium @a type. */
    void updateInformationPanes(UIMediumType type = UIMediumType_Invalid);
    /** Update information pane for hard-drive tab. */
    void updateInformationPanesHD();
    /** Update information pane for optical-disk tab. */
    void updateInformationPanesCD();
    /** Update information pane for floppy-disk tab. */
    void updateInformationPanesFD();

#ifdef Q_WS_MAC
    /** Mac OS X: Cleanup <i>Window</i> menu. */
    void cleanupMacWindowMenu();
#endif /* Q_WS_MAC */
    /** General cleanup wrapper. */
    void cleanup();

    /** Translates dialog content. */
    void retranslateUi();

    /** Creates UIMediumItem for corresponding @a medium. */
    UIMediumItem* createMediumItem(const UIMedium &medium);
    /** Creates UIMediumItemHD for corresponding @a medium. */
    UIMediumItem* createHardDiskItem(const UIMedium &medium);
    /** Updates UIMediumItem for corresponding @a medium. */
    void updateMediumItem(const UIMedium &medium);
    /** Deletes UIMediumItem for corresponding @a strMediumID. */
    void deleteMediumItem(const QString &strMediumID);

    /** Determines medium type for passed @a pTreeWidget. */
    UIMediumType mediumType(QTreeWidget *pTreeWidget) const;

    /** Returns current medium type. */
    UIMediumType currentMediumType() const;

    /** Returns tree-widget for passed medium @a type. */
    QTreeWidget* treeWidget(UIMediumType type) const;
    /** Returns current tree-widget. */
    QTreeWidget* currentTreeWidget() const;

    /** Returns item for passed medium @a type. */
    UIMediumItem* mediumItem(UIMediumType type) const;
    /** Returns current item. */
    UIMediumItem* currentMediumItem() const;

    /** Defines current item for passed @a pTreeWidget as @a pItem. */
    void setCurrentItem(QTreeWidget *pTreeWidget, QTreeWidgetItem *pItem);

    /** Performs search for the @a pTree child which corresponds to the @a condition but not @a pException. */
    static UIMediumItem* searchItem(QTreeWidget *pTree, const CheckIfSuitableBy &condition, CheckIfSuitableBy *pException = 0);
    /** Performs search for the @a pParentItem child which corresponds to the @a condition but not @a pException. */
    static UIMediumItem* searchItem(QTreeWidgetItem *pParentItem, const CheckIfSuitableBy &condition, CheckIfSuitableBy *pException = 0);

    /** Checks if @a action can be used for @a pItem. */
    static bool checkMediumFor(UIMediumItem *pItem, Action action);

    /** Casts passed QTreeWidgetItem @a pItem to UIMediumItem if possible. */
    static UIMediumItem* toMediumItem(QTreeWidgetItem *pItem);

    /** Format information-pane content. */
    static QString formatPaneText(const QString &strText, bool fCompact = true, const QString &strElipsis = "middle");

    /** Determines if passed @a medium attached to hidden machines only. */
    static bool isMediumAttachedToHiddenMachinesOnly(const UIMedium &medium);

    /** UIMediumManager singleton instance. */
    static UIMediumManager *m_spInstance;

    /** @name General variables.
     * @{ */
    /** Widget to center UIMediumManager according. */
    QWidget *m_pPseudoParentWidget;
    /** Holds whether UIMediumManager should be refreshed on invoke. */
    bool m_fRefresh;
    /** Holds whether UIMediumManager should preserve current item change. */
    bool m_fPreventChangeCurrentItem;
    /** @} */

    /** @name Tab-widget variables.
     * @{ */
    /** Holds whether hard-drive tab-widget have inaccessible item. */
    bool m_fInaccessibleHD;
    /** Holds whether optical-disk tab-widget have inaccessible item. */
    bool m_fInaccessibleCD;
    /** Holds whether floppy-disk tab-widget have inaccessible item. */
    bool m_fInaccessibleFD;
    /** Holds cached hard-drive tab-widget icon. */
    const QIcon m_iconHD;
    /** Holds cached optical-disk tab-widget icon. */
    const QIcon m_iconCD;
    /** Holds cached floppy-disk tab-widget icon. */
    const QIcon m_iconFD;
    /** Holds current hard-drive tree-view item ID. */
    QString m_strCurrentIdHD;
    /** Holds current optical-disk tree-view item ID. */
    QString m_strCurrentIdCD;
    /** Holds current floppy-disk tree-view item ID. */
    QString m_strCurrentIdFD;
    /** @} */

    /** @name Tool-bar and menu variables.
     * @{ */
    /** Tool-bar widget. */
    UIToolBar *m_pToolBar;
    /** Context menu object. */
    QMenu *m_pContextMenu;
    /** Menu-bar menu object. */
    QMenu *m_pMenu;
    /** Action to <i>copy</i> current item. */
    QAction *m_pActionCopy;
    /** Action to <i>modify</i> current item. */
    QAction *m_pActionModify;
    /** Action to <i>remove</i> current item. */
    QAction *m_pActionRemove;
    /** Action to <i>release</i> current item. */
    QAction *m_pActionRelease;
    /** Action to <i>refresh</i> current item. */
    QAction *m_pActionRefresh;
    /** @} */

    /** @name Progress-bar variables.
     * @{ */
    /** Progress-bar widget. */
    UIEnumerationProgressBar *m_pProgressBar;
    /** @} */
};

#endif /* !___UIMediumManager_h___ */
