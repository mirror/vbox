/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumManager class declaration.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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

/* Qt includes: */
#include <QMainWindow>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIMediumDefs.h"

/* Forward declarations: */
class UIMedium;
class UIMediumItem;
class UIToolBar;
class UIEnumerationProgressBar;
class QTabWidget;
class QITreeWidget;
class QTreeWidgetItem;
class QFrame;
class QLabel;
class QILabel;
class QIDialogButtonBox;


/** Functor interface allowing to check if passed UIMediumItem is suitable. */
class CheckIfSuitableBy
{
public:

    /** Destructs functor. */
    virtual ~CheckIfSuitableBy() { /* Makes MSC happy. */ }

    /** Determines whether passed @a pItem is suitable. */
    virtual bool isItSuitable(UIMediumItem *pItem) const = 0;
};


/** Medium Manager dialog. */
class UIMediumManager : public QIWithRetranslateUI<QMainWindow>
{
    Q_OBJECT;

    /** Item action types. */
    enum Action { Action_Add, Action_Edit, Action_Copy, Action_Modify, Action_Remove, Action_Release };

    /** Constructs Virtual Medium Manager dialog.
      * @param  pCenterWidget  Brings the pseudo-parent widget to center according to.
      * @param  fRefresh       Brings whether we should restart medium enumeration. */
    UIMediumManager(QWidget *pCenterWidget, bool fRefresh = true);
    /** Destructs Virtual Medium Manager dialog. */
    ~UIMediumManager();

public:

    /** Returns Virtual Medium Manager singleton instance. */
    static UIMediumManager *instance();
    /** Shows Virtual Medium Manager singleton instance, creates new if necessary. */
    static void showModeless(QWidget *pCenterWidget, bool fRefresh = true);

private slots:

    /** @name Medium operation stuff.
      * @{ */
        /** Handles VBoxGlobal::sigMediumCreated signal. */
        void sltHandleMediumCreated(const QString &strMediumID);
        /** Handles VBoxGlobal::sigMediumDeleted signal. */
        void sltHandleMediumDeleted(const QString &strMediumID);
    /** @} */

    /** @name Medium enumeration stuff.
      * @{ */
        /** Handles VBoxGlobal::sigMediumEnumerationStarted signal. */
        void sltHandleMediumEnumerationStart();
        /** Handles VBoxGlobal::sigMediumEnumerated signal. */
        void sltHandleMediumEnumerated(const QString &strMediumID);
        /** Handles VBoxGlobal::sigMediumEnumerationFinished signal. */
        void sltHandleMediumEnumerationFinish();
    /** @} */

    /** @name Menu/action stuff.
      * @{ */
        /** Handles command to copy medium. */
        void sltCopyMedium();
        /** Handles command to modify medium. */
        void sltModifyMedium();
        /** Handles command to remove medium. */
        void sltRemoveMedium();
        /** Handles command to release medium. */
        void sltReleaseMedium();
        /** Handles command to refresh medium. */
        void sltRefreshAll();
    /** @} */

    /** @name Tab-widget stuff.
      * @{ */
        /** Handles tab change case. */
        void sltHandleCurrentTabChanged();
        /** Handles item change case. */
        void sltHandleCurrentItemChanged();
        /** Handles item double-click case. */
        void sltHandleDoubleClick();
        /** Handles item context-menu-call case. */
        void sltHandleContextMenuCall(const QPoint &position);
    /** @} */

    /** @name Tree-widget stuff.
      * @{ */
        /** Adjusts tree-widgets according content. */
        void sltPerformTablesAdjustment();
    /** @} */

private:

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares this. */
        void prepareThis();
        /** Prepares connections. */
        void prepareConnections();
        /** Prepares actions. */
        void prepareActions();
        /** Prepares menu-bar. */
        void prepareMenuBar();
        /** Prepares context-menu. */
        void prepareContextMenu();
        /** Prepares central-widget. */
        void prepareCentralWidget();
        /** Prepares toolbar. */
        void prepareToolBar();
        /** Prepares tab-widget. */
        void prepareTabWidget();
        /** Prepares tab-widget's tab. */
        void prepareTab(UIMediumType type);
        /** Prepares tab-widget's tree-widget. */
        void prepareTreeWidget(UIMediumType type, int iColumns);
        /** Prepares tab-widget's information-container. */
        void prepareInformationContainer(UIMediumType type, int iFields);
        /** Prepares button-box. */
        void prepareButtonBox();
        /** Prepares progress-bar. */
        void prepareProgressBar();

        /** Repopulates tree-widgets content. */
        void repopulateTreeWidgets();

        /** Updates details according latest changes in current item of passed @a type. */
        void refetchCurrentMediumItem(UIMediumType type);
        /** Updates details according latest changes in current item of chosen type. */
        void refetchCurrentChosenMediumItem();
        /** Updates details according latest changes in all current items. */
        void refetchCurrentMediumItems();

        /** Updates actions according currently chosen item. */
        void updateActions();
        /** Updates action icons according currently chosen tab. */
        void updateActionIcons();
        /** Updates tab icons according last @a action happened with @a pItem. */
        void updateTabIcons(UIMediumItem *pItem, Action action);
        /** Updates information fields of passed medium @a type. */
        void updateInformationFields(UIMediumType type = UIMediumType_Invalid);
        /** Updates information fields for hard-drive tab. */
        void updateInformationFieldsHD();
        /** Updates information fields for optical-disk tab. */
        void updateInformationFieldsCD();
        /** Updates information fields for floppy-disk tab. */
        void updateInformationFieldsFD();

        /** Cleanups menu-bar. */
        void cleanupMenuBar();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name Event-handling stuff.
      * @{ */
        /** Handles translation event. */
        void retranslateUi();
    /** @} */

    /** @name Widget operation stuff.
      * @{ */
        /** Creates UIMediumItem for corresponding @a medium. */
        UIMediumItem *createMediumItem(const UIMedium &medium);
        /** Creates UIMediumItemHD for corresponding @a medium. */
        UIMediumItem *createHardDiskItem(const UIMedium &medium);
        /** Updates UIMediumItem for corresponding @a medium. */
        void updateMediumItem(const UIMedium &medium);
        /** Deletes UIMediumItem for corresponding @a strMediumID. */
        void deleteMediumItem(const QString &strMediumID);

        /** Returns tab for passed medium @a type. */
        QWidget *tab(UIMediumType type) const;
        /** Returns tree-widget for passed medium @a type. */
        QITreeWidget *treeWidget(UIMediumType type) const;
        /** Returns item for passed medium @a type. */
        UIMediumItem *mediumItem(UIMediumType type) const;
        /** Returns information-container for passed medium @a type. */
        QFrame *infoContainer(UIMediumType type) const;
        /** Returns information-label for passed medium @a type and @a iLabelIndex. */
        QLabel *infoLabel(UIMediumType type, int iLabelIndex) const;
        /** Returns information-field for passed medium @a type and @a iFieldIndex. */
        QILabel *infoField(UIMediumType type, int iFieldIndex) const;

        /** Returns medium type for passed @a pTreeWidget. */
        UIMediumType mediumType(QITreeWidget *pTreeWidget) const;

        /** Returns current medium type. */
        UIMediumType currentMediumType() const;
        /** Returns current tree-widget. */
        QITreeWidget *currentTreeWidget() const;
        /** Returns current item. */
        UIMediumItem *currentMediumItem() const;

        /** Defines current item for passed @a pTreeWidget as @a pItem. */
        void setCurrentItem(QITreeWidget *pTreeWidget, QTreeWidgetItem *pItem);
    /** @} */

    /** @name Helper stuff.
      * @{ */
        /** Returns tab index for passed UIMediumType. */
        static int tabIndex(UIMediumType type);
        /** Returns UIMediumType for passed tab index. */
        static UIMediumType mediumType(int iIndex);

        /** Performs search for the @a pTree child which corresponds to the @a condition but not @a pException. */
        static UIMediumItem *searchItem(QITreeWidget *pTree, const CheckIfSuitableBy &condition, CheckIfSuitableBy *pException = 0);
        /** Performs search for the @a pParentItem child which corresponds to the @a condition but not @a pException. */
        static UIMediumItem *searchItem(QTreeWidgetItem *pParentItem, const CheckIfSuitableBy &condition, CheckIfSuitableBy *pException = 0);

        /** Checks if @a action can be used for @a pItem. */
        static bool checkMediumFor(UIMediumItem *pItem, Action action);

        /** Casts passed QTreeWidgetItem @a pItem to UIMediumItem if possible. */
        static UIMediumItem *toMediumItem(QTreeWidgetItem *pItem);

        /** Formats information-field content. */
        static QString formatFieldText(const QString &strText, bool fCompact = true, const QString &strElipsis = "middle");
    /** @} */

    /** @name General variables.
      * @{ */
        /** Holds the Virtual Medium Manager singleton instance. */
        static UIMediumManager *s_pInstance;

        /** Holds the widget reference to center Virtual Medium Manager according. */
        QWidget *m_pPseudoParentWidget;
        /** Holds whether Virtual Medium Manager should be refreshed on invoke. */
        bool     m_fRefresh;
        /** Holds whether Virtual Medium Manager should preserve current item change. */
        bool     m_fPreventChangeCurrentItem;
    /** @} */

    /** @name Tab-widget variables.
      * @{ */
        /** Holds the tab-widget instance. */
        QTabWidget                  *m_pTabWidget;
        /** Holds the tab-widget tab-count. */
        const int                    m_iTabCount;
        /** Holds the map of tree-widget instances. */
        QMap<int, QITreeWidget*>     m_trees;
        /** Holds the map of information-container instances. */
        QMap<int, QFrame*>           m_containers;
        /** Holds the map of information-container label instances. */
        QMap<int, QList<QLabel*> >   m_labels;
        /** Holds the information-container field instances. */
        QMap<int, QList<QILabel*> >  m_fields;
        /** Holds whether hard-drive tab-widget have inaccessible item. */
        bool                         m_fInaccessibleHD;
        /** Holds whether optical-disk tab-widget have inaccessible item. */
        bool                         m_fInaccessibleCD;
        /** Holds whether floppy-disk tab-widget have inaccessible item. */
        bool                         m_fInaccessibleFD;
        /** Holds cached hard-drive tab-widget icon. */
        const QIcon                  m_iconHD;
        /** Holds cached optical-disk tab-widget icon. */
        const QIcon                  m_iconCD;
        /** Holds cached floppy-disk tab-widget icon. */
        const QIcon                  m_iconFD;
        /** Holds current hard-drive tree-view item ID. */
        QString                      m_strCurrentIdHD;
        /** Holds current optical-disk tree-view item ID. */
        QString                      m_strCurrentIdCD;
        /** Holds current floppy-disk tree-view item ID. */
        QString                      m_strCurrentIdFD;
    /** @} */

    /** @name Toolbar and menu variables.
      * @{ */
        /** Holds the toolbar widget instance. */
        UIToolBar *m_pToolBar;
        /** Holds the context-menu object instance. */
        QMenu     *m_pContextMenu;
        /** Holds the menu object instance. */
        QMenu     *m_pMenu;
        /** Holds the Copy action instance. */
        QAction   *m_pActionCopy;
        /** Holds the Modify action instance. */
        QAction   *m_pActionModify;
        /** Holds the Remove action instance. */
        QAction   *m_pActionRemove;
        /** Holds the Release action instance. */
        QAction   *m_pActionRelease;
        /** Holds the Refresh action instance. */
        QAction   *m_pActionRefresh;
    /** @} */

    /** @name Button-box variables.
      * @{ */
        /** Holds the dialog button-box instance. */
        QIDialogButtonBox        *m_pButtonBox;
        /** Holds the progress-bar widget instance. */
        UIEnumerationProgressBar *m_pProgressBar;
    /** @} */
};

#endif /* !___UIMediumManager_h___ */

