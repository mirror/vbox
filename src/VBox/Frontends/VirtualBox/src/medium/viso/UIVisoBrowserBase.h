/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoBrowserBase class declaration.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef FEQT_INCLUDED_SRC_medium_viso_UIVisoBrowserBase_h
#define FEQT_INCLUDED_SRC_medium_viso_UIVisoBrowserBase_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QModelIndex>
#include <QPointer>
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QItemSelection;
class QGridLayout;
class QLabel;
class QIToolBar;
class UIActionPool;
class UIFileTableNavigationWidget;

/** An abstract QWidget extension hosting a toolbar, a navigation widget, and table view. */
class UIVisoBrowserBase : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    ////void sigCreateFileTableViewContextMenu(QWidget *pMenuRequester, const QPoint &point);

public:

    UIVisoBrowserBase(UIActionPool *pActionPool, QWidget *pParent = 0);
    ~UIVisoBrowserBase();
    virtual void showHideHiddenObjects(bool bShow) = 0;

public slots:

    void sltTableViewItemDoubleClick(const QModelIndex &index);

protected:

    void prepareObjects();
    void prepareConnections();

    virtual void tableViewItemDoubleClick(const QModelIndex &index) = 0;
    virtual void setTableRootIndex(QModelIndex index = QModelIndex()) = 0;
    virtual void setPathFromNavigationWidget(const QString &strPath) = 0;

    virtual void resizeEvent(QResizeEvent *pEvent) RT_OVERRIDE;
    virtual bool eventFilter(QObject *pObj, QEvent *pEvent) RT_OVERRIDE;
    virtual void keyPressEvent(QKeyEvent *pEvent) RT_OVERRIDE;
    void updateNavigationWidgetPath(const QString &strPath);
    void setFileTableLabelText(const QString &strText);
    void enableForwardBackwardActions();

    QGridLayout        *m_pMainLayout;
    QIToolBar          *m_pToolBar;
    QPointer<UIActionPool> m_pActionPool;
    QAction             *m_pGoHome;
    QAction             *m_pGoUp;
    QAction             *m_pGoForward;
    QAction             *m_pGoBackward;
    QPointer<QMenu>      m_pSubMenu;

protected slots:

    void sltGoForward();
    void sltGoBackward();

private slots:

    void sltNavigationWidgetPathChange(const QString &strPath);

private:

    UIFileTableNavigationWidget *m_pNavigationWidget;
    QLabel *m_pFileTableLabel;
};

#endif /* !FEQT_INCLUDED_SRC_medium_viso_UIVisoBrowserBase_h */
