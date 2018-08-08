/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageExpert class declaration.
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

#ifndef ___UIWizardExportAppPageExpert_h___
#define ___UIWizardExportAppPageExpert_h___

/* GUI includes: */
#include "UIWizardExportAppPageBasic1.h"
#include "UIWizardExportAppPageBasic2.h"
#include "UIWizardExportAppPageBasic3.h"

/* Forward declarations: */
class QGroupBox;

/** UIWizardPage extension for UIWizardExportAppPage1, UIWizardExportAppPage2 and UIWizardExportAppPage3. */
class UIWizardExportAppPageExpert : public UIWizardPage,
                                    public UIWizardExportAppPage1,
                                    public UIWizardExportAppPage2,
                                    public UIWizardExportAppPage3
{
    Q_OBJECT;
    Q_PROPERTY(QStringList machineNames READ machineNames);
    Q_PROPERTY(QStringList machineIDs READ machineIDs);
    Q_PROPERTY(QString format READ format WRITE setFormat);
    Q_PROPERTY(QString path READ path WRITE setPath);
    Q_PROPERTY(MACAddressPolicy macAddressPolicy READ macAddressPolicy WRITE setMACAddressPolicy);
    Q_PROPERTY(bool manifestSelected READ isManifestSelected WRITE setManifestSelected);
    Q_PROPERTY(bool includeISOsSelected READ isIncludeISOsSelected WRITE setIncludeISOsSelected);
    Q_PROPERTY(CCloudProvider profiles READ profiles);
    Q_PROPERTY(QString profile READ profile);
    Q_PROPERTY(ExportAppliancePointer applianceWidget READ applianceWidget);

public:

    /** Constructs expert basic page.
      * @param  selectedVMNames  Brings the list of selected VM names. */
    UIWizardExportAppPageExpert(const QStringList &selectedVMNames);

protected:

    /** Handle any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override */;

    /** Allows access wizard from base part. */
    UIWizard* wizardImp() { return UIWizardPage::wizard(); }
    /** Allows access page from base part. */
    UIWizardPage* thisImp() { return this; }
    /** Allows access wizard-field from base part. */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;

    /** Returns whether page is complete. */
    virtual bool isComplete() const /* override */;

    /** Performs page validation. */
    virtual bool validatePage() /* override */;

private slots:

    /** Handles VM selector index change. */
    void sltVMSelectionChangeHandler();

    /** Handles format combo change. */
    void sltHandleFormatComboChange();

    /** Handles change in MAC address policy combo-box. */
    void sltHandleMACAddressPolicyComboChange();

    /** Handles change in account combo-box. */
    void sltHandleAccountComboChange();

private:

    /** Holds the VM selector container instance. */
    QGroupBox *m_pSelectorCnt;
    /** Holds the appliance widget container reference. */
    QGroupBox *m_pApplianceCnt;
    /** Holds the settings widget container reference. */
    QGroupBox *m_pSettingsCnt;
};

#endif /* !___UIWizardExportAppPageExpert_h___ */
