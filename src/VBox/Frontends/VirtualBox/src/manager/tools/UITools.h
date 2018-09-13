/* $Id$ */
/** @file
 * VBox Qt GUI - UITools class declaration.
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

#ifndef ___UITools_h___
#define ___UITools_h___

/* Qt includes: */
#include <QWidget>

/* Forward declarations: */
class QVBoxLayout;
class UIActionPool;
class UIToolsItem;
class UIToolsModel;
class UIToolsView;
class UIVirtualBoxManagerWidget;


/** Item classes. */
enum UIToolsClass
{
    UIToolsClass_Global,
    UIToolsClass_Machine
};


/** Item types. */
enum UIToolsType
{
    /* Global class: */
    UIToolsType_Media,
    UIToolsType_Network,
    /* Machine class: */
    UIToolsType_Details,
    UIToolsType_Snapshots,
    UIToolsType_Logs,
    /* Max */
    UIToolsType_Max
};


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
        void setToolsClass(UIToolsClass enmClass);
        /** Returns current tools class. */
        UIToolsClass toolsClass() const;
        /** Returns current tools type. */
        UIToolsType toolsType() const;
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


#endif /* !___UITools_h___ */
