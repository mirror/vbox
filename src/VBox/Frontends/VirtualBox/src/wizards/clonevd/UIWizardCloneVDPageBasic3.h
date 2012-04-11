/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVDPageBasic3 class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWizardCloneVDPageBasic3_h__
#define __UIWizardCloneVDPageBasic3_h__

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class QRadioButton;
class QCheckBox;
class QIRichTextLabel;
class QGroupBox;

/* 3rd page of the Clone Virtual Disk wizard: */
class UIWizardCloneVDPageBasic3 : public UIWizardPage
{
    Q_OBJECT;
    Q_PROPERTY(qulonglong mediumVariant READ mediumVariant WRITE setMediumVariant);

public:

    /* Constructor: */
    UIWizardCloneVDPageBasic3();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;

    /* Stuff for 'variant' field: */
    qulonglong mediumVariant() const;
    void setMediumVariant(qulonglong uMediumVariant);

    /* Variables: */
    QRadioButton *m_pDynamicalButton;
    QRadioButton *m_pFixedButton;
    QCheckBox *m_pSplitBox;

    /* Widgets: */
    QIRichTextLabel *m_pDescriptionLabel;
    QIRichTextLabel *m_pDynamicLabel;
    QIRichTextLabel *m_pFixedLabel;
    QIRichTextLabel *m_pSplitLabel;
    QGroupBox *m_pVariantContainer;
};

#endif // __UIWizardCloneVDPageBasic3_h__

