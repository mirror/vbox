/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooser class declaration.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooser_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooser_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* Forward declarations: */
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
        /** Notifies listeners about selection invalidated. */
        void sigSelectionInvalidated();

        /** Notifies listeners about toggling started. */
        void sigToggleStarted();
        /** Notifies listeners about toggling finished. */
        void sigToggleFinished();

        /** Notifies listeners about tool popup-menu request for certain tool @a enmClass and in specified @a position. */
        void sigToolMenuRequested(UIToolClass enmClass, const QPoint &position);
    /** @} */

    /** @name Cloud machine stuff.
      * @{ */
        /** Notifies about state change for cloud machine with certain @a uId. */
        void sigCloudMachineStateChange(const QUuid &uId);
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

    /** @name Current-item stuff.
      * @{ */
        /** Returns current-item. */
        UIVirtualMachineItem *currentItem() const;
        /** Returns a list of current-items. */
        QList<UIVirtualMachineItem*> currentItems() const;

        /** Returns whether group item is selected. */
        bool isGroupItemSelected() const;
        /** Returns whether global item is selected. */
        bool isGlobalItemSelected() const;
        /** Returns whether machine item is selected. */
        bool isMachineItemSelected() const;

        /** Returns whether single group is selected. */
        bool isSingleGroupSelected() const;
        /** Returns whether single local group is selected. */
        bool isSingleLocalGroupSelected() const;
        /** Returns whether single cloud profile group is selected. */
        bool isSingleCloudProfileGroupSelected() const;
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
        void sltToolMenuRequested(UIToolClass enmClass, const QPoint &position);
    /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares palette. */
        void preparePalette();
        /** Prepares model. */
        void prepareModel();
        /** Prepares widgets. */
        void prepareWidgets();
        /** Prepares connections. */
        void prepareConnections();
        /** Init model. */
        void initModel();

        /** Deinit model. */
        void deinitModel();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Holds the manager-widget reference. */
        UIVirtualBoxManagerWidget *m_pManagerWidget;

        /** Holds the Chooser-model instane. */
        UIChooserModel *m_pChooserModel;
        /** Holds the Chooser-view instane. */
        UIChooserView  *m_pChooserView;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooser_h */
