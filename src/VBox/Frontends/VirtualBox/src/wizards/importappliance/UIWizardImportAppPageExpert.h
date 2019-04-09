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

/* GUI includes: */
#include "UIWizardImportAppPageBasic1.h"
#include "UIWizardImportAppPageBasic2.h"

/* Forward declarations: */
class QGroupBox;

/** UIWizardPage extension for UIWizardImportAppPage1 and UIWizardImportAppPage2. */
class UIWizardImportAppPageExpert : public UIWizardPage,
                                    public UIWizardImportAppPage1,
                                    public UIWizardImportAppPage2
{
    Q_OBJECT;
    Q_PROPERTY(ImportAppliancePointer applianceWidget READ applianceWidget);

public:

    /** Constructs expert page.
      * @param  strFileName  Brings appliance file name. */
    UIWizardImportAppPageExpert(bool fImportFromOCIByDefault, const QString &strFileName);

private slots:

    /** Handles file-path change. */
    void sltFilePathChangeHandler();

private:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;

    /** Returns whether page is complete. */
    virtual bool isComplete() const /* override */;

    /** Performs page validation. */
    virtual bool validatePage() /* override */;

    /** Holds the appliance container instance. */
    QGroupBox *m_pApplianceCnt;
    /** Holds the settings container instance. */
    QGroupBox *m_pSettingsCnt;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageExpert_h */
