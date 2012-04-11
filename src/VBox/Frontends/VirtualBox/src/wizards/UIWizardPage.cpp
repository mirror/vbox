/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardPage class implementation
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

/* Global includes: */
#include <QAbstractButton>

/* Local includes: */
#include "UIWizardPage.h"
#include "UIWizard.h"
#include "VBoxGlobal.h"

UIWizardPage::UIWizardPage()
{
}

UIWizard* UIWizardPage::wizard() const
{
    return qobject_cast<UIWizard*>(QWizardPage::wizard());
}

QString UIWizardPage::standardHelpText() const
{
    return tr("Use the <b>%1</b> button to go to the next page of the wizard and the "
              "<b>%2</b> button to return to the previous page. "
              "You can also press <b>%3</b> if you want to cancel the execution "
              "of this wizard.</p>")
        .arg(VBoxGlobal::replaceHtmlEntities(VBoxGlobal::removeAccelMark(wizard()->buttonText(QWizard::NextButton).remove(" >"))))
        .arg(VBoxGlobal::replaceHtmlEntities(VBoxGlobal::removeAccelMark(wizard()->buttonText(QWizard::BackButton).remove("< "))))
#ifdef Q_WS_MAC
        .arg(QKeySequence("ESC").toString()); /* There is no button shown on Mac OS X, so just say the key sequence. */
#else /* Q_WS_MAC */
        .arg(VBoxGlobal::replaceHtmlEntities(VBoxGlobal::removeAccelMark(wizard()->buttonText(QWizard::CancelButton))));
#endif /* Q_WS_MAC */
}

void UIWizardPage::startProcessing()
{
    if (isFinalPage())
        wizard()->button(QWizard::FinishButton)->setEnabled(false);
}

void UIWizardPage::endProcessing()
{
    if (isFinalPage())
        wizard()->button(QWizard::FinishButton)->setEnabled(true);
}

