/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * QIWizard class declaration
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

#ifndef __QIWizard_h__
#define __QIWizard_h__

/* Global includes: */
#include <QWizard>

/* Local includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QIWizardPage;

/* QWizard class reimplementation with extended funtionality. */
class QIWizard : public QIWithRetranslateUI<QWizard>
{
    Q_OBJECT;

public:

    /* Constructor: */
    QIWizard(QWidget *pParent);

protected:

    /* Page related methods: */
    int addPage(QIWizardPage *pPage);
    void setPage(int iId, QIWizardPage *pPage);

    /* Translation stuff: */
    void retranslateAllPages();

    /* Adjusting stuff: */
    void resizeToGoldenRatio(double dRatio = 1.6);

    /* Design stuff: */
#ifndef Q_WS_MAC
    void assignWatermark(const QString &strWaterMark);
#else
    void assignBackground(const QString &strBackground);
#endif

private:

    /* Helpers: */
    void configurePage(QIWizardPage *pPage);
    void resizeAccordingLabelWidth(int iLabelWidth);
#ifndef Q_WS_MAC
    int proposedWatermarkHeight();
    void assignWatermarkHelper();
    QString m_strWatermarkName;
#endif /* !Q_WS_MAC */
};

/* QWizardPage class reimplementation with extended funtionality. */
class QIWizardPage : public QIWithRetranslateUI<QWizardPage>
{
    Q_OBJECT;

public:

    /* Constructor: */
    QIWizardPage();

    /* Translation stuff: */
    void retranslate() { retranslateUi(); }

protected:

    /* Helpers: */
    QIWizard* wizard() const;
    QString standardHelpText() const;
    void startProcessing();
    void endProcessing();
};

#endif // __QIWizard_h__

