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
        Page3,
        Page4
    };

    /** Expert page IDs. */
    enum
    {
        PageExpert
    };

    /** Constructs export appliance wizard passing @a pParent to the base-class.
      * @param  selectedVMNames  Brings the names of VMs to be exported. */
    UIWizardExportApp(QWidget *pParent, const QStringList &selectedVMNames = QStringList());

protected:

    /** Exports full appliance. */
    bool exportAppliance();
    /** Exports @a appliance VMs. */
    bool exportVMs(CAppliance &appliance);
    /** Composes universal resource identifier.
      * @param  fWithFile  Brings whether uri should include file name as well. */
    QString uri(bool fWithFile = true) const;

    /** @todo remove it */
    /* Who will be able to export appliance: */
    friend class UIWizardExportAppPage4;
    friend class UIWizardExportAppPageBasic4;
    friend class UIWizardExportAppPageExpert;

private slots:

    /** Handles page change to @a iId. */
    void sltCurrentIdChanged(int iId);
    /** Handles custom button @a iId click. */
    void sltCustomButtonClicked(int iId);

private:

    /** Handles translation event. */
    void retranslateUi();

    /** Prepares all. */
    void prepare();

    /** Holds the names of VMs to be exported. */
    QStringList m_selectedVMNames;
};

#endif /* !___UIWizardExportApp_h___ */
