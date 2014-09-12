/** @file
 * VBox Qt GUI - UISelectorWindow class declaration.
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

#ifndef __UISelectorWindow_h__
#define __UISelectorWindow_h__

/* Qt includes: */
#include <QMainWindow>
#include <QUrl>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QISplitter;
class QMenu;
class UIAction;
class UIActionPolymorphic;
class UIMainBar;
class UIToolBar;
class UIVMDesktop;
class UIVMItem;
class UIGChooser;
class UIGDetails;
class UIActionPool;
class QStackedWidget;

/* VM selector window class: */
class UISelectorWindow : public QIWithRetranslateUI2<QMainWindow>
{
    Q_OBJECT;

public:

    /* Constructor/destructor: */
    UISelectorWindow(UISelectorWindow **ppSelf,
                     QWidget* pParent = 0,
                     Qt::WindowFlags flags = Qt::Window);
    ~UISelectorWindow();

    /** Returns the action-pool instance. */
    UIActionPool* actionPool() const { return m_pActionPool; }

private slots:

    /* Handlers: Global-event stuff: */
    void sltStateChanged(QString strId);
    void sltSnapshotChanged(QString strId);

    /* Handler: Details-view stuff: */
    void sltDetailsViewIndexChanged(int iWidgetIndex);

    /* Handler: Medium enumeration stuff: */
    void sltHandleMediumEnumerationFinish();

    /* Handler: Menubar/status stuff: */
    void sltShowSelectorContextMenu(const QPoint &pos);

    /* Handlers: File-menu stuff: */
    void sltShowMediumManager();
    void sltShowImportApplianceWizard(const QString &strFileName = QString());
    void sltShowExportApplianceWizard();
#ifdef DEBUG
    void sltOpenExtraDataManagerWindow();
#endif /* DEBUG */
    void sltShowPreferencesDialog();
    void sltPerformExit();

    /* Handlers: Machine-menu slots: */
    void sltShowAddMachineDialog(const QString &strFileName = QString());
    void sltShowMachineSettingsDialog(const QString &strCategory = QString(),
                                      const QString &strControl = QString(),
                                      const QString &strId = QString());
    void sltShowCloneMachineWizard();
    void sltPerformStartOrShowAction();
    void sltPerformDiscardAction();
    void sltPerformPauseResumeAction(bool fPause);
    void sltPerformResetAction();
    void sltPerformSaveAction();
    void sltPerformACPIShutdownAction();
    void sltPerformPowerOffAction();
    void sltShowLogDialog();
    void sltShowMachineInFileManager();
    void sltPerformCreateShortcutAction();
    void sltGroupCloseMenuAboutToShow();
    void sltMachineCloseMenuAboutToShow();

    /* VM list slots: */
    void sltCurrentVMItemChanged(bool fRefreshDetails = true, bool fRefreshSnapshots = true, bool fRefreshDescription = true);
    void sltOpenUrls(QList<QUrl> list = QList<QUrl>());

    /* Handlers: Group saving stuff: */
    void sltGroupSavingUpdate();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Event handlers: */
    bool event(QEvent *pEvent);
    void showEvent(QShowEvent *pEvent);
    void polishEvent(QShowEvent *pEvent);
#ifdef Q_WS_MAC
    bool eventFilter(QObject *pObject, QEvent *pEvent);
#endif /* Q_WS_MAC */

    /* Helpers: Prepare stuff: */
    void prepareIcon();
    void prepareMenuBar();
    void prepareMenuFile(QMenu *pMenu);
    void prepareMenuGroup(QMenu *pMenu);
    void prepareMenuMachine(QMenu *pMenu);
    void prepareMenuGroupClose(QMenu *pMenu);
    void prepareMenuMachineClose(QMenu *pMenu);
    void prepareStatusBar();
    void prepareWidgets();
    void prepareConnections();
    void loadSettings();
    void saveSettings();
    void cleanupConnections();
    void cleanupMenuBar();

    /* Helpers: Current item stuff: */
    UIVMItem* currentItem() const;
    QList<UIVMItem*> currentItems() const;

    /* Helper: Action update stuff: */
    void updateActionsAppearance();

    /* Helpers: Action stuff: */
    bool isActionEnabled(int iActionIndex, const QList<UIVMItem*> &items);
    static bool isItemsPoweredOff(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemAbleToShutdown(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemSupportsShortcuts(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemAccessible(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemInaccessible(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemRemovable(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemCanBeStartedOrShowed(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemDiscardable(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemStarted(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemRunning(const QList<UIVMItem*> &items);

    /* Variables: */
    bool m_fPolished : 1;
    bool m_fWarningAboutInaccessibleMediumShown : 1;

    /** Holds the action-pool instance. */
    UIActionPool *m_pActionPool;

    /* Central splitter window: */
    QISplitter *m_pSplitter;

    /* Main toolbar: */
#ifndef Q_WS_MAC
    UIMainBar *m_pBar;
#endif /* !Q_WS_MAC */
    UIToolBar *mVMToolBar;

    /* Details widgets container: */
    QStackedWidget *m_pContainer;

    /* Graphics chooser/details: */
    UIGChooser *m_pChooser;
    UIGDetails *m_pDetails;

    /* VM details widget: */
    UIVMDesktop *m_pVMDesktop;

    /* 'Group' menu action pointers: */
    QList<UIAction*> m_groupActions;
    QAction *m_pGroupMenuAction;

    /* 'Machine' menu action pointers: */
    QList<UIAction*> m_machineActions;
    QAction *m_pMachineMenuAction;

    /* Other variables: */
    QRect m_geometry;
};

#endif // __UISelectorWindow_h__

