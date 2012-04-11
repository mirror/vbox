/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardExportAppPageBasic1 class declaration
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWizardExportAppPageBasic1_h__
#define __UIWizardExportAppPageBasic1_h__

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class QIRichTextLabel;
class QListWidget;

/* 1st page of the Export Appliance wizard: */
class UIWizardExportAppPageBasic1 : public UIWizardPage
{
    Q_OBJECT;
    Q_PROPERTY(QStringList machineNames READ machineNames);
    Q_PROPERTY(QStringList machineIDs READ machineIDs);

public:

    /* Constructor: */
    UIWizardExportAppPageBasic1(const QStringList &selectedVMNames);

private:

    /* Translate stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;
    bool validatePage();

    /* Navigation stuff: */
    int nextId() const;

    /* Helping stuff: */
    void populateVMSelectorItems();

    /* Stuff for 'machineNames' field: */
    QStringList machineNames() const;

    /* Stuff for 'machineIDs' field: */
    QStringList machineIDs() const;

    /* Variables: */
    QStringList m_selectedVMNames;

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
    QListWidget *m_pVMSelector;
};

#endif /* __UIWizardExportAppPageBasic1_h__ */

