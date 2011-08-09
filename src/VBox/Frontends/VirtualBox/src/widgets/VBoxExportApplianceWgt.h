/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxExportApplianceWgt class declaration
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxExportApplianceWgt_h__
#define __VBoxExportApplianceWgt_h__

#include "UIApplianceEditorWidget.h"

class VBoxExportApplianceWgt : public UIApplianceEditorWidget
{
    Q_OBJECT;

public:
    VBoxExportApplianceWgt (QWidget *aParent = NULL);

    CAppliance* init();

    void populate();
    void prepareExport();
};

#endif /* __VBoxExportApplianceWgt_h__ */

