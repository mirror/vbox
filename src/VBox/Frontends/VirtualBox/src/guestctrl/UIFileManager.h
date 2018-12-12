/* $Id$ */
/** @file
 * VBox Qt GUI - UIFileManager class declaration.
 */

/*
 * Copyright (C) 2016-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIFileManager_h___
#define ___UIFileManager_h___

/* Qt includes: */
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CEventListener.h"
#include "CEventSource.h"
#include "CGuest.h"
#include "CGuestSession.h"

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIGuestControlDefs.h"
#include "UIMainEventListener.h"

/* Forward declarations: */
class QHBoxLayout;
class QSplitter;
class QTableWidget;
class QTextEdit;
class QVBoxLayout;
class QITabWidget;
class CGuestSessionStateChangedEvent;
class UIActionPool;
class UIFileOperationsList;
class UIGuestControlConsole;
class UIGuestControlInterface;
class UIFileManagerPanel;
class UIFileManagerLogPanel;
class UIFileManagerOperationsPanel;
class UIFileManagerSessionPanel;
class UIFileManagerOptionsPanel;
class UIFileManagerGuestTable;
class UIFileManagerHostTable;
class UIGuestSessionCreateWidget;
class UIToolBar;

/** A Utility class to manage file  manager options. */
class UIFileManagerOptions
{

public:

    static UIFileManagerOptions* instance();
    static void create();
    static void destroy();

    bool bListDirectoriesOnTop;
    bool bAskDeleteConfirmation;
    bool bShowHumanReadableSizes;

private:

    UIFileManagerOptions();
    ~UIFileManagerOptions();

    static UIFileManagerOptions *m_pInstance;
};

/** A QWidget extension. it includes a QWidget extension for initiating a guest session
 *  one host and one guest file table views, a log viewer
 *  and some other file manager related widgets. */
class UIFileManager : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    UIFileManager(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                              const CGuest &comGuest, QWidget *pParent, bool fShowToolbar = true);
    ~UIFileManager();
    QMenu *menu() const;

#ifdef VBOX_WS_MAC
    /** Returns the toolbar. */
    UIToolBar *toolbar() const { return m_pToolBar; }
#endif

signals:

    void sigSetCloseButtonShortCut(QKeySequence);

protected:

    void retranslateUi();

private slots:

    void sltGuestSessionUnregistered(CGuestSession guestSession);
    void sltCreateSession(QString strUserName, QString strPassword);
    void sltCloseSession();
    void sltGuestSessionStateChanged(const CGuestSessionStateChangedEvent &cEvent);
    void sltReceieveLogOutput(QString strOutput, FileManagerLogType eLogType);
    void sltCopyGuestToHost();
    void sltCopyHostToGuest();
    void sltPanelActionToggled(bool fChecked);
    void sltListDirectoriesBeforeChanged();
    void sltReceieveNewFileOperation(const CProgress &comProgress);
    void sltFileOperationComplete(QUuid progressId);
    /** Performs whatever necessary when some signal about option change has been receieved. */
    void sltHandleOptionsUpdated();

    void sltTestCopy();
    void sltTestSession();

private:

    void prepareObjects();
    void prepareGuestListener();
    void prepareConnections();
    void prepareVerticalToolBar(QHBoxLayout *layout);
    void prepareToolBar();
    bool createSession(const QString& strUserName, const QString& strPassword,
                       const QString& strDomain = QString() /* not used currently */);

    void prepareListener(ComObjPtr<UIMainEventListenerImpl> &Qtistener,
                         CEventListener &comEventListener,
                         CEventSource comEventSource, QVector<KVBoxEventType>& eventTypes);

    void cleanupListener(ComObjPtr<UIMainEventListenerImpl> &QtListener,
                         CEventListener &comEventListener,
                         CEventSource comEventSource);

    void initFileTable();
    /** @name Perform operations needed after creating/ending a guest control session
      * @{ */
        void postSessionCreated();
        void postSessionClosed();
    /** @} */

    /** Saves list of panels and file manager options to the extra data. */
    void saveOptions();
    /** Show the panels that have been visible the last time file manager is closed. */
    void restorePanelVisibility();
    /** Loads file manager options. This should be done before widget creation
     *  since some widgets are initilized with these options */
    void loadOptions();
    void hidePanel(UIFileManagerPanel *panel);
    void showPanel(UIFileManagerPanel *panel);
    /** Makes sure escape key is assigned to only a single widget. This is done by checking
        several things in the following order:
        - when there are no more panels visible assign it to the parent dialog
        - grab it from the dialog as soon as a panel becomes visible again
        - assign it to the most recently "unhidden" panel */
    void manageEscapeShortCut();
    void copyToGuest();
    void copyToHost();
    template<typename T>
    QStringList               getFsObjInfoStringList(const T &fsObjectInfo) const;
    void                      appendLog(const QString &strLog, FileManagerLogType eLogType);
    CGuest                    m_comGuest;
    CGuestSession             m_comGuestSession;
    QVBoxLayout              *m_pMainLayout;
    QSplitter                *m_pVerticalSplitter;
    UIToolBar                *m_pToolBar;
    UIToolBar                *m_pVerticalToolBar;

    UIGuestControlConsole    *m_pConsole;
    UIGuestControlInterface  *m_pControlInterface;
    UIFileManagerGuestTable         *m_pGuestFileTable;
    UIFileManagerHostTable   *m_pHostFileTable;

    ComObjPtr<UIMainEventListenerImpl> m_pQtGuestListener;
    ComObjPtr<UIMainEventListenerImpl> m_pQtSessionListener;
    CEventListener m_comSessionListener;
    CEventListener m_comGuestListener;
    const EmbedTo  m_enmEmbedding;
    UIActionPool  *m_pActionPool;
    const bool     m_fShowToolbar;
    QMap<UIFileManagerPanel*, QAction*> m_panelActionMap;
    QList<UIFileManagerPanel*>          m_visiblePanelsList;
    UIFileManagerOptionsPanel          *m_pOptionsPanel;
    UIFileManagerLogPanel              *m_pLogPanel;
    UIFileManagerSessionPanel          *m_pSessionPanel;
    UIFileManagerOperationsPanel       *m_pOperationsPanel;
    friend class UIFileManagerOptionsPanel;
    friend class UIFileManagerPanel;
    friend class UIFileManagerDialog;
};

#endif /* !___UIFileManager_h___ */
