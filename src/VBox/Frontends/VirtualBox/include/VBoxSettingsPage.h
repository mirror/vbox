/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxSettingsPage class declaration
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxSettingsPage_h__
#define __VBoxSettingsPage_h__

#include "QIWithRetranslateUI.h"
#include "COMDefs.h"

#include <QWidget>

class VBoxGlobalSettings;
class QIWidgetValidator;

class VBoxSettingsPage : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    VBoxSettingsPage (QWidget *aParent = 0)
        : QIWithRetranslateUI<QWidget> (aParent)
        , mFirstWidget (0) {}

    /* Global settings set/get stuff */
    virtual void getFrom (const CSystemProperties & /* aProps */,
                          const VBoxGlobalSettings & /* aGs */) {}
    virtual void putBackTo (CSystemProperties & /* aProps */,
                            VBoxGlobalSettings & /* aGs */) {}

    /* VM settings set/get stuff */
    virtual void getFrom (const CMachine & /* aMachine */) {}
    virtual void putBackTo() {}

    virtual void setValidator (QIWidgetValidator * /* aVal */) {}
    virtual bool revalidate (QString & /* aWarnText */, QString & /* aTitle */)
        { return true; }

    virtual void setOrderAfter (QWidget *aWidget) { mFirstWidget = aWidget; }

protected:

    QWidget *mFirstWidget;
};

#endif // __VBoxSettingsPage_h__

