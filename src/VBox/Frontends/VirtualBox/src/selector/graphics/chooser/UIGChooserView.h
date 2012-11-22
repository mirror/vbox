/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooserView class declaration
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGChooserView_h__
#define __UIGChooserView_h__

/* Qt includes: */
#include <QGraphicsView>

/* Forward declarations: */
class UIGChooserItem;

/* Graphics chooser-view: */
class UIGChooserView : public QGraphicsView
{
    Q_OBJECT;

signals:

    /* Notifier: Resize stuff: */
    void sigResized();

public:

    /* Constructor: */
    UIGChooserView(QWidget *pParent);

private slots:

    /* Handler: Root-item stuff: */
    void sltHandleRootItemResized(const QSizeF &size, int iMinimumWidth);

    /* Handler: Focus-item stuff: */
    void sltFocusChanged(UIGChooserItem *pFocusItem);

private:

    /* Handler: Resize-event stuff: */
    void resizeEvent(QResizeEvent *pEvent);

    /* Helper: Update stuff: */
    void updateSceneRect(const QSizeF &sizeHint = QSizeF());
};

#endif /* __UIGChooserView_h__ */

