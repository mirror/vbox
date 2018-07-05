/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic1 class declaration.
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

#ifndef ___UIWizardExportAppPageBasic1_h___
#define ___UIWizardExportAppPageBasic1_h___

/* GUI includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class QListWidget;
class QIRichTextLabel;


/** UIWizardPageBase extension for 1st page of the Export Appliance wizard. */
class UIWizardExportAppPage1 : public UIWizardPageBase
{
protected:

    /** Constructs 1st page base. */
    UIWizardExportAppPage1();

    /** Populates VM selector items on the basis of passed @a selectedVMNames. */
    void populateVMSelectorItems(const QStringList &selectedVMNames);

    /** Returns a list of selected machine names. */
    QStringList machineNames() const;

    /** Returns a list of selected machine IDs. */
    QStringList machineIDs() const;

    /** Holds the VM selector instance. */
    QListWidget *m_pVMSelector;
};


/** UIWizardPage extension for 1st page of the Export Appliance wizard, extends UIWizardExportAppPage1 as well. */
class UIWizardExportAppPageBasic1 : public UIWizardPage, public UIWizardExportAppPage1
{
    Q_OBJECT;
    Q_PROPERTY(QStringList machineNames READ machineNames);
    Q_PROPERTY(QStringList machineIDs READ machineIDs);

public:

    /** Constructs 1st basic page.
      * @param  selectedVMNames  Brings the list of selected VM names. */
    UIWizardExportAppPageBasic1(const QStringList &selectedVMNames);

private:

    /** Handles translation event. */
    void retranslateUi();

    /** Performs page initialization. */
    void initializePage();

    /** Returns whether page is complete. */
    bool isComplete() const;
    /** Performs page validation. */
    bool validatePage();

    /** Returns next page ID. */
    int nextId() const;

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;
};

#endif /* !___UIWizardExportAppPageBasic1_h___ */
