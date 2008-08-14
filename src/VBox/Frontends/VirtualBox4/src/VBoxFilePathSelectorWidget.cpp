/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: VBoxFilePathSelectorWidget class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "VBoxFilePathSelectorWidget.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileIconProvider>

enum
{
    PathId = 0,
    SelectId,
    ResetId
};

VBoxFilePathSelectorWidget::VBoxFilePathSelectorWidget (QWidget *aParent)
    : QIWithRetranslateUI<QComboBox> (aParent)
    , mIconProvider (new QFileIconProvider())
    , mCopyAction (new QAction (this))
    , mMode (Mode_Folder)
{
    /* Populate items */
    insertItem (PathId, "");
    insertItem (SelectId, "");
    insertItem (ResetId, "");

    /* Attaching known icons */
    setItemIcon (SelectId, VBoxGlobal::iconSet (":/select_file_16px.png"));
    setItemIcon (ResetId, VBoxGlobal::iconSet (":/eraser_16px.png"));

    /* Setup context menu */
    addAction (mCopyAction);
    mCopyAction->setShortcut (QKeySequence (QKeySequence::Copy));
    mCopyAction->setShortcutContext (Qt::WidgetShortcut);

    /* Setup connections */
    connect (this, SIGNAL (activated (int)), this, SLOT (onActivated (int)));
    connect (mCopyAction, SIGNAL (triggered (bool)), this, SLOT (copyToClipboard()));

    /* Applying language settings */
    retranslateUi();

    /* Initial Setup */
    setContextMenuPolicy (Qt::ActionsContextMenu);
    setMinimumWidth (200);
}

VBoxFilePathSelectorWidget::~VBoxFilePathSelectorWidget()
{
    delete mIconProvider;
}

void VBoxFilePathSelectorWidget::setMode (Mode aMode)
{
    mMode = aMode;
}

VBoxFilePathSelectorWidget::Mode VBoxFilePathSelectorWidget::mode() const
{
    return mMode;
}

void VBoxFilePathSelectorWidget::setResetEnabled (bool aEnabled)
{
    if (!aEnabled && count() - 1 == ResetId)
        removeItem (ResetId);
    else if (aEnabled && count() - 1 == ResetId - 1)
    {
        insertItem (ResetId, "");
        setItemIcon (ResetId, VBoxGlobal::iconSet (":/eraser_16px.png"));
    }
    retranslateUi();
}

bool VBoxFilePathSelectorWidget::isResetEnabled() const
{
    return (count() - 1  == ResetId);
}

bool VBoxFilePathSelectorWidget::isModified() const
{
    return true;
}

void VBoxFilePathSelectorWidget::setPath (const QString &aPath)
{
    mPath = aPath.isEmpty() ? QString::null : aPath;
    refreshText();
}

QString VBoxFilePathSelectorWidget::path() const
{
    return mPath;
}

void VBoxFilePathSelectorWidget::resizeEvent (QResizeEvent *aEvent)
{
    QIWithRetranslateUI<QComboBox>::resizeEvent (aEvent);
    refreshText();
}

void VBoxFilePathSelectorWidget::retranslateUi()
{
    /* How do we interpret the "nothing selected" item? */
    if (isResetEnabled())
    {
        mNoneStr = tr ("<reset to default>");
        mNoneTip = tr ("The actual default path value will be displayed after "
                       "accepting the changes and opening this dialog again.");
    }
    else
    {
        mNoneStr = tr ("<not selected>");
        mNoneTip = tr ("Please use the <b>Other...</b> item from the drop-down "
                       "list to select a desired path.");
    }

    /* Retranslate 'path' item */
    if (mPath.isNull())
    {
        setItemText (PathId, mNoneStr);
        setItemData (PathId, mNoneTip, Qt::ToolTipRole);
        setToolTip (mNoneTip);
    }

    /* Retranslate 'select' item */
    setItemText (SelectId, tr ("Other..."));

    /* Retranslate 'reset' item */
    if (count() - 1 == ResetId)
        setItemText (ResetId, tr ("Reset"));

    /* Set tooltips of the above two items based on the mode */
    switch (mMode)
    {
        case Mode_Folder:
            setItemData (SelectId,
                         tr ("Opens a dialog to select a different folder."),
                         Qt::ToolTipRole);
            setItemData (ResetId,
                         tr ("Resets the folder path to the default value."),
                         Qt::ToolTipRole);
            break;
        case Mode_File:
            setItemData (SelectId,
                         tr ("Opens a dialog to select a different file."),
                         Qt::ToolTipRole);
            setItemData (ResetId,
                         tr ("Resets the file path to the default value."),
                         Qt::ToolTipRole);
            break;
        default:
            AssertFailedBreak();
    }

    /* Retranslate copy action */
    mCopyAction->setText (tr ("&Copy"));
}

void VBoxFilePathSelectorWidget::onActivated (int aIndex)
{
    switch (aIndex)
    {
        case SelectId:
        {
            emit selectPath();
            break;
        }
        case ResetId:
        {
            setPath (QString::null);
            break;
        }
        default:
            break;
    }
    setCurrentIndex (PathId);
}

void VBoxFilePathSelectorWidget::copyToClipboard()
{
    QString text (fullPath());
    /* Copy the current text to the selection and global clipboard. */
    if (QApplication::clipboard()->supportsSelection())
        QApplication::clipboard()->setText (text, QClipboard::Selection);
    QApplication::clipboard()->setText (text, QClipboard::Clipboard);
}

QIcon VBoxFilePathSelectorWidget::defaultIcon() const
{
    if (mMode == Mode_Folder)
        return mIconProvider->icon (QFileIconProvider::Folder);
    else
        return mIconProvider->icon (QFileIconProvider::File);
}

QString VBoxFilePathSelectorWidget::fullPath (bool aAbsolute) const
{
    if (!mPath.isNull())
    {
        switch (mMode)
        {
            case Mode_Folder:
                return aAbsolute ? QDir (mPath).absolutePath() :
                                   QDir (mPath).path();
            case Mode_File:
                return aAbsolute ? QFileInfo (mPath).absoluteFilePath() :
                                   QFileInfo (mPath).filePath();
            default:
                AssertFailedBreak();
        }
    }
    return QString::null;
}

QString VBoxFilePathSelectorWidget::shrinkText (int aWidth) const
{
    QString fullText (fullPath (false));
    if (fullText.isEmpty())
        return fullText;

    int oldSize = fontMetrics().width (fullText);
    int indentSize = fontMetrics().width ("...x");

    /* Compress text */
    int start = 0;
    int finish = 0;
    int position = 0;
    int textWidth = 0;
    do {
        textWidth = fontMetrics().width (fullText);
        if (textWidth + indentSize > aWidth)
        {
            start = 0;
            finish = fullText.length();

            /* Selecting remove position */
            QRegExp regExp ("([\\\\/][^\\\\^/]+[\\\\/]?$)");
            int newFinish = regExp.indexIn (fullText);
            if (newFinish != -1)
                finish = newFinish;
            position = (finish - start) / 2;

            if (position == finish)
               break;

            fullText.remove (position, 1);
        }
    } while (textWidth + indentSize > aWidth);

    fullText.insert (position, "...");
    int newSize = fontMetrics().width (fullText);

    return newSize < oldSize ? fullText : fullPath (false);
}

void VBoxFilePathSelectorWidget::refreshText()
{
    if (mPath.isNull())
    {
        if (itemText (PathId) != mNoneStr)
        {
            setItemText (PathId, mNoneStr);
            setItemIcon (PathId, QIcon());
            setItemData (PathId, mNoneTip, Qt::ToolTipRole);
            setToolTip (mNoneTip);
        }
    }
    else
    {
        /* Compress text in combobox */
        QStyleOptionComboBox options;
        options.initFrom (this);
        QRect rect = QApplication::style()->subControlRect (
            QStyle::CC_ComboBox, &options, QStyle::SC_ComboBoxEditField);
        setItemText (PathId, shrinkText (rect.width() - iconSize().width()));

        /* Attach corresponding icon */
        setItemIcon (PathId, QFileInfo (mPath).exists() ?
                             mIconProvider->icon (QFileInfo (mPath)) :
                             defaultIcon());

        /* Set the tooltip */
        setToolTip (fullPath());
        setItemData (PathId, toolTip(), Qt::ToolTipRole);
    }
}

