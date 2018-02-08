/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationConfiguration class declaration.
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
#include "CConsole.h"

/* Forward declarations: */
class QITreeWidget;
class QVBoxLayout;
class UIGuestSessionsEventHandler;

/** QWidget extension
  * providing GUI with guest session information and control tab in session-information window. */
class UIInformationGuestSession : public QWidget
{
    Q_OBJECT;

signals:

public:

    UIInformationGuestSession(QWidget *pParent, const CConsole &console);

private slots:

    void sltGuestSessionsUpdated();

private:

    void prepareLayout();
    void prepareWidgets();
    void prepareEventHandler();
    void updateTreeWidget();

    CConsole             m_comConsole;
    QVBoxLayout         *m_pMainLayout;
    UIGuestSessionsEventHandler *m_pGuestSessionsEventHandler;
    QITreeWidget        *m_pTreeWidget;

};

#endif /* !___UIInformationGuestSession_h___ */
