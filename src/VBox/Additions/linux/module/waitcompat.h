/** @file
 *
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


#ifndef wait_event_timeout
#define wait_event_timeout(wq, condition, timeout)              \
({                                                              \
        long __ret = timeout;                                   \
        if (!(condition))                                       \
                __wait_event_timeout(wq, condition, __ret);     \
        __ret;                                                  \
 })

#define __wait_event_timeout(wq, condition, ret)                \
do {                                                            \
        wait_queue_t __wait;                                    \
        init_waitqueue_entry(&__wait, current);                 \
                                                                \
        add_wait_queue(&wq, &__wait);                           \
        for (;;) {                                              \
                set_current_state(TASK_UNINTERRUPTIBLE);        \
                if (condition)                                  \
                        break;                                  \
                ret = schedule_timeout(ret);                    \
                if (!ret)                                       \
                        break;                                  \
        }                                                       \
        current->state = TASK_RUNNING;                          \
        remove_wait_queue(&wq, &__wait);                        \
} while (0)
#endif
