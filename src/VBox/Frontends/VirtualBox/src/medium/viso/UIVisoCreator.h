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
class UIVisoCreatorOptionsPanel;
class UIVisoCreatorPanel;
class UIVisoConfigurationPanel;

class UIVisoCreator : public QIWithRetranslateUI<QIMainDialog>
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
    void sltPanelActionToggled(bool fChecked);
    void sltHandleVisoNameChanged(const QString& strVisoName);
    void sltHandleCustomVisoOptionsChanged(const QStringList &customVisoOptions);
    void sltHandleShowHiddenObjectsChange(bool fShow);
    void sltHandleHidePanel(UIVisoCreatorPanel *pPanel);

private:
    struct VisoOptions
    {
        VisoOptions()
            :m_strVisoName("ad-hoc-viso"){}
        QString m_strVisoName;
        /** Additions viso options to be inserted to the viso file as separate lines. */
        QStringList m_customOptions;
    };

    struct BrowserOptions
    {
        BrowserOptions()
            :m_fShowHiddenObjects(true){}
        bool m_fShowHiddenObjects;
    };

    void prepareObjects();
    void prepareConnections();
    void prepareActions();
    /** Set the root index of the m_pTableModel to the current index of m_pTreeModel. */
    void setTableRootIndex(QModelIndex index = QModelIndex() );
    void setTreeCurrentIndex(QModelIndex index = QModelIndex() );
    void hidePanel(UIVisoCreatorPanel *panel);
    void showPanel(UIVisoCreatorPanel *panel);
    /** Makes sure escape key is assigned to only a single widget. This is done by checking
        several things in the following order:
        - when there are no more panels visible assign it to the parent dialog
        - grab it from the dialog as soon as a panel becomes visible again
        - assign it to the most recently "unhidden" panel */
    void manageEscapeShortCut();

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
    UIVisoCreatorOptionsPanel *m_pCreatorOptionsPanel;
    UIVisoConfigurationPanel  *m_pConfigurationPanel;
    QMap<UIVisoCreatorPanel*, QAction*> m_panelActionMap;
    QList<UIVisoCreatorPanel*>          m_visiblePanelsList;
};

#endif /* !FEQT_INCLUDED_SRC_medium_viso_UIVisoCreator_h */
