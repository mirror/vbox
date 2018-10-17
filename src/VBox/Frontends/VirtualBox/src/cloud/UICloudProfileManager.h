/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudProfileManager class declaration.
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

#ifndef ___UICloudProfileManager_h___
#define ___UICloudProfileManager_h___

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QAbstractButton;
class QTreeWidgetItem;
class QITreeWidget;
class UIActionPool;
class UICloudProfileDetailsWidget;
class UIItemCloudProfile;
class UIItemCloudProvider;
class UIToolBar;
struct UIDataCloudProfile;
struct UIDataCloudProvider;
class CCloudProfile;
class CCloudProvider;


/** QWidget extension providing GUI with the pane to control cloud profile related functionality. */
class UICloudProfileManagerWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about cloud profile details-widget @a fVisible. */
    void sigCloudProfileDetailsVisibilityChanged(bool fVisible);
    /** Notifies listeners about cloud profile details data @a fDiffers. */
    void sigCloudProfileDetailsDataChanged(bool fDiffers);

public:

    /** Constructs Cloud Profile Manager widget.
      * @param  enmEmbedding  Brings the type of widget embedding.
      * @param  pActionPool   Brings the action-pool reference.
      * @param  fShowToolbar  Brings whether we should create/show toolbar. */
    UICloudProfileManagerWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                bool fShowToolbar = true, QWidget *pParent = 0);

    /** Returns the menu. */
    QMenu *menu() const;

#ifdef VBOX_WS_MAC
    /** Returns the toolbar. */
    UIToolBar *toolbar() const { return m_pToolBar; }
#endif

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

public slots:

    /** @name Details-widget stuff.
      * @{ */
        /** Handles command to reset cloud profile details changes. */
        void sltResetCloudProfileDetailsChanges();
        /** Handles command to apply cloud profile details changes. */
        void sltApplyCloudProfileDetailsChanges();
    /** @} */

private slots:

    /** @name Menu/action stuff.
      * @{ */
        /** Handles command to create cloud profile. */
        void sltCreateCloudProfile();
        /** Handles command to remove cloud profile. */
        void sltRemoveCloudProfile();
        /** Handles command to make cloud profile details @a fVisible. */
        void sltToggleCloudProfileDetailsVisibility(bool fVisible);
        /** Handles command to refresh cloud profiles. */
        void sltRefreshCloudProfiles();
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
        /** Prepares actions. */
        void prepareActions();
        /** Prepares widgets. */
        void prepareWidgets();
        /** Prepares toolbar. */
        void prepareToolBar();
        /** Prepares tree-widget. */
        void prepareTreeWidget();
        /** Prepares details-widget. */
        void prepareDetailsWidget();
        /** Load settings: */
        void loadSettings();
    /** @} */

    /** @name Loading stuff.
      * @{ */
        /** Loads cloud stuff. */
        void loadCloudStuff();
        /** Loads cloud @a comProvider data to passed @a data container. */
        void loadCloudProvider(const CCloudProvider &comProvider, UIDataCloudProvider &data);
        /** Loads cloud @a comProfile data to passed @a data container. */
        void loadCloudProfile(const CCloudProfile &comProfile, UIDataCloudProfile &data);
    /** @} */

    /** @name Tree-widget stuff.
      * @{ */
        /** Creates a new tree-widget item on the basis of passed @a data, @a fChooseItem if requested. */
        void createItemForCloudProvider(const UIDataCloudProvider &data, bool fChooseItem);

        /** Creates a new tree-widget item as a child of certain @a pParent, on the basis of passed @a data, @a fChooseItem if requested. */
        void createItemForCloudProfile(QTreeWidgetItem *pParent, const UIDataCloudProfile &data, bool fChooseItem);
        /** Updates the passed tree-widget item on the basis of passed @a data, @a fChooseItem if requested. */
        void updateItemForCloudProfile(const UIDataCloudProfile &data, bool fChooseItem, UIItemCloudProfile *pItem);
    /** @} */

    /** @name General variables.
      * @{ */
        /** Holds the widget embedding type. */
        const EmbedTo m_enmEmbedding;
        /** Holds the action-pool reference. */
        UIActionPool *m_pActionPool;
        /** Holds whether we should create/show toolbar. */
        const bool    m_fShowToolbar;
    /** @} */

    /** @name Toolbar and menu variables.
      * @{ */
        /** Holds the toolbar instance. */
        UIToolBar *m_pToolBar;
    /** @} */

    /** @name Splitter variables.
      * @{ */
        /** Holds the tree-widget instance. */
        QITreeWidget *m_pTreeWidget;
        /** Holds the details-widget instance. */
        UICloudProfileDetailsWidget *m_pDetailsWidget;
    /** @} */
};


/** QIManagerDialogFactory extension used as a factory for Cloud Profile Manager dialog. */
class UICloudProfileManagerFactory : public QIManagerDialogFactory
{
public:

    /** Constructs Cloud Profile Manager factory acquiring additional arguments.
      * @param  pActionPool  Brings the action-pool reference. */
    UICloudProfileManagerFactory(UIActionPool *pActionPool = 0);

protected:

    /** Creates derived @a pDialog instance.
      * @param  pCenterWidget  Brings the widget reference to center according to. */
    virtual void create(QIManagerDialog *&pDialog, QWidget *pCenterWidget) /* override */;

    /** Holds the action-pool reference. */
    UIActionPool *m_pActionPool;
};


/** QIManagerDialog extension providing GUI with the dialog to control cloud profile related functionality. */
class UICloudProfileManager : public QIWithRetranslateUI<QIManagerDialog>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about data change rejected and should be reseted. */
    void sigDataChangeRejected();
    /** Notifies listeners about data change accepted and should be applied. */
    void sigDataChangeAccepted();

private slots:

    /** @name Button-box stuff.
      * @{ */
        /** Handles button-box button click. */
        void sltHandleButtonBoxClick(QAbstractButton *pButton);
    /** @} */

private:

    /** Constructs Cloud Profile Manager dialog.
      * @param  pCenterWidget  Brings the widget reference to center according to.
      * @param  pActionPool    Brings the action-pool reference. */
    UICloudProfileManager(QWidget *pCenterWidget, UIActionPool *pActionPool);

    /** @name Event-handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;
    /** @} */

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Configures all. */
        virtual void configure() /* override */;
        /** Configures central-widget. */
        virtual void configureCentralWidget() /* override */;
        /** Configures button-box. */
        virtual void configureButtonBox() /* override */;
        /** Perform final preparations. */
        virtual void finalize() /* override */;
    /** @} */

    /** @name Widget stuff.
      * @{ */
        /** Returns the widget. */
        virtual UICloudProfileManagerWidget *widget() /* override */;
    /** @} */

    /** @name Action related variables.
      * @{ */
        /** Holds the action-pool reference. */
        UIActionPool *m_pActionPool;
    /** @} */

    /** Allow factory access to private/protected members: */
    friend class UICloudProfileManagerFactory;
};

#endif /* !___UICloudProfileManager_h___ */
