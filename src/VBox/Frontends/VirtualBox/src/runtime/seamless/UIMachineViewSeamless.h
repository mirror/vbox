/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineViewSeamless class declaration
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___UIMachineViewSeamless_h___
#define ___UIMachineViewSeamless_h___

/* Local includes */
#include "UIMachineView.h"

class UIMachineViewSeamless : public UIMachineView
{
    Q_OBJECT;

protected:

    /* Seamless machine view constructor/destructor: */
    UIMachineViewSeamless(  UIMachineWindow *pMachineWindow
                          , VBoxDefs::RenderMode renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                          , bool bAccelerate2DVideo
#endif
                          , ulong uMonitor);
    virtual ~UIMachineViewSeamless();

private slots:

    /* Console callback handlers: */
    void sltAdditionsStateChanged();

    /* Watch dog for desktop resizes: */
    void sltDesktopResized();

private:

    /* Event handlers: */
    bool event(QEvent *pEvent);
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Prepare helpers: */
    void prepareBackup();
    void prepareFilters();
    void prepareConnections();
    void prepareConsoleConnections();
    void prepareSeamless();

    /* Cleanup helpers: */
    void cleanupSeamless();
    //void cleanupConsoleConnections() {}
    //void prepareConnections() {}
    //void cleanupFilters() {}
    //void prepareBackup() {}

    /* Private helpers: */
    QRect availableGeometry();

    /* Private variables: */
    QRegion m_lastVisibleRegion;
    QSize m_normalSize;

    /* Friend classes: */
    friend class UIMachineView;
};

#endif // !___UIMachineViewSeamless_h___

