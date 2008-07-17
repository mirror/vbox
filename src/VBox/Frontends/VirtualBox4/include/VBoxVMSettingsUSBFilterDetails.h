/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsUSBFilterDetails class declaration
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef __VBoxVMSettingsUSBFilterDetails_h__
#define __VBoxVMSettingsUSBFilterDetails_h__

#include "VBoxVMSettingsUSBFilterDetails.gen.h"
#include "QIWithRetranslateUI.h"
#include "QIDialog.h"
#include "VBoxVMSettingsUSB.h"

class VBoxVMSettingsUSBFilterDetails : public QIWithRetranslateUI2<QIDialog>,
                                       public Ui::VBoxVMSettingsUSBFilterDetails
{
    Q_OBJECT;

public:

    VBoxVMSettingsUSBFilterDetails (VBoxVMSettingsUSB::FilterType aType, QWidget *aParent = NULL);

protected:

    void retranslateUi();

private:

    /* Private member vars */
    VBoxVMSettingsUSB::FilterType mType;
};

#endif /* __VBoxVMSettingsUSBFilterDetails_h__ */

