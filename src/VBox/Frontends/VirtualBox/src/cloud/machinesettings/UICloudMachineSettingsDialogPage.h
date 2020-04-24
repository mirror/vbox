/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudMachineSettingsDialogPage class declaration.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_cloud_machinesettings_UICloudMachineSettingsDialogPage_h
#define FEQT_INCLUDED_SRC_cloud_machinesettings_UICloudMachineSettingsDialogPage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QPointer>
#include <QWidget>

/* GUI includes: */
#include "UIFormEditorWidget.h"

/* Forward declarations: */
class CForm;

/** Cloud machine settings dialog page. */
class UICloudMachineSettingsDialogPage : public QWidget
{
    Q_OBJECT;

public:

    /** Constructs cloud machine settings dialog page passing @a pParent to the base-class. */
    UICloudMachineSettingsDialogPage(QWidget *pParent);

    /** Defines page @a comForm. */
    void setForm(const CForm &comForm);

    /** Makes sure page data committed. */
    void makeSureDataCommitted();

private:

    /** Prepares all. */
    void prepare();

    /** Holds the form editor widget instance. */
    UIFormEditorWidgetPointer  m_pFormEditor;
};

/** Safe pointer to Form Editor widget. */
typedef QPointer<UICloudMachineSettingsDialogPage> UISafePointerCloudMachineSettingsDialogPage;

#endif /* !FEQT_INCLUDED_SRC_cloud_machinesettings_UICloudMachineSettingsDialogPage_h */
