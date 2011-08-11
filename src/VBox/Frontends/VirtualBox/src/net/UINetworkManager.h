/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UINetworkManager class declaration
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UINetworkManager_h__
#define __UINetworkManager_h__

/* Global includes: */
#include <QNetworkAccessManager>

/* QNetworkAccessManager class reimplementation providing
 * network access for the VirtualBox application purposes. */
class UINetworkManager : public QNetworkAccessManager
{
    Q_OBJECT;

public:

    /* Instance: */
    static UINetworkManager* instance() { return m_pInstance; }

    /* Create/destroy singleton: */
    static void create();
    static void destroy();

private:

    /* Constructor/destructor: */
    UINetworkManager();
    ~UINetworkManager();

    /* Prepare/cleanup: */
    void prepare();
    void cleanup();

    /* Instance: */
    static UINetworkManager *m_pInstance;
};

#define gNetworkManager UINetworkManager::instance()

#endif // __UINetworkManager_h__
