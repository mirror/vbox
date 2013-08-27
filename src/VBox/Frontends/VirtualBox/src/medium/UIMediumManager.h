/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMediumManager class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#include "UIMedium.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"
#include "CVirtualBox.h"

/* Forward declarations: */
class UIToolBar;
class UIMediumItem;
class UIEnumerationProgressBar;

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
    void sltHandleMediumAdded(const UIMedium &medium);
    void sltHandleMediumUpdated(const UIMedium &medium);
    void sltHandleMediumRemoved(UIMediumType type, const QString &strId);

    /* Handlers: Medium-enumeration stuff: */
    void sltHandleMediumEnumerationStart();
    void sltHandleMediumEnumerated(const UIMedium &medium);
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

    /* Handler: Machine stuff: */
    void sltHandleMachineStateChanged(QString strId, KMachineState state);

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

    /* Helper: Medium-modification stuff: */
    bool releaseMediumFrom(const UIMedium &medium, const QString &strMachineId);
    bool releaseHardDiskFrom(const UIMedium &medium, CMachine &machine);
    bool releaseOpticalDiskFrom(const UIMedium &medium, CMachine &machine);
    bool releaseFloppyDiskFrom(const UIMedium &medium, CMachine &machine);

    /* Internal API: Tree-widget access stuff: */
    QTreeWidget* treeWidget(UIMediumType type) const;
    UIMediumType currentTreeWidgetType() const;
    QTreeWidget* currentTreeWidget() const;
    UIMediumItem* toMediumItem(QTreeWidgetItem *aItem) const;
    void setCurrentItem(QTreeWidget *pTree, QTreeWidgetItem *pItem);

    UIMediumItem* createHardDiskItem (QTreeWidget *aTree, const UIMedium &aMedium) const;

    void updateTabIcons (UIMediumItem *aItem, ItemAction aAction);

    UIMediumItem* searchItem (QTreeWidget *aTree, const QString &aId) const;

    bool checkMediumFor (UIMediumItem *aItem, Action aAction);

    void clearInfoPanes();
    void prepareToRefresh (int aTotal = 0);

    QString formatPaneText (const QString &aText, bool aCompact = true, const QString &aElipsis = "middle");

    /* Helper: Enumeration stuff: */
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

