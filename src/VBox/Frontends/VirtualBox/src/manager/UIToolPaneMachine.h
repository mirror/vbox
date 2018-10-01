/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolPaneMachine class declaration.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIToolPaneMachine_h___
#define ___UIToolPaneMachine_h___

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "UIExtraDataDefs.h"

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
class CMachine;


/** QWidget subclass representing container for tool panes. */
class UIToolPaneMachine : public QWidget
{
    Q_OBJECT;

signals:

    /** Redirects signal from UIVirtualBoxManager to UIDetails. */
    void sigSlidingStarted();
    /** Redirects signal from UIVirtualBoxManager to UIDetails. */
    void sigToggleStarted();
    /** Redirects signal from UIVirtualBoxManager to UIDetails. */
    void sigToggleFinished();
    /** Redirects signal from UIDetails to UIVirtualBoxManager. */
    void sigLinkClicked(const QString &strCategory, const QString &strControl, const QString &strId);

public:

    /** Constructs tools pane passing @a pParent to the base-class. */
    UIToolPaneMachine(UIActionPool *pActionPool, QWidget *pParent = 0);
    /** Destructs tools pane. */
    virtual ~UIToolPaneMachine() /* override */;

    /** Returns type of tool currently opened. */
    ToolTypeMachine currentTool() const;
    /** Returns whether tool of particular @a enmType is opened. */
    bool isToolOpened(ToolTypeMachine enmType) const;
    /** Activates tool of passed @a enmType, creates new one if necessary. */
    void openTool(ToolTypeMachine enmType);
    /** Closes tool of passed @a enmType, deletes one if exists. */
    void closeTool(ToolTypeMachine enmType);

    /** Defines error @a strDetails and switches to Error pane. */
    void setErrorDetails(const QString &strDetails);

    /** Defines current machine @a pItem. */
    void setCurrentItem(UIVirtualMachineItem *pItem);

    /** Defines the machine @a items. */
    void setItems(const QList<UIVirtualMachineItem*> &items);

    /** Defines the @a comMachine object. */
    void setMachine(const CMachine &comMachine);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares stacked-layout. */
    void prepareStackedLayout();
    /** Cleanups all. */
    void cleanup();

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
};

#endif /* !___UIToolPaneMachine_h___ */

