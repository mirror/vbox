/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMSummaryPage class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMSummaryPage_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMSummaryPage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QSet>

/* GUI includes: */
#include "QIFileDialog.h"
#include "UINativeWizardPage.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"

/* Forward declarations: */
class QButtonGroup;
class QCheckBox;
class QRadioButton;
class QLabel;
class QIRichTextLabel;
class QIToolButton;
class UIMediaComboBox;
class UIMediumSizeEditor;


class UIWizardNewVMSummaryPage : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardNewVMSummaryPage();

protected:


private slots:


private:

    void prepare();
    void createConnections();


    virtual void retranslateUi() /* override final */;
    virtual void initializePage() /* override final */;
    virtual bool isComplete() const /* override final */;
    virtual bool validatePage() /* override final */;



    /** @name Widgets
     * @{ */
       QIRichTextLabel *m_pLabel;
    /** @} */


};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMSummaryPage_h */
