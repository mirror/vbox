/* $Id$ */
/** @file
 * VBox Qt GUI - UIGRuntimeInformation class declaration.
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

#ifndef __UIGRuntimeInformation_h__
#define __UIGRuntimeInformation_h__

/* Qt includes: */
#include <QWidget>

/* Forward declartions: */
class QVBoxLayout;
class UIGInformationModel;
class UIGInformationView;
class UIVMItem;

/* Details widget: */
class UIGRuntimeInformation : public QWidget
{
    Q_OBJECT;

signals:

    /* Notifier: Link processing stuff: */
    //void sigLinkClicked(const QString &strCategory, const QString &strControl, const QString &strId);

    /* Notifier: Sliding stuff: */
    void sigSlidingStarted();

    /* Notifiers: Toggle stuff: */
    void sigToggleStarted();
    void sigToggleFinished();

public:

    /* Constructor: */
    UIGRuntimeInformation(QWidget *pParent);

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

#endif /* __UIGRuntimeInformation_h__ */

