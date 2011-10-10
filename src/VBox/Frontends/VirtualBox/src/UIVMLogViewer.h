/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIVMLogViewer class declaration
 */

/*
 * Copyright (C) 2008-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIVMLogViewer_h__
#define __UIVMLogViewer_h__

/* Global includes: */
#include <QMainWindow>
#include <QMap>
#include <QPair>

/* Local includes: */
#include "UIVMLogViewer.gen.h"
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"

/* Forward declarations: */
class QPushButton;
class QTextEdit;
class QITabWidget;
class UIVMLogViewer;
class UIVMLogViewerSearchPanel;

/* Typedefs: */
typedef QMap<QString, UIVMLogViewer*> VMLogViewerMap;
typedef QPair<QString, QTextEdit*> LogPage;
typedef QList<LogPage> LogBook;

/* VM Log Viewer window: */
class UIVMLogViewer : public QIWithRetranslateUI2<QMainWindow>,
                      public Ui::UIVMLogViewer
{
    Q_OBJECT;

public:

    /* Static method to create/show VM Log Viewer: */
    static void showLogViewerFor(QWidget *pParent, const CMachine &machine);

protected:

    /* Constructor/destructor: */
    UIVMLogViewer(QWidget *pParent, Qt::WindowFlags flags, const CMachine &machine);
    ~UIVMLogViewer();

private slots:

    /* Button slots: */
    void search();
    void refresh();
    bool close();
    void save();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Event handlers: */
    void showEvent(QShowEvent *aEvent);
    void keyPressEvent(QKeyEvent *pEvent);

    /* Various helpers: */
    QTextEdit* currentLogPage();
    QTextEdit* createLogPage(const QString &strPage);

    /* Array containing all VM Log Viewers: */
    static VMLogViewerMap m_viewers;

    /* VM Log Viewer variables: */
    bool m_fIsPolished;
    CMachine m_machine;
    QITabWidget *m_pViewerContainer;
    UIVMLogViewerSearchPanel *m_pSearchPanel;
    LogBook m_book;

    /* Buttons: */
    QPushButton *mBtnHelp;
    QPushButton *mBtnFind;
    QPushButton *mBtnRefresh;
    QPushButton *mBtnClose;
    QPushButton *mBtnSave;

    /* Friends: */
    friend class UIVMLogViewerSearchPanel;
};

#endif // __UIVMLogViewer_h__

