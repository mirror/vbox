/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * QIWizard class declaration
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
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

/* Global includes */
#include <QWizard>

/* Local includes */
#include "QIWithRetranslateUI.h"

/* Global forwards */
class QTextEdit;

class QIWizard : public QIWithRetranslateUI<QWizard>
{
    Q_OBJECT;

public:

    QIWizard(QWidget *pParent);

protected:

    void resizeToGoldenRatio();
    void assignWatermark(const QString &strWaterMark);
    void assignBackground(const QString &strBg);

private:

    void resizeAccordingLabelWidth(int iLabelWidth);
};

class QIWizardPage : public QIWithRetranslateUI<QWizardPage>
{
    Q_OBJECT;

public:

    QIWizardPage();

    QSize minimumSizeHint() const;
    void setMinimumSizeHint(const QSize &minimumSizeHint);

protected:

    static void setSummaryFieldLinesNumber(QTextEdit *pSummaryField, int iNumber);

    QString standardHelpText() const;

private:

    QSize m_MinimumSizeHint;
};

#endif // __QIWizard_h__

