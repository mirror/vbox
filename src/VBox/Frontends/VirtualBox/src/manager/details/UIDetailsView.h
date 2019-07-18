/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsView class declaration.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_details_UIDetailsView_h
#define FEQT_INCLUDED_SRC_manager_details_UIDetailsView_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIGraphicsView.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class UIDetails;

/* Graphics details-view: */
class UIDetailsView : public QIWithRetranslateUI<QIGraphicsView>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about resize. */
    void sigResized();

public:

    /** Constructs a details-view passing @a pParent to the base-class.
      * @param  pParent  Brings the details container to embed into. */
    UIDetailsView(UIDetails *pParent);

    /** Returns the details reference. */
    UIDetails *details() const { return m_pDetails; }

public slots:

    /** Handles minimum width @a iHint change. */
    void sltMinimumWidthHintChanged(int iHint);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Handles resize @a pEvent. */
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;

private:

    /** Prepares all. */
    void prepare();

    /** Updates scene rectangle. */
    void updateSceneRect();

    /** Holds the details reference. */
    UIDetails *m_pDetails;

    /** Updates scene rectangle. */
    int m_iMinimumWidthHint;
};

#endif /* !FEQT_INCLUDED_SRC_manager_details_UIDetailsView_h */
