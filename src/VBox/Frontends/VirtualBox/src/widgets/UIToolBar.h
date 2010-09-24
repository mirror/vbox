/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIToolBar class declaration & implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIToolBar_h___
#define ___UIToolBar_h___

/* Global includes */
#include <QToolBar>

/* Global forward declarations */
class QMainWindow;

/**
 *  The UIToolBar class is a simple QToolBar reimplementation to disable
 *  its built-in context menu and add some default behavior we need.
 */
class UIToolBar : public QToolBar
{
public:

    UIToolBar(QWidget *pParent);

#ifdef Q_WS_MAC
    void setMacToolbar();
    void setShowToolBarButton(bool fShow);
#endif /* Q_WS_MAC */

    void updateLayout();
    void setUsesTextLabel(bool fEnable);

private:

    /* Private member vars */
    QMainWindow *m_pMainWindow;
};

#endif // !___UIToolBar_h___

