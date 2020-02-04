/* $Id$ */
/** @file
 * VBox Qt GUI - UITools class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_tools_UITools_h
#define FEQT_INCLUDED_SRC_manager_tools_UITools_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI icludes: */
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class QVBoxLayout;
class UIActionPool;
class UIToolsItem;
class UIToolsModel;
class UIToolsView;
class UIVirtualBoxManagerWidget;

/** QWidget extension used as VM Tools-pane. */
class UITools : public QWidget
{
    Q_OBJECT;

signals:

    /** @name General stuff.
      * @{ */
        /** Notifies listeners about selection changed. */
        void sigSelectionChanged();

        /** Notifies listeners about expanding started. */
        void sigExpandingStarted();
        /** Notifies listeners about expanding finished. */
        void sigExpandingFinished();
    /** @} */

public:

    /** Constructs Tools-pane passing @a pParent to the base-class. */
    UITools(UIVirtualBoxManagerWidget *pParent);
    /** Destructs Tools-pane. */
    virtual ~UITools() /* override */;

    /** @name General stuff.
      * @{ */
        /** Returns the manager-widget reference. */
        UIVirtualBoxManagerWidget *managerWidget() const { return m_pManagerWidget; }

        /** Returns the action-pool reference. */
        UIActionPool *actionPool() const;

        /** Return the Tools-model instance. */
        UIToolsModel *model() const { return m_pToolsModel; }
        /** Return the Tools-view instance. */
        UIToolsView *view() const { return m_pToolsView; }

        /** Defines current tools @a enmClass. */
        void setToolsClass(UIToolClass enmClass);
        /** Returns current tools class. */
        UIToolClass toolsClass() const;

        /** Defines current tools @a enmType. */
        void setToolsType(UIToolType enmType);
        /** Returns current tools type. */
        UIToolType toolsType() const;

        /** Returns last selected global tool. */
        UIToolType lastSelectedToolGlobal() const;
        /** Returns last selected machine tool. */
        UIToolType lastSelectedToolMachine() const;

        /** Defines whether certain @a enmClass of tools is @a fEnabled.*/
        void setToolsEnabled(UIToolClass enmClass, bool fEnabled);
        /** Returns whether certain class of tools is enabled.*/
        bool areToolsEnabled(UIToolClass enmClass) const;
    /** @} */

    /** @name Current item stuff.
      * @{ */
        /** Returns current item. */
        UIToolsItem *currentItem() const;
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
        QVBoxLayout  *m_pMainLayout;
        /** Holds the Tools-model instane. */
        UIToolsModel *m_pToolsModel;
        /** Holds the Tools-view instane. */
        UIToolsView  *m_pToolsView;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_manager_tools_UITools_h */
