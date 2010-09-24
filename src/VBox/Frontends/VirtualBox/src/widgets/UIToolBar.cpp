/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIToolBar class implementation
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

/* Local includes */
#include "UIToolBar.h"
#ifdef Q_WS_MAC
# include "VBoxUtils.h"
#endif

/* Global includes */
#include <QLayout>
#include <QMainWindow>
/* Note: This styles are available on _all_ platforms. */
#include <QCleanlooksStyle>
#include <QWindowsStyle>

UIToolBar::UIToolBar(QWidget *pParent)
    : QToolBar(pParent)
    , m_pMainWindow(qobject_cast <QMainWindow*>(pParent))
{
    setFloatable(false);
    setMovable(false);

    /* Remove that ugly frame panel around the toolbar.
     * Doing that currently for Cleanlooks & Windows styles. */
    if (qobject_cast <QCleanlooksStyle*>(QToolBar::style()) ||
        qobject_cast <QWindowsStyle*>(QToolBar::style()))
        setStyleSheet("QToolBar { border: 0px none black; }");

    if (layout())
        layout()->setContentsMargins(0, 0, 0, 0);;

    setContextMenuPolicy(Qt::NoContextMenu);
}

#ifdef Q_WS_MAC
void UIToolBar::setMacToolbar()
{
    if (m_pMainWindow)
        m_pMainWindow->setUnifiedTitleAndToolBarOnMac(true);
}

void UIToolBar::setShowToolBarButton(bool fShow)
{
    ::darwinSetShowsToolbarButton(this, fShow);
}
#endif /* Q_WS_MAC */

void UIToolBar::updateLayout()
{
#ifdef Q_WS_MAC
    /* There is a bug in Qt Cocoa which result in showing a "more arrow" when
       the necessary size of the toolbar is increased. Also for some languages
       the with doesn't match if the text increase. So manually adjust the size
       after changing the text. */
    QSizePolicy sp = sizePolicy();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    adjustSize();
    setSizePolicy(sp);
    layout()->invalidate();
    layout()->activate();
#endif /* Q_WS_MAC */
}

void UIToolBar::setUsesTextLabel(bool fEnable)
{
    Qt::ToolButtonStyle tbs = Qt::ToolButtonTextUnderIcon;
    if (!fEnable)
        tbs = Qt::ToolButtonIconOnly;

    if (m_pMainWindow)
        m_pMainWindow->setToolButtonStyle(tbs);
    else
        setToolButtonStyle(tbs);
}

