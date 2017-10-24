/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVD class declaration.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIWizardCloneVD_h___
#define ___UIWizardCloneVD_h___

/* GUI includes: */
#include "UIWizard.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"


/** UIWizard subclass to clone virtual disk image files. */
class UIWizardCloneVD : public UIWizard
{
    Q_OBJECT;

public:

    /** Basic Page IDs. */
    enum
    {
        Page1,
        Page2,
        Page3,
        Page4
    };

    /** Expert Page IDs. */
    enum
    {
        PageExpert
    };

    /** Constructs wizard to clone @a sourceVirtualDisk passing @a pParent to the base-class. */
    UIWizardCloneVD(QWidget *pParent, const CMedium &sourceVirtualDisk);

    /** Returns target virtual-disk. */
    CMedium virtualDisk() const { return m_virtualDisk; }

protected:

    /** Makes a copy of source virtual-disk. */
    bool copyVirtualDisk();

    /** Who will be able to copy virtual-disk. */
    friend class UIWizardCloneVDPageBasic4;
    friend class UIWizardCloneVDPageExpert;

private:

    /** Handles translation event. */
    void retranslateUi();

    /** Prepares all. */
    void prepare();

    /** Holds the source virtual disk wrapper. */
    CMedium m_sourceVirtualDisk;
    /** Holds the target virtual disk wrapper. */
    CMedium m_virtualDisk;
};

#endif /* !___UIWizardCloneVD_h___ */

