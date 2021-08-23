/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVDPageBasic2 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDPageBasic2_h
#define FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDPageBasic2_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizardPage.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QButtonGroup;
class QRadioButton;
class QCheckBox;
class QIRichTextLabel;
class UIWizardCloneVD;
class CMediumFormat;
class UIDiskVariantGroupBox;

class UIWizardCloneVDPageBasic2 : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs basic page. */
    UIWizardCloneVDPageBasic2(KDeviceType enmDeviceType);

private:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;
    void prepare();
    UIWizardCloneVD *cloneWizard() const;

    /** Prepares the page. */
    virtual void initializePage() /* override */;

    /** Returns whether the page is complete. */
    virtual bool isComplete() const /* override */;
    void setWidgetVisibility(const CMediumFormat &mediumFormat);

    /** Holds the description label instance. */
    QIRichTextLabel *m_pDescriptionLabel;
    /** Holds the 'Dynamic' description label instance. */
    QIRichTextLabel *m_pDynamicLabel;
    /** Holds the 'Fixed' description label instance. */
    QIRichTextLabel *m_pFixedLabel;
    /** Holds the 'Split to 2GB files' description label instance. */
    QIRichTextLabel *m_pSplitLabel;

    UIDiskVariantGroupBox *m_pVariantGroupBox;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDPageBasic2_h */
