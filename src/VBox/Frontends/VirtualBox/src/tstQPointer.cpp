/* $Id$ */
/** @file
 * VBox Qt GUI - Testcase for QPointer delete behaviour.
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

/* Qt includes: */
#include <QApplication>
#include <QMessageBox>
#include <QPointer>

/* Other VBox includes: */
#include <iprt/stream.h>
#include <iprt/initterm.h>


int main(int argc, char **argv)
{
    RTR3InitExe(argc, &argv, 0);

    QApplication App(argc, argv);

    /* Pattern: */
    {
        QPointer<QObject> ptrObj = new QObject();
        delete ptrObj;
    }

    return 0;
}

