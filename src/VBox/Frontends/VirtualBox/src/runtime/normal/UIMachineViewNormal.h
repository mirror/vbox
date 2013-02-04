/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineViewNormal class declaration
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

#ifndef ___UIMachineViewNormal_h___
#define ___UIMachineViewNormal_h___

/* Local includes */
#include "UIMachineView.h"

class UIMachineViewNormal : public UIMachineView
{
    Q_OBJECT;

protected:

    /* Normal machine-view constructor: */
    UIMachineViewNormal(  UIMachineWindow *pMachineWindow
                        , ulong uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                        , bool bAccelerate2DVideo
#endif
    );
    /* Normal machine-view destructor: */
    virtual ~UIMachineViewNormal();

private slots:

    /* Console callback handlers: */
    void sltAdditionsStateChanged();

#ifdef Q_WS_X11
    /* Slot to perform synchronized geometry normalization.
     * Currently its only required under X11 as of its async nature: */
    virtual void sltNormalizeGeometry() { normalizeGeometry(true); }
#endif /* Q_WS_X11 */

private:

    /* Event handlers: */
    bool event(QEvent *pEvent);
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Prepare helpers: */
    void prepareCommon();
    void prepareFilters();
    void prepareConsoleConnections();
    //void loadMachineViewSettings();
    /** If the last guest size hint was sent to switch to fullscreen or
     * seamless mode then send one to restore the old view size.
     * @note This method also does some hacks to suppress intermediary resizes
     *        to the old fullscreen size. */
    void maybeResendResizeHint();

    /* Cleanup helpers: */
    void saveMachineViewSettings();
    //void cleanupConsoleConnections() {}
    //void cleanupFilters() {}
    //void cleanupCommon() {}

    /* Hidden setters: */
    void setGuestAutoresizeEnabled(bool bEnabled);

    /* Private helpers: */
    void normalizeGeometry(bool fAdjustPosition);
    QRect workingArea() const;
    QSize calculateMaxGuestSize() const;
    void maybeRestrictMinimumSize();

    /* Private members: */
    bool m_bIsGuestAutoresizeEnabled : 1;

    /* Friend classes: */
    friend class UIMachineView;
};

#endif // !___UIMachineViewNormal_h___

