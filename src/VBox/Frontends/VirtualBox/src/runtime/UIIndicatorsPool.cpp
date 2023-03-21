/* $Id$ */
/** @file
 * VBox Qt GUI - UIIndicatorsPool class implementation.
 */

/*
 * Copyright (C) 2010-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QAccessibleWidget>
#include <QHBoxLayout>
#include <QPainter>
#include <QStyle>
#include <QTimer>

/* GUI includes: */
#include "QIStatusBarIndicator.h"
#include "QIWithRetranslateUI.h"
#include "UIAnimationFramework.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIExtraDataManager.h"
#include "UIHostComboEditor.h"
#include "UIIconPool.h"
#include "UIIndicatorsPool.h"
#include "UIMachine.h"


/** QIStateStatusBarIndicator extension for Runtime UI. */
class UISessionStateStatusBarIndicator : public QIWithRetranslateUI<QIStateStatusBarIndicator>
{
    Q_OBJECT;

public:

    /** Constructor which remembers passed @a session object. */
    UISessionStateStatusBarIndicator(IndicatorType enmType, UIMachine *pMachine);

    /** Returns the indicator type. */
    IndicatorType type() const { return m_enmType; }

    /** Returns the indicator description. */
    virtual QString description() const { return m_strDescription; }

public slots:

    /** Abstract update routine. */
    virtual void updateAppearance() = 0;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

    /** Holds the indicator type. */
    const IndicatorType m_enmType;

    /** Holds the machine UI reference. */
    UIMachine *m_pMachine;

    /** Holds the indicator description. */
    QString m_strDescription;

    /** Holds the table format. */
    static const QString s_strTable;
    /** Holds the table row format 1. */
    static const QString s_strTableRow1;
    /** Holds the table row format 2. */
    static const QString s_strTableRow2;
    /** Holds the table row format 3. */
    static const QString s_strTableRow3;
    /** Holds the table row format 4. */
    static const QString s_strTableRow4;
};


/* static */
const QString UISessionStateStatusBarIndicator::s_strTable = QString("<table cellspacing=5 style='white-space:pre'>%1</table>");
/* static */
const QString UISessionStateStatusBarIndicator::s_strTableRow1 = QString("<tr><td colspan='2'><nobr><b>%1</b></nobr></td></tr>");
/* static */
const QString UISessionStateStatusBarIndicator::s_strTableRow2 = QString("<tr><td><nobr>%1:</nobr></td><td><nobr>%2</nobr></td></tr>");
/* static */
const QString UISessionStateStatusBarIndicator::s_strTableRow3 = QString("<tr><td><nobr>%1</nobr></td><td><nobr>%2</nobr></td></tr>");
/* static */
const QString UISessionStateStatusBarIndicator::s_strTableRow4 = QString("<tr><td><nobr>&nbsp;%1:</nobr></td><td><nobr>%2</nobr></td></tr>");


/** QAccessibleWidget extension used as an accessibility interface for UISessionStateStatusBarIndicator. */
class QIAccessibilityInterfaceForUISessionStateStatusBarIndicator : public QAccessibleWidget
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating UISessionStateStatusBarIndicator accessibility interface: */
        if (pObject && strClassname == QLatin1String("UISessionStateStatusBarIndicator"))
            return new QIAccessibilityInterfaceForUISessionStateStatusBarIndicator(qobject_cast<QWidget*>(pObject));

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pWidget to the base-class. */
    QIAccessibilityInterfaceForUISessionStateStatusBarIndicator(QWidget *pWidget)
        : QAccessibleWidget(pWidget, QAccessible::Button)
    {}

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text /* enmTextRole */) const RT_OVERRIDE
    {
        /* Sanity check: */
        AssertPtrReturn(indicator(), 0);

        /* Return the indicator description: */
        return indicator()->description();
    }

private:

    /** Returns corresponding UISessionStateStatusBarIndicator. */
    UISessionStateStatusBarIndicator *indicator() const { return qobject_cast<UISessionStateStatusBarIndicator*>(widget()); }
};


UISessionStateStatusBarIndicator::UISessionStateStatusBarIndicator(IndicatorType enmType, UIMachine *pMachine)
    : m_enmType(enmType)
    , m_pMachine(pMachine)
{
    /* Install UISessionStateStatusBarIndicator accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForUISessionStateStatusBarIndicator::pFactory);
}

void UISessionStateStatusBarIndicator::retranslateUi()
{
    /* Translate description: */
    m_strDescription = tr("%1 status-bar indicator", "like 'hard-disk status-bar indicator'")
                         .arg(gpConverter->toString(type()));

    /* Update appearance finally: */
    updateAppearance();
}


/** UISessionStateStatusBarIndicator extension for Runtime UI: Hard-drive indicator. */
class UIIndicatorHardDrive : public UISessionStateStatusBarIndicator
{
    Q_OBJECT;

public:

    /** Constructs indicator passing @a pMachine to the base-class. */
    UIIndicatorHardDrive(UIMachine *pMachine)
        : UISessionStateStatusBarIndicator(IndicatorType_HardDisks, pMachine)
    {
        /* Assign state-icons: */
        setStateIcon(KDeviceActivity_Idle,    UIIconPool::iconSet(":/hd_16px.png"));
        setStateIcon(KDeviceActivity_Reading, UIIconPool::iconSet(":/hd_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, UIIconPool::iconSet(":/hd_write_16px.png"));
        setStateIcon(KDeviceActivity_Null,    UIIconPool::iconSet(":/hd_disabled_16px.png"));
        /* Configure connection: */
        connect(pMachine, &UIMachine::sigMachineStateChange,
                this, &UIIndicatorHardDrive::updateAppearance);
        connect(pMachine, &UIMachine::sigStorageDeviceChange,
                this, &UIIndicatorHardDrive::updateAppearance);
        connect(pMachine, &UIMachine::sigMediumChange,
                this, &UIIndicatorHardDrive::updateAppearance);
        /* Translate finally: */
        retranslateUi();
    }

protected slots:

    /** Update routine. */
    virtual void updateAppearance() RT_OVERRIDE
    {
        /* Acquire data: */
        QString strFullData;
        bool fAttachmentsPresent = false;
        m_pMachine->acquireHardDiskStatusInfo(strFullData, fAttachmentsPresent);

        /* Show/hide indicator if there are no attachments
         * and parent is visible already: */
        if (   parentWidget()
            && parentWidget()->isVisible())
            setVisible(fAttachmentsPresent);

        /* Update tool-tip: */
        if (!strFullData.isEmpty())
            setToolTip(s_strTable.arg(strFullData));
        /* Update indicator state: */
        setState(fAttachmentsPresent ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }
};


/** UISessionStateStatusBarIndicator extension for Runtime UI: Optical-drive indicator. */
class UIIndicatorOpticalDisks : public UISessionStateStatusBarIndicator
{
    Q_OBJECT;

public:

    /** Constructs indicator passing @a pMachine to the base-class. */
    UIIndicatorOpticalDisks(UIMachine *pMachine)
        : UISessionStateStatusBarIndicator(IndicatorType_OpticalDisks, pMachine)
    {
        /* Assign state-icons: */
        setStateIcon(KDeviceActivity_Idle,    UIIconPool::iconSet(":/cd_16px.png"));
        setStateIcon(KDeviceActivity_Reading, UIIconPool::iconSet(":/cd_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, UIIconPool::iconSet(":/cd_write_16px.png"));
        setStateIcon(KDeviceActivity_Null,    UIIconPool::iconSet(":/cd_disabled_16px.png"));
        /* Configure connection: */
        connect(pMachine, &UIMachine::sigMachineStateChange,
                this, &UIIndicatorOpticalDisks::updateAppearance);
        connect(pMachine, &UIMachine::sigStorageDeviceChange,
                this, &UIIndicatorOpticalDisks::updateAppearance);
        connect(pMachine, &UIMachine::sigMediumChange,
                this, &UIIndicatorOpticalDisks::updateAppearance);
        /* Translate finally: */
        retranslateUi();
    }

protected slots:

    /** Update routine. */
    virtual void updateAppearance() RT_OVERRIDE
    {
        QString strFullData;
        bool fAttachmentsPresent = false;
        bool fAttachmentsMounted = false;
        m_pMachine->acquireOpticalDiskStatusInfo(strFullData, fAttachmentsPresent, fAttachmentsMounted);

        /* Show/hide indicator if there are no attachments
         * and parent is visible already: */
        if (   parentWidget()
            && parentWidget()->isVisible())
            setVisible(fAttachmentsPresent);

        /* Update tool-tip: */
        if (!strFullData.isEmpty())
            setToolTip(s_strTable.arg(strFullData));
        /* Update indicator state: */
        setState(fAttachmentsMounted ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }
};


/** UISessionStateStatusBarIndicator extension for Runtime UI: Floppy-drive indicator. */
class UIIndicatorFloppyDisks : public UISessionStateStatusBarIndicator
{
    Q_OBJECT;

public:

    /** Constructs indicator passing @a pMachine to the base-class. */
    UIIndicatorFloppyDisks(UIMachine *pMachine)
        : UISessionStateStatusBarIndicator(IndicatorType_FloppyDisks, pMachine)
    {
        /* Assign state-icons: */
        setStateIcon(KDeviceActivity_Idle,    UIIconPool::iconSet(":/fd_16px.png"));
        setStateIcon(KDeviceActivity_Reading, UIIconPool::iconSet(":/fd_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, UIIconPool::iconSet(":/fd_write_16px.png"));
        setStateIcon(KDeviceActivity_Null,    UIIconPool::iconSet(":/fd_disabled_16px.png"));
        /* Configure connection: */
        connect(pMachine, &UIMachine::sigMachineStateChange,
                this, &UIIndicatorFloppyDisks::updateAppearance);
        connect(pMachine, &UIMachine::sigStorageDeviceChange,
                this, &UIIndicatorFloppyDisks::updateAppearance);
        connect(pMachine, &UIMachine::sigMediumChange,
                this, &UIIndicatorFloppyDisks::updateAppearance);
        /* Translate finally: */
        retranslateUi();
    }

protected slots:

    /** Update routine. */
    virtual void updateAppearance() RT_OVERRIDE
    {
        QString strFullData;
        bool fAttachmentsPresent = false;
        bool fAttachmentsMounted = false;
        m_pMachine->acquireFloppyDiskStatusInfo(strFullData, fAttachmentsPresent, fAttachmentsMounted);

        /* Show/hide indicator if there are no attachments
         * and parent is visible already: */
        if (   parentWidget()
            && parentWidget()->isVisible())
            setVisible(fAttachmentsPresent);

        /* Update tool-tip: */
        if (!strFullData.isEmpty())
            setToolTip(s_strTable.arg(strFullData));
        /* Update indicator state: */
        setState(fAttachmentsMounted ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }
};


/** UISessionStateStatusBarIndicator extension for Runtime UI: Audio indicator. */
class UIIndicatorAudio : public UISessionStateStatusBarIndicator
{
    Q_OBJECT;

public:

    /** Audio states. */
    enum AudioState
    {
        AudioState_AllOff   = 0,
        AudioState_OutputOn = RT_BIT(0),
        AudioState_InputOn  = RT_BIT(1),
        AudioState_AllOn    = AudioState_InputOn | AudioState_OutputOn
    };

    /** Constructs indicator passing @a pMachine to the base-class. */
    UIIndicatorAudio(UIMachine *pMachine)
        : UISessionStateStatusBarIndicator(IndicatorType_Audio, pMachine)
    {
        /* Assign state-icons: */
        setStateIcon(AudioState_AllOff, UIIconPool::iconSet(":/audio_all_off_16px.png"));
        setStateIcon(AudioState_OutputOn, UIIconPool::iconSet(":/audio_input_off_16px.png"));
        setStateIcon(AudioState_InputOn, UIIconPool::iconSet(":/audio_output_off_16px.png"));
        setStateIcon(AudioState_AllOn, UIIconPool::iconSet(":/audio_16px.png"));
        /* Configure connection: */
        connect(pMachine, &UIMachine::sigMachineStateChange,
                this, &UIIndicatorAudio::updateAppearance);
        connect(pMachine, &UIMachine::sigAudioAdapterChange,
                this, &UIIndicatorAudio::updateAppearance);
        /* Translate finally: */
        retranslateUi();
    }

protected slots:

    /** Update routine. */
    virtual void updateAppearance() RT_OVERRIDE
    {
        QString strFullData;
        bool fAudioEnabled = false;
        bool fEnabledOutput = false;
        bool fEnabledInput = false;
        m_pMachine->acquireAudioStatusInfo(strFullData, fAudioEnabled, fEnabledOutput, fEnabledInput);

        /* Show/hide indicator if there are no attachments
         * and parent is visible already: */
        if (   parentWidget()
            && parentWidget()->isVisible())
            setVisible(fAudioEnabled);

        /* Update tool-tip: */
        if (!strFullData.isEmpty())
            setToolTip(s_strTable.arg(strFullData));
        /* Update indicator state: */
        AudioState enmState = AudioState_AllOff;
        if (fEnabledOutput)
            enmState = (AudioState)(enmState | AudioState_OutputOn);
        if (fEnabledInput)
            enmState = (AudioState)(enmState | AudioState_InputOn);
        setState(enmState);
    }
};


/** UISessionStateStatusBarIndicator extension for Runtime UI: Network indicator. */
class UIIndicatorNetwork : public UISessionStateStatusBarIndicator
{
    Q_OBJECT;

public:

    /** Constructs indicator passing @a pMachine to the base-class. */
    UIIndicatorNetwork(UIMachine *pMachine)
        : UISessionStateStatusBarIndicator(IndicatorType_Network, pMachine)
    {
        /* Assign state-icons: */
        setStateIcon(KDeviceActivity_Idle,    UIIconPool::iconSet(":/nw_16px.png"));
        setStateIcon(KDeviceActivity_Reading, UIIconPool::iconSet(":/nw_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, UIIconPool::iconSet(":/nw_write_16px.png"));
        setStateIcon(KDeviceActivity_Null,    UIIconPool::iconSet(":/nw_disabled_16px.png"));
        /* Configure connection: */
        connect(m_pMachine, &UIMachine::sigMachineStateChange,
                this, &UIIndicatorNetwork::updateAppearance);
        connect(m_pMachine, &UIMachine::sigNetworkAdapterChange,
                this, &UIIndicatorNetwork::updateAppearance);
        /* Translate finally: */
        retranslateUi();
    }

protected slots:

    /** Update routine. */
    virtual void updateAppearance() RT_OVERRIDE
    {
        QString strFullData;
        bool fAdaptersPresent = false;
        bool fCablesDisconnected = true;
        m_pMachine->acquireNetworkStatusInfo(strFullData, fAdaptersPresent, fCablesDisconnected);

        /* Show/hide indicator if there are no attachments
         * and parent is visible already: */
        if (   parentWidget()
            && parentWidget()->isVisible())
            setVisible(fAdaptersPresent);

        /* Update tool-tip: */
        if (!strFullData.isEmpty())
            setToolTip(s_strTable.arg(strFullData));
        /* Update indicator state: */
        setState(fAdaptersPresent && !fCablesDisconnected ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }
};


/** UISessionStateStatusBarIndicator extension for Runtime UI: USB indicator. */
class UIIndicatorUSB : public UISessionStateStatusBarIndicator
{
    Q_OBJECT;

public:

    /** Constructs indicator passing @a pMachine to the base-class. */
    UIIndicatorUSB(UIMachine *pMachine)
        : UISessionStateStatusBarIndicator(IndicatorType_USB, pMachine)
    {
        /* Assign state-icons: */
        setStateIcon(KDeviceActivity_Idle,    UIIconPool::iconSet(":/usb_16px.png"));
        setStateIcon(KDeviceActivity_Reading, UIIconPool::iconSet(":/usb_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, UIIconPool::iconSet(":/usb_write_16px.png"));
        setStateIcon(KDeviceActivity_Null,    UIIconPool::iconSet(":/usb_disabled_16px.png"));
        /* Configure connection: */
        connect(pMachine, &UIMachine::sigMachineStateChange,
                this, &UIIndicatorUSB::updateAppearance);
        /* Translate finally: */
        retranslateUi();
    }

protected slots:

    /** Update routine. */
    virtual void updateAppearance() RT_OVERRIDE
    {
        QString strFullData;
        bool fUsbEnabled = false;
        m_pMachine->acquireUsbStatusInfo(strFullData, fUsbEnabled);

        /* Show/hide indicator if there are no attachments
         * and parent is visible already: */
        if (   parentWidget()
            && parentWidget()->isVisible())
            setVisible(fUsbEnabled);

        /* Update tool-tip: */
        if (!strFullData.isEmpty())
            setToolTip(s_strTable.arg(strFullData));
        /* Update indicator state: */
        setState(fUsbEnabled ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }
};


/** UISessionStateStatusBarIndicator extension for Runtime UI: Shared-folders indicator. */
class UIIndicatorSharedFolders : public UISessionStateStatusBarIndicator
{
    Q_OBJECT;

public:

    /** Constructs indicator passing @a pMachine to the base-class. */
    UIIndicatorSharedFolders(UIMachine *pMachine)
        : UISessionStateStatusBarIndicator(IndicatorType_SharedFolders, pMachine)
    {
        /* Assign state-icons: */
        setStateIcon(KDeviceActivity_Idle,    UIIconPool::iconSet(":/sf_16px.png"));
        setStateIcon(KDeviceActivity_Reading, UIIconPool::iconSet(":/sf_read_16px.png"));
        setStateIcon(KDeviceActivity_Writing, UIIconPool::iconSet(":/sf_write_16px.png"));
        setStateIcon(KDeviceActivity_Null,    UIIconPool::iconSet(":/sf_disabled_16px.png"));
        /* Configure connection: */
        connect(pMachine, &UIMachine::sigMachineStateChange,
                this, &UIIndicatorSharedFolders::updateAppearance);
        /* Translate finally: */
        retranslateUi();
    }

protected slots:

    /** Update routine. */
    virtual void updateAppearance() RT_OVERRIDE
    {
        QString strFullData;
        bool fFoldersPresent = false;
        m_pMachine->acquireSharedFoldersStatusInfo(strFullData, fFoldersPresent);

        /* Update tool-tip: */
        if (!strFullData.isEmpty())
            setToolTip(s_strTable.arg(strFullData));
        /* Update indicator state: */
        setState(fFoldersPresent ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }
};


/** UISessionStateStatusBarIndicator extension for Runtime UI: Display indicator. */
class UIIndicatorDisplay : public UISessionStateStatusBarIndicator
{
    Q_OBJECT;

public:

    /** Display states. */
    enum DisplayState
    {
        DisplayState_Unavailable = 0,
        DisplayState_Software = 1,
        DisplayState_Hardware = 2
    };

    /** Constructs indicator passing @a pMachine to the base-class. */
    UIIndicatorDisplay(UIMachine *pMachine)
        : UISessionStateStatusBarIndicator(IndicatorType_Display, pMachine)
    {
        /* Assign state-icons: */
        setStateIcon(DisplayState_Unavailable, UIIconPool::iconSet(":/display_software_disabled_16px.png"));
        setStateIcon(DisplayState_Software,    UIIconPool::iconSet(":/display_software_16px.png"));
        setStateIcon(DisplayState_Hardware,    UIIconPool::iconSet(":/display_hardware_16px.png"));
        /* Configure connection: */
        connect(pMachine, &UIMachine::sigMachineStateChange,
                this, &UIIndicatorDisplay::updateAppearance);
        /* Translate finally: */
        retranslateUi();
    }

protected slots:

    /** Update routine. */
    virtual void updateAppearance() RT_OVERRIDE
    {
        QString strFullData;
        bool fAcceleration3D = false;
        m_pMachine->acquireDisplayStatusInfo(strFullData, fAcceleration3D);

        /* Update tool-tip: */
        if (!strFullData.isEmpty())
            setToolTip(s_strTable.arg(strFullData));
        /* Update indicator state: */
        DisplayState enmState = DisplayState_Unavailable;
        if (m_pMachine->machineState() != KMachineState_Null)
        {
            if (!fAcceleration3D)
                enmState = DisplayState_Software;
            else
                enmState = DisplayState_Hardware;
        }
        setState(enmState);
    }
};


/** UISessionStateStatusBarIndicator extension for Runtime UI: Recording indicator. */
class UIIndicatorRecording : public UISessionStateStatusBarIndicator
{
    Q_OBJECT;
    Q_PROPERTY(double rotationAngleStart READ rotationAngleStart);
    Q_PROPERTY(double rotationAngleFinal READ rotationAngleFinal);
    Q_PROPERTY(double rotationAngle READ rotationAngle WRITE setRotationAngle);

    /** Recording states. */
    enum RecordingState
    {
        RecordingState_Unavailable = 0,
        RecordingState_Disabled    = 1,
        RecordingState_Enabled     = 2,
        RecordingState_Paused      = 3
    };

public:

    /** Constructs indicator passing @a pMachine to the base-class. */
    UIIndicatorRecording(UIMachine *pMachine)
        : UISessionStateStatusBarIndicator(IndicatorType_Recording, pMachine)
        , m_pAnimation(0)
        , m_dRotationAngle(0)
    {
        /* Assign state-icons: */
        setStateIcon(RecordingState_Unavailable, UIIconPool::iconSet(":/video_capture_disabled_16px.png"));
        setStateIcon(RecordingState_Disabled,    UIIconPool::iconSet(":/video_capture_16px.png"));
        setStateIcon(RecordingState_Enabled,     UIIconPool::iconSet(":/movie_reel_16px.png"));
        setStateIcon(RecordingState_Paused,      UIIconPool::iconSet(":/movie_reel_16px.png"));
        /* Configure connection: */
        connect(pMachine, &UIMachine::sigMachineStateChange,
                this, &UIIndicatorRecording::updateAppearance);
        /* Create *enabled* state animation: */
        m_pAnimation = UIAnimationLoop::installAnimationLoop(this, "rotationAngle",
                                                             "rotationAngleStart", "rotationAngleFinal",
                                                             1000);
        /* Translate finally: */
        retranslateUi();
    }

protected:

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) RT_OVERRIDE
    {
        /* Call to base-class: */
        UISessionStateStatusBarIndicator::paintEvent(pEvent);

        /* Create new painter: */
        QPainter painter(this);
        /* Configure painter for *enabled* state: */
        if (state() == RecordingState_Enabled)
        {
            /* Configure painter for smooth animation: */
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            /* Shift rotation origin according pixmap center: */
            painter.translate(height() / 2, height() / 2);
            /* Rotate painter: */
            painter.rotate(rotationAngle());
            /* Unshift rotation origin according pixmap center: */
            painter.translate(- height() / 2, - height() / 2);
        }
        /* Draw contents: */
        drawContents(&painter);
    }

protected slots:

    /** Update routine. */
    virtual void updateAppearance() RT_OVERRIDE
    {
        QString strFullData;
        bool fRecordingEnabled = false;
        bool fMachinePaused = false;
        m_pMachine->acquireRecordingStatusInfo(strFullData, fRecordingEnabled, fMachinePaused);

        /* Update tool-tip: */
        if (!strFullData.isEmpty())
            setToolTip(s_strTable.arg(strFullData));
        /* Set initial indicator state: */
        RecordingState enmState = RecordingState_Unavailable;
        if (m_pMachine->machineState() != KMachineState_Null)
        {
            if (!fRecordingEnabled)
                enmState = RecordingState_Disabled;
            else if (!fMachinePaused)
                enmState = RecordingState_Enabled;
            else
                enmState = RecordingState_Paused;
        }
        setState(enmState);
    }

private slots:

    /** Handles state change. */
    void setState(int iState)
    {
        /* Update animation state: */
        switch (iState)
        {
            case RecordingState_Disabled:
                m_pAnimation->stop();
                m_dRotationAngle = 0;
                break;
            case RecordingState_Enabled:
                m_pAnimation->start();
                break;
            case RecordingState_Paused:
                m_pAnimation->stop();
                break;
            default:
                break;
        }
        /* Call to base-class: */
        QIStateStatusBarIndicator::setState(iState);
    }

private:

    /** Returns rotation start angle. */
    double rotationAngleStart() const { return 0; }
    /** Returns rotation finish angle. */
    double rotationAngleFinal() const { return 360; }
    /** Returns current rotation angle. */
    double rotationAngle() const { return m_dRotationAngle; }
    /** Defines current rotation angle. */
    void setRotationAngle(double dRotationAngle) { m_dRotationAngle = dRotationAngle; update(); }

    /** Holds the rotation animation instance. */
    UIAnimationLoop *m_pAnimation;
    /** Holds current rotation angle. */
    double  m_dRotationAngle;
};


/** UISessionStateStatusBarIndicator extension for Runtime UI: Features indicator. */
class UIIndicatorFeatures : public UISessionStateStatusBarIndicator
{
    Q_OBJECT;

public:

    /** Constructs indicator passing @a pMachine to the base-class. */
    UIIndicatorFeatures(UIMachine *pMachine)
        : UISessionStateStatusBarIndicator(IndicatorType_Features, pMachine)
        , m_uEffectiveCPULoad(0)
    {
        /* Assign state-icons: */
        /** @todo  The vtx_amdv_disabled_16px.png icon isn't really approprate anymore (no raw-mode),
         * might want to get something different for KVMExecutionEngine_Emulated or reuse the
         * vm_execution_engine_native_api_16px.png one... @bugref{9898} */
        setStateIcon(KVMExecutionEngine_NotSet, UIIconPool::iconSet(":/vtx_amdv_disabled_16px.png"));
        setStateIcon(KVMExecutionEngine_Emulated, UIIconPool::iconSet(":/vtx_amdv_disabled_16px.png"));
        setStateIcon(KVMExecutionEngine_HwVirt, UIIconPool::iconSet(":/vtx_amdv_16px.png"));
        setStateIcon(KVMExecutionEngine_NativeApi, UIIconPool::iconSet(":/vm_execution_engine_native_api_16px.png"));
        /* Configure machine state-change listener: */
        connect(m_pMachine, &UIMachine::sigMachineStateChange,
                this, &UIIndicatorFeatures::sltHandleMachineStateChange);
        m_pTimerAutoUpdate = new QTimer(this);
        if (m_pTimerAutoUpdate)
        {
            connect(m_pTimerAutoUpdate, &QTimer::timeout,
                    this, &UIIndicatorFeatures::sltHandleTimeout);
            /* Start the timer immediately if the machine is running: */
            sltHandleMachineStateChange();
        }
        /* Translate finally: */
        retranslateUi();
    }

protected:

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) RT_OVERRIDE
    {
        /* Call to base-class: */
        UISessionStateStatusBarIndicator::paintEvent(pEvent);

        /* Create new painter: */
        QPainter painter(this);
        /* Draw a thin bar on th right hand side of the icon indication CPU load: */
        QLinearGradient gradient(0, 0, 0, height());
        gradient.setColorAt(1.0, Qt::green);
        gradient.setColorAt(0.5, Qt::yellow);
        gradient.setColorAt(0.0, Qt::red);
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        /* Use 20% of the icon width to draw the indicator bar: */
        painter.drawRect(QRect(QPoint(0.8 * width(), (100 - m_uEffectiveCPULoad) / 100.f * height()),
                               QPoint(width(), height())));
        /* Draw an empty rect. around the CPU load bar: */
        int iBorderThickness = 1;
        QRect outRect(QPoint(0.8 * width(), 0),
                      QPoint(width() - 2 * iBorderThickness,  height() - 2 * iBorderThickness));
        if (m_pMachine->machineState() == KMachineState_Running)
            painter.setPen(QPen(Qt::black, 1));
        else
            painter.setPen(QPen(Qt::gray, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(outRect);
    }

protected slots:

    /** Update routine. */
    virtual void updateAppearance() RT_OVERRIDE
    {
        QString strFullData;
        KVMExecutionEngine enmEngine = KVMExecutionEngine_NotSet;
        m_pMachine->acquireFeaturesStatusInfo(strFullData, enmEngine);

        /* Update tool-tip: */
        if (!strFullData.isEmpty())
            setToolTip(s_strTable.arg(strFullData));
        /* Update indicator state: */
        setState(enmEngine);
    }

private slots:

    /** Updates auto-update timer depending on machine state. */
    void sltHandleMachineStateChange()
    {
        if (m_pMachine->machineState() == KMachineState_Running)
        {
            /* Start auto-update timer otherwise: */
            m_pTimerAutoUpdate->start(1000);
            return;
        }
        /* Stop auto-update timer otherwise: */
        m_pTimerAutoUpdate->stop();
    }

    /** Handles timer timeout with CPU load percentage update. */
    void sltHandleTimeout()
    {
        m_pMachine->acquireEffectiveCPULoad(m_uEffectiveCPULoad);
        update();
    }

private:

    /** Holds the auto-update timer instance. */
    QTimer *m_pTimerAutoUpdate;

    /** Holds the effective CPU load. */
    ulong  m_uEffectiveCPULoad;
};


/** UISessionStateStatusBarIndicator extension for Runtime UI: Mouse indicator. */
class UIIndicatorMouse : public UISessionStateStatusBarIndicator
{
    Q_OBJECT;

public:

    /** Constructor, using @a pMachine for state-update routine. */
    UIIndicatorMouse(UIMachine *pMachine)
        : UISessionStateStatusBarIndicator(IndicatorType_Mouse, pMachine)
    {
        /* Assign state-icons: */
        setStateIcon(0, UIIconPool::iconSet(":/mouse_disabled_16px.png"));
        setStateIcon(1, UIIconPool::iconSet(":/mouse_16px.png"));
        setStateIcon(2, UIIconPool::iconSet(":/mouse_seamless_16px.png"));
        setStateIcon(3, UIIconPool::iconSet(":/mouse_can_seamless_16px.png"));
        setStateIcon(4, UIIconPool::iconSet(":/mouse_can_seamless_uncaptured_16px.png"));
        /* Configure connection: */
        connect(m_pMachine, &UIMachine::sigMouseStateChange,
                this, static_cast<void(UIIndicatorMouse::*)(int)>(&UIIndicatorMouse::setState));
        /* Translate finally: */
        retranslateUi();
    }

protected slots:

    /** Update routine. */
    virtual void updateAppearance() RT_OVERRIDE
    {
        const QString strToolTip = QApplication::translate("UIIndicatorsPool",
                                                           "Indicates whether the host mouse pointer is "
                                                           "captured by the guest OS:%1", "Mouse tooltip");
        QString strFullData;
        strFullData += s_strTableRow3
            .arg(QString("<img src=:/mouse_disabled_16px.png/>"))
            .arg(QApplication::translate("UIIndicatorsPool", "pointer is not captured", "Mouse tooltip"));
        strFullData += s_strTableRow3
            .arg(QString("<img src=:/mouse_16px.png/>"))
            .arg(QApplication::translate("UIIndicatorsPool", "pointer is captured", "Mouse tooltip"));
        strFullData += s_strTableRow3
            .arg(QString("<img src=:/mouse_seamless_16px.png/>"))
            .arg(QApplication::translate("UIIndicatorsPool", "mouse integration (MI) is On", "Mouse tooltip"));
        strFullData += s_strTableRow3
            .arg(QString("<img src=:/mouse_can_seamless_16px.png/>"))
            .arg(QApplication::translate("UIIndicatorsPool", "MI is Off, pointer is captured", "Mouse tooltip"));
        strFullData += s_strTableRow3
            .arg(QString("<img src=:/mouse_can_seamless_uncaptured_16px.png/>"))
            .arg(QApplication::translate("UIIndicatorsPool", "MI is Off, pointer is not captured", "Mouse tooltip"));
        strFullData = s_strTable.arg(strFullData);
        strFullData += QApplication::translate("UIIndicatorsPool",
                                               "Note that the mouse integration feature requires Guest "
                                               "Additions to be installed in the guest OS.", "Mouse tooltip");

        /* Update tool-tip: */
        setToolTip(strToolTip.arg(strFullData));
    }

private slots:

    /** Handles state change. */
    void setState(int iState)
    {
        if ((iState & UIMouseStateType_MouseAbsoluteDisabled) &&
            (iState & UIMouseStateType_MouseAbsolute) &&
            !(iState & UIMouseStateType_MouseCaptured))
        {
            QIStateStatusBarIndicator::setState(4);
        }
        else
        {
            QIStateStatusBarIndicator::setState(iState & (UIMouseStateType_MouseAbsolute | UIMouseStateType_MouseCaptured));
        }
    }
};

/** UISessionStateStatusBarIndicator extension for Runtime UI: Keyboard indicator. */
class UIIndicatorKeyboard : public UISessionStateStatusBarIndicator
{
    Q_OBJECT;

public:

    /** Constructor, using @a pMachine for state-update routine. */
    UIIndicatorKeyboard(UIMachine *pMachine)
        : UISessionStateStatusBarIndicator(IndicatorType_Keyboard, pMachine)
    {
        /* Assign state-icons: */
        setStateIcon(0, UIIconPool::iconSet(":/hostkey_disabled_16px.png"));
        setStateIcon(1, UIIconPool::iconSet(":/hostkey_16px.png"));
        setStateIcon(3, UIIconPool::iconSet(":/hostkey_captured_16px.png"));
        setStateIcon(5, UIIconPool::iconSet(":/hostkey_pressed_16px.png"));
        setStateIcon(7, UIIconPool::iconSet(":/hostkey_captured_pressed_16px.png"));
        setStateIcon(9, UIIconPool::iconSet(":/hostkey_checked_16px.png"));
        setStateIcon(11, UIIconPool::iconSet(":/hostkey_captured_checked_16px.png"));
        setStateIcon(13, UIIconPool::iconSet(":/hostkey_pressed_checked_16px.png"));
        setStateIcon(15, UIIconPool::iconSet(":/hostkey_captured_pressed_checked_16px.png"));
        /* Configure connection: */
        connect(pMachine, &UIMachine::sigKeyboardStateChange,
                this, static_cast<void(UIIndicatorKeyboard::*)(int)>(&UIIndicatorKeyboard::setState));
        /* Translate finally: */
        retranslateUi();
    }

protected slots:

    /** Update routine. */
    virtual void updateAppearance() RT_OVERRIDE
    {
        const QString strToolTip = QApplication::translate("UIIndicatorsPool",
                                                           "Indicates whether the host keyboard is "
                                                           "captured by the guest OS:%1", "Keyboard tooltip");
        QString strFullData;
        strFullData += s_strTableRow3
            .arg(QString("<img src=:/hostkey_16px.png/>"))
            .arg(QApplication::translate("UIIndicatorsPool", "keyboard is not captured", "Keyboard tooltip"));
        strFullData += s_strTableRow3
            .arg(QString("<img src=:/hostkey_captured_16px.png/>"))
            .arg(QApplication::translate("UIIndicatorsPool", "keyboard is captured", "Keyboard tooltip"));
        strFullData = s_strTable.arg(strFullData);

        /* Update tool-tip: */
        setToolTip(strToolTip.arg(strFullData));
    }
};

/** QITextStatusBarIndicator extension for Runtime UI: Keyboard-extension indicator. */
class UIIndicatorKeyboardExtension : public QIWithRetranslateUI<QITextStatusBarIndicator>
{
    Q_OBJECT;

public:

    /** Constructor. */
    UIIndicatorKeyboardExtension()
    {
        /* Make sure host-combination label will be updated: */
        connect(gEDataManager, &UIExtraDataManager::sigRuntimeUIHostKeyCombinationChange,
                this, &UIIndicatorKeyboardExtension::sltUpdateAppearance);
        /* Translate finally: */
        retranslateUi();
    }

public slots:

    /** Update routine. */
    void sltUpdateAppearance()
    {
        setText(UIHostCombo::toReadableString(gEDataManager->hostKeyCombination()));
    }

private:

    /** Retranslation routine. */
    void retranslateUi()
    {
        sltUpdateAppearance();
        setToolTip(QApplication::translate("UIMachineWindowNormal",
                                           "Shows the currently assigned Host key.<br>"
                                           "This key, when pressed alone, toggles the keyboard and mouse "
                                           "capture state. It can also be used in combination with other keys "
                                           "to quickly perform actions from the main menu."));
    }
};


UIIndicatorsPool::UIIndicatorsPool(UIMachine *pMachine, QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pMachine(pMachine)
    , m_fEnabled(false)
    , m_pMainLayout(0)
    , m_pTimerAutoUpdate(0)
{
    prepare();
}

UIIndicatorsPool::~UIIndicatorsPool()
{
    cleanup();
}

void UIIndicatorsPool::updateAppearance(IndicatorType indicatorType)
{
    /* Skip missed indicators: */
    if (!m_pool.contains(indicatorType))
        return;

    /* Get indicator: */
    QIStatusBarIndicator *pIndicator = m_pool.value(indicatorType);

    /* Assert indicators with NO appearance: */
    UISessionStateStatusBarIndicator *pSessionStateIndicator =
        qobject_cast<UISessionStateStatusBarIndicator*>(pIndicator);
    AssertPtrReturnVoid(pSessionStateIndicator);

    /* Update indicator appearance: */
    pSessionStateIndicator->updateAppearance();
}

void UIIndicatorsPool::setAutoUpdateIndicatorStates(bool fEnabled)
{
    /* Make sure auto-update timer exists: */
    AssertPtrReturnVoid(m_pTimerAutoUpdate);

    /* Start/stop timer: */
    if (fEnabled)
        m_pTimerAutoUpdate->start(100);
    else
        m_pTimerAutoUpdate->stop();
}

QPoint UIIndicatorsPool::mapIndicatorPositionToGlobal(IndicatorType enmIndicatorType, const QPoint &indicatorPosition)
{
    if (m_pool.contains(enmIndicatorType))
        return m_pool.value(enmIndicatorType)->mapToGlobal(indicatorPosition);
    return QPoint(0, 0);
}

void UIIndicatorsPool::sltHandleConfigurationChange(const QUuid &uMachineID)
{
    /* Skip unrelated machine IDs: */
    if (uiCommon().managedVMUuid() != uMachineID)
        return;

    /* Update pool: */
    updatePool();
}

void UIIndicatorsPool::sltAutoUpdateIndicatorStates()
{
    /* We should update states for following indicators: */
    QVector<KDeviceType> deviceTypes;
    if (m_pool.contains(IndicatorType_HardDisks))
        deviceTypes.append(KDeviceType_HardDisk);
    if (m_pool.contains(IndicatorType_OpticalDisks))
        deviceTypes.append(KDeviceType_DVD);
    if (m_pool.contains(IndicatorType_FloppyDisks))
        deviceTypes.append(KDeviceType_Floppy);
    if (m_pool.contains(IndicatorType_USB))
        deviceTypes.append(KDeviceType_USB);
    if (m_pool.contains(IndicatorType_Network))
        deviceTypes.append(KDeviceType_Network);
    if (m_pool.contains(IndicatorType_SharedFolders))
        deviceTypes.append(KDeviceType_SharedFolder);
    if (m_pool.contains(IndicatorType_Display))
        deviceTypes.append(KDeviceType_Graphics3D);

    /* Acquire current device states from the machine: */
    QVector<KDeviceActivity> states;
    m_pMachine->acquireDeviceActivity(deviceTypes, states);

    /* Update indicators with the acquired states: */
    for (int iIndicator = 0; iIndicator < states.size(); ++iIndicator)
    {
        QIStatusBarIndicator *pIndicator = 0;
        switch (deviceTypes[iIndicator])
        {
            case KDeviceType_HardDisk:     pIndicator = m_pool.value(IndicatorType_HardDisks); break;
            case KDeviceType_DVD:          pIndicator = m_pool.value(IndicatorType_OpticalDisks); break;
            case KDeviceType_Floppy:       pIndicator = m_pool.value(IndicatorType_FloppyDisks); break;
            case KDeviceType_USB:          pIndicator = m_pool.value(IndicatorType_USB); break;
            case KDeviceType_Network:      pIndicator = m_pool.value(IndicatorType_Network); break;
            case KDeviceType_SharedFolder: pIndicator = m_pool.value(IndicatorType_SharedFolders); break;
            case KDeviceType_Graphics3D:   pIndicator = m_pool.value(IndicatorType_Display); break;
            default: AssertFailed(); break;
        }
        if (pIndicator)
            updateIndicatorStateForDevice(pIndicator, states[iIndicator]);
    }
}

void UIIndicatorsPool::sltContextMenuRequest(QIStatusBarIndicator *pIndicator, QContextMenuEvent *pEvent)
{
    /* If that is one of pool indicators: */
    foreach (IndicatorType indicatorType, m_pool.keys())
        if (m_pool[indicatorType] == pIndicator)
        {
            /* Notify listener: */
            emit sigContextMenuRequest(indicatorType, pEvent->pos());
            return;
        }
}

void UIIndicatorsPool::prepare()
{
    /* Prepare connections: */
    prepareConnections();
    /* Prepare contents: */
    prepareContents();
    /* Prepare auto-update timer: */
    prepareUpdateTimer();
}

void UIIndicatorsPool::prepareConnections()
{
    /* Listen for the status-bar configuration changes: */
    connect(gEDataManager, &UIExtraDataManager::sigStatusBarConfigurationChange,
            this, &UIIndicatorsPool::sltHandleConfigurationChange);
}

void UIIndicatorsPool::prepareContents()
{
    /* Create main-layout: */
    m_pMainLayout = new QHBoxLayout(this);
    AssertPtrReturnVoid(m_pMainLayout);
    {
        /* Configure main-layout: */
        m_pMainLayout->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
        m_pMainLayout->setSpacing(5);
#else
        m_pMainLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) / 2);
#endif
        /* Update pool: */
        updatePool();
    }
}

void UIIndicatorsPool::prepareUpdateTimer()
{
    /* Create auto-update timer: */
    m_pTimerAutoUpdate = new QTimer(this);
    AssertPtrReturnVoid(m_pTimerAutoUpdate);
    {
        /* Configure auto-update timer: */
        connect(m_pTimerAutoUpdate, &QTimer::timeout,
                this, &UIIndicatorsPool::sltAutoUpdateIndicatorStates);
        setAutoUpdateIndicatorStates(true);
    }
}

void UIIndicatorsPool::updatePool()
{
    /* Acquire status-bar availability: */
    m_fEnabled = gEDataManager->statusBarEnabled(uiCommon().managedVMUuid());
    /* If status-bar is not enabled: */
    if (!m_fEnabled)
    {
        /* Remove all indicators: */
        while (!m_pool.isEmpty())
        {
            const IndicatorType firstType = m_pool.keys().first();
            delete m_pool.value(firstType);
            m_pool.remove(firstType);
        }
        /* And return: */
        return;
    }

    /* Acquire status-bar restrictions: */
    m_restrictions = gEDataManager->restrictedStatusBarIndicators(uiCommon().managedVMUuid());
    /* Make sure 'Recording' is restricted as well if no features supported: */
    if (   !m_restrictions.contains(IndicatorType_Recording)
        && !uiCommon().supportedRecordingFeatures())
        m_restrictions << IndicatorType_Recording;

    /* Remove restricted indicators: */
    foreach (const IndicatorType &indicatorType, m_restrictions)
    {
        if (m_pool.contains(indicatorType))
        {
            delete m_pool.value(indicatorType);
            m_pool.remove(indicatorType);
        }
    }

    /* Acquire status-bar order: */
    m_order = gEDataManager->statusBarIndicatorOrder(uiCommon().managedVMUuid());
    /* Make sure the order is complete taking restrictions into account: */
    for (int iType = IndicatorType_Invalid; iType < IndicatorType_Max; ++iType)
    {
        /* Get iterated type: */
        IndicatorType type = (IndicatorType)iType;
        /* Skip invalid type: */
        if (type == IndicatorType_Invalid)
            continue;
        /* Take restriction/presence into account: */
        bool fRestricted = m_restrictions.contains(type);
        bool fPresent = m_order.contains(type);
        if (fRestricted && fPresent)
            m_order.removeAll(type);
        else if (!fRestricted && !fPresent)
            m_order << type;
    }

    /* Add/Update allowed indicators: */
    foreach (const IndicatorType &indicatorType, m_order)
    {
        /* Indicator exists: */
        if (m_pool.contains(indicatorType))
        {
            /* Get indicator: */
            QIStatusBarIndicator *pIndicator = m_pool.value(indicatorType);
            /* Make sure it have valid position: */
            const int iWantedIndex = indicatorPosition(indicatorType);
            const int iActualIndex = m_pMainLayout->indexOf(pIndicator);
            if (iActualIndex != iWantedIndex)
            {
                /* Re-inject indicator into main-layout at proper position: */
                m_pMainLayout->removeWidget(pIndicator);
                m_pMainLayout->insertWidget(iWantedIndex, pIndicator);
            }
        }
        /* Indicator missed: */
        else
        {
            /* Create indicator: */
            switch (indicatorType)
            {
                case IndicatorType_HardDisks:         m_pool[indicatorType] = new UIIndicatorHardDrive(m_pMachine);     break;
                case IndicatorType_OpticalDisks:      m_pool[indicatorType] = new UIIndicatorOpticalDisks(m_pMachine);  break;
                case IndicatorType_FloppyDisks:       m_pool[indicatorType] = new UIIndicatorFloppyDisks(m_pMachine);   break;
                case IndicatorType_Audio:             m_pool[indicatorType] = new UIIndicatorAudio(m_pMachine);         break;
                case IndicatorType_Network:           m_pool[indicatorType] = new UIIndicatorNetwork(m_pMachine);       break;
                case IndicatorType_USB:               m_pool[indicatorType] = new UIIndicatorUSB(m_pMachine);           break;
                case IndicatorType_SharedFolders:     m_pool[indicatorType] = new UIIndicatorSharedFolders(m_pMachine); break;
                case IndicatorType_Display:           m_pool[indicatorType] = new UIIndicatorDisplay(m_pMachine);       break;
                case IndicatorType_Recording:         m_pool[indicatorType] = new UIIndicatorRecording(m_pMachine);     break;
                case IndicatorType_Features:          m_pool[indicatorType] = new UIIndicatorFeatures(m_pMachine);      break;
                case IndicatorType_Mouse:             m_pool[indicatorType] = new UIIndicatorMouse(m_pMachine);         break;
                case IndicatorType_Keyboard:          m_pool[indicatorType] = new UIIndicatorKeyboard(m_pMachine);      break;
                case IndicatorType_KeyboardExtension: m_pool[indicatorType] = new UIIndicatorKeyboardExtension;         break;
                default: break;
            }
            /* Configure indicator: */
            connect(m_pool.value(indicatorType), &QIStatusBarIndicator::sigContextMenuRequest,
                    this, &UIIndicatorsPool::sltContextMenuRequest);
            /* Insert indicator into main-layout at proper position: */
            m_pMainLayout->insertWidget(indicatorPosition(indicatorType), m_pool.value(indicatorType));
        }
    }
}

void UIIndicatorsPool::cleanupUpdateTimer()
{
    /* Destroy auto-update timer: */
    AssertPtrReturnVoid(m_pTimerAutoUpdate);
    {
        m_pTimerAutoUpdate->stop();
        delete m_pTimerAutoUpdate;
        m_pTimerAutoUpdate = 0;
    }
}

void UIIndicatorsPool::cleanupContents()
{
    /* Cleanup indicators: */
    while (!m_pool.isEmpty())
    {
        const IndicatorType firstType = m_pool.keys().first();
        delete m_pool.value(firstType);
        m_pool.remove(firstType);
    }
}

void UIIndicatorsPool::cleanup()
{
    /* Cleanup auto-update timer: */
    cleanupUpdateTimer();
    /* Cleanup indicators: */
    cleanupContents();
}

void UIIndicatorsPool::contextMenuEvent(QContextMenuEvent *pEvent)
{
    /* Do not pass-through context menu events,
     * otherwise they will raise the underlying status-bar context-menu. */
    pEvent->accept();
}

int UIIndicatorsPool::indicatorPosition(IndicatorType indicatorType) const
{
    int iPosition = 0;
    foreach (const IndicatorType &iteratedIndicatorType, m_order)
        if (iteratedIndicatorType == indicatorType)
            return iPosition;
        else
            ++iPosition;
    return iPosition;
}

void UIIndicatorsPool::updateIndicatorStateForDevice(QIStatusBarIndicator *pIndicator, KDeviceActivity enmState)
{
    /* Assert indicators with NO state: */
    QIStateStatusBarIndicator *pStateIndicator = qobject_cast<QIStateStatusBarIndicator*>(pIndicator);
    AssertPtrReturnVoid(pStateIndicator);

    /* Skip indicators with NULL state: */
    if (pStateIndicator->state() == KDeviceActivity_Null)
        return;

    /* Paused VM have all indicator states set to IDLE: */
    if (m_pMachine->isPaused())
    {
        /* If current state differs from IDLE => set the IDLE one:  */
        if (pStateIndicator->state() != KDeviceActivity_Idle)
            pStateIndicator->setState(KDeviceActivity_Idle);
    }
    else
    {
        /* If current state differs from actual => set the actual one: */
        if (pStateIndicator->state() != enmState)
            pStateIndicator->setState(enmState);
    }
}

#include "UIIndicatorsPool.moc"
