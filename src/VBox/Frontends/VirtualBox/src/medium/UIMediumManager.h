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

public slots:

    /* Handler: Refresh stuff: */
    void refreshAll();

private slots:

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
    void sltHandleCurrentItemChanged(QTreeWidgetItem *pItem, QTreeWidgetItem *pPrevItem = 0);
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
    void preapreTabWidget();
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

    /* Helper: Populate stuff: */
    void populateTreeWidgets();

    /* Helpers: Cleanup stuff: */
#ifdef Q_WS_MAC
    void cleanupMacWindowMenu();
#endif /* Q_WS_MAC */
    void cleanup();

    /* Handler: Translation stuff: */
    void retranslateUi();

    /* Helpers: Medium-modification stuff: */
    bool releaseMediumFrom(const UIMedium &medium, const QString &strMachineId);
    bool releaseHardDiskFrom(const UIMedium &medium, CMachine &machine);
    bool releaseOpticalDiskFrom(const UIMedium &medium, CMachine &machine);
    bool releaseFloppyDiskFrom(const UIMedium &medium, CMachine &machine);

    /* Internal API: Tree-widget access stuff: */
    QTreeWidget* treeWidget(UIMediumType type) const;
    UIMediumType currentTreeWidgetType() const;
    QTreeWidget* currentTreeWidget() const;
    void setCurrentItem(QTreeWidget *pTree, QTreeWidgetItem *pItem);
    UIMediumItem* toMediumItem(QTreeWidgetItem *pItem) const;
    UIMediumItem* searchItem(QTreeWidget *pTree, const CheckIfSuitableBy &functor) const;
    UIMediumItem* searchItem(QTreeWidgetItem *pParentItem, const CheckIfSuitableBy &functor) const;
    UIMediumItem* createHardDiskItem(QTreeWidget *pTree, const UIMedium &medium) const;

    /* Internal API: Tab-widget access stuff: */
    void updateTabIcons(UIMediumItem *pItem, ItemAction action);

    /* Helpers: Other stuff: */
    bool checkMediumFor(UIMediumItem *pItem, Action action);
    void clearInfoPanes();
    void prepareToRefresh(int iTotal = 0);

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
