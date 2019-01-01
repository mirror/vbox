/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageExpert class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageExpert_h
#define FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageExpert_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Local includes: */
#include "UIWizardImportAppPageBasic1.h"
#include "UIWizardImportAppPageBasic2.h"

/* Forward declarations: */
class QGroupBox;

/* Expert page of the Import Appliance wizard: */
class UIWizardImportAppPageExpert : public UIWizardPage,
                                    public UIWizardImportAppPage1,
                                    public UIWizardImportAppPage2
{
    Q_OBJECT;
    Q_PROPERTY(ImportAppliancePointer applianceWidget READ applianceWidget);

public:

    /* Constructor: */
    UIWizardImportAppPageExpert(const QString &strFileName);

private slots:

    /* File-path change handler: */
    void sltFilePathChangeHandler();

private:

    /* Translate stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;
    bool validatePage();

    /* Widgets: */
    QGroupBox *m_pApplianceCnt;
    QGroupBox *m_pSettingsCnt;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageExpert_h */

