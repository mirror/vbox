/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIVMDesktop class declarations
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

#ifndef __UIVMDesktop_h__
#define __UIVMDesktop_h__

/* Qt includes: */
#include <QWidget>

/* Forward declarations: */
class CMachine;
class UIVMDesktopPrivate;
class UIVMItem;
class UIToolBar;
class QStackedLayout;

/* Class representing widget which contains three panes:
 * 1. Information pane reflecting base information about VirtualBox,
 * 2. Inaccessible machine pane reflecting information about
 *    currently chosen inaccessible VM and allowing to operate over it,
 * 3. Snapshot pane allowing to operate over the snapshots. */
class UIVMDesktop: public QWidget
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIVMDesktop(QAction *pRefreshAction, QWidget *pParent);

    /* Helpers: Update stuff: */
    void updateDetailsText(const QString &strText);
    void updateDetailsError(const QString &strError);

private:

    /* Variables: */
    UIVMDesktopPrivate *m_pDesktopPrivate;
};

#endif /* !__UIVMDesktop_h__ */

