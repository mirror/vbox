/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasic4 class declaration.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic4_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic4_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QVariant>

/* GUI includes: */
#include "UIWizardPage.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"

/* Forward declarations: */
class QRadioButton;
class QIRichTextLabel;
class QIToolButton;
class UIBaseMemoryEditor;
class UIMediaComboBox;
class UIVirtualCPUEditor;

/** 3rd page of the New Virtual Machine wizard (base part). */
class UIWizardNewVMPage4 : public UIWizardPageBase
{

protected:

    /** Constructor. */
    UIWizardNewVMPage4();


    /** @name Property getters/setters
     * @{ */
       int baseMemory() const;
       int VCPUCount() const;
    /** @} */

    QWidget *createHardwareWidgets();
    void retranslateWidgets();



    /** @name Widgets
     * @{ */
       UIBaseMemoryEditor *m_pBaseMemoryEditor;
       UIVirtualCPUEditor *m_pVirtualCPUEditor;
    /** @} */

};

/** 3rd page of the New Virtual Machine wizard (basic extension). */
class UIWizardNewVMPageBasic4 : public UIWizardPage, public UIWizardNewVMPage4
{
    Q_OBJECT;
    Q_PROPERTY(int baseMemory READ baseMemory);
    Q_PROPERTY(int VCPUCount READ VCPUCount);

public:

    /** Constructor. */
    UIWizardNewVMPageBasic4();

protected:

    /** Wrapper to access 'wizard' from base part. */
    UIWizard *wizardImp() const { return wizard(); }
    /** Wrapper to access 'this' from base part. */
    UIWizardPage* thisImp() { return this; }
    /** Wrapper to access 'wizard-field' from base part. */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

private slots:


private:


    /** Prepare stuff. */
    void prepare();
    void createConnections();
    void retranslateUi();
    void initializePage();
    void cleanupPage();

    bool isComplete() const;

    /** Widgets. */
    QIRichTextLabel *m_pLabel;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic4_h */
