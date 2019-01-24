/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumSearchWidget class declaration.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_medium_UIMediumSearchWidget_h
#define FEQT_INCLUDED_SRC_medium_UIMediumSearchWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QAction;
class QLineEdit;
class QIComboBox;
class QIDialogButtonBox;
class QIToolButton;

/** QWidget extension providing a simple way to enter a earch term and search type for medium searching
 *  in virtual media manager, medium selection dialog, etc. */
class  SHARED_LIBRARY_STUFF UIMediumSearchWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigPerformSearch();
    void sigShowNextMatchingItem();
    void sigShowPreviousMatchingItem();

public:

    enum SearchType
    {
        SearchByName,
        SearchByUUID,
        SearchByMax
    };

public:

    UIMediumSearchWidget(QWidget *pParent = 0);
    SearchType searchType() const;
    QString searchTerm() const;

protected:

    void retranslateUi() /* override */;

private:

    void              prepareWidgets();
    QIComboBox       *m_pSearchComboxBox;
    QLineEdit        *m_pSearchTermLineEdit;
    QIToolButton     *m_pShowNextMatchButton;
    QIToolButton     *m_pShowPreviousMatchButton;
};

#endif /* !FEQT_INCLUDED_SRC_medium_UIMediumSearchWidget_h */
