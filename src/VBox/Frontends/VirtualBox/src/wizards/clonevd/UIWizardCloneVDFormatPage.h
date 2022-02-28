/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVDFormatPage class declaration.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDFormatPage_h
#define FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDFormatPage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QSet>

/* GUI includes: */
#include "UINativeWizardPage.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QIRichTextLabel;
class UIDiskFormatsGroupBox;

/** 2nd page of the Clone Virtual Disk Image wizard (basic extension): */
class UIWizardCloneVDFormatPage : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs basic page.
      * @param  enmDeviceType  Brings the device type to limit format to. */
    UIWizardCloneVDFormatPage(KDeviceType enmDeviceType);

private slots:

    void sltMediumFormatChanged();

private:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;
    void prepare(KDeviceType enmDeviceType);

    /** Prepares the page. */
    virtual void initializePage() RT_OVERRIDE;

    /** Returns whether the page is complete. */
    virtual bool isComplete() const RT_OVERRIDE;

    /** Holds the description label instance. */
    QIRichTextLabel *m_pLabel;
    UIDiskFormatsGroupBox *m_pFormatGroupBox;

    QSet<QString> m_userModifiedParameters;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDFormatPage_h */
