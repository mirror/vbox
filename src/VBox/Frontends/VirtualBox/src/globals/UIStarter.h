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

/** QObject subclass allowing to start GUI part
  * of VirtualBox application in async mode. */
class UIStarter : public QObject
{
    Q_OBJECT;

    /** Constructs UI starter. */
    UIStarter();
    /** Destructs UI starter. */
    virtual ~UIStarter() /* override */;

    /** Prepares everything. */
    void prepare();
    /** Cleanups everything. */
    void cleanup();

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

    /** Shows corresponding part of the UI. */
    void sltShowUI();
    /** Restarts corresponding part of the UI. */
    void sltRestartUI();
    /** Destroys corresponding part of the UI. */
    void sltDestroyUI();

    /** Opens URLs in Selector UI. */
    void sltOpenURLs();

private:

    /** Holds the singleton UI starter instance. */
    static UIStarter *s_pInstance;
};

/** Singleton UI starter 'official' name. */
#define gStarter UIStarter::instance()

#endif /* !___UIStarter_h___ */

