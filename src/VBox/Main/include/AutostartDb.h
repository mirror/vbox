/** @file
 * Main - Autostart database Interfaces.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___autostart_h
#define ___autostart_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/critsect.h>

class AutostartDb
{
    public:

        AutostartDb();
        ~AutostartDb();

        /**
         * Add a autostart VM to the global database.
         *
         * @returns VBox status code.
         * @param   pszVMId    ID of the VM to add.
         */
        int addAutostartVM(const char *pszVMId);

        /**
         * Remove a autostart VM from the global database.
         *
         * @returns VBox status code.
         * @param   pszVMId    ID of the VM to remove.
         */
        int removeAutostartVM(const char *pszVMId);

        /**
         * Add a autostop VM to the global database.
         *
         * @returns VBox status code.
         * @param   pszVMId    ID of the VM to add.
         */
        int addAutostopVM(const char *pszVMId);

        /**
         * Remove a autostop VM from the global database.
         *
         * @returns VBox status code.
         * @param   pszVMId    ID of the VM to remove.
         */
        int removeAutostopVM(const char *pszVMId);

    private:

#ifdef RT_OS_LINUX
        /** Critical section protecting the database against concurrent access. */
        RTCRITSECT      CritSect;
#endif
};

#endif  /* !___autostart_h */

