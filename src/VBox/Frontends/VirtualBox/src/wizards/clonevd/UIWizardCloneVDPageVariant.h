/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVDPageVariant class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDPageVariant_h
#define FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDPageVariant_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizardPage.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QIRichTextLabel;
class UIWizardCloneVD;
class CMediumFormat;
class UIDiskVariantGroupBox;

class UIWizardCloneVDPageVariant : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs basic page. */
    UIWizardCloneVDPageVariant(KDeviceType enmDeviceType);

private slots:

    void sltMediumVariantChanged(qulonglong uVariant);

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

#endif /* !FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDPageVariant_h */
