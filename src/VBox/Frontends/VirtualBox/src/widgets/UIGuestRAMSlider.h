/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestRAMSlider class declaration.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef ___UIGuestRAMSlider_h___
#define ___UIGuestRAMSlider_h___

/* GUI includes: */
#include "QIAdvancedSlider.h"
#include "UILibraryDefs.h"

/** QIAdvancedSlider subclass used as a guest RAM slider. */
class SHARED_LIBRARY_STUFF UIGuestRAMSlider : public QIAdvancedSlider
{
    Q_OBJECT;

public:

    /** Constructs guest RAM slider passing @a pParent to the base-class. */
    UIGuestRAMSlider(QWidget *pParent = 0);
    /** Constructs guest RAM slider passing @a pParent and @a enmOrientation to the base-class. */
    UIGuestRAMSlider(Qt::Orientation enmOrientation, QWidget *pParent = 0);

    /** Returns the minimum RAM. */
    uint minRAM() const;
    /** Returns the maximum optimal RAM. */
    uint maxRAMOpt() const;
    /** Returns the maximum allowed RAM. */
    uint maxRAMAlw() const;
    /** Returns the maximum possible RAM. */
    uint maxRAM() const;

private:

    /** Prepares all. */
    void prepare();

    /** Calculates page step for passed @a iMaximum value. */
    int calcPageStep(int iMaximum) const;

    /** Holds the minimum RAM. */
    uint m_uMinRAM;
    /** Holds the maximum optimal RAM. */
    uint m_uMaxRAMOpt;
    /** Holds the maximum allowed RAM. */
    uint m_uMaxRAMAlw;
    /** Holds the maximum possible RAM. */
    uint m_uMaxRAM;
};

#endif /* !___UIGuestRAMSlider_h___ */
