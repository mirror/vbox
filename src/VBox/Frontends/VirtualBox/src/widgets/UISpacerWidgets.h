/* $Id$ */
/** @file
 * VBox Qt GUI - UISpacerWidgets declarations.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UISpacerWidgets_h__
#define __UISpacerWidgets_h__

/* Global includes */
#include <QWidget>

class UISpacerWidget: public QWidget
{
    Q_OBJECT;

public:

    UISpacerWidget(QWidget *pParent = 0)
      : QWidget(pParent)
    {
        setContentsMargins(0, 0, 0, 0);
        setOrientation(Qt::Horizontal);
    }

    void setOrientation(Qt::Orientation o)
    {
        m_orientation = o;
        if (m_orientation == Qt::Horizontal)
            setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
        else
            setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));
    }
    Qt::Orientation orientation() const { return m_orientation; }

    QSize sizeHint() const { return QSize(0, 0); }

private:
    /* Private member vars */
    Qt::Orientation m_orientation;
};

class UIHorizontalSpacerWidget: public UISpacerWidget
{
    Q_OBJECT;

public:
    UIHorizontalSpacerWidget(QWidget *pParent = 0)
      : UISpacerWidget(pParent)
    {}
};

class UIVerticalSpacerWidget: public UISpacerWidget
{
    Q_OBJECT;

public:
    UIVerticalSpacerWidget(QWidget *pParent = 0)
      : UISpacerWidget(pParent)
    {
        setOrientation(Qt::Vertical);
    }
};

#endif /* !__UISpacerWidgets_h__ */

