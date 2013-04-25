/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIVMCloseDialog class implementation
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QVBoxLayout>
# include <QHBoxLayout>
# include <QGridLayout>
# include <QLabel>
# include <QRadioButton>
# include <QCheckBox>
# include <QPushButton>

/* GUI includes: */
# include "UIVMCloseDialog.h"
# include "UIMessageCenter.h"
# include "VBoxGlobal.h"
# include "QIDialogButtonBox.h"

/* COM includes: */
# include "CSession.h"
# include "CConsole.h"
# include "CSnapshot.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIVMCloseDialog::UIVMCloseDialog(QWidget *pParent, const CMachine &machine, const CSession &session)
    : QIWithRetranslateUI<QIDialog>(pParent)
    , m_strExtraDataOptionSave("save")
    , m_strExtraDataOptionShutdown("shutdown")
    , m_strExtraDataOptionPowerOff("powerOff")
    , m_strExtraDataOptionDiscard("discardCurState")
    , m_fValid(false)
    , m_fIsACPIEnabled(false)
{
    /* Prepare: */
    prepare();

    /* Configure: */
    setSizeGripEnabled(false);
    configure(machine, session);

    /* Retranslate finally: */
    retranslateUi();
}

/* static */
UIVMCloseDialog::ResultCode UIVMCloseDialog::parseResultCode(const QString &strCloseAction)
{
    ResultCode resultCode = ResultCode_Cancel;
    if (!strCloseAction.compare("Save", Qt::CaseInsensitive))
        resultCode = ResultCode_Save;
    else if (!strCloseAction.compare("Shutdown", Qt::CaseInsensitive))
        resultCode = ResultCode_Shutdown;
    else if (!strCloseAction.compare("PowerOff", Qt::CaseInsensitive))
        resultCode = ResultCode_PowerOff;
    return resultCode;
}

void UIVMCloseDialog::sltUpdateWidgetAvailability()
{
    /* Discard option should be enabled only on power-off action: */
    m_pDiscardCheckBox->setEnabled(m_pPowerOffRadio->isChecked());
}

void UIVMCloseDialog::accept()
{
    /* Calculate result: */
    if (m_pSaveRadio->isChecked())
        setResult(ResultCode_Save);
    else if (m_pShutdownRadio->isChecked())
        setResult(ResultCode_Shutdown);
    else if (m_pPowerOffRadio->isChecked())
    {
        if (!m_pDiscardCheckBox->isChecked() || !m_pDiscardCheckBox->isVisible())
            setResult(ResultCode_PowerOff);
        else
            setResult(ResultCode_PowerOff_With_Discarding);
    }

    /* Read the last user's choice for the given VM: */
    QStringList previousChoice = m_machine.GetExtraData(GUI_LastCloseAction).split(',');
    /* Memorize the last user's choice for the given VM: */
    QString strLastAction = m_strExtraDataOptionPowerOff;
    switch (result())
    {
        case ResultCode_Save:
        {
            strLastAction = m_strExtraDataOptionSave;
            break;
        }
        case ResultCode_Shutdown:
        {
            strLastAction = m_strExtraDataOptionShutdown;
            break;
        }
        case ResultCode_PowerOff:
        {
            if (previousChoice[0] == m_strExtraDataOptionShutdown && !m_fIsACPIEnabled)
                strLastAction = m_strExtraDataOptionShutdown;
            else
                strLastAction = m_strExtraDataOptionPowerOff;
            break;
        }
        case ResultCode_PowerOff_With_Discarding:
        {
            strLastAction = m_strExtraDataOptionPowerOff + "," +
                            m_strExtraDataOptionDiscard;
        }
        default: break;
    }
    m_machine.SetExtraData(GUI_LastCloseAction, strLastAction);

    /* Hide the dialog: */
    hide();
}

void UIVMCloseDialog::setPixmap(const QPixmap &pixmap)
{
    /* Make sure pixmap is valid: */
    if (pixmap.isNull())
        return;

    /* Assign new pixmap: */
    m_pIcon->setPixmap(pixmap);
}

void UIVMCloseDialog::setSaveButtonEnabled(bool fEnabled)
{
    m_pSaveIcon->setEnabled(fEnabled);
    m_pSaveRadio->setEnabled(fEnabled);
}

void UIVMCloseDialog::setSaveButtonVisible(bool fVisible)
{
    m_pSaveIcon->setVisible(fVisible);
    m_pSaveRadio->setVisible(fVisible);
}

void UIVMCloseDialog::setShutdownButtonEnabled(bool fEnabled)
{
    m_pShutdownIcon->setEnabled(fEnabled);
    m_pShutdownRadio->setEnabled(fEnabled);
}

void UIVMCloseDialog::setShutdownButtonVisible(bool fVisible)
{
    m_pShutdownIcon->setVisible(fVisible);
    m_pShutdownRadio->setVisible(fVisible);
}

void UIVMCloseDialog::setPowerOffButtonEnabled(bool fEnabled)
{
    m_pPowerOffIcon->setEnabled(fEnabled);
    m_pPowerOffRadio->setEnabled(fEnabled);
}

void UIVMCloseDialog::setPowerOffButtonVisible(bool fVisible)
{
    m_pPowerOffIcon->setVisible(fVisible);
    m_pPowerOffRadio->setVisible(fVisible);
}

void UIVMCloseDialog::setDiscardCheckBoxVisible(bool fVisible)
{
    m_pDiscardCheckBox->setVisible(fVisible);
}

void UIVMCloseDialog::prepare()
{
    /* Prepare 'main' layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        /* Prepare 'top' layout: */
        QHBoxLayout *pTopLayout = new QHBoxLayout;
        {
            /* Prepare 'top-left' layout: */
            QVBoxLayout *pTopLeftLayout = new QVBoxLayout;
            {
                /* Prepare 'icon': */
                m_pIcon = new QLabel(this);
                {
                    /* Configure icon: */
                    m_pIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                    m_pIcon->setPixmap(QPixmap(":/os_unknown.png"));
                }
                /* Configure layout: */
                pTopLeftLayout->setSpacing(0);
                pTopLeftLayout->setContentsMargins(0, 0, 0, 0);
                pTopLeftLayout->addWidget(m_pIcon);
                pTopLeftLayout->addStretch();
            }
            /* Prepare 'top-right' layout: */
            QVBoxLayout *pTopRightLayout = new QVBoxLayout;
            {
                /* Prepare 'text' label: */
                m_pLabel = new QLabel(this);
                /* Prepare 'choice' layout: */
                QGridLayout *pChoiceLayout = new QGridLayout;
                {
                    /* Prepare 'save' icon: */
                    m_pSaveIcon = new QLabel(this);
                    {
                        /* Configure icon: */
                        m_pSaveIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                        m_pSaveIcon->setPixmap(QPixmap(":/save_state_16px.png"));
                    }
                    /* Prepare 'save' radio-button: */
                    m_pSaveRadio = new QRadioButton(this);
                    {
                        /* Configure button: */
                        connect(m_pSaveRadio, SIGNAL(toggled(bool)), this, SLOT(sltUpdateWidgetAvailability()));
                    }
                    /* Prepare 'shutdown' icon: */
                    m_pShutdownIcon = new QLabel(this);
                    {
                        /* Configure icon: */
                        m_pShutdownIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                        m_pShutdownIcon->setPixmap(QPixmap(":/acpi_16px.png"));
                    }
                    /* Prepare 'shutdown' radio-button: */
                    m_pShutdownRadio = new QRadioButton(this);
                    {
                        /* Configure button: */
                        connect(m_pShutdownRadio, SIGNAL(toggled(bool)), this, SLOT(sltUpdateWidgetAvailability()));
                    }
                    /* Prepare 'power-off' icon: */
                    m_pPowerOffIcon = new QLabel(this);
                    {
                        /* Configure icon: */
                        m_pPowerOffIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                        m_pPowerOffIcon->setPixmap(QPixmap(":/poweroff_16px.png"));
                    }
                    /* Prepare 'shutdown' radio-button: */
                    m_pPowerOffRadio = new QRadioButton(this);
                    {
                        /* Configure button: */
                        connect(m_pPowerOffRadio, SIGNAL(toggled(bool)), this, SLOT(sltUpdateWidgetAvailability()));
                    }
                    /* Prepare 'discard' check-box: */
                    m_pDiscardCheckBox = new QCheckBox(this);
                    /* Configure layout: */
#ifdef Q_WS_MAC
                    pChoiceLayout->setSpacing(15);
#else /* Q_WS_MAC */
                    pChoiceLayout->setSpacing(6);
#endif /* !Q_WS_MAC */
                    pChoiceLayout->setContentsMargins(0, 0, 0, 0);
                    pChoiceLayout->addWidget(m_pSaveIcon, 0, 0);
                    pChoiceLayout->addWidget(m_pSaveRadio, 0, 1);
                    pChoiceLayout->addWidget(m_pShutdownIcon, 1, 0);
                    pChoiceLayout->addWidget(m_pShutdownRadio, 1, 1);
                    pChoiceLayout->addWidget(m_pPowerOffIcon, 2, 0);
                    pChoiceLayout->addWidget(m_pPowerOffRadio, 2, 1);
                    pChoiceLayout->addWidget(m_pDiscardCheckBox, 3, 1);
                }
                /* Configure layout: */
#ifdef Q_WS_MAC
                pTopRightLayout->setSpacing(15);
#else /* Q_WS_MAC */
                pTopRightLayout->setSpacing(6);
#endif /* !Q_WS_MAC */
                pTopRightLayout->setContentsMargins(0, 0, 0, 0);
                pTopRightLayout->addWidget(m_pLabel);
                pTopRightLayout->addItem(pChoiceLayout);
            }
            /* Configure layout: */
            pTopLayout->setSpacing(20);
            pTopLayout->setContentsMargins(0, 0, 0, 0);
            pTopLayout->addItem(pTopLeftLayout);
            pTopLayout->addItem(pTopRightLayout);
        }
        /* Prepare button-box: */
        QIDialogButtonBox *pButtonBox = new QIDialogButtonBox(this);
        {
            /* Configure button-box: */
            pButtonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Help | QDialogButtonBox::NoButton | QDialogButtonBox::Ok);
            connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
            connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
            connect(pButtonBox, SIGNAL(helpRequested()), &msgCenter(), SLOT(sltShowHelpHelpDialog()));
        }
        /* Configure layout: */
        pMainLayout->setSpacing(20);
#ifdef Q_WS_MAC
        pMainLayout->setContentsMargins(40, 20, 40, 20);
#endif /* Q_WS_MAC */
        pMainLayout->addItem(pTopLayout);
        pMainLayout->addWidget(pButtonBox);
    }
}

void UIVMCloseDialog::configure(const CMachine &machine, const CSession &session)
{
    /* Assign machine: */
    m_machine = machine;

    /* Get machine-state: */
    KMachineState machineState = m_machine.GetState();

    /* Assign pixmap: */
    setPixmap(vboxGlobal().vmGuestOSTypeIcon(m_machine.GetOSTypeId()));

    /* Check which close-actions are resticted: */
    QStringList restictedActionsList = m_machine.GetExtraData(GUI_RestrictedCloseActions).split(',');
    bool fIsStateSavingAllowed = !restictedActionsList.contains("SaveState", Qt::CaseInsensitive);
    bool fIsACPIShutdownAllowed = !restictedActionsList.contains("Shutdown", Qt::CaseInsensitive);
    bool fIsPowerOffAllowed = !restictedActionsList.contains("PowerOff", Qt::CaseInsensitive);
    bool fIsPowerOffAndRestoreAllowed = fIsPowerOffAllowed && !restictedActionsList.contains("Restore", Qt::CaseInsensitive);

    /* Make 'Save state' button visible/hidden depending on restriction: */
    setSaveButtonVisible(fIsStateSavingAllowed);
    /* Make 'Save state' button enabled/disabled depending on machine-state: */
    setSaveButtonEnabled(machineState != KMachineState_Stuck);
    /* Make 'Shutdown button' visible/hidden depending on restriction: */
    setShutdownButtonVisible(fIsACPIShutdownAllowed);
    /* Make 'Shutdown button' enabled/disabled depending on ACPI-state & machine-state: */
    m_fIsACPIEnabled = session.GetConsole().GetGuestEnteredACPIMode();
    setShutdownButtonEnabled(m_fIsACPIEnabled && machineState != KMachineState_Stuck);
    /* Make 'Power off' button visible/hidden depending on restriction: */
    setPowerOffButtonVisible(fIsPowerOffAllowed);
    /* Make the Restore Snapshot checkbox visible/hidden depending on snapshot count & restrictions: */
    setDiscardCheckBoxVisible(fIsPowerOffAndRestoreAllowed && m_machine.GetSnapshotCount() > 0);
    /* Assign Restore Snapshot checkbox text: */
    if (!m_machine.GetCurrentSnapshot().isNull())
        m_strDiscardCheckBoxText = m_machine.GetCurrentSnapshot().GetName();

    /* Read the last user's choice for the given VM: */
    QStringList lastAction = m_machine.GetExtraData(GUI_LastCloseAction).split(',');

    /* Check which radio-button should be initially chosen: */
    QRadioButton *pRadioButtonToChoose = 0;
    /* If choosing 'last choice' is possible: */
    if (lastAction[0] == m_strExtraDataOptionSave && fIsStateSavingAllowed)
    {
        pRadioButtonToChoose = m_pSaveRadio;
    }
    else if (lastAction[0] == m_strExtraDataOptionShutdown && fIsACPIShutdownAllowed && m_fIsACPIEnabled)
    {
        pRadioButtonToChoose = m_pShutdownRadio;
    }
    else if (lastAction[0] == m_strExtraDataOptionPowerOff && fIsPowerOffAllowed)
    {
        pRadioButtonToChoose = m_pPowerOffRadio;
        if (fIsPowerOffAndRestoreAllowed)
            m_pDiscardCheckBox->setChecked(lastAction.count() > 1 && lastAction[1] == m_strExtraDataOptionDiscard);
    }
    /* Else 'default choice' will be used: */
    else
    {
        if (fIsACPIShutdownAllowed && m_fIsACPIEnabled)
            pRadioButtonToChoose = m_pShutdownRadio;
        else if (fIsStateSavingAllowed)
            pRadioButtonToChoose = m_pSaveRadio;
        else if (fIsPowerOffAllowed)
            pRadioButtonToChoose = m_pPowerOffRadio;
    }

    /* If some radio-button chosen: */
    if (pRadioButtonToChoose)
    {
        /* Check and focus it: */
        pRadioButtonToChoose->setChecked(true);
        pRadioButtonToChoose->setFocus();
        sltUpdateWidgetAvailability();
        m_fValid = true;
    }
}

void UIVMCloseDialog::retranslateUi()
{
    /* Translate title: */
    setWindowTitle(tr("Close Virtual Machine"));

    /* Translate 'text' label: */
    m_pLabel->setText(tr("You want to:"));

    /* Translate radio-buttons: */
    m_pSaveRadio->setText(tr("&Save the machine state"));
    m_pSaveRadio->setWhatsThis(tr("<p>Saves the current execution state of the virtual machine to the physical hard disk of the host PC.</p>"
                                  "<p>Next time this machine is started, it will be restored from the saved state and continue execution "
                                  "from the same place you saved it at, which will let you continue your work immediately.</p>"
                                  "<p>Note that saving the machine state may take a long time, depending on the guest operating "
                                  "system type and the amount of memory you assigned to the virtual machine.</p>"));
    m_pShutdownRadio->setText(tr("S&end the shutdown signal"));
    m_pShutdownRadio->setWhatsThis(tr("<p>Sends the ACPI Power Button press event to the virtual machine.</p>"
                                      "<p>Normally, the guest operating system running inside the virtual machine will detect this event "
                                      "and perform a clean shutdown procedure. This is a recommended way to turn off the virtual machine "
                                      "because all applications running inside it will get a chance to save their data and state.</p>"
                                      "<p>If the machine doesn't respond to this action then the guest operating system may be misconfigured "
                                      "or doesn't understand ACPI Power Button events at all. In this case you should select the "
                                      "<b>Power off the machine</b> action to stop virtual machine execution.</p>"));
    m_pPowerOffRadio->setText(tr("&Power off the machine"));
    m_pPowerOffRadio->setWhatsThis(tr("<p>Turns off the virtual machine.</p>"
                                      "<p>Note that this action will stop machine execution immediately so that the guest operating system "
                                      "running inside it will not be able to perform a clean shutdown procedure which may result in "
                                      "<i>data loss</i> inside the virtual machine. Selecting this action is recommended only if the "
                                      "virtual machine does not respond to the <b>Send the shutdown signal</b> action.</p>"));
    m_pDiscardCheckBox->setText(tr("&Restore current snapshot '%1'").arg(m_strDiscardCheckBoxText));
    m_pDiscardCheckBox->setToolTip(tr("Restore the machine state stored in the current snapshot"));
    m_pDiscardCheckBox->setWhatsThis(tr("<p>When checked, the machine will be returned to the state stored in the current snapshot after "
                                        "it is turned off. This is useful if you are sure that you want to discard the results of your "
                                        "last sessions and start again at that snapshot.</p>"));
}

void UIVMCloseDialog::polishEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QIDialog::polishEvent(pEvent);

    /* Make the dialog-size fixed: */
    setFixedSize(size());
}

