/* $Id$ */
/** @file
 * VBox Qt GUI - UIFormEditorWidget class declaration.
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

#ifndef FEQT_INCLUDED_SRC_widgets_UIFormEditorWidget_h
#define FEQT_INCLUDED_SRC_widgets_UIFormEditorWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* Forward declarations: */
class UIFormEditorModel;
class UIFormEditorView;
class CVirtualSystemDescriptionForm;

/** QWidget subclass representing model/view Form Editor widget. */
class UIFormEditorWidget : public QWidget
{
    Q_OBJECT;

public:

    /** Constructs Form Editor widget passing @a pParent to the base-class. */
    UIFormEditorWidget(QWidget *pParent = 0);

    /** Defines virtual system description @a comForm to be edited. */
    void setVirtualSystemDescriptionForm(const CVirtualSystemDescriptionForm &comForm);

protected:

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

private:

    /** Prepares all. */
    void prepare();

    /** Adjusts table column sizes. */
    void adjustTable();

    /** Holds the table-view instance. */
    UIFormEditorView  *m_pTableView;
    /** Holds the table-model instance. */
    UIFormEditorModel *m_pTableModel;
};

/** Safe pointer to Form Editor widget. */
typedef QPointer<UIFormEditorWidget> UIFormEditorWidgetPointer;

#endif /* !FEQT_INCLUDED_SRC_widgets_UIFormEditorWidget_h */
