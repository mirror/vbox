/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolPaneGlobal class declaration.
 */

/*
 * Copyright (C) 2017-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIToolPaneGlobal_h___
#define ___UIToolPaneGlobal_h___

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class QHBoxLayout;
class QStackedLayout;
class QVBoxLayout;
class UIActionPool;
class UIWelcomePane;
class UIHostNetworkManagerWidget;
class UIMediumManagerWidget;
class UIVirtualMachineItem;
class CMachine;


/** QWidget subclass representing container for tool panes. */
class UIToolPaneGlobal : public QWidget
{
    Q_OBJECT;

public:

    /** Constructs tools pane passing @a pParent to the base-class. */
    UIToolPaneGlobal(UIActionPool *pActionPool, QWidget *pParent = 0);
    /** Destructs tools pane. */
    virtual ~UIToolPaneGlobal() /* override */;

    /** Returns type of tool currently opened. */
    ToolTypeGlobal currentTool() const;
    /** Returns whether tool of particular @a enmType is opened. */
    bool isToolOpened(ToolTypeGlobal enmType) const;
    /** Activates tool of passed @a enmType, creates new one if necessary. */
    void openTool(ToolTypeGlobal enmType);
    /** Closes tool of passed @a enmType, deletes one if exists. */
    void closeTool(ToolTypeGlobal enmType);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares stacked-layout. */
    void prepareStackedLayout();
    /** Cleanups all. */
    void cleanup();

    /** Holds the action pool reference. */
    UIActionPool *m_pActionPool;

    /** Holds the stacked-layout instance. */
    QStackedLayout             *m_pLayout;
    /** Holds the Welcome pane instance. */
    UIWelcomePane              *m_pPaneWelcome;
    /** Holds the Virtual Media Manager instance. */
    UIMediumManagerWidget      *m_pPaneMedia;
    /** Holds the Host Network Manager instance. */
    UIHostNetworkManagerWidget *m_pPaneNetwork;
};

#endif /* !___UIToolPaneGlobal_h___ */

