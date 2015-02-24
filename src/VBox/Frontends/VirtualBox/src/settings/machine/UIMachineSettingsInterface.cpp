/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsInterface class implementation.
 */

/*
 * Copyright (C) 2008-2015 Oracle Corporation
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

/* GUI includes: */
# include "UIMachineSettingsInterface.h"
# include "UIExtraDataManager.h"
# include "UIActionPool.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIMachineSettingsInterface::UIMachineSettingsInterface(const QString strMachineID)
    : m_strMachineID(strMachineID)
    , m_pActionPool(0)
{
    /* Prepare: */
    prepare();
}

UIMachineSettingsInterface::~UIMachineSettingsInterface()
{
    /* Cleanup: */
    cleanup();
}

/* Load data to cache from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsInterface::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_cache.clear();

    /* Prepare interface data: */
    UIDataSettingsMachineInterface interfaceData;

    /* Cache interface data: */
    interfaceData.m_dScaleFactor = gEDataManager->scaleFactor(m_machine.GetId());
#ifdef Q_WS_MAC
    interfaceData.m_fUseUnscaledHiDPIOutput = gEDataManager->useUnscaledHiDPIOutput(m_machine.GetId());
#else /* !Q_WS_MAC */
    interfaceData.m_fShowMiniToolBar = gEDataManager->miniToolbarEnabled(m_machine.GetId());
    interfaceData.m_fMiniToolBarAtTop = gEDataManager->miniToolbarAlignment(m_machine.GetId()) == Qt::AlignTop;
#endif /* !Q_WS_MAC */

    /* Cache interface data: */
    m_cache.cacheInitialData(interfaceData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsInterface::getFromCache()
{
    /* Get interface data from cache: */
    const UIDataSettingsMachineInterface &interfaceData = m_cache.base();

    /* Prepare interface data: */
    m_pEditorGuestScreenScale->setValue(interfaceData.m_dScaleFactor * 100);
#ifdef Q_WS_MAC
    m_pCheckBoxUnscaledHiDPIOutput->setChecked(interfaceData.m_fUseUnscaledHiDPIOutput);
#else /* !Q_WS_MAC */
    m_pCheckBoxShowMiniToolBar->setChecked(interfaceData.m_fShowMiniToolBar);
    m_pComboToolBarAlignment->setChecked(interfaceData.m_fMiniToolBarAtTop);
#endif /* !Q_WS_MAC */

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsInterface::putToCache()
{
    /* Prepare interface data: */
    UIDataSettingsMachineInterface interfaceData = m_cache.base();

    /* Gather interface data from page: */
    interfaceData.m_dScaleFactor = (double)m_pEditorGuestScreenScale->value() / 100;
#ifdef Q_WS_MAC
    interfaceData.m_fUseUnscaledHiDPIOutput = m_pCheckBoxUnscaledHiDPIOutput->isChecked();
#else /* !Q_WS_MAC */
    interfaceData.m_fShowMiniToolBar = m_pCheckBoxShowMiniToolBar->isChecked();
    interfaceData.m_fMiniToolBarAtTop = m_pComboToolBarAlignment->isChecked();
#endif /* !Q_WS_MAC */

    /* Cache interface data: */
    m_cache.cacheCurrentData(interfaceData);
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsInterface::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Make sure machine is in valid mode & interface data was changed: */
    if (isMachineInValidMode() && m_cache.wasChanged())
    {
        /* Get interface data from cache: */
        const UIDataSettingsMachineInterface &interfaceData = m_cache.data();

        /* Store interface data: */
        if (isMachineInValidMode())
        {
            gEDataManager->setScaleFactor(interfaceData.m_dScaleFactor, m_machine.GetId());
#ifdef Q_WS_MAC
            gEDataManager->setUseUnscaledHiDPIOutput(interfaceData.m_fUseUnscaledHiDPIOutput, m_machine.GetId());
#else /* !Q_WS_MAC */
            gEDataManager->setMiniToolbarEnabled(interfaceData.m_fShowMiniToolBar, m_machine.GetId());
            gEDataManager->setMiniToolbarAlignment(interfaceData.m_fMiniToolBarAtTop ? Qt::AlignTop : Qt::AlignBottom, m_machine.GetId());
#endif /* !Q_WS_MAC */
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsInterface::setOrderAfter(QWidget *pWidget)
{
    /* Tab-order: */
    setTabOrder(pWidget, m_pSliderGuestScreenScale);
    setTabOrder(m_pSliderGuestScreenScale, m_pEditorGuestScreenScale);
    setTabOrder(m_pEditorGuestScreenScale, m_pCheckBoxUnscaledHiDPIOutput);
    setTabOrder(m_pCheckBoxUnscaledHiDPIOutput, m_pCheckBoxShowMiniToolBar);
    setTabOrder(m_pCheckBoxShowMiniToolBar, m_pComboToolBarAlignment);
}

void UIMachineSettingsInterface::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsInterface::retranslateUi(this);
}

void UIMachineSettingsInterface::polishPage()
{
    /* Polish interface availability: */
    m_pMenuBarEditor->setEnabled(isMachineInValidMode());
    m_pLabelGuestScreenScale->setEnabled(isMachineInValidMode());
    m_pSliderGuestScreenScale->setEnabled(isMachineInValidMode());
    m_pLabelGuestScreenScaleMin->setEnabled(isMachineInValidMode());
    m_pLabelGuestScreenScaleMax->setEnabled(isMachineInValidMode());
    m_pEditorGuestScreenScale->setEnabled(isMachineInValidMode());
#ifdef Q_WS_MAC
    m_pLabelHiDPI->setEnabled(isMachineInValidMode());
    m_pCheckBoxUnscaledHiDPIOutput->setEnabled(isMachineInValidMode());
    m_pLabelMiniToolBar->hide();
    m_pCheckBoxShowMiniToolBar->hide();
    m_pComboToolBarAlignment->hide();
#else /* !Q_WS_MAC */
    m_pLabelHiDPI->hide();
    m_pCheckBoxUnscaledHiDPIOutput->hide();
    m_pLabelMiniToolBar->setEnabled(isMachineInValidMode());
    m_pCheckBoxShowMiniToolBar->setEnabled(isMachineInValidMode());
    m_pComboToolBarAlignment->setEnabled(isMachineInValidMode() && m_pCheckBoxShowMiniToolBar->isChecked());
#endif /* !Q_WS_MAC */
    m_pStatusBarEditor->setEnabled(isMachineInValidMode());
}

void UIMachineSettingsInterface::sltHandleGuestScreenScaleSliderChange()
{
    /* Apply proposed scale-factor: */
    m_pEditorGuestScreenScale->blockSignals(true);
    m_pEditorGuestScreenScale->setValue(m_pSliderGuestScreenScale->value());
    m_pEditorGuestScreenScale->blockSignals(false);
}

void UIMachineSettingsInterface::sltHandleGuestScreenScaleEditorChange()
{
    /* Apply proposed scale-factor: */
    m_pSliderGuestScreenScale->blockSignals(true);
    m_pSliderGuestScreenScale->setValue(m_pEditorGuestScreenScale->value());
    m_pSliderGuestScreenScale->blockSignals(false);
}

void UIMachineSettingsInterface::prepare()
{
    /* Apply UI decorations: */
    Ui::UIMachineSettingsInterface::setupUi(this);

    /* Create personal action-pool: */
    m_pActionPool = UIActionPool::create(UIActionPoolType_Runtime);
    m_pMenuBarEditor->setActionPool(m_pActionPool);

    /* Assign corresponding machine ID: */
    m_pMenuBarEditor->setMachineID(m_strMachineID);
    m_pStatusBarEditor->setMachineID(m_strMachineID);

    /* Prepare scale-factor slider: */
    m_pSliderGuestScreenScale->setMinimum(100);
    m_pSliderGuestScreenScale->setMaximum(200);
    m_pSliderGuestScreenScale->setPageStep(10);
    m_pSliderGuestScreenScale->setSingleStep(1);
    m_pSliderGuestScreenScale->setTickInterval(10);
    m_pSliderGuestScreenScale->setSnappingEnabled(true);
    connect(m_pSliderGuestScreenScale, SIGNAL(valueChanged(int)), this, SLOT(sltHandleGuestScreenScaleSliderChange()));

    /* Prepare scale-factor editor: */
    m_pEditorGuestScreenScale->setMinimum(100);
    m_pEditorGuestScreenScale->setMaximum(200);
    vboxGlobal().setMinimumWidthAccordingSymbolCount(m_pEditorGuestScreenScale, 5);
    connect(m_pEditorGuestScreenScale, SIGNAL(valueChanged(int)), this, SLOT(sltHandleGuestScreenScaleEditorChange()));

    /* Translate finally: */
    retranslateUi();
}

void UIMachineSettingsInterface::cleanup()
{
    /* Destroy personal action-pool: */
    UIActionPool::destroy(m_pActionPool);
}

