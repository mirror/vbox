/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVMTypePage class declaration.
 */

/*
 * Copyright (C) 2011-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMTypePage_h
#define FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMTypePage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QSet>

/* Local includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class QIRichTextLabel;
class UICloneVMCloneTypeGroupBox;

class UIWizardCloneVMTypePage : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardCloneVMTypePage(bool fAdditionalInfo);

private slots:

    void sltCloneTypeChanged(bool fIsFullClone);

private:

    void retranslateUi();
    void initializePage();
    void prepare();
    bool validatePage();

    QIRichTextLabel *m_pLabel;
    bool m_fAdditionalInfo;
    UICloneVMCloneTypeGroupBox *m_pCloneTypeGroupBox;
    QSet<QString> m_userModifiedParameters;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMTypePage_h */
