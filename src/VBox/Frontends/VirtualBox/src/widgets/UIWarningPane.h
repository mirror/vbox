/* $Id$ */
/** @file
 * VBox Qt GUI - UIWarningPane class declaration.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIWarningPane_h___
#define ___UIWarningPane_h___

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class QHBoxLayout;
class QEvent;
class QLabel;
class QObject;
class QString;
class QTimer;
class QWidget;
class UIPageValidator;

/** QWidget subclass used a settings dialog warning-pane. */
class SHARED_LIBRARY_STUFF UIWarningPane : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies about hover enter event.
      * @param  pValidator  Brings the validator reference. */
    void sigHoverEnter(UIPageValidator *pValidator);
    /** Notifies about hover leave event.
      * @param  pValidator  Brings the validator reference. */
    void sigHoverLeave(UIPageValidator *pValidator);

public:

    /** Constructs warning-pane passing @a pParent to the base-class. */
    UIWarningPane(QWidget *pParent = 0);

    /** Defines current @a strWarningLabel text. */
    void setWarningLabel(const QString &strWarningLabel);

    /** Registers corresponding @a pValidator. */
    void registerValidator(UIPageValidator *pValidator);

protected:

    /** Preprocesses Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

private slots:

    /** Handles hover timer timeout. */
    void sltHandleHoverTimer();

private:

    /** Prepares all. */
    void prepare();

    /** Holds the icon layout instance. */
    QHBoxLayout *m_pIconLayout;
    /** Holds the text label instance. */
    QLabel      *m_pTextLabel;

    /** Holds the page validators list. */
    QList<UIPageValidator*>  m_validators;
    /** Holds the page icons list. */
    QList<QLabel*>           m_icons;
    /** Holds the icons hovered-states list. */
    QList<bool>              m_hovered;

    /** Holds the hover timer instance. */
    QTimer *m_pHoverTimer;
    /** Holds the hovered icon-label position. */
    int     m_iHoveredIconLabelPosition;
};

#endif /* !___UIWarningPane_h___ */
