/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportApp class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportApp_h
#define FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportApp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIWizard.h"

/* Forward declarations: */
class CAppliance;

/** Export Appliance wizard. */
class UIWizardExportApp : public UIWizard
{
    Q_OBJECT;

public:

    /** Basic page IDs. */
    enum
    {
        Page1,
        Page2,
        Page3
    };

    /** Expert page IDs. */
    enum
    {
        PageExpert
    };

    /** Constructs export appliance wizard passing @a pParent to the base-class.
      * @param  selectedVMNames  Brings the names of VMs to be exported. */
    UIWizardExportApp(QWidget *pParent, const QStringList &selectedVMNames = QStringList(), bool fFastTraverToExportOCI = false);

    /** Exports full appliance. */
    bool exportAppliance();

    /** Composes universal resource identifier.
      * @param  fWithFile  Brings whether uri should include file name as well. */
    QString uri(bool fWithFile = true) const;

protected slots:

    /** Handles page change to @a iId. */
    virtual void sltCurrentIdChanged(int iId) /* override */;

    /** Handles custom button @a iId click. */
    virtual void sltCustomButtonClicked(int iId) /* override */;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Prepares all. */
    virtual void prepare() /* override */;

private:

    /** Exports @a comAppliance VMs. */
    bool exportVMs(CAppliance &comAppliance);

    /** Holds the names of VMs to be exported. */
    QStringList  m_selectedVMNames;
    /** Holds whether we should fast travel to page 2. */
    bool         m_fFastTraverToExportOCI;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportApp_h */
