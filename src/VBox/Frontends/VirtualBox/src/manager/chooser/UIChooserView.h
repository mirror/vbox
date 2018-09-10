/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserView class declaration.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIChooserView_h___
#define ___UIChooserView_h___

/* GUI includes: */
#include "QIGraphicsView.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class UIChooser;

/** QIGraphicsView extension used as VM chooser pane view. */
class UIChooserView : public QIWithRetranslateUI<QIGraphicsView>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about resize. */
    void sigResized();

public:

    /** Constructs a chooser-view passing @a pParent to the base-class.
      * @param  pParent  Brings the chooser container to embed into. */
    UIChooserView(UIChooser *pParent);

    /** @name General stuff.
      * @{ */
        /** Returns the chooser reference. */
        UIChooser *chooser() const { return m_pChooser; }
    /** @} */

public slots:

    /** @name General stuff.
      * @{ */
        /** Handles focus change to @a pFocusItem. */
        void sltFocusChanged();
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Handles minimum width @a iHint change. */
        void sltMinimumWidthHintChanged(int iHint);
        /** Handles minimum height @a iHint change. */
        void sltMinimumHeightHintChanged(int iHint);
    /** @} */

protected:

    /** @name Event handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;

        /** Handles resize @a pEvent. */
        virtual void resizeEvent(QResizeEvent *pEvent) /* override */;
    /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares palette. */
        void preparePalette();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Updates scene rectangle. */
        void updateSceneRect();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Holds the chooser pane reference. */
        UIChooser *m_pChooser;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Holds the minimum width hint. */
        int m_iMinimumWidthHint;
        /** Holds the minimum height hint. */
        int m_iMinimumHeightHint;
    /** @} */
};

#endif /* !___UIChooserView_h___ */
