/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxClientEventHandler class declaration.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_globals_UIVirtualBoxClientEventHandler_h
#define FEQT_INCLUDED_SRC_globals_UIVirtualBoxClientEventHandler_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>

/* GUI includes: */
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"
#include "CMediumAttachment.h"

/* Forward declarations: */
class UIVirtualBoxClientEventHandlerProxy;

/** Singleton QObject extension providing GUI with CVirtualBoxClient event-source. */
class SHARED_LIBRARY_STUFF UIVirtualBoxClientEventHandler : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about the VBoxSVC become @a fAvailable. */
    void sigVBoxSVCAvailabilityChange(bool fAvailable);

public:

    /** Returns singleton instance. */
    static UIVirtualBoxClientEventHandler *instance();
    /** Destroys singleton instance. */
    static void destroy();

protected:

    /** Constructs VirtualBoxClient event handler. */
    UIVirtualBoxClientEventHandler();

    /** Prepares all. */
    void prepare();
    /** Prepares connections. */
    void prepareConnections();

private:

    /** Holds the singleton instance. */
    static UIVirtualBoxClientEventHandler *s_pInstance;

    /** Holds the VirtualBoxClient event proxy instance. */
    UIVirtualBoxClientEventHandlerProxy *m_pProxy;
};

/** Singleton VirtualBoxClient Event Handler 'official' name. */
#define gVBoxClientEvents UIVirtualBoxClientEventHandler::instance()

#endif /* !FEQT_INCLUDED_SRC_globals_UIVirtualBoxClientEventHandler_h */
