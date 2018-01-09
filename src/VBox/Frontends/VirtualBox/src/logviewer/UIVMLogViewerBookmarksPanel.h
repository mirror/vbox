/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIVMLogViewerBookmarksPanel_h___
#define ___UIVMLogViewerBookmarksPanel_h___

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class UIVMFilterLineEdit;
class UIMiniCancelButton;
class UIVMLogViewerWidget;


/** QWidget extension
  * providing GUI for bookmark management. Show a list of bookmarks currently set
  for displayed log page. It has controls to navigate and clear bookmarks. */
class UIVMLogViewerBookmarksPanel : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

public:

    UIVMLogViewerBookmarksPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer);

    /* Adds a single bookmark to an existing list of bookmarks. Possibly called
       by UIVMLogViewerWidget when user adds a bookmark thru context menu etc. */
    void addBookmark(const QPair<int, QString> &newBookmark);
    /* Clear the bookmark list and show this list instead. Probably done after
       user switches to another log page tab etc. */
    void setBookmarksList(const QVector<QPair<int, QString> > &bookmarkList);
    void update();
    /* @a index is the index of the curent bookmark. */
    void setBookmarkIndex(int index);

public slots:


private slots:


private:

    void prepare();
    void prepareWidgets();
    void prepareConnections();

    /** Handles the translation event. */
    void retranslateUi();

    /** Handles Qt @a pEvent, used for keyboard processing. */
    bool eventFilter(QObject *pObject, QEvent *pEvent);
    /** Handles the Qt show @a pEvent. */
    void showEvent(QShowEvent *pEvent);
    /** Handles the Qt hide @a pEvent. */
    void hideEvent(QHideEvent *pEvent);

    const int m_iMaxBookmarkTextLength;
    /** Holds the reference to VM Log-Viewer this panel belongs to. */
    UIVMLogViewerWidget *m_pViewer;
    /** Holds the instance of main-layout we create. */
    QHBoxLayout         *m_pMainLayout;
    /** Holds the instance of close-button we create. */
    UIMiniCancelButton  *m_pCloseButton;
    QComboBox           *m_pBookmarksComboBox;
    QPushButton         *m_clearAllButton;
    QPushButton         *m_clearCurrentButton;
};

#endif /* !___UIVMLogViewerBookmarksPanel_h___ */
