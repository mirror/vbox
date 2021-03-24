/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolBox class declaration.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_UIToolBox_h
#define FEQT_INCLUDED_SRC_widgets_UIToolBox_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QFrame>
#include <QMap>

/* Local includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QVBoxLayout;
class QLabel;
class UIToolBoxPage;

/** A Qframe extension with similar API and functionality like QToolBox. I needed some for
  * flexibility (like a second icon at the right hand side of the title etc.). */
class  SHARED_LIBRARY_STUFF UIToolBox : public QIWithRetranslateUI<QFrame>
{

    Q_OBJECT;

signals:


public:

    UIToolBox(QWidget *pParent = 0);
    bool insertPage(int iIndex, QWidget *pWidget, const QString &strTitle, bool fAddEnableCheckBox = false);
    void setPageEnabled(int iIndex, bool fEnabled);
    void setPageTitle(int iIndex, const QString &strTitle);
    void setPageTitleIcon(int iIndex, const QIcon &icon, const QString &strIconToolTip = QString());
    void setCurrentPage(int iIndex);

protected:

    virtual void retranslateUi() /* override */;

private slots:

    void sltHandleShowPageWidget();

private:

    void prepare();

    QVBoxLayout *m_pMainLayout;
    QMap<int, UIToolBoxPage*> m_pages;
    int m_iCurrentPageIndex;
    int m_iPageCount;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_UIToolBox_h */
