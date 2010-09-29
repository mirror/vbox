/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UISettingsPage class declaration
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UISettingsPage_h__
#define __UISettingsPage_h__

/* Global includes */
#include <QWidget>

/* Local includes */
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"

/* Forward declarations */
class VBoxGlobalSettings;
class QIWidgetValidator;

/* Settings page base class: */
class UISettingsPage : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /* Settings page constructor: */
    UISettingsPage(QWidget *pParent = 0)
        : QIWithRetranslateUI<QWidget>(pParent)
        , m_pFirstWidget(0)
    {
    }

    /* Global settings set/get stuff */
    virtual void getFrom (const CSystemProperties & /* aProps */,
                          const VBoxGlobalSettings & /* aGs */) {}
    virtual void putBackTo (CSystemProperties & /* aProps */,
                            VBoxGlobalSettings & /* aGs */) {}

    /* VM settings set/get stuff */
    virtual void getFrom (const CMachine & /* aMachine */) {}
    virtual void putBackTo() {}

    /* Validation stuff: */
    virtual void setValidator(QIWidgetValidator * /* pValidator */) {}
    virtual bool revalidate(QString & /* strWarningText */, QString & /* strTitle */) { return true; }

    /* Navigation stuff: */
    virtual void setOrderAfter(QWidget *pWidget) { m_pFirstWidget = pWidget; }

protected:

    QWidget *m_pFirstWidget;
};

#endif // __UISettingsPage_h__

