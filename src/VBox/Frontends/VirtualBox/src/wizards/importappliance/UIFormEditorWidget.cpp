/* $Id$ */
/** @file
 * VBox Qt GUI - UIFormEditorWidget class implementation.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QComboBox>
#include <QHeaderView>
#include <QItemEditorFactory>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

/* GUI includes: */
#include "QITableView.h"
#include "UIFormEditorWidget.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CBooleanFormValue.h"
#include "CChoiceFormValue.h"
#include "CFormValue.h"
#include "CStringFormValue.h"
#include "CVirtualSystemDescriptionForm.h"


/** QITableView extension used as Form Editor table-view. */
class UIFormEditorView : public QITableView
{
    Q_OBJECT;

public:

    /** Constructs Form Editor table-view. */
    UIFormEditorView(QWidget *pParent = 0);

protected:

    /** Returns the number of children. */
    virtual int childCount() const /* override */;
    /** Returns the child item with @a iIndex. */
    virtual QITableViewRow *childItem(int iIndex) const /* override */;
};


/*********************************************************************************************************************************
*   Class UIFormEditorView implementation.                                                                                       *
*********************************************************************************************************************************/

UIFormEditorView::UIFormEditorView(QWidget * /* pParent = 0 */)
{
}

int UIFormEditorView::childCount() const
{
    /* No model => no children: */
    return 0;
}

QITableViewRow *UIFormEditorView::childItem(int iIndex) const
{
    Q_UNUSED(iIndex);

    /* No model => no children: */
    return 0;
}


/*********************************************************************************************************************************
*   Class UIFormEditorWidget implementation.                                                                                     *
*********************************************************************************************************************************/

UIFormEditorWidget::UIFormEditorWidget(QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pTableView(0)
{
    prepare();
}

void UIFormEditorWidget::prepare()
{
    /* Create layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if (pLayout)
    {
        /* Create view: */
        m_pTableView = new UIFormEditorView(this);
        if (m_pTableView)
        {
            m_pTableView->setTabKeyNavigation(false);
            m_pTableView->verticalHeader()->hide();
            m_pTableView->verticalHeader()->setDefaultSectionSize((int)(m_pTableView->verticalHeader()->minimumSectionSize() * 1.33));
            m_pTableView->setSelectionMode(QAbstractItemView::SingleSelection);

            /* Add into layout: */
            pLayout->addWidget(m_pTableView);
        }
    }
}


#include "UIFormEditorWidget.moc"
