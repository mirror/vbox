/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMHardwarePageBasic class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMHardwarePageBasic_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMHardwarePageBasic_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QVariant>

/* GUI includes: */
#include "UINativeWizardPage.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"

/* Forward declarations: */
class QRadioButton;
class QCheckBox;
class QIRichTextLabel;
class UIBaseMemoryEditor;
class UIVirtualCPUEditor;
class UIWizardNewVM;

namespace UIWizardNewVMHardwarePage
{
}
//        int baseMemory() const;
//        int VCPUCount() const;
//        bool EFIEnabled() const;


class UIWizardNewVMHardwarePageBasic : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardNewVMHardwarePageBasic();

protected:


private slots:

    void sltMemoryAmountChanged(int iValue);
    void sltCPUCountChanged(int iCount);

private:

    /** Prepare stuff. */
    void prepare();
    void createConnections();
    void retranslateUi();
    void initializePage();
    void cleanupPage();
    QWidget *createHardwareWidgets();
    bool isComplete() const;

    UIWizardNewVM *m_pWizard;

    /** @name Widgets
     * @{ */
       UIBaseMemoryEditor *m_pBaseMemoryEditor;
       UIVirtualCPUEditor *m_pVirtualCPUEditor;
       QCheckBox          *m_pEFICheckBox;
       QIRichTextLabel    *m_pLabel;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMHardwarePageBasic_h */
