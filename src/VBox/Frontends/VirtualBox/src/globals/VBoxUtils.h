/* $Id$ */
/** @file
 * VBox Qt GUI - Declarations of utility classes and functions.
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

#ifndef ___VBoxUtils_h___
#define ___VBoxUtils_h___

/* Qt includes: */
#include <QMouseEvent>
#include <QWidget>
#include <QTextBrowser>

/* GUI includes: */
#include "UILibraryDefs.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

/* Other VBox includes: */
#include <iprt/types.h>


/** QObject subclass,
  * allowing to apply string-property value for a certain QObject. */
class SHARED_LIBRARY_STUFF QObjectPropertySetter : public QObject
{
    Q_OBJECT;

public:

    /** Constructs setter for a property with certain @a strName, passing @a pParent to the base-class. */
    QObjectPropertySetter(QObject *pParent, const QString &strName)
        : QObject(pParent), m_strName(strName)
    {}

public slots:

    /** Assigns string property @a strValue. */
    void sltAssignProperty(const QString &strValue)
    {
        parent()->setProperty(m_strName.toLatin1().constData(), strValue);
    }

private:

    /** Holds the property name. */
    const QString m_strName;
};


#endif /* !___VBoxUtils_h___ */

