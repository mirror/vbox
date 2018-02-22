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
#include "CGuest.h"

/* Forward declarations: */
class QVBoxLayout;
class QSplitter;


/** QWidget extension
  * providing GUI with guest session information and control tab in session-information window. */
class UIGuestControlFileManager : public QWidget
{
    Q_OBJECT;

public:

    UIGuestControlFileManager(QWidget *pParent, const CGuest &comGuest);

private:

    void prepareObjects();
    void prepareConnections();

    CGuest         m_comGuest;
    QVBoxLayout   *m_pMainLayout;
    QSplitter     *m_pSplitter;
    QWidget       *m_pSessionCreateWidget;
};

#endif /* !___UIGuestControlFileManager_h___ */
