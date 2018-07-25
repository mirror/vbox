/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportApp class declaration.
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

#ifndef ___UIWizardExportApp_h___
#define ___UIWizardExportApp_h___

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
    UIWizardExportApp(QWidget *pParent, const QStringList &selectedVMNames = QStringList());

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
    QStringList m_selectedVMNames;
};

#endif /* !___UIWizardExportApp_h___ */
