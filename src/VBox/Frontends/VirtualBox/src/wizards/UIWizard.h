/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizard class declaration.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIWizard_h___
#define ___UIWizard_h___

/* Qt includes: */
#include <QPointer>
#include <QWizard>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIExtraDataDefs.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QShowEvent;
class QString;
class QWidget;
class UIWizardPage;

/** QWizard extension with advanced functionality. */
class SHARED_LIBRARY_STUFF UIWizard : public QIWithRetranslateUI<QWizard>
{
    Q_OBJECT;

public:

    /** Returns wizard mode. */
    WizardMode mode() const { return m_enmMode; }

    /** Prepare all. */
    virtual void prepare();

protected:

    /** Constructs wizard passing @a pParent to the base-class.
      * @param  enmType  Brings the wizard type.
      * @param  enmMode  Brings the wizard mode. */
    UIWizard(QWidget *pParent, WizardType enmType, WizardMode enmMode = WizardMode_Auto);

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;

    /** Assigns @a pPage as a wizard page with certain @a iId. */
    void setPage(int iId, UIWizardPage *pPage);
    /** Removes all the pages. */
    void cleanup();

    /** Resizes wizard to golden ratio. */
    void resizeToGoldenRatio();

#ifndef VBOX_WS_MAC
    /** Assigns @a strWaterMark. */
    void assignWatermark(const QString &strWaterMark);
#else
    /** Assigns @a strBackground. */
    void assignBackground(const QString &strBackground);
#endif

protected slots:

    /** Handles current-page change to page with @a iId. */
    virtual void sltCurrentIdChanged(int iId);
    /** Handles custome-button click for button with @a iId. */
    virtual void sltCustomButtonClicked(int iId);

private:

    /** Performs pages translation. */
    void retranslatePages();

    /** Configures certain @a pPage. */
    void configurePage(UIWizardPage *pPage);

    /** Resizes wizard according certain @a iLabelWidth. */
    void resizeAccordingLabelWidth(int iLabelWidth);

    /** Returns ratio corresponding to current wizard type. */
    double ratio() const;

#ifndef VBOX_WS_MAC
    /** Returns proposed watermark height. */
    int proposedWatermarkHeight();
    /** Assigns cached watermark. */
    void assignWatermarkHelper();
#endif

    /** Holds the wizard type. */
    WizardType  m_enmType;
    /** Holds the wizard mode. */
    WizardMode  m_enmMode;
#ifndef VBOX_WS_MAC
    /** Holds the watermark name. */
    QString     m_strWatermarkName;
#endif
};

/** Wizard interface safe-pointer. */
typedef QPointer<UIWizard> UISafePointerWizard;

#endif /* !___UIWizard_h___ */
