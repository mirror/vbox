/* $Id$ */
/** @file
 * VBox Qt GUI - UINativeWizardPage class declaration.
 */

/*
 * Copyright (C) 2009-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_UINativeWizardPage_h
#define FEQT_INCLUDED_SRC_wizards_UINativeWizardPage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class UINativeWizard;

/** QWidget extension with advanced functionality emulating QWizardPage behavior. */
class SHARED_LIBRARY_STUFF UINativeWizardPage : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies about page validity changes. */
    void completeChanged();

public:

    /** Constructs wizard page. */
    UINativeWizardPage();

    /** Redirects the translation call to actual handler. */
    void retranslate() { retranslateUi(); }

    /** Defines page @a strTitle. */
    void setTitle(const QString &strTitle);
    /** Returns page title. */
    QString title() const;

    /** Handles the page initialization. */
    virtual void initializePage() {}
    /** Tests the page for completeness, enables the Next button if Ok.
      * @returns whether all conditions to go to next page are satisfied. */
    virtual bool isComplete() const { return true; }
    /** Tests the page for validity, tranfers to the Next page is Ok.
      * @returns whether page state to go to next page is bearable. */
    virtual bool validatePage() { return true; }

protected:

    /** Returns wizard this page belongs to. */
    UINativeWizard *wizard() const;

    template<typename T>
        T *wizardWindow() const
    {
        return   parentWidget() && parentWidget()->window()
            ? qobject_cast<T*>(parentWidget()->window())
            : 0;
    }


    /** Holds the page title. */
    QString  m_strTitle;
};


#endif /* !FEQT_INCLUDED_SRC_wizards_UINativeWizardPage_h */
