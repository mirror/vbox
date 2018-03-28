/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxAboutDlg class declaration.
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

#ifndef ___VBoxAboutDlg_h___
#define ___VBoxAboutDlg_h___

/* Qt includes: */
#include <QPixmap>

/* GUI includes: */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QEvent;
class QLabel;
class QVBoxLayout;

/** QIDialog extension
  * used to show the About-VirtualBox dialog. */
class VBoxAboutDlg : public QIWithRetranslateUI2<QIDialog>
{
    Q_OBJECT;

public:

    /** Constructs dialog passing @a pParent to the base-class.
      * @param  strVersion  Brings the version number of VirtualBox. */
    VBoxAboutDlg(QWidget *pParent, const QString &strVersion);

protected:

    /** Handles any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override */;

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** Prepares all. */
    void prepare();
    /** Prepares main-layout. */
    void prepareMainLayout();
    /** Prepares label. */
    void prepareLabel();
    /** Prepares close-button. */
    void prepareCloseButton();

    /** Holds the pseudo-parent widget reference. */
    QObject *m_pPseudoParent;

    /** Holds the About-VirtualBox text. */
    QString  m_strAboutText;
    /** Holds the VirtualBox version number. */
    QString  m_strVersion;

    /** Holds the About-VirtualBox image. */
    QPixmap  m_pixmap;
    /** Holds the About-VirtualBox dialog size. */
    QSize    m_size;

    /** Holds About-VirtualBox main-layout instance. */
    QVBoxLayout *m_pMainLayout;
    /** Holds About-VirtualBox text-label instance. */
    QLabel      *m_pLabel;
};

#endif /* !___VBoxAboutDlg_h___ */

