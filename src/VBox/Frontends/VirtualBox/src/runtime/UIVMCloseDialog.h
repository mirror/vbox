/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIVMCloseDialog class declaration
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIVMCloseDialog_h__
#define __UIVMCloseDialog_h__

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "QIDialog.h"
#include "UIDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* Forward declarations: */
class CSession;
class QLabel;
class QRadioButton;
class QCheckBox;

/* QIDialog extension to handle Runtime UI close-event: */
class UIVMCloseDialog : public QIWithRetranslateUI<QIDialog>
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIVMCloseDialog(QWidget *pParent, const CMachine &machine, const CSession &session);

    /* API: Validation stuff: */
    bool isValid() const { return m_fValid; }

    /* Static API: Parse string containing result-code: */
    static MachineCloseAction parseResultCode(const QString &strCloseAction);

private slots:

    /* Handler: Update stuff: */
    void sltUpdateWidgetAvailability();

    /* Handler: Accept stuff: */
    void accept();

private:

    /* API: Pixmap stuff: */
    void setPixmap(const QPixmap &pixmap);

    /* API: Save-button stuff: */
    void setSaveButtonEnabled(bool fEnabled);
    void setSaveButtonVisible(bool fVisible);
    /* API: Shutdown-button stuff: */
    void setShutdownButtonEnabled(bool fEnabled);
    void setShutdownButtonVisible(bool fVisible);
    /* API: Power-off-button stuff: */
    void setPowerOffButtonEnabled(bool fEnabled);
    void setPowerOffButtonVisible(bool fVisible);
    /* API: Discard-check-box stuff: */
    void setDiscardCheckBoxVisible(bool fVisible);

    /* Helpers: Prepare stuff: */
    void prepare();
    void configure(const CMachine &machine, const CSession &session);

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Handler: Polish-event stuff: */
    void polishEvent(QShowEvent *pEvent);

    /* Widgets: */
    QLabel *m_pIcon;
    QLabel *m_pLabel;
    QLabel *m_pSaveIcon;
    QRadioButton *m_pSaveRadio;
    QLabel *m_pShutdownIcon;
    QRadioButton *m_pShutdownRadio;
    QLabel *m_pPowerOffIcon;
    QRadioButton *m_pPowerOffRadio;
    QCheckBox *m_pDiscardCheckBox;

    /* Variables: */
    const QString m_strExtraDataOptionSave;
    const QString m_strExtraDataOptionShutdown;
    const QString m_strExtraDataOptionPowerOff;
    const QString m_strExtraDataOptionDiscard;
    bool m_fValid;
    CMachine m_machine;
    bool m_fIsACPIEnabled;
    QString m_strDiscardCheckBoxText;
};

#endif // __UIVMCloseDialog_h__

