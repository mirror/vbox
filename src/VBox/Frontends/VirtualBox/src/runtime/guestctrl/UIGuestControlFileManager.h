/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileManager class declaration.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGuestControlFileManager_h___
#define ___UIGuestControlFileManager_h___

/* Qt includes: */
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CEventListener.h"
#include "CEventSource.h"
#include "CGuest.h"
#include "CGuestSession.h"

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIMainEventListener.h"

/* Forward declarations: */
class QHBoxLayout;
class QTextEdit;
class QVBoxLayout;
class QSplitter;
class QITabWidget;
class CGuestSessionStateChangedEvent;
class UIFileOperationsList;
class UIGuestControlConsole;
class UIGuestControlInterface;
class UIGuestFileTable;
class UIHostFileTable;
class UIGuestSessionCreateWidget;
class UIToolBar;

class UIGuestControlFileManagerSettings
{
public:
    static UIGuestControlFileManagerSettings* instance();
    static void create();
    static void destroy();

    bool bListDirectoriesOnTop;
private:
    UIGuestControlFileManagerSettings();


    static UIGuestControlFileManagerSettings *m_pInstance;
};

/** A QWidget extension. it includes a QWidget extension for initiating a guest session
 *  one host and one guest file table views, a log viewer
 *  and some other file manager related widgets. */
class UIGuestControlFileManager : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    UIGuestControlFileManager(QWidget *pParent, const CGuest &comGuest);
    ~UIGuestControlFileManager();
    //const UIGuestControlFileManagerSettings& settings() const;

protected:

    void retranslateUi();

private slots:

    void sltGuestSessionUnregistered(CGuestSession guestSession);
    void sltCreateSession(QString strUserName, QString strPassword);
    void sltCloseSession();
    void sltGuestSessionStateChanged(const CGuestSessionStateChangedEvent &cEvent);
    void sltReceieveLogOutput(QString strOutput);
    void sltCopyGuestToHost();
    void sltCopyHostToGuest();

private:

    void prepareObjects();
    void prepareGuestListener();
    void prepareConnections();
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
    void postSessionCreated();
    void postSessionClosed();
    void saveSettings();
    void loadSettings();

    template<typename T>
    QStringList       getFsObjInfoStringList(const T &fsObjectInfo) const;

    const int                   m_iMaxRecursionDepth;
    CGuest                      m_comGuest;
    CGuestSession               m_comGuestSession;
    QVBoxLayout                *m_pMainLayout;
    QSplitter                  *m_pVerticalSplitter;
    QTextEdit                  *m_pLogOutput;
    UIToolBar                  *m_pToolBar;
    QAction                    *m_pCopyGuestToHost;
    QAction                    *m_pCopyHostToGuest;
    QWidget                    *m_pFileTableContainerWidget;
    QHBoxLayout                *m_pFileTableContainerLayout;
    QITabWidget                *m_pTabWidget;

    UIFileOperationsList       *m_pFileOperationsList;
    UIGuestControlConsole      *m_pConsole;
    UIGuestControlInterface    *m_pControlInterface;
    /* m_pSessionCreateWidget is a QWidget extension enabling user to start/stop
     * a Guest Control session with password/username fields etc.. */
    UIGuestSessionCreateWidget *m_pSessionCreateWidget;
    UIGuestFileTable           *m_pGuestFileTable;
    UIHostFileTable            *m_pHostFileTable;

    ComObjPtr<UIMainEventListenerImpl> m_pQtGuestListener;
    ComObjPtr<UIMainEventListenerImpl> m_pQtSessionListener;
    CEventListener m_comSessionListener;
    CEventListener m_comGuestListener;
    //UIGuestControlFileManagerSettings m_settings;
};

#endif /* !___UIGuestControlFileManager_h___ */
