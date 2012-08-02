/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * QIAdvancedToolBar class implementation
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

#ifndef __QIAdvancedToolBar_h__
#define __QIAdvancedToolBar_h__

/* Qt includes: */
#include <QWidget>
#include <QToolButton>

/* Forward declaration: */
class QHBoxLayout;
class QAction;
class QIAdvancedToolButton;

/* More configurable tool-bar with exclusive actions: */
class QIAdvancedToolBar : public QWidget
{
    Q_OBJECT;

signals:

    /* Notify external listeners about action triggering: */
    void sigActionTriggered(int iActionIndex);
    void sigActionToggled(int iActionIndex, bool fChecked);

public:

    /* Constructor: */
    QIAdvancedToolBar(QWidget *pParent);

    /* Add action API: */
    int addAction(const QString &strActionText);
    int addAction(const QIcon &actionIcon, const QString &strActionText);

    /* Layouting API: */
    void setToolBarMargins(int iLeft, int iTop, int iRight, int iBottom);
    void setToolBarSpacing(int iSpacing);

    /* Set required button to be checked: */
    void setCurrentButton(int iCurrentButtonIndex);

    /* Tool-button checkable API: */
    void setToolButtonCheckable(int iButtonIndex, bool fCheckable);

    /* Tool-buttons style API: */
    void setToolButtonsStyle(Qt::ToolButtonStyle toolButtonStyle);

    /* Tool-buttons unique API: */
    void setToolButtonsUnique(bool fButtonsUnique);

private slots:

    /* Handles particular action trigger: */
    void sltActionTriggered(QAction *pTriggeredAction);
    /* Handles particular action toggling: */
    void sltActionToggled(bool fChecked);

private:

    /* Helpers: */
    void createToolButtonForLastAddedAction();

    /* Variables: */
    QHBoxLayout *m_pMainLayout;
    QList<QAction*> m_actions;
    QList<QIAdvancedToolButton*> m_button;
    Qt::ToolButtonStyle m_toolButtonStyle;
    bool m_fButtonUnique;
};

#endif /* __QIAdvancedToolBar_h__ */

