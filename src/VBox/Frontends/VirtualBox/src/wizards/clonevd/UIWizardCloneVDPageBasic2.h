/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVDPageBasic2 class declaration
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

#ifndef __UIWizardCloneVDPageBasic2_h__
#define __UIWizardCloneVDPageBasic2_h__

/* Local includes: */
#include "UIWizardPage.h"
#include "COMDefs.h"

/* Forward declarations: */
class QVBoxLayout;
class QRadioButton;
class QIRichTextLabel;
class QGroupBox;
class QButtonGroup;

/* 2nd page of the Clone Virtual Disk wizard: */
class UIWizardCloneVDPageBasic2 : public UIWizardPage
{
    Q_OBJECT;
    Q_PROPERTY(CMediumFormat mediumFormat READ mediumFormat WRITE setMediumFormat);

public:

    /* Constructor: */
    UIWizardCloneVDPageBasic2();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;

    /* Navigation stuff: */
    int nextId() const;

    /* Helping stuff: */
    QRadioButton* addFormatButton(QVBoxLayout *pFormatsLayout, CMediumFormat mediumFormat);

    /* Stuff for 'mediumFormat' field: */
    CMediumFormat mediumFormat() const;
    void setMediumFormat(const CMediumFormat &mediumFormat);

    /* Variables: */
    QRadioButton *m_pDefaultButton;
    QButtonGroup *m_pButtonGroup;
    QList<CMediumFormat> m_formats;
    QStringList m_formatNames;

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
    QGroupBox *m_pFormatContainer;
};

#endif // __UIWizardCloneVDPageBasic2_h__

