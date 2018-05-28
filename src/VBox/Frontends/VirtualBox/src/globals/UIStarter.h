/* $Id$ */
/** @file
 * VBox Qt GUI - UIStarter class declaration.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIStarter_h___
#define ___UIStarter_h___

/* Qt includes: */
#include <QObject>

/** QObject subclass allowing to control GUI part
  * of VirtualBox application in sync/async modes. */
class UIStarter : public QObject
{
    Q_OBJECT;

    /** Constructs UI starter. */
    UIStarter();
    /** Destructs UI starter. */
    virtual ~UIStarter() /* override */;

public:

    /** Returns the singleton UI starter instance. */
    static UIStarter *instance() { return s_pInstance; }

    /** Create the singleton UI starter instance. */
    static void create();
    /** Create the singleton UI starter instance. */
    static void destroy();

    /** Init VBoxGlobal connections. */
    void init();
    /** Deinit VBoxGlobal connections. */
    void deinit();

private slots:

    /** Prepares everything. */
    void prepare();

    /** Starts corresponding part of the UI. */
    void sltStartUI();
    /** Restarts corresponding part of the UI. */
    void sltRestartUI();

    /** Cleanups everything. */
    void cleanup();

    /** Opens URLs in Selector UI. */
    void sltOpenURLs();

    /** Handles commit data request. */
    void sltHandleCommitDataRequest();

private:

    /** Holds the singleton UI starter instance. */
    static UIStarter *s_pInstance;
};

/** Singleton UI starter 'official' name. */
#define gStarter UIStarter::instance()

#endif /* !___UIStarter_h___ */
