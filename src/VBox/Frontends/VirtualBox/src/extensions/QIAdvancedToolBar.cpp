/* $Id$ */
/** @file
 * VBox Qt GUI - QIAdvancedToolBar class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QHBoxLayout>
# include <QAction>
# include <QToolButton>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
#include "QIAdvancedToolBar.h"


/* More configurable tool-button for QIAdvancedToolBar: */
class QIAdvancedToolButton : public QToolButton
{
    Q_OBJECT;

public:

    /* Constructor: */
    QIAdvancedToolButton(QIAdvancedToolBar *pParent)
        : QToolButton(pParent), m_fIgnoreMousePressIfChecked(false) {}

    /* Size-hint: */
    QSize sizeHint() const
    {
        /* Make the button a little bit taller: */
        return QSize((int)QToolButton::sizeHint().width(), (int)(QToolButton::sizeHint().height() * 1.2));
    }

    /* Set the button to ignore mouse-press events if checked: */
    void setIgnoreMousePressIfChecked(bool fIgnore)
    {
        m_fIgnoreMousePressIfChecked = fIgnore;
    }

private:

    /* Mouse-press event reimplementation: */
    void mousePressEvent(QMouseEvent *pEvent)
    {
        /* Ignore event if button is in 'checked' state: */
        if (m_fIgnoreMousePressIfChecked && isChecked())
            return;
        /* Call to base-class: */
        QToolButton::mousePressEvent(pEvent);
    }

    /* Variables: */
    bool m_fIgnoreMousePressIfChecked;
};

QIAdvancedToolBar::QIAdvancedToolBar(QWidget *pParent)
    : QWidget(pParent)
    , m_pMainLayout(0)
    , m_toolButtonStyle(Qt::ToolButtonIconOnly)
    , m_fButtonUnique(false)
{
    /* Create main-layout: */
    m_pMainLayout = new QHBoxLayout(this);
#if defined (VBOX_WS_WIN)
    m_pMainLayout->setContentsMargins(1, 1, 1, 1);
#elif defined (VBOX_WS_X11)
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
#else
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
#endif
    m_pMainLayout->setSpacing(1);
    m_pMainLayout->addStretch();
}

int QIAdvancedToolBar::addAction(const QString &strActionText)
{
    /* Add action: */
    m_actions.append(new QAction(this));
    /* Set passed attributes: */
    QAction *pAddedAction = m_actions.last();
    pAddedAction->setText(strActionText);

    /* Create tool-button: */
    createToolButtonForLastAddedAction();

    /* Return added action position: */
    return m_actions.size() - 1;
}

int QIAdvancedToolBar::addAction(const QIcon &actionIcon, const QString &strActionText)
{
    /* Add action: */
    m_actions.append(new QAction(this));
    /* Set passed attributes: */
    QAction *pAddedAction = m_actions.last();
    pAddedAction->setText(strActionText);
    pAddedAction->setIcon(actionIcon);

    /* Create tool-button: */
    createToolButtonForLastAddedAction();

    /* Return added action position: */
    return m_actions.size() - 1;
}

void QIAdvancedToolBar::setToolBarMargins(int iLeft, int iTop, int iRight, int iBottom)
{
    m_pMainLayout->setContentsMargins(iLeft, iTop, iRight, iBottom);
}

void QIAdvancedToolBar::setToolBarSpacing(int iSpacing)
{
    m_pMainLayout->setSpacing(iSpacing);
}

void QIAdvancedToolBar::setCurrentButton(int iCurrentButtonIndex)
{
    /* Make sure passed index feats the bounds: */
    if (iCurrentButtonIndex < 0 || iCurrentButtonIndex >= m_actions.size())
        return;

    /* Get corresponding action: */
    QAction *pRequiredAction = m_actions[iCurrentButtonIndex];
    /* Make sure other actions are unchecked: */
    foreach (QAction *pAction, m_actions)
        if (pAction != pRequiredAction && pAction->isChecked())
            pAction->setChecked(false);
    /* Make sure required action is checked: */
    if (!pRequiredAction->isChecked())
        pRequiredAction->setChecked(true);
}

void QIAdvancedToolBar::setToolButtonCheckable(int iButtonIndex, bool fCheckable)
{
    /* Make sure passed index feats the bounds: */
    if (iButtonIndex < 0 || iButtonIndex >= m_actions.size())
        return;

    /* Get corresponding action: */
    QAction *pRequiredAction = m_actions[iButtonIndex];
    pRequiredAction->setCheckable(fCheckable);
}

void QIAdvancedToolBar::setToolButtonsStyle(Qt::ToolButtonStyle toolButtonStyle)
{
    /* Remember tool-button style: */
    m_toolButtonStyle = toolButtonStyle;
    /* Propagate it to all the currently existing buttons: */
    foreach(QToolButton *pButton, m_button)
        pButton->setToolButtonStyle(m_toolButtonStyle);
}

void QIAdvancedToolBar::setToolButtonsUnique(bool fButtonsUnique)
{
    /* Remember tool-button 'unique' flag: */
    m_fButtonUnique = fButtonsUnique;
    /* Propagate it to all the currently existing buttons: */
    foreach (QIAdvancedToolButton *pButton, m_button)
        pButton->setIgnoreMousePressIfChecked(m_fButtonUnique);
}

void QIAdvancedToolBar::sltActionTriggered(QAction *pTriggeredAction)
{
    /* If every button is unique: */
    if (m_fButtonUnique)
    {
        /* Uncheck all the other actions: */
        foreach(QAction *pAction, m_actions)
        {
            if (pAction != pTriggeredAction)
                pAction->setChecked(false);
        }
    }
    /* Notify listeners: */
    emit sigActionTriggered(m_actions.indexOf(pTriggeredAction));
}

void QIAdvancedToolBar::sltActionToggled(bool fChecked)
{
    /* Check if sender is valid: */
    QIAdvancedToolButton *pSender = qobject_cast<QIAdvancedToolButton*>(sender());
    if (!pSender)
        return;

    /* Determine corresponding action: */
    int iButtonIndex = m_button.indexOf(pSender);
    /* Notify listeners: */
    emit sigActionToggled(iButtonIndex, fChecked);
}

void QIAdvancedToolBar::createToolButtonForLastAddedAction()
{
    /* Add tool-button: */
    m_button.append(new QIAdvancedToolButton(this));
    QIAdvancedToolButton *pAddedToolButton = m_button.last();
    connect(pAddedToolButton, SIGNAL(triggered(QAction*)), this, SLOT(sltActionTriggered(QAction*)));
    connect(pAddedToolButton, SIGNAL(toggled(bool)), this, SLOT(sltActionToggled(bool)));
    m_pMainLayout->insertWidget(m_button.size() - 1, pAddedToolButton);

    /* Initialize tool-button: */
    pAddedToolButton->setToolButtonStyle(m_toolButtonStyle);
    pAddedToolButton->setIgnoreMousePressIfChecked(m_fButtonUnique);
    pAddedToolButton->setDefaultAction(m_actions.last());
}

#include "QIAdvancedToolBar.moc"

