/* $Id$ */
/** @file
 * VBox Qt GUI - UIApplianceExportEditorWidget class declaration.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_UIApplianceExportEditorWidget_h
#define FEQT_INCLUDED_SRC_widgets_UIApplianceExportEditorWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIApplianceEditorWidget.h"

class UIApplianceExportEditorWidget: public UIApplianceEditorWidget
{
    Q_OBJECT;

public:
    UIApplianceExportEditorWidget(QWidget *pParent = NULL);

    CAppliance *init();

    void populate();
    void prepareExport();
};

#endif /* !FEQT_INCLUDED_SRC_widgets_UIApplianceExportEditorWidget_h */

