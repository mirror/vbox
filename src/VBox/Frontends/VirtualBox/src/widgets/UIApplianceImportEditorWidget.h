/* $Id$ */
/** @file
 * VBox Qt GUI - UIApplianceImportEditorWidget class declaration.
 */

/*
 * Copyright (C) 2009-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_UIApplianceImportEditorWidget_h
#define FEQT_INCLUDED_SRC_widgets_UIApplianceImportEditorWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIApplianceEditorWidget.h"

/* COM includes: */
#include "CAppliance.h"

/** UIApplianceEditorWidget subclass for Import Appliance wizard. */
class UIApplianceImportEditorWidget: public UIApplianceEditorWidget
{
    Q_OBJECT;

public:

    /** Constructs widget passing @a pParent to the base-class. */
    UIApplianceImportEditorWidget(QWidget *pParent = 0);

    /** Assigns @a comAppliance and populates widget contents. */
    virtual void setAppliance(const CAppliance &comAppliance) /* override final */;

    /** Prepares import by pushing edited data back to appliance. */
    void prepareImport();
};

#endif /* !FEQT_INCLUDED_SRC_widgets_UIApplianceImportEditorWidget_h */
