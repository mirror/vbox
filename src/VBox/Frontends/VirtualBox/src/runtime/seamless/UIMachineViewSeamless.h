/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineViewSeamless class declaration
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMachineViewSeamless_h___
#define ___UIMachineViewSeamless_h___

/* Local includes */
#include "UIMachineView.h"

class UIMachineViewSeamless : public UIMachineView
{
    Q_OBJECT;

public:

    /* Public getters: */
    QRegion lastVisibleRegion() const { return m_lastVisibleRegion; }

protected:

    /* Seamless machine-view constructor: */
    UIMachineViewSeamless(  UIMachineWindow *pMachineWindow
                          , ulong uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                          , bool bAccelerate2DVideo
#endif
    );
    /* Seamless machine-view destructor: */
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
    void prepareCommon();
    void prepareFilters();
    void prepareConnections();
    void prepareConsoleConnections();
    void prepareSeamless();

    /* Cleanup helpers: */
    void cleanupSeamless();
    //void cleanupConsoleConnections() {}
    //void prepareConnections() {}
    //void cleanupFilters() {}
    //void cleanupCommon() {}

    /* Private helpers: */
    void normalizeGeometry(bool /* fAdjustPosition */) {}
    QRect workingArea() const;
    void calculateMaxGuestSize();
    void maybeRestrictMinimumSize() {}

    /* Private variables: */
    QRegion m_lastVisibleRegion;

    /* Friend classes: */
    friend class UIMachineView;
};

#endif // !___UIMachineViewSeamless_h___

