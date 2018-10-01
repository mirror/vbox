/* $Id$ */
/** @file
 * VBox Qt GUI - UIWelcomePane class declaration.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIWelcomePane_h___
#define ___UIWelcomePane_h___

/* Qt includes: */
#include <QWidget>

/* Forward declarations: */
class QIcon;
class QString;
class UIWelcomePanePrivate;


/** QWidget subclass holding Welcome information about VirtualBox. */
class UIWelcomePane : public QWidget
{
    Q_OBJECT;

public:

    /** Constructs Welcome pane passing @a pParent to the base-class. */
    UIWelcomePane(QWidget *pParent = 0);

    /** Defines a tools pane welcome @a strText. */
    void setToolsPaneText(const QString &strText);
    /** Defines a tools pane welcome @a icon. */
    void setToolsPaneIcon(const QIcon &icon);
    /** Add a tool element.
      * @param  pAction         Brings tool action reference.
      * @param  strDescription  Brings the tool description. */
    void addToolDescription(QAction *pAction, const QString &strDescription);
    /** Removes all tool elements. */
    void removeToolDescriptions();

private:

    /** Holds the private Welcome pane instance. */
    UIWelcomePanePrivate *m_pDesktopPrivate;
};

#endif /* !___UIWelcomePane_h___ */
