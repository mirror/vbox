/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasic2 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic2_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic2_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class UIBaseMemorySlider;
class UIBaseMemoryEditor;
class UIVirtualCPUEditor;
class QSpinBox;
class QLabel;
class QIRichTextLabel;

/* 2nd page of the New Virtual Machine wizard (base part): */
class UIWizardNewVMPage2 : public UIWizardPageBase
{
protected:

    /* Constructor: */
    UIWizardNewVMPage2();

    int baseMemory() const;
    int VCPUCount() const;

    /* Widgets: */
    UIBaseMemoryEditor *m_pBaseMemoryEditor;
    UIVirtualCPUEditor *m_pVirtualCPUEditor;
};

/* 2nd page of the New Virtual Machine wizard (basic extension): */
class UIWizardNewVMPageBasic2 : public UIWizardPage, public UIWizardNewVMPage2
{
    Q_OBJECT;
    Q_PROPERTY(int baseMemory READ baseMemory);
    Q_PROPERTY(int VCPUCount READ VCPUCount);

public:

    /* Constructor: */
    UIWizardNewVMPageBasic2();

private slots:

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic2_h */
