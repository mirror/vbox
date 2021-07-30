/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDPageExpert class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDPageExpert_h
#define FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDPageExpert_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class QGroupBox;
class UIDiskFormatsGroupBox;
class UIDiskVariantGroupBox;
class UIMediumSizeAndPathGroupBox;

/* Expert page of the New Virtual Hard Drive wizard: */
class SHARED_LIBRARY_STUFF UIWizardNewVDPageExpert : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardNewVDPageExpert(const QString &strDefaultName, const QString &strDefaultPath, qulonglong uDefaultSize);

protected:


private slots:

    void sltMediumFormatChanged();
    void sltSelectLocationButtonClicked();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void prepare();
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;
    bool validatePage();
    void updateDiskWidgetsAfterMediumFormatChange();


    /* Widgets: */
    UIMediumSizeAndPathGroupBox *m_pSizeAndPathGroup;
    UIDiskFormatsGroupBox *m_pFormatGroup;
    UIDiskVariantGroupBox *m_pVariantGroup;

    QString m_strDefaultName;
    QString m_strDefaultPath;
    qulonglong m_uDefaultSize;
    qulonglong m_uMediumSizeMin;
    qulonglong m_uMediumSizeMax;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDPageExpert_h */
