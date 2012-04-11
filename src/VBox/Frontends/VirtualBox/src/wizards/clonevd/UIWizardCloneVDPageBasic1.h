/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVDPageBasic1 class declaration
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

#ifndef __UIWizardCloneVDPageBasic1_h__
#define __UIWizardCloneVDPageBasic1_h__

/* Local includes: */
#include "UIWizardPage.h"
#include "COMDefs.h"

/* Forward declarations: */
class QIRichTextLabel;
class QGroupBox;
class VBoxMediaComboBox;
class QIToolButton;

/* 1st page of the Clone Virtual Disk wizard: */
class UIWizardCloneVDPageBasic1 : public UIWizardPage
{
    Q_OBJECT;
    Q_PROPERTY(CMedium sourceVirtualDisk READ sourceVirtualDisk WRITE setSourceVirtualDisk);

public:

    /* Constructor: */
    UIWizardCloneVDPageBasic1(const CMedium &sourceVirtualDisk);

private slots:

    /* Handlers for source virtual-disk change: */
    void sltHandleOpenSourceDiskClick();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;

    /* Stuff for 'sourceVirtualDisk' field: */
    CMedium sourceVirtualDisk() const;
    void setSourceVirtualDisk(const CMedium &sourceVirtualDisk);

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
    QGroupBox *m_pSourceDiskContainer;
    VBoxMediaComboBox *m_pSourceDiskSelector;
    QIToolButton *m_pOpenSourceDiskButton;
};

#endif // __UIWizardCloneVDPageBasic1_h__

