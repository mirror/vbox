/* $Id$ */
/** @file
 * VBox Qt GUI - UIGInformation class declaration.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGInformation_h__
#define __UIGInformation_h__

/* Qt includes: */
#include <QWidget>

/* Forward declartions: */
class QVBoxLayout;
class UIGInformationModel;
class UIGInformationView;
class UIVMItem;

/* Details widget: */
class UIGInformation : public QWidget
{
    Q_OBJECT;

signals:

    /* Notifier: Sliding stuff: */
    void sigSlidingStarted();

    /* Notifiers: Toggle stuff: */
    void sigToggleStarted();
    void sigToggleFinished();

public:

    /* Constructor: */
    UIGInformation(QWidget *pParent);

    /* API: Current item(s) stuff: */
    void setItems(const QList<UIVMItem*> &items);

private:

    /* Helpers: Prepare stuff: */
    void preparePalette();
    void prepareLayout();
    void prepareModel();
    void prepareView();
    void prepareConnections();

    /* Variables: */
    QVBoxLayout *m_pMainLayout;
    UIGInformationModel *m_pDetailsModel;
    UIGInformationView *m_pDetailsView;
};

#endif /* __UIGInformation_h__ */

