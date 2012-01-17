#
# VBox GUI: additional Qt project file (for Qt Designer).
#
# NOTE: This file is intended to be opened by Qt Designer
#       as a project file (to work with .ui files)
#

#
# Copyright (C) 2006-2011 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

TEMPLATE	= app
LANGUAGE	= C++

FORMS = \
    src/VBoxVMInformationDlg.ui \
    src/VBoxMediaManagerDlg.ui \
    src/VBoxSnapshotDetailsDlg.ui \
    src/VBoxTakeSnapshotDlg.ui \
    src/UIVMLogViewer.ui \
    src/settings/UISettingsDialog.ui \
    src/settings/global/UIGlobalSettingsGeneral.ui \
    src/settings/global/UIGlobalSettingsInput.ui \
    src/settings/global/UIGlobalSettingsUpdate.ui \
    src/settings/global/UIGlobalSettingsLanguage.ui \
    src/settings/global/UIGlobalSettingsDisplay.ui \
    src/settings/global/UIGlobalSettingsNetwork.ui \
    src/settings/global/UIGlobalSettingsNetworkDetails.ui \
    src/settings/global/UIGlobalSettingsExtension.ui \
    src/settings/global/UIGlobalSettingsProxy.ui \
    src/settings/machine/UIMachineSettingsGeneral.ui \
    src/settings/machine/UIMachineSettingsSystem.ui \
    src/settings/machine/UIMachineSettingsDisplay.ui \
    src/settings/machine/UIMachineSettingsStorage.ui \
    src/settings/machine/UIMachineSettingsAudio.ui \
    src/settings/machine/UIMachineSettingsNetwork.ui \
    src/settings/machine/UIMachineSettingsSerial.ui \
    src/settings/machine/UIMachineSettingsParallel.ui \
    src/settings/machine/UIMachineSettingsUSB.ui \
    src/settings/machine/UIMachineSettingsUSBFilterDetails.ui \
    src/settings/machine/UIMachineSettingsSF.ui \
    src/settings/machine/UIMachineSettingsSFDetails.ui \
    src/wizards/clonevm/UICloneVMWizardPage1.ui \
    src/wizards/clonevm/UICloneVMWizardPage2.ui \
    src/wizards/clonevm/UICloneVMWizardPage3.ui \
    src/wizards/newvm/UINewVMWzdPage1.ui \
    src/wizards/newvm/UINewVMWzdPage2.ui \
    src/wizards/newvm/UINewVMWzdPage3.ui \
    src/wizards/newvm/UINewVMWzdPage4.ui \
    src/wizards/newvm/UINewVMWzdPage5.ui \
    src/wizards/newhd/UINewHDWizardPageWelcome.ui \
    src/wizards/newhd/UINewHDWizardPageFormat.ui \
    src/wizards/newhd/UINewHDWizardPageVariant.ui \
    src/wizards/newhd/UINewHDWizardPageOptions.ui \
    src/wizards/newhd/UINewHDWizardPageSummary.ui \
    src/wizards/firstrun/UIFirstRunWzdPage1.ui \
    src/wizards/firstrun/UIFirstRunWzdPage2.ui \
    src/wizards/firstrun/UIFirstRunWzdPage3.ui \
    src/wizards/exportappliance/UIExportApplianceWzdPage1.ui \
    src/wizards/exportappliance/UIExportApplianceWzdPage2.ui \
    src/wizards/exportappliance/UIExportApplianceWzdPage3.ui \
    src/wizards/exportappliance/UIExportApplianceWzdPage4.ui \
    src/wizards/importappliance/UIImportApplianceWzdPage1.ui \
    src/wizards/importappliance/UIImportApplianceWzdPage2.ui \
    src/widgets/UIApplianceEditorWidget.ui \
    src/selector/VBoxSnapshotsWgt.ui \
    src/runtime/UIVMCloseDialog.ui

TRANSLATIONS = \
	nls/VirtualBox_ar.ts \
	nls/VirtualBox_bg.ts \
	nls/VirtualBox_ca.ts \
	nls/VirtualBox_ca_VA.ts \
	nls/VirtualBox_cs.ts \
	nls/VirtualBox_da.ts \
	nls/VirtualBox_de.ts \
	nls/VirtualBox_el.ts \
	nls/VirtualBox_en.ts \
	nls/VirtualBox_es.ts \
	nls/VirtualBox_eu.ts \
	nls/VirtualBox_fi.ts \
	nls/VirtualBox_fr.ts \
	nls/VirtualBox_gl_ES.ts \
	nls/VirtualBox_hu.ts \
	nls/VirtualBox_id.ts \
	nls/VirtualBox_it.ts \
	nls/VirtualBox_ja.ts \
	nls/VirtualBox_km_KH.ts \
	nls/VirtualBox_ko.ts \
	nls/VirtualBox_lt.ts \
	nls/VirtualBox_nl.ts \
	nls/VirtualBox_pl.ts \
	nls/VirtualBox_pt.ts \
	nls/VirtualBox_pt_BR.ts \
	nls/VirtualBox_ro.ts \
	nls/VirtualBox_ru.ts \
	nls/VirtualBox_sk.ts \
	nls/VirtualBox_sr.ts \
	nls/VirtualBox_sv.ts \
	nls/VirtualBox_tr.ts \
	nls/VirtualBox_uk.ts \
	nls/VirtualBox_zh_CN.ts \
	nls/VirtualBox_zh_TW.ts

