/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDPageSizeLocation class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDPageSizeLocation_h
#define FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDPageSizeLocation_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class CMediumFormat;
class UIMediumSizeAndPathGroupBox;

class SHARED_LIBRARY_STUFF UIWizardNewVDPageSizeLocation : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardNewVDPageSizeLocation(const QString &strDefaultName, const QString &strDefaultPath, qulonglong uDefaultSize);

protected:


private slots:

    void sltSelectLocationButtonClicked();
    void sltMediumSizeChanged(qulonglong uSize);
    void sltMediumPathChanged(const QString &strPath);

private:

    void retranslateUi();
    void initializePage();
    bool isComplete() const;
    bool validatePage();
    void prepare();

    UIMediumSizeAndPathGroupBox *m_pMediumSizePathGroup;
    qulonglong m_uMediumSizeMin;
    qulonglong m_uMediumSizeMax;
    QString m_strDefaultName;
    QString m_strDefaultPath;
    qulonglong m_uDefaultSize;
    QSet<QString> m_userModifiedParameters;
};


#endif /* !FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDPageSizeLocation_h */
