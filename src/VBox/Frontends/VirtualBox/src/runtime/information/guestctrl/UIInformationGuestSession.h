/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationGuestSession class declaration.
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

#ifndef ___UIInformationGuestSession_h___
#define ___UIInformationGuestSession_h___

/* Qt includes: */
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CGuest.h"
#include "CEventListener.h"

/* GUI includes: */
#include "UIMainEventListener.h"

/* Forward declarations: */
class QITreeWidget;
class QVBoxLayout;
class QSplitter;
class UIGuestControlConsole;
class UIGuestControlInterface;
class UIGuestSessionsEventHandler;
class UIGuestControlTreeWidget;

/** QWidget extension
  * providing GUI with guest session information and control tab in session-information window. */
class UIInformationGuestSession : public QWidget
{
    Q_OBJECT;

public:

    UIInformationGuestSession(QWidget *pParent, const CGuest &comGuest);

private slots:

    void sltGuestSessionsUpdated();
    void sltConsoleCommandEntered(const QString &strCommand);
    void sltConsoleOutputReceived(const QString &strOutput);

    void sltGuestSessionRegistered(CGuestSession guestSession);
    void sltGuestSessionUnregistered(CGuestSession guestSession);

    void sltTreeItemUpdated();
    void sltCloseSessionOrProcess();

private:

    void prepareObjects();
    void prepareConnections();
    void prepareListener();
    void initGuestSessionTree();
    void updateTreeWidget();
    void cleanupListener();
    void addGuestSession(CGuestSession guestSession);

    CGuest                    m_comGuest;
    QVBoxLayout              *m_pMainLayout;
    QSplitter                *m_pSplitter;
    UIGuestControlTreeWidget *m_pTreeWidget;
    UIGuestControlConsole    *m_pConsole;
    UIGuestControlInterface  *m_pControlInterface;

    /** Holds the Qt event listener instance. */
    ComObjPtr<UIMainEventListenerImpl> m_pQtListener;
    /** Holds the COM event listener instance. */
    CEventListener m_comEventListener;
};

#endif /* !___UIInformationGuestSession_h___ */
