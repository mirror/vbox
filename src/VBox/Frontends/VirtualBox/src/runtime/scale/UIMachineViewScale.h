/** @file
 * VBox Qt GUI - UIMachineViewScale class declaration.
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

private slots:

    /* Slot to perform guest resize: */
    void sltPerformGuestScale();

    /* Handler: Frame-buffer NotifyUpdate stuff: */
    void sltHandleNotifyUpdate(int iX, int iY, int iW, int iH);

private:

    /* Event handlers: */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Prepare helpers: */
    //void loadMachineViewSettings() {}

    /* Cleanup helpers: */
    void saveMachineViewSettings();

    /** Applies machine-view scale-factor. */
    void applyMachineViewScaleFactor();

    /** Resends guest size-hint if necessary. */
    void maybeResendSizeHint();

    /* Private helpers: */
    QSize sizeHint() const;
    QRect workingArea() const;
    QSize calculateMaxGuestSize() const;
    void updateSliders();

    /* Friend classes: */
    friend class UIMachineView;
};

#endif // !___UIMachineViewScale_h___

