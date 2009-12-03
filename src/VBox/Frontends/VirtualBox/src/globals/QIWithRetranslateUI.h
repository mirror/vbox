/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Declarations of utility classes and functions
 * VirtualBox Qt extensions: QIWithRetranslateUI class declaration
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef __QIWithRetranslateUI_h
#define __QIWithRetranslateUI_h

/* Qt includes */
#include <QObject>
#include <QEvent>

template <class Base>
class QIWithRetranslateUI: public Base
{
public:

    QIWithRetranslateUI (QWidget *aParent = 0) : Base (aParent) {}

protected:

    virtual void changeEvent (QEvent *aEvent)
    {
        Base::changeEvent (aEvent);
        switch (aEvent->type())
        {
            case QEvent::LanguageChange:
            {
                retranslateUi();
                aEvent->accept();
                break;
            }
            default: 
                break;
        }
    }

    virtual void retranslateUi() = 0;
};

template <class Base>
class QIWithRetranslateUI2: public Base
{
public:

    QIWithRetranslateUI2 (QWidget *aParent = 0, Qt::WindowFlags aFlags = 0) : Base (aParent, aFlags) {}
    
protected:

    virtual void changeEvent (QEvent *aEvent)
    {
        Base::changeEvent (aEvent);
        switch (aEvent->type())
        {
            case QEvent::LanguageChange:
            {
                retranslateUi();
                aEvent->accept();
                break;
            }
            default: 
                break;
        }
    }

    virtual void retranslateUi() = 0;
};

#endif /* __QIWithRetranslateUI_h */

