/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDPageBasic2 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDPageBasic2_h
#define FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDPageBasic2_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class CMediumFormat;
class QButtonGroup;
class QRadioButton;
class QCheckBox;
class QIRichTextLabel;


/* 2nd page of the New Virtual Hard Drive wizard (base part): */
class SHARED_LIBRARY_STUFF UIWizardNewVDPage2 : public UIWizardPageBase
{
protected:

    /** Constructor: */
    UIWizardNewVDPage2();

    QWidget *createMediumVariantWidgets(bool fWithLabels);
    qulonglong mediumVariant() const;
    void setMediumVariant(qulonglong uMediumVariant);
    void retranslateWidgets();
    /** Check Medium format capability and decide if certain widgets can be shown. */
    void setWidgetVisibility(CMediumFormat &mediumFormat);
    /** @name Widgets
     * @{ */
        QButtonGroup *m_pVariantButtonGroup;
        QRadioButton *m_pDynamicalButton;
        QRadioButton *m_pFixedButton;
        QCheckBox *m_pSplitBox;
    /** @} */

    /** @name Rich text labels. Declared/defined here since the are shared in more than one child class
     * @{ */
        QIRichTextLabel *m_pDescriptionLabel;
        QIRichTextLabel *m_pDynamicLabel;
        QIRichTextLabel *m_pFixedLabel;
        QIRichTextLabel *m_pSplitLabel;
    /** @} */
};


/* 2nd page of the New Virtual Hard Drive wizard (basic extension): */
class SHARED_LIBRARY_STUFF UIWizardNewVDPageBasic2 : public UIWizardPage, public UIWizardNewVDPage2
{
    Q_OBJECT;
    Q_PROPERTY(qulonglong mediumVariant READ mediumVariant WRITE setMediumVariant);

public:

    /* Constructor: */
    UIWizardNewVDPageBasic2();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;
};


#endif /* !FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDPageBasic2_h */
