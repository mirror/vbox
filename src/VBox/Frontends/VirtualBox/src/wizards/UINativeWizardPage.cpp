/* $Id$ */
/** @file
 * VBox Qt GUI - UINativeWizardPage class implementation.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UINativeWizard.h"
#include "UINativeWizardPage.h"


UINativeWizardPage::UINativeWizardPage()
{
}

void UINativeWizardPage::setTitle(const QString &strTitle)
{
    m_strTitle = strTitle;
}

QString UINativeWizardPage::title() const
{
    return m_strTitle;
}

UINativeWizard *UINativeWizardPage::wizard() const
{
    return   parentWidget() && parentWidget()->window()
           ? qobject_cast<UINativeWizard*>(parentWidget()->window())
           : 0;
}
