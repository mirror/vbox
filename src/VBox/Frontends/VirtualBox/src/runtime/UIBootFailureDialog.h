/* $Id$ */
/** @file
 * VBox Qt GUI - UIBootFailureDialog class declaration.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_runtime_UIBootFailureDialog_h
#define FEQT_INCLUDED_SRC_runtime_UIBootFailureDialog_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIMainDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIMedium.h"
#include "UIMediumDefs.h"


/* Forward declarations: */
class QCheckBox;
class QLabel;
class QVBoxLayout;
class QIDialogButtonBox;
class QIRichTextLabel;
class UIFilePathSelector;

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/** QIDialog extension providing GUI with a dialog to select an existing medium. */
class UIBootFailureDialog : public QIWithRetranslateUI<QIMainDialog>
{

    Q_OBJECT;

signals:

public:

    enum ReturnCode
    {
        ReturnCode_Close = 0,
        ReturnCode_Reset,
        ReturnCode_Max
    };

    UIBootFailureDialog(QWidget *pParent, const CMachine &comMachine);
    ~UIBootFailureDialog();
    QString bootMediumPath() const;

protected:

    virtual void showEvent(QShowEvent *pEvent) RT_OVERRIDE;

private slots:

    void sltCancel();
    void sltReset();
    void sltFileSelectorPathChanged(const QString &strPath);

private:

    QPixmap iconPixmap();
    /* Checks if selected iso exists and readable. Returns false if not. Returns true if nothing is selected. */
    bool checkISOImage() const;

    /** @name Event-handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() RT_OVERRIDE;
    /** @} */

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Configures all. */
            void configure();
            void prepareWidgets();
            void prepareConnections();
    /** @} */

    void          setTitle();
    QWidget              *m_pParent;

    QWidget              *m_pCentralWidget;
    QVBoxLayout          *m_pMainLayout;
    QIDialogButtonBox    *m_pButtonBox;
    QPushButton          *m_pCloseButton;
    QPushButton          *m_pResetButton;
    QIRichTextLabel      *m_pLabel;
    UIFilePathSelector   *m_pBootImageSelector;
    QLabel               *m_pBootImageLabel;
    QLabel               *m_pIconLabel;
    QCheckBox            *m_pSuppressDialogCheckBox;
    CMachine              m_comMachine;
};

#endif /* !FEQT_INCLUDED_SRC_runtime_UIBootFailureDialog_h */
