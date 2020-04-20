/* $Id$ */
/** @file
 * VBox Qt GUI - UIQObjectStuff class implementation.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
#include "UIQObjectStuff.h"


UIQObjectPropertySetter::UIQObjectPropertySetter(QObject *pObject, const QString &strPropertyName, const QVariant &value)
    : m_strPropertyName(strPropertyName)
    , m_value(value)
{
    /* Add object into list: */
    m_objects << pObject;
    /* Init properties: */
    init();
}

UIQObjectPropertySetter::UIQObjectPropertySetter(const QList<QObject*> &objects, const QString &strPropertyName, const QVariant &value)
    : m_strPropertyName(strPropertyName)
    , m_value(value)
{
    /* Add objects into list: */
    foreach (QObject *pObject, objects)
        m_objects << pObject;
    /* Init properties: */
    init();
}

UIQObjectPropertySetter::~UIQObjectPropertySetter()
{
    /* Deinit properties: */
    deinit();
    /* Notify listeners that we are done: */
    emit sigAboutToBeDestroyed();
}

void UIQObjectPropertySetter::init()
{
    foreach (const QPointer<QObject> &pObject, m_objects)
    {
        if (pObject)
        {
            pObject->setProperty(m_strPropertyName.toLatin1().constData(), m_value);
            //printf("UIQObjectPropertySetter::UIQObjectPropertySetter: Property {%s} set.\n",
            //       m_strPropertyName.toLatin1().constData());
        }
    }
}

void UIQObjectPropertySetter::deinit()
{
    foreach (const QPointer<QObject> &pObject, m_objects)
    {
        if (pObject)
        {
            pObject->setProperty(m_strPropertyName.toLatin1().constData(), QVariant());
            //printf("UIQObjectPropertySetter::~UIQObjectPropertySetter: Property {%s} cleared.\n",
            //       m_strPropertyName.toLatin1().constData());
        }
    }
}
