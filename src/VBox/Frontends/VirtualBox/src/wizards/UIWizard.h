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

/* Qt includes: */
#include <QWizard>
#include <QPointer>

/* Local includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class UIWizardPage;

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

/* Wizard mode: */
enum UIWizardMode
{
    UIWizardMode_Auto,
    UIWizardMode_Basic,
    UIWizardMode_Expert
};

/* QWizard class reimplementation with extended funtionality. */
class UIWizard : public QIWithRetranslateUI<QWizard>
{
    Q_OBJECT;

public:

    /* Mode related stuff: */
    UIWizardMode mode() { return m_mode; }

    /* Page related methods: */
    virtual void prepare();

protected slots:

    /* Page change handler: */
    virtual void sltCurrentIdChanged(int iId);
    /* Custom button 1 click handler: */
    virtual void sltCustomButtonClicked(int iId);

protected:

    /* Constructor: */
    UIWizard(QWidget *pParent, UIWizardType type, UIWizardMode mode = UIWizardMode_Auto);

    /* Translation stuff: */
    void retranslateUi();
    void retranslatePages();

    /* Page related methods: */
    void setPage(int iId, UIWizardPage *pPage);
    void cleanup();

    /* Adjusting stuff: */
    void resizeToGoldenRatio();

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
    double ratio();
#ifndef Q_WS_MAC
    int proposedWatermarkHeight();
    void assignWatermarkHelper();
#endif /* !Q_WS_MAC */
    static QString nameForType(UIWizardType type);
    static UIWizardMode loadModeForType(UIWizardType type);

    /* Variables: */
    UIWizardType m_type;
    UIWizardMode m_mode;
#ifndef Q_WS_MAC
    QString m_strWatermarkName;
#endif /* !Q_WS_MAC */
};

typedef QPointer<UIWizard> UISafePointerWizard;

#endif // __UIWizard_h__

