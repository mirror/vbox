/* $Id$ */
/** @file
 * VBox Qt GUI - QIStyledItemDelegate class declaration.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */
/* Qt includes: */
# include <QStyledItemDelegate>
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/** Own QStyledItemDelegate implementation. */
class QIStyledItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT;

public:

    /** Constructor. */
    QIStyledItemDelegate(QObject *pParent) : QStyledItemDelegate(pParent) {}

private:

    /** Returns the widget used to edit the item specified by @a index for editing.
      * The @a pParent widget and style @a option are used to control how the editor widget appears.
      * Besides Qt description copy-pasted above we are installing the hook to redirect editor's sigCommitData signal. */
    QWidget* createEditor(QWidget *pParent, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        /* Call to base-class to get actual editor created: */
        QWidget *pEditor = QStyledItemDelegate::createEditor(pParent, option, index);
        /* All the stuff we actually need from QIStyledItemDelegate is to redirect this one signal: */
        connect(pEditor, SIGNAL(sigCommitData(QWidget*)), this, SIGNAL(commitData(QWidget*)));
        /* Return actual editor: */
        return pEditor;
    }
};
