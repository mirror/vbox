/* $Id$ */
/** @file
 * VBox Qt GUI - UIConsoleEventHandler class declaration.
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

#ifndef ___UIGuestSessionsEventHandler_h___
#define ___UIGuestSessionsEventHandler_h___

/* Qt includes: */
#include <QObject>
#include "QITreeWidget.h"


/* Forward declarations: */
class CGuest;
class UIGuestSessionsEventHandlerImp;

class UIGuestSessionsEventHandler : public QObject
{
    Q_OBJECT;

signals:

    void sigGuestSessionsUpdated();

public:

    UIGuestSessionsEventHandler(QObject *parent, CGuest comGuest);
    ~UIGuestSessionsEventHandler();

    /** Returns the guest session (and processes) hierarchy as a vector of QTreeWidgetItem */
    void populateGuestSessionsTree(QITreeWidget *pTreeWidget);

protected:

private:

    UIGuestSessionsEventHandlerImp *m_pPrivateImp;
};

#endif /* !___UIConsoleEventHandler_h___ */
