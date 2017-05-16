/* $Id$ */
/** @file
 * VBox Qt GUI - UIHostNetworkManager class declaration.
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

#ifndef ___UIHostNetworkManager_h___
#define ___UIHostNetworkManager_h___

/* Qt includes: */
#include <QMainWindow>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class CHostNetworkInterface;
class QAbstractButton;
class QTreeWidgetItem;
class QIDialogButtonBox;
class QITreeWidget;
class UIHostNetworkDetailsDialog;
class UIItemHostNetwork;
class UIToolBar;
struct UIDataHostNetwork;


/** Host Network Manager dialog. */
class UIHostNetworkManager : public QIWithRetranslateUI<QMainWindow>
{
    Q_OBJECT;

    /** Constructs Host Network Manager dialog.
      * @param  pCenterWidget  Brings the pseudo-parent widget to center according to. */
    UIHostNetworkManager(QWidget *pCenterWidget);
    /** Destructs Host Network Manager dialog. */
    ~UIHostNetworkManager();

public:

    /** Returns Host Network Manager singleton instance. */
    static UIHostNetworkManager *instance();
    /** Shows Host Network Manager singleton instance, creates new if necessary. */
    static void showModeless(QWidget *pCenterWidget);

protected:

    /** @name Event-handling stuff.
     * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;

        /** Handles resize @a pEvent. */
        virtual void resizeEvent(QResizeEvent *pEvent) /* override */;

        /** Handles show @a pEvent. */
        virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** @} */

private slots:

    /** @name Menu/action stuff.
     * @{ */
        /** Handles command to add host network. */
        void sltAddHostNetwork();
        /** Handles command to remove host network. */
        void sltRemoveHostNetwork();
        /** Handles command to make host network details @a fVisible. */
        void sltToggleHostNetworkDetailsVisibility(bool fVisible);
        /** Handles command to apply host network details changes. */
        void sltApplyHostNetworkDetailsChanges();
    /** @} */

    /** @name Tree-widget stuff.
     * @{ */
        /** Handles command to adjust tree-widget. */
        void sltAdjustTreeWidget();

        /** Handles tree-widget @a pItem change. */
        void sltHandleItemChange(QTreeWidgetItem *pItem);
        /** Handles tree-widget current item change. */
        void sltHandleCurrentItemChange();
        /** Handles context menu request for tree-widget @a position. */
        void sltHandleContextMenuRequest(const QPoint &position);
    /** @} */

private:

    /** @name Prepare/cleanup cascade.
     * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares this. */
        void prepareThis();
        /** Prepares actions. */
        void prepareActions();
        /** Prepares menu-bar. */
        void prepareMenuBar();
        /** Prepares central-widget. */
        void prepareCentralWidget();
        /** Prepares tool-bar. */
        void prepareToolBar();
        /** Prepares tree-widget. */
        void prepareTreeWidget();
        /** Prepares details-widget. */
        void prepareDetailsWidget();
        /** Prepares button-box. */
        void prepareButtonBox();

        /** Cleanup menu-bar. */
        void cleanupMenuBar();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name Loading stuff.
     * @{ */
        /** Loads host networks. */
        void loadHostNetworks();
        /** Loads host @a comInterface data to passed @a data container. */
        void loadHostNetwork(const CHostNetworkInterface &comInterface, UIDataHostNetwork &data);
    /** @} */

    /** @name Tree-widget stuff.
     * @{ */
        /** Creates a new tree-widget item on the basis of passed @a data, @a fChooseItem if requested. */
        void createItemForNetworkHost(const UIDataHostNetwork &data, bool fChooseItem);
        /** Updates the passed tree-widget item on the basis of passed @a data, @a fChooseItem if requested. */
        void updateItemForNetworkHost(const UIDataHostNetwork &data, bool fChooseItem, UIItemHostNetwork *pItem);
    /** @} */

    /** @name General variables.
     * @{ */
        /** Holds the Host Network Manager singleton instance. */
        static UIHostNetworkManager *s_pInstance;

        /** Widget to center UIMediumManager according. */
        QWidget *m_pPseudoParentWidget;
    /** @} */

    /** @name Tool-bar and menu variables.
     * @{ */
        /** Holds the tool-bar instance. */
        UIToolBar *m_pToolBar;
        /** Holds menu-bar menu object instance. */
        QMenu     *m_pMenu;
        /** Holds the Add action instance. */
        QAction   *m_pActionAdd;
        /** Holds the Remove action instance. */
        QAction   *m_pActionRemove;
        /** Holds the Details action instance. */
        QAction   *m_pActionDetails;
        /** Holds the Commit action instance. */
        QAction   *m_pActionCommit;
    /** @} */

    /** @name Splitter variables.
     * @{ */
        /** Holds the tree-widget instance. */
        QITreeWidget *m_pTreeWidget;
        /** Holds the details-widget instance. */
        UIHostNetworkDetailsDialog *m_pDetailsWidget;
    /** @} */

    /** @name Button-box variables.
     * @{ */
        /** Holds the dialog button-box instance. */
        QIDialogButtonBox *m_pButtonBox;
    /** @} */
};

#endif /* !___UIHostNetworkManager_h___ */

