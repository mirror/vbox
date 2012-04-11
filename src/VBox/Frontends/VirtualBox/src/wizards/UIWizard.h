/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizard class declaration
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWizard_h__
#define __UIWizard_h__

/* Global includes: */
#include <QWizard>

/* Local includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class UIWizardPage;

/* QWizard class reimplementation with extended funtionality. */
class UIWizard : public QIWithRetranslateUI<QWizard>
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIWizard(QWidget *pParent);

protected:

    /* Wizard type: */
    enum UIWizardType
    {
        UIWizardType_NewVM,
        UIWizardType_CloneVM,
        UIWizardType_ExportAppliance,
        UIWizardType_ImportAppliance,
        UIWizardType_FirstRun,
        UIWizardType_NewVD,
        UIWizardType_CloneVD
    };

    /* Page related methods: */
    int addPage(UIWizardPage *pPage);
    void setPage(int iId, UIWizardPage *pPage);

    /* Translation stuff: */
    void retranslateAllPages();

    /* Adjusting stuff: */
    void resizeToGoldenRatio(UIWizardType wizardType);

    /* Design stuff: */
#ifndef Q_WS_MAC
    void assignWatermark(const QString &strWaterMark);
#else
    void assignBackground(const QString &strBackground);
#endif

    /* Show event: */
    void showEvent(QShowEvent *pShowEvent);

private:

    /* Helpers: */
    void configurePage(UIWizardPage *pPage);
    void resizeAccordingLabelWidth(int iLabelWidth);
    double ratioForWizardType(UIWizardType wizardType);
#ifndef Q_WS_MAC
    int proposedWatermarkHeight();
    void assignWatermarkHelper();
    QString m_strWatermarkName;
#endif /* !Q_WS_MAC */
};

#endif // __UIWizard_h__

