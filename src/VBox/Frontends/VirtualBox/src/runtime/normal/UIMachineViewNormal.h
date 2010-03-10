/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineViewNormal class declaration
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

#ifndef ___UIMachineViewNormal_h___
#define ___UIMachineViewNormal_h___

/* Local includes */
#include "UIMachineView.h"

class UIMachineViewNormal : public UIMachineView
{
    Q_OBJECT;

protected:

    /* Normal machine view constructor/destructor: */
    UIMachineViewNormal(  UIMachineWindow *pMachineWindow
                        , VBoxDefs::RenderMode renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                        , bool bAccelerate2DVideo
#endif
                        , ulong uMonitor);
    virtual ~UIMachineViewNormal();

private slots:

    /* Slot to perform guest resize: */
    void sltPerformGuestResize(const QSize &size = QSize());

    /* Console callback handlers: */
    void sltAdditionsStateChanged();

    /* Watch dog for desktop resizes: */
    void sltDesktopResized();

private:

    /* Event handlers: */
    bool event(QEvent *pEvent);
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Prepare helpers: */
    void prepareFilters();
    void prepareConnections();
    void prepareConsoleConnections();
    //void loadMachineViewSettings();

    /* Cleanup helpers: */
    void saveMachineViewSettings();
    //void cleanupConsoleConnections() {}
    //void prepareConnections() {}
    //void cleanupFilters() {}

    /* Hidden setters: */
    void setGuestAutoresizeEnabled(bool bEnabled);

    /* Private helpers: */
    void normalizeGeometry(bool fAdjustPosition);
    QRect availableGeometry();
    void calculateDesktopGeometry();
    void maybeRestrictMinimumSize();
    void updateSliders();

    /* Private members: */
    bool m_bIsGuestAutoresizeEnabled : 1;
    bool m_fShouldWeDoResize : 1;

    /* Friend classes: */
    friend class UIMachineView;
};

#endif // !___UIMachineViewNormal_h___

