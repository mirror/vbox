/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooser class declaration.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIChooser_h___
#define ___UIChooser_h___

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class QVBoxLayout;
class UIActionPool;
class UIChooserModel;
class UIChooserView;
class UIVirtualBoxManagerWidget;
class UIVirtualMachineItem;

/** QWidget extension used as VM Chooser-pane. */
class UIChooser : public QWidget
{
    Q_OBJECT;

signals:

    /** @name General stuff.
      * @{ */
        /** Notifies listeners about selection changed. */
        void sigSelectionChanged();

        /** Notifies listeners about sliding started. */
        void sigSlidingStarted();

        /** Notifies listeners about toggling started. */
        void sigToggleStarted();
        /** Notifies listeners about toggling finished. */
        void sigToggleFinished();

        /** Notifies listeners about tool popup-menu request for certain tool @a enmClass and in specified @a position. */
        void sigToolMenuRequested(UIToolsClass enmClass, const QPoint &position);
    /** @} */

    /** @name Group saving stuff.
      * @{ */
        /** Notifies listeners about group saving state change. */
        void sigGroupSavingStateChanged();
    /** @} */

public:

    /** Constructs Chooser-pane passing @a pParent to the base-class. */
    UIChooser(UIVirtualBoxManagerWidget *pParent);
    /** Destructs Chooser-pane. */
    virtual ~UIChooser() /* override */;

    /** @name General stuff.
      * @{ */
        /** Returns the manager-widget reference. */
        UIVirtualBoxManagerWidget *managerWidget() const { return m_pManagerWidget; }

        /** Returns the action-pool reference. */
        UIActionPool *actionPool() const;

        /** Return the Chooser-model instance. */
        UIChooserModel *model() const { return m_pChooserModel; }
        /** Return the Chooser-view instance. */
        UIChooserView *view() const { return m_pChooserView; }
    /** @} */

    /** @name Current item stuff.
      * @{ */
        /** Returns current item. */
        UIVirtualMachineItem *currentItem() const;
        /** Returns a list of current items. */
        QList<UIVirtualMachineItem*> currentItems() const;

        /** Returns whether group item is selected. */
        bool isGroupItemSelected() const;
        /** Returns whether global item is selected. */
        bool isGlobalItemSelected() const;
        /** Returns whether machine item is selected. */
        bool isMachineItemSelected() const;

        /** Returns whether single group is selected. */
        bool isSingleGroupSelected() const;
        /** Returns whether all machine items of one group is selected. */
        bool isAllItemsOfOneGroupSelected() const;
    /** @} */

    /** @name Group saving stuff.
      * @{ */
        /** Returns whether group saving is in progress. */
        bool isGroupSavingInProgress() const;
    /** @} */

public slots:

    /** @name General stuff.
      * @{ */
        /** Handles toolbar resize to @a newSize. */
        void sltHandleToolbarResize(const QSize &newSize);
    /** @} */

private slots:

    /** @name General stuff.
      * @{ */
        /** Handles signal about tool popup-menu request for certain tool @a enmClass and in specified @a position. */
        void sltToolMenuRequested(UIToolsClass enmClass, const QPoint &position);
    /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares palette. */
        void preparePalette();
        /** Prepares layout. */
        void prepareLayout();
        /** Prepares model. */
        void prepareModel();
        /** Prepares view. */
        void prepareView();
        /** Prepares connections. */
        void prepareConnections();
        /** Loads settings. */
        void loadSettings();

        /** Saves settings. */
        void saveSettings();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Holds the manager-widget reference. */
        UIVirtualBoxManagerWidget *m_pManagerWidget;

        /** Holds the main layout instane. */
        QVBoxLayout    *m_pMainLayout;
        /** Holds the Chooser-model instane. */
        UIChooserModel *m_pChooserModel;
        /** Holds the Chooser-view instane. */
        UIChooserView  *m_pChooserView;
    /** @} */
};

#endif /* !___UIChooser_h___ */
