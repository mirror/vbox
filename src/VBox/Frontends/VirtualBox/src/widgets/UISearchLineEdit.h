/* $Id$ */
/** @file
 * VBox Qt GUI - UISearchLineEdit class declaration.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_UISearchLineEdit_h
#define FEQT_INCLUDED_SRC_widgets_UISearchLineEdit_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/* Qt includes */
#include <QLineEdit>

/* GUI includes: */
#include "UILibraryDefs.h"

/** A QLineEdit extension with an overlay label drawn on the right hand side of it.
  * mostly used for entering a search term and then label show total number of matched items
  * and currently selected, scrolled item. */
class SHARED_LIBRARY_STUFF
UISearchLineEdit : public QLineEdit
{

    Q_OBJECT;

public:

    UISearchLineEdit(QWidget *pParent = 0);
    void setMatchCount(int iMatchCount);
    void setScroolToIndex(int iScrollToIndex);

protected:

    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

private:

    int m_iMatchCount;
    int m_iScrollToIndex;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_UISearchLineEdit_h */
