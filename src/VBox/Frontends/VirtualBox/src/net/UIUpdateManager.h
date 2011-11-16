/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIUpdateManager class declaration
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIUpdateManager_h__
#define __UIUpdateManager_h__

/* Global includes: */
#include <QUrl>

/* Local includes: */
#include "UIUpdateDefs.h"

/* Forward declarations: */
class QEventLoop;

/* Singleton to check for the new VirtualBox version.
 * Performs update of required parts if necessary. */
class UIUpdateManager : public QObject
{
    Q_OBJECT;

public:

    /* Schedule manager: */
    static void schedule();
    /* Shutdown manager: */
    static void shutdown();
    /* Manager instance: */
    static UIUpdateManager* instance() { return m_pInstance; }

public slots:

    /* Force call for new version check: */
    void sltForceCheck();

private slots:

    /* Slot to check if update is necessary: */
    void sltCheckIfUpdateIsNecessary(bool fForceCall = false);

    /* Handle downloaded extension pack: */
    void sltHandleDownloadedExtensionPack(const QString &strSource, const QString &strTarget);

private:

    /* Constructor/destructor: */
    UIUpdateManager();
    ~UIUpdateManager();

    /* Helping stuff: */
    void checkIfUpdateIsNecessary(bool fForceCall);
    void checkIfUpdateIsNecessaryForExtensionPack(bool fForceCall);

    /* Variables: */
    static UIUpdateManager* m_pInstance;
    quint64 m_uTime;
};
#define gUpdateManager UIUpdateManager::instance()

/* Class to check for the new VirtualBox version: */
class UINewVersionChecker : public QObject
{
    Q_OBJECT;

public:

    /* Constructor: */
    UINewVersionChecker(bool fForceCall);

    /* Function to check if new version is available: */
    void checkForTheNewVersion();

private slots:

    /* Slot to analyze new version check reply: */
    void sltHandleCheckReply();

private:

    /* Variables: */
    QUrl m_url;
    bool m_fForceCall;
    QEventLoop *m_pLoop;
};

#endif // __UIUpdateManager_h__
