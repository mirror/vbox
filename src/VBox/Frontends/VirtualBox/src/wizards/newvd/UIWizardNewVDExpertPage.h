/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDExpertPage class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDExpertPage_h
#define FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDExpertPage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class QGroupBox;
class UIDiskFormatsComboBox;
class UIDiskVariantWidget;
class UIMediumSizeAndPathGroupBox;

/** Expert page of the New Virtual Hard Drive wizard. */
class SHARED_LIBRARY_STUFF UIWizardNewVDExpertPage : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardNewVDExpertPage(const QString &strDefaultName, const QString &strDefaultPath, qulonglong uDefaultSize);

private slots:

    void sltMediumFormatChanged();
    void sltSelectLocationButtonClicked();
    void sltMediumVariantChanged(qulonglong uVariant);
    void sltMediumPathChanged(const QString &strPath);
    void sltMediumSizeChanged(qulonglong uSize);

private:

    /** Translation stuff. */
    void retranslateUi();

    /** Prepare stuff. */
    void prepare();
    void initializePage();

    /** Validation stuff. */
    bool isComplete() const;
    bool validatePage();
    void updateDiskWidgetsAfterMediumFormatChange();

   /** @name Widgets
     * @{ */
       UIMediumSizeAndPathGroupBox *m_pSizeAndPathGroup;
       UIDiskFormatsComboBox *m_pFormatComboBox;
       UIDiskVariantWidget *m_pVariantWidget;
       QGroupBox *m_pFormatVariantGroupBox;
   /** @} */

   /** @name Variable
     * @{ */
       QString m_strDefaultName;
       QString m_strDefaultPath;
       qulonglong m_uDefaultSize;
       qulonglong m_uMediumSizeMin;
       qulonglong m_uMediumSizeMax;
   /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDExpertPage_h */
