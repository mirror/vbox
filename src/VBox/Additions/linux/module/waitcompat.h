/* copied from linux/wait.h */

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

