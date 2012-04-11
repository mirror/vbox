/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVM class declaration
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWizardCloneVM_h__
#define __UIWizardCloneVM_h__

/* Local includes: */
#include "UIWizard.h"
#include "COMDefs.h"

/* Clone VM wizard: */
class UIWizardCloneVM : public UIWizard
{
    Q_OBJECT;

public:

    /* Page IDs: */
    enum
    {
        Page1,
        Page2,
        Page3
    };

    /* Constructor: */
    UIWizardCloneVM(QWidget *pParent, const CMachine &machine, CSnapshot snapshot = CSnapshot());

protected:

    /* CLone VM stuff: */
    bool cloneVM();

    /* Who will be able to clone virtual-machine: */
    friend class UIWizardCloneVMPageBasic1;
    friend class UIWizardCloneVMPageBasic2;
    friend class UIWizardCloneVMPageBasic3;

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Variables: */
    CMachine m_machine;
    CSnapshot m_snapshot;
};

#endif // __UIWizardCloneVM_h__

