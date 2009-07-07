/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGuestRAMSlider class declaration
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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


#ifndef __VBoxGuestRAMSlider_h__
#define __VBoxGuestRAMSlider_h__

/* VBox includes */
#include "QIAdvancedSlider.h"

class VBoxGuestRAMSlider: public QIAdvancedSlider
{
public:
    VBoxGuestRAMSlider (QWidget *aParent = 0);
    VBoxGuestRAMSlider (Qt::Orientation aOrientation, QWidget *aParent = 0);

    uint minRAM() const;
    uint maxRAMOpt() const;
    uint maxRAMAlw() const;
    uint maxRAM() const;

private:
    /* Private methods */
    void init();
    int calcPageStep (int aMax) const;

    /* Private member vars */
    uint mMinRAM;
    uint mMaxRAMOpt;
    uint mMaxRAMAlw;
    uint mMaxRAM;
};

#endif /* __VBoxGuestRAMSlider_h__ */

