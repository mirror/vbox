/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasicHardware class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicHardware_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicHardware_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class UIBaseMemoryEditor;
class UIVirtualCPUEditor;
class QSpinBox;
class QLabel;
class QIRichTextLabel;

/* 2nd page of the New Virtual Machine wizard (base part): */
class UIWizardNewVMPageHardware : public UIWizardPageBase
{
protected:

    /* Constructor: */
    UIWizardNewVMPageHardware();

    int baseMemory() const;
    int VCPUCount() const;

    /* Widgets: */
    UIBaseMemoryEditor *m_pBaseMemoryEditor;
    UIVirtualCPUEditor *m_pVirtualCPUEditor;
};

/* 2nd page of the New Virtual Machine wizard (basic extension): */
class UIWizardNewVMPageBasicHardware : public UIWizardPage, public UIWizardNewVMPageHardware
{
    Q_OBJECT;
    Q_PROPERTY(int baseMemory READ baseMemory);
    Q_PROPERTY(int VCPUCount READ VCPUCount);

public:

    /* Constructor: */
    UIWizardNewVMPageBasicHardware();

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

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasicHardware_h */
