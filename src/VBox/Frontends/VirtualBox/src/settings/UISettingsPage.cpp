/* $Id$ */
/** @file
 * VBox Qt GUI - UISettingsPage class implementation.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "UIConverter.h"
# include "UISettingsPage.h"
# include "QIWidgetValidator.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UISettingsPage implementation.                                                                                         *
*********************************************************************************************************************************/

UISettingsPage::UISettingsPage()
    : m_enmConfigurationAccessLevel(ConfigurationAccessLevel_Null)
    , m_cId(-1)
    , m_pFirstWidget(0)
    , m_pValidator(0)
    , m_fIsValidatorBlocked(true)
    , m_fProcessed(false)
    , m_fFailed(false)
{
}

void UISettingsPage::notifyOperationProgressError(const QString &strErrorInfo)
{
    QMetaObject::invokeMethod(this,
                              "sigOperationProgressError",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(QString, strErrorInfo));
}

void UISettingsPage::setValidator(UIPageValidator *pValidator)
{
    /* Make sure validator is not yet assigned: */
    AssertMsg(!m_pValidator, ("Validator already assigned!\n"));
    if (m_pValidator)
        return;

    /* Assign validator: */
    m_pValidator = pValidator;
}

void UISettingsPage::setConfigurationAccessLevel(ConfigurationAccessLevel enmConfigurationAccessLevel)
{
    m_enmConfigurationAccessLevel = enmConfigurationAccessLevel;
    polishPage();
}

void UISettingsPage::revalidate()
{
    /* Revalidate if possible: */
    if (m_pValidator && !m_fIsValidatorBlocked)
        m_pValidator->revalidate();
}


/*********************************************************************************************************************************
*   Class UISettingsPageGlobal implementation.                                                                                   *
*********************************************************************************************************************************/

UISettingsPageGlobal::UISettingsPageGlobal()
{
}

GlobalSettingsPageType UISettingsPageGlobal::internalID() const
{
    return static_cast<GlobalSettingsPageType>(id());
}

QString UISettingsPageGlobal::internalName() const
{
    return gpConverter->toInternalString(internalID());
}

QPixmap UISettingsPageGlobal::warningPixmap() const
{
    return gpConverter->toWarningPixmap(internalID());
}

void UISettingsPageGlobal::fetchData(const QVariant &data)
{
    /* Fetch data to m_properties: */
    m_properties = data.value<UISettingsDataGlobal>().m_properties;
}

void UISettingsPageGlobal::uploadData(QVariant &data) const
{
    /* Upload m_properties to data: */
    data = QVariant::fromValue(UISettingsDataGlobal(m_properties));
}


/*********************************************************************************************************************************
*   Class UISettingsPageMachine implementation.                                                                                  *
*********************************************************************************************************************************/

UISettingsPageMachine::UISettingsPageMachine()
{
}

MachineSettingsPageType UISettingsPageMachine::internalID() const
{
    return static_cast<MachineSettingsPageType>(id());
}

QString UISettingsPageMachine::internalName() const
{
    return gpConverter->toInternalString(internalID());
}

QPixmap UISettingsPageMachine::warningPixmap() const
{
    return gpConverter->toWarningPixmap(internalID());
}

void UISettingsPageMachine::fetchData(const QVariant &data)
{
    /* Fetch data to m_machine & m_console: */
    m_machine = data.value<UISettingsDataMachine>().m_machine;
    m_console = data.value<UISettingsDataMachine>().m_console;
}

void UISettingsPageMachine::uploadData(QVariant &data) const
{
    /* Upload m_machine & m_console to data: */
    data = QVariant::fromValue(UISettingsDataMachine(m_machine, m_console));
}
