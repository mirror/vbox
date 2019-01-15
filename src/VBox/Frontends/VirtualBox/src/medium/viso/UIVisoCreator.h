/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoCreator class declaration.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_medium_viso_UIVisoCreator_h
#define FEQT_INCLUDED_SRC_medium_viso_UIVisoCreator_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif



/* Qt includes: */


/* Qt includes: */
#include <QModelIndex>

/* GUI includes: */
#include "QIMainDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIVisoCreatorDefs.h"

/* Forward declarations: */
class QItemSelection;
class QMenu;
class QSplitter;
class QVBoxLayout;
class QIDialogButtonBox;
class UIActionPool;
class UIToolBar;
class UIVisoHostBrowser;
class UIVisoContentBrowser;

class SHARED_LIBRARY_STUFF UIVisoCreator : public QIWithRetranslateUI<QIMainDialog>
{
    Q_OBJECT;

public:

    UIVisoCreator(QWidget *pParent = 0, const QString& strMachineName = QString());
    ~UIVisoCreator();
    QStringList entryList() const;
    const QString &visoName() const;
    const QStringList &customOptions() const;

#ifdef VBOX_WS_MAC
    /** Returns the toolbar. */
    UIToolBar *toolbar() const { return m_pToolBar; }
#endif

protected:

    void retranslateUi();

private slots:

    void sltHandleAddObjectsToViso(QStringList pathList);
    void sltHandleOptionsAction();
    void sltHandleConfigurationAction();

 private:

    void prepareObjects();
    void prepareConnections();
    void prepareActions();
    /** Set the root index of the m_pTableModel to the current index of m_pTreeModel. */
    void setTableRootIndex(QModelIndex index = QModelIndex() );
    void setTreeCurrentIndex(QModelIndex index = QModelIndex() );
    void checkBrowserOptions(const BrowserOptions &browserOptions);
    void checkVisoOptions(const VisoOptions &visoOptions);

    QVBoxLayout          *m_pMainLayout;
    QSplitter            *m_pVerticalSplitter;
    UIVisoHostBrowser    *m_pHostBrowser;
    UIVisoContentBrowser *m_pVisoBrowser;
    QIDialogButtonBox    *m_pButtonBox;
    UIToolBar            *m_pToolBar;
    QAction              *m_pActionConfiguration;
    QAction              *m_pActionOptions;
    VisoOptions           m_visoOptions;
    BrowserOptions        m_browserOptions;
    QWidget              *m_pCentralWidget;
    QMenu                *m_pMainMenu;
    QMenu                *m_pHostBrowserMenu;
    QMenu                *m_pVisoContentBrowserMenu;
    QString               m_strMachineName;
};

#endif /* !FEQT_INCLUDED_SRC_medium_viso_UIVisoCreator_h */
