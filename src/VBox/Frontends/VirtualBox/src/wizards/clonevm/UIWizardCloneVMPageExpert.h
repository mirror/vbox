/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVMPageExpert class declaration.
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWizardCloneVMPageExpert_h__
#define __UIWizardCloneVMPageExpert_h__

/* Local includes: */
#include "UIWizardCloneVMPageBasic1.h"
#include "UIWizardCloneVMPageBasic2.h"
#include "UIWizardCloneVMPageBasic3.h"

/* Forward declarations: */
class QGroupBox;

/* Expert page of the Clone Virtual Machine wizard: */
class UIWizardCloneVMPageExpert : public UIWizardPage,
                                  public UIWizardCloneVMPage1,
                                  public UIWizardCloneVMPage2,
                                  public UIWizardCloneVMPage3
{
    Q_OBJECT;
    Q_PROPERTY(QString cloneName READ cloneName WRITE setCloneName);
    Q_PROPERTY(QString cloneFilePath READ cloneFilePath WRITE setCloneFilePath);
    Q_PROPERTY(bool linkedClone READ isLinkedClone);
    Q_PROPERTY(KCloneMode cloneMode READ cloneMode WRITE setCloneMode);
    Q_PROPERTY(MACAddressClonePolicy macAddressClonePolicy READ macAddressClonePolicy WRITE setMACAddressClonePolicy);
    Q_PROPERTY(bool keepDiskNames READ keepDiskNames WRITE setKeepDiskNames);
    Q_PROPERTY(bool keepHWUUIDs READ keepHWUUIDs WRITE setKeepHWUUIDs);

public:

    /* Constructor: */
    UIWizardCloneVMPageExpert(const QString &strOriginalName, const QString &strDefaultPath,
                              bool fAdditionalInfo, bool fShowChildsOption, const QString &strGroup);

private slots:

    /* Button toggle handler: */
    void sltButtonToggled(QAbstractButton *pButton, bool fChecked);
    void sltNameChanged();
    void sltPathChanged();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;
    bool validatePage();

    /* Widgets: */
    QGroupBox   *m_pNameCnt;
    QGroupBox   *m_pCloneTypeCnt;
    QGroupBox   *m_pCloneModeCnt;
    QGroupBox   *m_pCloneOptionsCnt;
    QGridLayout *m_pCloneOptionsLayout;
};

#endif // __UIWizardCloneVMPageExpert_h__
