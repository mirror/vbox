/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIToolBar class declaration
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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

/* Qt includes: */
#include <QToolBar>

/* Forward declarations: */
class QMainWindow;

/* UI tool-bar prototype class: */
class UIToolBar : public QToolBar
{
public:

    /* Constructor: */
    UIToolBar(QWidget *pParent = 0);

    /* API: Text-label stuff: */
    void setUsesTextLabel(bool fEnable);

#ifdef Q_WS_MAC
    /* API: Mac toolbar stuff: */
    void setMacToolbar();
    void setShowToolBarButton(bool fShow);
    void updateLayout();
#endif /* Q_WS_MAC */

private:

    /* Variables: */
    QMainWindow *m_pMainWindow;
};

#endif /* !___UIToolBar_h___ */
