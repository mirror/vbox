/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: UIHotKeyEditor class implementation
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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
#include <QApplication>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QKeyEvent>

/* GUI includes; */
#include "UIHotKeyEditor.h"

/* A line-edit representing hot-key editor: */
class UIHotKeyLineEdit : public QLineEdit
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIHotKeyLineEdit(QWidget *pParent);

private slots:

    /* Handler: Selection preserver stuff: */
    void sltDeselect() { deselect(); }

private:

    /* Handlers: Key event processing stuff: */
    void keyPressEvent(QKeyEvent *pEvent);
    void keyReleaseEvent(QKeyEvent *pEvent);
    bool isKeyEventIgnored(QKeyEvent *pEvent);
};

UIHotKeyLineEdit::UIHotKeyLineEdit(QWidget *pParent)
    : QLineEdit(pParent)
{
    /* Configure self: */
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    setContextMenuPolicy(Qt::NoContextMenu);

    /* Connect selection preserver: */
    connect(this, SIGNAL(selectionChanged()), this, SLOT(sltDeselect()));
}

void UIHotKeyLineEdit::keyPressEvent(QKeyEvent *pEvent)
{
    /* Is this event ignored? */
    if (isKeyEventIgnored(pEvent))
        return;
    /* Call to base-class: */
    QLineEdit::keyPressEvent(pEvent);
}

void UIHotKeyLineEdit::keyReleaseEvent(QKeyEvent *pEvent)
{
    /* Is this event ignored? */
    if (isKeyEventIgnored(pEvent))
        return;
    /* Call to base-class: */
    QLineEdit::keyReleaseEvent(pEvent);
}

bool UIHotKeyLineEdit::isKeyEventIgnored(QKeyEvent *pEvent)
{
    /* Ignore some keys: */
    switch (pEvent->key())
    {
        /* Ignore cursor keys: */
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
            pEvent->ignore();
            return true;
        /* Default handling for others: */
        default: break;
    }
    /* Do not ignore key by default: */
    return false;
}


UIHotKeyEditor::UIHotKeyEditor(QWidget *pParent)
    : QWidget(pParent)
    , m_pMainLayout(new QHBoxLayout(this))
    , m_pLineEdit(new UIHotKeyLineEdit(this))
    , m_iTakenKey(-1)
    , m_fSequenceTaken(false)
    , m_fTakenKeyReleased(true)
{
    /* Configure self: */
    setFocusProxy(m_pLineEdit);

    /* Configure layout: */
    m_pMainLayout->setSpacing(0);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    m_pMainLayout->addWidget(m_pLineEdit);

    /* Configure line-edit: */
    m_pLineEdit->installEventFilter(this);
}

bool UIHotKeyEditor::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Special handling for our line-edit only: */
    if (pWatched != m_pLineEdit)
        return QWidget::eventFilter(pWatched, pEvent);

    /* Special handling for key events only: */
    if (pEvent->type() != QEvent::KeyPress &&
        pEvent->type() != QEvent::KeyRelease)
        return QWidget::eventFilter(pWatched, pEvent);

    /* Cast passed event to required type: */
    QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);

    /* Should we skip that event to our line-edit? */
    if (shouldWeSkipKeyEventToLineEdit(pKeyEvent))
        return false;

    /* Fetch modifiers state: */
    fetchModifiersState();

    /* Handle key event: */
    switch (pEvent->type())
    {
        case QEvent::KeyPress: handleKeyPress(pKeyEvent); break;
        case QEvent::KeyRelease: handleKeyRelease(pKeyEvent); break;
        default: break;
    }

    /* Reflect sequence: */
    reflectSequence();

    /* Prevent further key event handling: */
    return true;
}

bool UIHotKeyEditor::shouldWeSkipKeyEventToLineEdit(QKeyEvent *pEvent)
{
    /* Special handling for some keys: */
    switch (pEvent->key())
    {
        /* Skip Escape to our line-edit: */
        case Qt::Key_Escape: return true;
        /* Skip Return/Enter to our line-edit: */
        case Qt::Key_Return:
        case Qt::Key_Enter: return true;
        /* Skip cursor keys to our line-edit: */
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down: return true;
        /* Default handling for others: */
        default: break;
    }
    /* Do not skip by default: */
    return false;
}

void UIHotKeyEditor::keyPressEvent(QKeyEvent *pEvent)
{
    /* Is this event ignored? */
    if (isKeyEventIgnored(pEvent))
        return;
    /* Call to base-class: */
    return QWidget::keyPressEvent(pEvent);
}

void UIHotKeyEditor::keyReleaseEvent(QKeyEvent *pEvent)
{
    /* Is this event ignored? */
    if (isKeyEventIgnored(pEvent))
        return;
    /* Call to base-class: */
    return QWidget::keyReleaseEvent(pEvent);
}

bool UIHotKeyEditor::isKeyEventIgnored(QKeyEvent *pEvent)
{
    /* Ignore some keys: */
    switch (pEvent->key())
    {
        /* Ignore cursor keys: */
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
            pEvent->ignore();
            return true;
        /* Default handling for others: */
        default: break;
    }
    /* Do not ignore key by default: */
    return false;
}

void UIHotKeyEditor::fetchModifiersState()
{
    /* If full sequence was not yet taken: */
    if (!m_fSequenceTaken)
    {
        /* Recreate the set of taken modifiers: */
        m_takenModifiers.clear();
        Qt::KeyboardModifiers currentModifiers = QApplication::keyboardModifiers();
        if (currentModifiers != Qt::NoModifier)
        {
            if ((m_takenModifiers.size() < 3) && (currentModifiers & Qt::ShiftModifier))
                m_takenModifiers << Qt::SHIFT;
            if ((m_takenModifiers.size() < 3) && (currentModifiers & Qt::ControlModifier))
                m_takenModifiers << Qt::CTRL;
            if ((m_takenModifiers.size() < 3) && (currentModifiers & Qt::AltModifier))
                m_takenModifiers << Qt::ALT;
            if ((m_takenModifiers.size() < 3) && (currentModifiers & Qt::MetaModifier))
                m_takenModifiers << Qt::META;
        }
    }
}

void UIHotKeyEditor::handleKeyPress(QKeyEvent *pKeyEvent)
{
    /* If full sequence was not yet taken: */
    if (!m_fSequenceTaken)
    {
        /* If finalizing key is pressed: */
        if (!pKeyEvent->text().isEmpty())
        {
            /* Remember taken key: */
            m_iTakenKey = pKeyEvent->key();
            /* Mark taken key pressed: */
            m_fTakenKeyReleased = false;
            /* Mark full sequence taken: */
            m_fSequenceTaken = true;
        }
        /* If something other is pressed: */
        else
        {
            /* Clear taken key: */
            m_iTakenKey = -1;
        }
    }
}

void UIHotKeyEditor::handleKeyRelease(QKeyEvent *pKeyEvent)
{
    /* If full sequence was taken already: */
    if (m_fSequenceTaken)
    {
        /* If taken key is released: */
        if (pKeyEvent->key() == m_iTakenKey)
        {
            /* Mark taken key released: */
            m_fTakenKeyReleased = true;
        }
        /* If no modifiers currently held and taken key was released already: */
        if ((QApplication::keyboardModifiers() == Qt::NoModifier) && m_fTakenKeyReleased)
        {
            /* Reset taken sequence: */
            m_fSequenceTaken = false;
        }
    }
}

void UIHotKeyEditor::reflectSequence()
{
    /* Prepare sequence: */
    QString strSequence;

    /* Append sequence with modifier names: */
    QStringList modifierNames;
    foreach (int iTakenModifier, m_takenModifiers)
        modifierNames << QKeySequence(iTakenModifier).toString();
    if (!modifierNames.isEmpty())
        strSequence += modifierNames.join("");

    /* Append sequence with main key name: */
    if (m_iTakenKey != -1)
        strSequence += QKeySequence(m_iTakenKey).toString();

    /* Draw sequence: */
    m_pLineEdit->setText(strSequence);
}

UIHotKey UIHotKeyEditor::hotKey() const
{
    return m_pLineEdit->text();
}

void UIHotKeyEditor::setHotKey(const UIHotKey &hotKey)
{
    m_pLineEdit->setText(hotKey.toString());
}

#include "UIHotKeyEditor.moc"

