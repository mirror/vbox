/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooser class declaration
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGChooser_h__
#define __UIGChooser_h__

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "UIGChooserItem.h"

/* Forward declartions: */
class UIVMItem;
class QVBoxLayout;
class UIGChooserModel;
class UIGChooserView;
class QStatusBar;

/* Graphics selector widget: */
class UIGChooser : public QWidget
{
    Q_OBJECT;

signals:

    /* Notifier: Selection change: */
    void sigSelectionChanged();

    /* Notifier: Sliding start: */
    void sigSlidingStarted();

    /* Notifiers: Group saving stuff: */
    void sigGroupSavingStarted();
    void sigGroupSavingFinished();

public:

    /* Constructor/destructor: */
    UIGChooser(QWidget *pParent);
    ~UIGChooser();

    /* API: Current item stuff: */
    void setCurrentItem(int iCurrentItemIndex);
    UIVMItem* currentItem() const;
    QList<UIVMItem*> currentItems() const;
    bool singleGroupSelected() const;

    /* API: Status bar stuff: */
    void setStatusBar(QStatusBar *pStatusBar);

    /* API: Group saving stuff: */
    bool isGroupSavingInProgress() const;

private:

    /* Helpers: */
    void prepareConnections();

    /* Variables: */
    QVBoxLayout *m_pMainLayout;
    UIGChooserModel *m_pChooserModel;
    UIGChooserView *m_pChooserView;
    QStatusBar *m_pStatusBar;
};

#endif /* __UIGChooser_h__ */

