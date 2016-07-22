/* $Id$ */
/** @file
 * VBox Qt GUI - UIBar class declaration.
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

#ifndef __UIBar_h__
#define __UIBar_h__

/* Global includes */
#include <QWidget>

class UIBar : public QWidget
{
    Q_OBJECT;

public:

    UIBar(QWidget *pParent = 0);
    QSize sizeHint() const;

    void setContentWidget(QWidget *pWidget);
    QWidget* contentWidget() const;

protected:

    void paintEvent(QPaintEvent *pEvent);

#ifdef VBOX_WS_MAC
    void paintContentDarwin(QPainter *pPainter);
#else /* VBOX_WS_MAC */
    void paintContent(QPainter *pPainter);
#endif /* !VBOX_WS_MAC */

private:

    /* Private member vars */
    QWidget *m_pContentWidget;
};

class UIMainBar: public UIBar
{
    Q_OBJECT;

public:

    UIMainBar(QWidget *pParent = 0);

protected:

    void paintEvent(QPaintEvent *pEvent);

private:

    /* Private member vars */
    bool m_fShowBetaLabel;
};

#endif /* !__UIBar_h__ */

