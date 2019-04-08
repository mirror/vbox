/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageBasic1 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageBasic1_h
#define FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageBasic1_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIWizardImportAppDefs.h"
#include "UIWizardPage.h"

/* Forward declarations: */
class QLabel;
class QStackedLayout;
class QIComboBox;
class QIRichTextLabel;
class UIEmptyFilePathSelector;

/** Source type converter namespace. */
namespace ImportSourceTypeConverter
{
    /** Converts QString <= ImportSourceType. */
    QString toString(ImportSourceType enmType);
}

/** UIWizardPageBase extension for 1st page of the Import Appliance wizard. */
class UIWizardImportAppPage1 : public UIWizardPageBase
{
protected:

    /** Constructs 1st page base. */
    UIWizardImportAppPage1();

    /** Holds the source type label instance. */
    QLabel     *m_pSourceLabel;
    /** Holds the source type selector instance. */
    QIComboBox *m_pSourceSelector;

    /** Holds the stacked layout instance. */
    QStackedLayout              *m_pStackedLayout;
    /** Holds the stacked layout widget indexes map. */
    QMap<ImportSourceType, int>  m_stackedLayoutIndexMap;

    /** Holds the file selector instance. */
    UIEmptyFilePathSelector *m_pFileSelector;
};

/** UIWizardPage extension for 1st page of the Import Appliance wizard, extends UIWizardImportAppPage1 as well. */
class UIWizardImportAppPageBasic1 : public UIWizardPage, public UIWizardImportAppPage1
{
    Q_OBJECT;

public:

    /** Constructs 1st basic page. */
    UIWizardImportAppPageBasic1();

private slots:

    /** Handles change of import source to one with specified @a iIndex. */
    void sltHandleSourceChange(int iIndex);

private:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;

    /** Returns whether page is complete. */
    virtual bool isComplete() const /* override */;

    /** Performs page validation. */
    virtual bool validatePage() /* override */;

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageBasic1_h */
