/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVMPageBasic2 class declaration
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWizardCloneVMPageBasic2_h__
#define __UIWizardCloneVMPageBasic2_h__

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class QIRichTextLabel;
class QRadioButton;

/* 2nd page of the Clone Virtual Machine wizard: */
class UIWizardCloneVMPageBasic2 : public UIWizardPage
{
    Q_OBJECT;
    Q_PROPERTY(bool linkedClone READ isLinkedClone);

public:

    /* Constructor: */
    UIWizardCloneVMPageBasic2(bool fAdditionalInfo);

private slots:

    /* Button click handler: */
    void buttonClicked(QAbstractButton *pButton);

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool validatePage();

    /* Navigation stuff: */
    int nextId() const;

    /* Stuff for 'linkedClone' field: */
    bool isLinkedClone() const;

    /* Variables: */
    bool m_fAdditionalInfo;

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
    QRadioButton *m_pFullCloneRadio;
    QRadioButton *m_pLinkedCloneRadio;
};

#endif // __UIWizardCloneVMPageBasic2_h__

