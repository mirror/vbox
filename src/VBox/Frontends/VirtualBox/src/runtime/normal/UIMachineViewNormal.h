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

    UIMachineViewNormal(  UIMachineWindow *pMachineWindow
                        , VBoxDefs::RenderMode renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                        , bool bAccelerate2DVideo
#endif
    );

    void normalizeGeometry(bool bAdjustPosition = false);

    void maybeRestrictMinimumSize();

private slots:

    void doResizeHint(const QSize &aSize = QSize());

    void doResizeDesktop(int);

    void sltToggleGuestAutoresize(bool bOn);

    void sltAdditionsStateChanged(const QString &strVersion, bool bIsActive,
                                  bool bIsSeamlessSupported, bool bIsGraphicsSupported);
private:

    bool m_bIsGuestAutoresizeEnabled : 1;

    friend class UIMachineView;
};

#endif // !___UIMachineViewNormal_h___
