/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVD class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVD_h
#define FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVD_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizard.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"
#include "CMediumFormat.h"

#define newVDWizardPropertySet(functionName, value)                      \
    do                                                                   \
    {                                                                    \
        UIWizardNewVD *pWizard = qobject_cast<UIWizardNewVD*>(wizard()); \
        if (pWizard)                                                     \
            pWizard->set##functionName(value);                           \
    }                                                                    \
    while(0)

/* New Virtual Hard Drive wizard: */
class SHARED_LIBRARY_STUFF UIWizardNewVD : public UINativeWizard
{
    Q_OBJECT;

public:

    UIWizardNewVD(QWidget *pParent,
                  const QString &strDefaultName, const QString &strDefaultPath,
                  qulonglong uDefaultSize, WizardMode mode = WizardMode_Auto);


    qulonglong mediumVariant() const;
    void setMediumVariant(qulonglong uMediumVariant);

    const CMediumFormat &mediumFormat();
    void setMediumFormat(const CMediumFormat &mediumFormat);

    const QString &mediumPath() const;
    void setMediumPath(const QString &strMediumPath);

    qulonglong mediumSize() const;
    void setMediumSize(qulonglong mediumSize);

protected:

    /* Creates virtual-disk: */
    bool createVirtualDisk();

    virtual void populatePages() /* final override */;

private:

    /* Translation stuff: */
    void retranslateUi();

    qulonglong m_uMediumVariant;
    CMediumFormat m_comMediumFormat;
    QString m_strMediumPath;
    qulonglong m_uMediumSize;


    QString     m_strDefaultName;
    QString     m_strDefaultPath;
    qulonglong  m_uDefaultSize;
};

typedef QPointer<UIWizardNewVD> UISafePointerWizardNewVD;

#endif /* !FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVD_h */
