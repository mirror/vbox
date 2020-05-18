/* $Id$ */
/** @file
 * VBox Qt GUI - UIErrorPane class declaration.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_UIErrorPane_h
#define FEQT_INCLUDED_SRC_manager_UIErrorPane_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* Forward declarations: */
class QTextBrowser;

/** QWidget subclass representing error pane reflecting
  * information about currently chosen inaccessible VM. */
class UIErrorPane : public QWidget
{
    Q_OBJECT;

public:

    /** Constructs error pane passing @a pParent to the base-class. */
    UIErrorPane(QWidget *pParent = 0);

    /** Defines error @a strDetails. */
    void setErrorDetails(const QString &strDetails);

private:

    /** Prepares all. */
    void prepare();

    /** Holds the text-browser instance. */
    QTextBrowser *m_pBrowserDetails;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIErrorPane_h */
