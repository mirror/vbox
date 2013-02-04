/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineViewScale class declaration
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMachineViewScale_h___
#define ___UIMachineViewScale_h___

/* Local includes */
#include "UIMachineView.h"

class UIMachineViewScale : public UIMachineView
{
    Q_OBJECT;

protected:

    /* Scale machine-view constructor: */
    UIMachineViewScale(  UIMachineWindow *pMachineWindow
                       , ulong uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                       , bool bAccelerate2DVideo
#endif
    );
    /* Scale machine-view destructor: */
    virtual ~UIMachineViewScale();

    virtual void takePauseShotLive();
    virtual void takePauseShotSnapshot();
    virtual void resetPauseShot();
    void scalePauseShot();

private slots:

    /* Slot to perform guest resize: */
    void sltPerformGuestScale();

private:

    /* Event handlers: */
    bool event(QEvent *pEvent);
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Prepare helpers: */
    // void prepareFrameBuffer();
    //void loadMachineViewSettings();

    /* Cleanup helpers: */
    void saveMachineViewSettings();
    //void cleanupConnections() {}
    //void cleanupFrameBuffer() {}

    /* Private helpers: */
    QSize sizeHint() const;
    void normalizeGeometry(bool /* fAdjustPosition */) {}
    QRect workingArea() const;
    QSize calculateMaxGuestSize() const;
    void maybeRestrictMinimumSize() {}
    void updateSliders();

    /* Private members: */
    QImage *m_pPauseImage;

    /* Friend classes: */
    friend class UIMachineView;
};

#endif // !___UIMachineViewScale_h___

