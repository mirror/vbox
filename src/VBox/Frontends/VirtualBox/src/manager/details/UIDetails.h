/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetails class declaration.
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

#ifndef __UIDetails_h__
#define __UIDetails_h__

/* Qt includes: */
#include <QWidget>

/* Forward declartions: */
class QVBoxLayout;
class UIDetailsModel;
class UIDetailsView;
class UIVirtualMachineItem;

/* Details widget: */
class UIDetails : public QWidget
{
    Q_OBJECT;

signals:

    /* Notifier: Link processing stuff: */
    void sigLinkClicked(const QString &strCategory, const QString &strControl, const QString &strId);

    /* Notifier: Sliding stuff: */
    void sigSlidingStarted();

    /* Notifiers: Toggle stuff: */
    void sigToggleStarted();
    void sigToggleFinished();

public:

    /* Constructor: */
    UIDetails(QWidget *pParent = 0);

    /** Return the Details-model instance. */
    UIDetailsModel *model() const { return m_pDetailsModel; }
    /** Return the Details-view instance. */
    UIDetailsView *view() const { return m_pDetailsView; }

    /* API: Current item(s) stuff: */
    void setItems(const QList<UIVirtualMachineItem*> &items);

private:

    /* Helpers: Prepare stuff: */
    void preparePalette();
    void prepareLayout();
    void prepareModel();
    void prepareView();
    void prepareConnections();

    /* Variables: */
    QVBoxLayout *m_pMainLayout;
    UIDetailsModel *m_pDetailsModel;
    UIDetailsView *m_pDetailsView;
};

#endif /* __UIDetails_h__ */

