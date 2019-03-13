/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserSearchWidget class declaration.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserSearchWidget_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserSearchWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QHBoxLayout;
class QILineEdit;

/** QWidget extension used as virtual machine search widget in the VM Chooser-pane. */
class UIChooserSearchWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigRedoSearch(const QString &strSearchTerm, int iItemSearchFlags);

public:

    UIChooserSearchWidget(QWidget *pParent);

protected:

    void showEvent(QShowEvent *pEvent);
    virtual void retranslateUi() /* override */;

public slots:

private slots:

    void sltHandleSearchTermChange(const QString &strSearchTerm);

private:

    void prepareWidgets();
    void prepareConnections();

    QILineEdit  *m_pLineEdit;
    QHBoxLayout *m_pMainLayout;
};

#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserSearchWidget_h */
