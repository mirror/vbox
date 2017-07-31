/* $Id$ */
/** @file
 * VBox Qt GUI - UIDesktopPane class declaration.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIDesktopPane_h___
#define ___UIDesktopPane_h___

/* Qt includes: */
#include <QWidget>

/* Forward declarations: */
class UIDesktopPanePrivate;


/** QWidget subclass representing container which have two panes:
  * 1. Text details pane reflecting base information about VirtualBox,
  * 2. Error details pane reflecting information about currently chosen
  *    inaccessible VM and allowing to operate over it. */
class UIDesktopPane : public QWidget
{
    Q_OBJECT;

public:

    /** Constructs desktop pane passing @a pParent to the base-class.
      * @param  pRefreshAction  Brings the refresh action reference. */
    UIDesktopPane(QAction *pRefreshAction = 0, QWidget *pParent = 0);

    /** Updates @a strText details and switches to text details pane. */
    void updateDetailsText(const QString &strText);
    /** Updates @a strError details and switches to error details pane. */
    void updateDetailsError(const QString &strError);

private:

    /** Holds the private desktop pane instance. */
    UIDesktopPanePrivate *m_pDesktopPrivate;
};

#endif /* !___UIDesktopPane_h___ */

