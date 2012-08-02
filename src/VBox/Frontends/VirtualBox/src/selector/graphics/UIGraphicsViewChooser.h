/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGraphicsViewChooser class declaration
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

#ifndef __UIGraphicsViewChooser_h__
#define __UIGraphicsViewChooser_h__

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "UIGSelectorItem.h"

/* Forward declartions: */
class UIVMItem;
class QVBoxLayout;
class QIAdvancedToolBar;
class UIGraphicsSelectorModel;
class UIGraphicsSelectorView;
class QStatusBar;

/* Graphics selector widget: */
class UIGraphicsViewChooser : public QWidget
{
    Q_OBJECT;

signals:

    /* Notify listeners about selection changes: */
    void sigSelectionChanged();

public:

    /* Constructor: */
    UIGraphicsViewChooser(QWidget *pParent);

    /* API: Current item stuff: */
    void setCurrentItem(int iCurrentItemIndex);
    UIVMItem* currentItem() const;
    QList<UIVMItem*> currentItems() const;

    /* API: Status bar stuff: */
    void setStatusBar(QStatusBar *pStatusBar);

private slots:

    /* Handler: Action toggling: */
    void sltHandleActionToggling(int iActionIndex, bool fChecked);

private:

    /* Helpers: */
    void prepareConnections();

    /* Variables: */
    QVBoxLayout *m_pMainLayout;
    QIAdvancedToolBar *m_pToolBar;
    int m_iGroupActionIndex;
    UIGraphicsSelectorModel *m_pSelectorModel;
    UIGraphicsSelectorView *m_pSelectorView;
    QStatusBar *m_pStatusBar;
};

#endif /* __UIGraphicsViewChooser_h__ */

