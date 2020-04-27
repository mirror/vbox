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

/* COM includes: */
#include "COMEnums.h"
#include "CForm.h"

/** Cloud machine settings dialog page. */
class UICloudMachineSettingsDialogPage : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies listeners about value has became @a fValid. */
    void sigValidChanged(bool fValid);

public:

    /** Constructs cloud machine settings dialog page passing @a pParent to the base-class. */
    UICloudMachineSettingsDialogPage(QWidget *pParent);

    /** Returns page form. */
    CForm form() const { return m_comForm; }
    /** Returns page filter. */
    QString filter() const { return m_strFilter; }

public slots:

    /** Defines page @a comForm. */
    void setForm(const CForm &comForm);
    /** Defines page @a strFilter. */
    void setFilter(const QString &strFilter);

    /** Makes sure page data committed. */
    void makeSureDataCommitted();

private:

    /** Prepares all. */
    void prepare();

    /** Updates editor. */
    void updateEditor();

    /** Holds the form editor widget instance. */
    UIFormEditorWidgetPointer  m_pFormEditor;

    /** Holds the page form. */
    CForm    m_comForm;
    /** Holds the page filter. */
    QString  m_strFilter;
};

/** Safe pointer to Form Editor widget. */
typedef QPointer<UICloudMachineSettingsDialogPage> UISafePointerCloudMachineSettingsDialogPage;

#endif /* !FEQT_INCLUDED_SRC_cloud_machinesettings_UICloudMachineSettingsDialogPage_h */
