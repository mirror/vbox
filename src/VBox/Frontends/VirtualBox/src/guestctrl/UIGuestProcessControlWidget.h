/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestProcessControlWidget class declaration.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_guestctrl_UIGuestProcessControlWidget_h
#define FEQT_INCLUDED_SRC_guestctrl_UIGuestProcessControlWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CGuest.h"
#include "CEventListener.h"

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIMainEventListener.h"

/* Forward declarations: */
class QITreeWidget;
class QVBoxLayout;
class QSplitter;
class UIActionPool;
class UIGuestControlConsole;
class UIGuestControlInterface;
class UIGuestSessionsEventHandler;
class UIGuestControlTreeWidget;
class UIToolBar;

/** QWidget extension
  * providing GUI with guest session information and control tab in session-information window. */
class UIGuestProcessControlWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    UIGuestProcessControlWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                const CGuest &comGuest, QWidget *pParent, bool fShowToolbar = false);
    ~UIGuestProcessControlWidget();

protected:

    void retranslateUi();

private slots:

    void sltGuestSessionsUpdated();
    void sltConsoleCommandEntered(const QString &strCommand);
    void sltConsoleOutputReceived(const QString &strOutput);

    void sltGuestSessionRegistered(CGuestSession guestSession);
    void sltGuestSessionUnregistered(CGuestSession guestSession);
    void sltGuestControlErrorText(QString strError);

    void sltTreeItemUpdated();
    void sltCloseSessionOrProcess();

private:

    void prepareObjects();
    void prepareConnections();
    void prepareToolBar();
    void prepareListener();
    void initGuestSessionTree();
    void updateTreeWidget();
    void cleanupListener();
    void addGuestSession(CGuestSession guestSession);
    void saveSettings();
    void loadSettings();

    CGuest                    m_comGuest;
    QVBoxLayout              *m_pMainLayout;
    QSplitter                *m_pSplitter;
    UIGuestControlTreeWidget *m_pTreeWidget;
    UIGuestControlConsole    *m_pConsole;
    UIGuestControlInterface  *m_pControlInterface;
    const EmbedTo             m_enmEmbedding;
    UIActionPool             *m_pActionPool;
    UIToolBar                *m_pToolBar;

    /** Holds the Qt event listener instance. */
    ComObjPtr<UIMainEventListenerImpl> m_pQtListener;
    /** Holds the COM event listener instance. */
    CEventListener m_comEventListener;
    const bool     m_fShowToolbar;
};

#endif /* !FEQT_INCLUDED_SRC_guestctrl_UIGuestProcessControlWidget_h */
