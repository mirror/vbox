/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QISplitter class declaration.
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

#ifndef ___QISplitter_h___
#define ___QISplitter_h___

/* Qt includes: */
#include <QSplitter>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class QSplitterHandle;

/** QSplitter subclass with extended functionality. */
class SHARED_LIBRARY_STUFF QISplitter : public QSplitter
{
    Q_OBJECT;

public:

    /** Handle types. */
    enum Type { Native, Shade };

    /** Constructs splitter passing @a pParent to the base-class. */
    QISplitter(QWidget *pParent = 0);
    /** Constructs splitter passing @a orientation and @a pParent to the base-class. */
    QISplitter(Qt::Orientation orientation, QWidget *pParent = 0);

    /** Defines handle @a type. */
    void setHandleType(Type type) { m_type = type; }
    /** Returns handle type. */
    Type handleType() const { return m_type; }

    /** Configure custom colors defined as @a color1 and @a color2. */
    void configureColors(const QColor &color1, const QColor &color2) { m_color1 = color1; m_color2 = color2; }

protected:

    /** Preprocesses Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

    /** Handles show @a pEvent. */
    void showEvent(QShowEvent *pEvent);

    /** Creates handle. */
    QSplitterHandle *createHandle();

private:

    /** Holds the serialized base-state. */
    QByteArray m_baseState;

    /** Holds the handle type. */
    Type m_type;

    /** Holds whether the splitter is polished. */
    bool m_fPolished : 1;
#ifdef VBOX_WS_MAC
    /** Holds whether handle is grabbed. */
    bool m_fHandleGrabbed : 1;
#endif /* VBOX_WS_MAC */

    /** Holds color1. */
    QColor m_color1;
    /** Holds color2. */
    QColor m_color2;
};

#endif /* !___QISplitter_h___ */
