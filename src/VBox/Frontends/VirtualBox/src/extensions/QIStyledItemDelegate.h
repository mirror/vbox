/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIStyledItemDelegate class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QIStyledItemDelegate_h___
#define ___QIStyledItemDelegate_h___

/* Qt includes: */
#include <QStyledItemDelegate>

/* GUI includes: */
#include "UILibraryDefs.h"

/** QStyledItemDelegate subclass extending standard functionality. */
class SHARED_LIBRARY_STUFF QIStyledItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a pEditor created for particular model @a index. */
    void sigEditorCreated(QWidget *pEditor, const QModelIndex &index) const;

    /** Notifies listeners about editor's Enter key triggering. */
    void sigEditorEnterKeyTriggered();

public:

    /** Constructs delegate passing @a pParent to the base-class. */
    QIStyledItemDelegate(QObject *pParent);

    /** Defines whether delegate should watch for the editor's data commits. */
    void setWatchForEditorDataCommits(bool fWatch);
    /** Defines whether delegate should watch for the editor's Enter key triggering. */
    void setWatchForEditorEnterKeyTriggering(bool fWatch);

protected:

    /** Returns the widget used to edit the item specified by @a index.
      * The @a pParent widget and style @a option are used to control how the editor widget appears. */
    virtual QWidget *createEditor(QWidget *pParent,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const /* override */;

private:

    /** Holds whether delegate should watch for the editor's data commits. */
    bool m_fWatchForEditorDataCommits : 1;
    /** Holds whether delegate should watch for the editor's Enter key triggering. */
    bool m_fWatchForEditorEnterKeyTriggering : 1;
};

#endif /* !___QIStyledItemDelegate_h___ */
