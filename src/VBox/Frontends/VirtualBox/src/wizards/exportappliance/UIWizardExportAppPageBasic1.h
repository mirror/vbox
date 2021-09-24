/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic1 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportAppPageBasic1_h
#define FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportAppPageBasic1_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes */
#include <QUuid>

/* GUI includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class QListWidget;
class QIRichTextLabel;
class UIWizardExportApp;

/** Namespace for 1st basic page of the Export Appliance wizard. */
namespace UIWizardExportAppPage1
{
    /** Populates @a pVMSelector with items on the basis of passed @a selectedVMNames. */
    void populateVMItems(QListWidget *pVMSelector, const QStringList &selectedVMNames);

    /** Refresh a list of saved machines selected in @a pVMSelector. */
    void refreshSavedMachines(QStringList &savedMachines, QListWidget *pVMSelector);

    /** Returns a list of machine names selected in @a pVMSelector. */
    QStringList machineNames(QListWidget *pVMSelector);
    /** Returns a list of machine IDs selected in @a pVMSelector. */
    QList<QUuid> machineIDs(QListWidget *pVMSelector);
}

/** UINativeWizardPage extension for 1st basic page of the Export Appliance wizard,
  * based on UIWizardAddCloudVMPage1 namespace functions. */
class UIWizardExportAppPageBasic1 : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs 1st basic page.
      * @param  selectedVMNames        Brings the list of selected VM names.
      * @param  fFastTravelToNextPage  Brings whether we should fast-travel to next page. */
    UIWizardExportAppPageBasic1(const QStringList &selectedVMNames, bool fFastTravelToNextPage);

protected:

    /** Returns wizard this page belongs to. */
    UIWizardExportApp *wizard() const;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;

    /** Returns whether page is complete. */
    virtual bool isComplete() const /* override */;

    /** Performs page validation. */
    virtual bool validatePage() /* override */;

private slots:

    /** Handles VM item selection change. */
    void sltHandleVMItemSelectionChanged();

private:

    /** Updates machines. */
    void updateMachines();

    /** Holds the list of selected VM names. */
    const QStringList  m_selectedVMNames;
    /** Holds whether we should fast travel to next page. */
    bool               m_fFastTravelToNextPage;

    /** Holds the main label instance. */
    QIRichTextLabel *m_pLabelMain;
    /** Holds the VM selector instance. */
    QListWidget     *m_pVMSelector;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportAppPageBasic1_h */
