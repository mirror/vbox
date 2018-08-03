/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooser class declaration.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIChooser_h__
#define __UIChooser_h__

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "UIChooserItem.h"

/* Forward declartions: */
class UIVirtualMachineItem;
class QVBoxLayout;
class UIVirtualBoxManagerWidget;
class UIActionPool;
class UIChooserModel;
class UIChooserView;
class QStatusBar;

/* Graphics selector widget: */
class UIChooser : public QWidget
{
    Q_OBJECT;

signals:

    /* Notifier: Selection change: */
    void sigSelectionChanged();

    /* Notifier: Sliding stuff: */
    void sigSlidingStarted();

    /* Notifiers: Toggle stuff: */
    void sigToggleStarted();
    void sigToggleFinished();

    /* Notifier: Group-saving stuff: */
    void sigGroupSavingStateChanged();

public:

    /* Constructor/destructor: */
    UIChooser(UIVirtualBoxManagerWidget *pParent);
    ~UIChooser();

    /** Returns the manager-widget reference. */
    UIVirtualBoxManagerWidget *managerWidget() const { return m_pManagerWidget; }
    /** Returns the action-pool reference. */
    UIActionPool* actionPool() const;

    /** Return the Chooser-model instance. */
    UIChooserModel *model() const { return m_pChooserModel; }
    /** Return the Chooser-view instance. */
    UIChooserView *view() const { return m_pChooserView; }

    /* API: Current-item stuff: */
    UIVirtualMachineItem* currentItem() const;
    QList<UIVirtualMachineItem*> currentItems() const;
    bool isSingleGroupSelected() const;
    bool isAllItemsOfOneGroupSelected() const;

    /* API: Group-saving stuff: */
    bool isGroupSavingInProgress() const;

private:

    /* Helpers: Prepare stuff: */
    void preparePalette();
    void prepareLayout();
    void prepareModel();
    void prepareView();
    void prepareConnections();
    void load();

    /* Helper: Cleanup stuff: */
    void save();

    /** Holds the manager-widget reference. */
    UIVirtualBoxManagerWidget *m_pManagerWidget;

    /* Variables: */
    QVBoxLayout *m_pMainLayout;
    UIChooserModel *m_pChooserModel;
    UIChooserView *m_pChooserView;
};

#endif /* __UIChooser_h__ */

