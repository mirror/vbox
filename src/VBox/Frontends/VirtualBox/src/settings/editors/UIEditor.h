/* $Id$ */
/** @file
 * VBox Qt GUI - UIEditor class declaration.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/** QWidget sub-class used as editor interface. */
class SHARED_LIBRARY_STUFF UIEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIEditor(QWidget *pParent = 0);

protected:

    /** Holds the list of sub-editors. */
    QList<UIEditor*> m_editors;
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIEditor_h */
