/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkManagerDialog stuff declaration.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UINetworkManagerDialog_h___
#define ___UINetworkManagerDialog_h___

/* Qt includes: */
#include <QMainWindow>
#include <QMap>
#include <QUuid>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QLabel;
class QUuid;
class QVBoxLayout;
class QIDialogButtonBox;
class UINetworkRequest;
class UINetworkRequestWidget;

/** QMainWindow reimplementation to reflect network-requests. */
class UINetworkManagerDialog : public QIWithRetranslateUI<QMainWindow>
{
    Q_OBJECT;

signals:

    /** Asks listener (network-manager) to cancel all network-requests. */
    void sigCancelNetworkRequests();

public slots:

    /** Shows the dialog, make sure its visible. */
    void showNormal();

protected:

    /** Allows creation of UINetworkManagerDialog to UINetworkManager. */
    friend class UINetworkManager;
    /** Constructs Network Manager Dialog. */
    UINetworkManagerDialog();

    /** Allows adding/removing network-request widgets to UINetworkRequest. */
    friend class UINetworkRequest;
    /** Adds network-request widget. */
    void addNetworkRequestWidget(UINetworkRequest *pNetworkRequest);
    /** Removes network-request widget. */
    void removeNetworkRequestWidget(const QUuid &uuid);

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;

    /** Handles key-press @a pEvent. */
    virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;

private slots:

    /** Handles 'Cancel All' button-press. */
    void sltHandleCancelAllButtonPress();

private:

    /** Holds the label instance. */
    QLabel                               *m_pLabel;
    /** Holds the widget layout instance. */
    QVBoxLayout                          *m_pWidgetsLayout;
    /** Holds the dialog-button-box instance. */
    QIDialogButtonBox                    *m_pButtonBox;
    /** Holds the map of the network request widget instances. */
    QMap<QUuid, UINetworkRequestWidget*>  m_widgets;
};

#endif /* !___UINetworkManagerDialog_h___ */

