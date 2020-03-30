/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolPaneMachine class declaration.
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_UIToolPaneMachine_h
#define FEQT_INCLUDED_SRC_manager_UIToolPaneMachine_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* Forward declarations: */
class QHBoxLayout;
class QStackedLayout;
class QVBoxLayout;
class UIActionPool;
class UIDetails;
class UIErrorPane;
class UISnapshotPane;
class UIVirtualMachineItem;
class UIVMLogViewerWidget;


/** QWidget subclass representing container for tool panes. */
class UIToolPaneMachine : public QWidget
{
    Q_OBJECT;

signals:

    /** Redirects signal from UIVirtualBoxManager to UIDetails. */
    void sigToggleStarted();
    /** Redirects signal from UIVirtualBoxManager to UIDetails. */
    void sigToggleFinished();
    /** Redirects signal from UIDetails to UIVirtualBoxManager. */
    void sigLinkClicked(const QString &strCategory, const QString &strControl, const QUuid &uId);

    /** Notifies listeners about current Snapshot pane item change. */
    void sigCurrentSnapshotItemChange();

public:

    /** Constructs tools pane passing @a pParent to the base-class. */
    UIToolPaneMachine(UIActionPool *pActionPool, QWidget *pParent = 0);
    /** Destructs tools pane. */
    virtual ~UIToolPaneMachine() /* override */;

    /** Defines whether this pane is @a fActive. */
    void setActive(bool fActive);
    /** Returns whether this pane is active. */
    bool active() const { return m_fActive; }

    /** Returns type of tool currently opened. */
    UIToolType currentTool() const;
    /** Returns whether tool of particular @a enmType is opened. */
    bool isToolOpened(UIToolType enmType) const;
    /** Activates tool of passed @a enmType, creates new one if necessary. */
    void openTool(UIToolType enmType);
    /** Closes tool of passed @a enmType, deletes one if exists. */
    void closeTool(UIToolType enmType);

    /** Defines error @a strDetails and switches to Error pane. */
    void setErrorDetails(const QString &strDetails);

    /** Defines current machine @a pItem. */
    void setCurrentItem(UIVirtualMachineItem *pItem);

    /** Defines the machine @a items. */
    void setItems(const QList<UIVirtualMachineItem*> &items);

    /** Defines the @a comMachine object. */
    void setMachine(const CMachine &comMachine);

    /** Returns whether current-state item of Snapshot pane is selected. */
    bool isCurrentStateItemSelected() const;

private:

    /** Prepares all. */
    void prepare();
    /** Prepares stacked-layout. */
    void prepareStackedLayout();
    /** Cleanups all. */
    void cleanup();

    /** Handles token change. */
    void handleTokenChange();

    /** Holds the action pool reference. */
    UIActionPool *m_pActionPool;

    /** Holds current machine item reference. */
    UIVirtualMachineItem *m_pItem;

    /** Holds the stacked-layout instance. */
    QStackedLayout      *m_pLayout;
    /** Holds the Error pane instance. */
    UIErrorPane         *m_pPaneError;
    /** Holds the Details pane instance. */
    UIDetails           *m_pPaneDetails;
    /** Holds the Snapshots pane instance. */
    UISnapshotPane      *m_pPaneSnapshots;
    /** Holds the Logviewer pane instance. */
    UIVMLogViewerWidget *m_pPaneLogViewer;

    /** Holds whether this pane is active. */
    bool  m_fActive;

    /** Holds the cache of passed items. */
    QList<UIVirtualMachineItem*>  m_items;
    /** Holds the cache of passed machine. */
    CMachine                      m_comMachine;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIToolPaneMachine_h */

