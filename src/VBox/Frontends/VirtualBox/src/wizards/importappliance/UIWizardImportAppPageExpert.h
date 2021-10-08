/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageExpert class declaration.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
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
    Q_PROPERTY(bool isSourceCloudOne READ isSourceCloudOne);
    Q_PROPERTY(CCloudProfile profile READ profile);
    Q_PROPERTY(CAppliance cloudAppliance READ cloudAppliance);
    Q_PROPERTY(CVirtualSystemDescriptionForm vsdForm READ vsdForm);
    Q_PROPERTY(QString machineId READ machineId);
    Q_PROPERTY(MACAddressImportPolicy macAddressImportPolicy READ macAddressImportPolicy);
    Q_PROPERTY(bool importHDsAsVDI READ importHDsAsVDI);

public:

    /** Constructs expert page.
      * @param  fImportFromOCIByDefault  Brings whether we should propose import from OCI by default.
      * @param  strFileName              Brings appliance file name. */
    UIWizardImportAppPageExpert(bool fImportFromOCIByDefault, const QString &strFileName);

protected:

    /** Allows access wizard from base part. */
    UIWizard *wizardImp() const { return UIWizardPage::wizard(); }
    /** Allows to access 'field()' from base part. */
    virtual QVariant fieldImp(const QString &strFieldName) const /* override */ { return UIWizardPage::field(strFieldName); }

    /** Handles translation event. */
    virtual void retranslateUi() /* override final */;

    /** Performs page initialization. */
    virtual void initializePage() /* override final */;

    /** Returns whether page is complete. */
    virtual bool isComplete() const /* override final */;

    /** Performs page validation. */
    virtual bool validatePage() /* override final */;

    /** Updates page appearance. */
    virtual void updatePageAppearance() /* override final */;

private slots:

    /** Handles source combo change. */
    void sltHandleSourceComboChange();

    /** Handles imported file selector change. */
    void sltHandleImportedFileSelectorChange();
    /** Handles profile combo change. */
    void sltHandleProfileComboChange();
    /** Handles profile tool-button click. */
    void sltHandleProfileButtonClick();
    /** Handles instance list change. */
    void sltHandleInstanceListChange();

    /** Handles import path editor change. */
    void sltHandleImportPathEditorChange();

    /** Handles MAC address import policy combo changes. */
    void sltHandleMACImportPolicyComboChange();

private:

    /** Handles the appliance file name. */
    QString  m_strFileName;

    /** Holds the source container instance. */
    QGroupBox *m_pCntSource;
    /** Holds the settings container instance. */
    QGroupBox *m_pSettingsCnt;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageExpert_h */
