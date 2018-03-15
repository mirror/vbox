/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIWithRetranslateUI class declaration.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QIWithRetranslateUI_h___
#define ___QIWithRetranslateUI_h___

/* Qt includes: */
#include <QApplication>
#include <QEvent>
#include <QGraphicsWidget>
#include <QObject>
#include <QWidget>


/** Template for automatic language translations of underlying QWidget. */
template <class Base>
class QIWithRetranslateUI : public Base
{
public:

    /** Constructs translatable widget passing @a pParent to the base-class. */
    QIWithRetranslateUI(QWidget *pParent = 0) : Base(pParent) {}

protected:

    /** Handles standard Qt change @a pEvent. */
    virtual void changeEvent(QEvent *pEvent)
    {
        /* Call to base-class: */
        Base::changeEvent(pEvent);
        /* Handle LanguageChange events: */
        switch (pEvent->type())
        {
            case QEvent::LanguageChange:
            {
                retranslateUi();
                pEvent->accept();
                break;
            }
            default:
                break;
        }
    }

    /** Handles translation event. */
    virtual void retranslateUi() = 0;
};


/** Template for automatic language translations of underlying QWidget with certain flags. */
template <class Base>
class QIWithRetranslateUI2 : public Base
{
public:

    /** Constructs translatable widget passing @a pParent and @a fFlags to the base-class. */
    QIWithRetranslateUI2(QWidget *pParent = 0, Qt::WindowFlags fFlags = 0) : Base(pParent, fFlags) {}

protected:

    /** Handles standard Qt change @a pEvent. */
    virtual void changeEvent(QEvent *pEvent)
    {
        /* Call to base-class: */
        Base::changeEvent(pEvent);
        /* Handle LanguageChange events: */
        switch (pEvent->type())
        {
            case QEvent::LanguageChange:
            {
                retranslateUi();
                pEvent->accept();
                break;
            }
            default:
                break;
        }
    }

    /** Handles translation event. */
    virtual void retranslateUi() = 0;
};


/** Template for automatic language translations of underlying QObject. */
template <class Base>
class QIWithRetranslateUI3 : public Base
{
public:

    /** Constructs translatable widget passing @a pParent to the base-class. */
    QIWithRetranslateUI3(QObject *pParent = 0)
        : Base(pParent)
    {
        qApp->installEventFilter(this);
    }

protected:

    /** Pre-handles standard Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent)
    {
        /* Handle LanguageChange events for qApp only: */
        if (pObject == qApp)
        {
            switch (pEvent->type())
            {
                case QEvent::LanguageChange: retranslateUi(); break;
                default: break;
            }
        }
        /* Call to base-class: */
        return Base::eventFilter(pObject, pEvent);
    }

    /** Handles translation event. */
    virtual void retranslateUi() = 0;
};


/** Template for automatic language translations of underlying QGraphicsWidget. */
template <class Base>
class QIWithRetranslateUI4 : public Base
{
public:

    /** Constructs translatable widget passing @a pParent to the base-class. */
    QIWithRetranslateUI4(QGraphicsWidget *pParent = 0)
        : Base(pParent)
    {
        qApp->installEventFilter(this);
    }

protected:

    /** Pre-handles standard Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent)
    {
        /* Handle LanguageChange events: */
        switch (pEvent->type())
        {
            case QEvent::LanguageChange: retranslateUi(); break;
            default: break;
        }
        /* Call to base-class: */
        return Base::eventFilter(pObject, pEvent);
    }

    /** Handles translation event. */
    virtual void retranslateUi() = 0;
};


#endif /* !___QIWithRetranslateUI_h___ */

