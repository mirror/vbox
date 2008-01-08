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

#ifndef wait_event_interruptible_timeout

/* We need to define these ones here as they only exist in kernels 2.6 and up */

#define __wait_event_interruptible_timeout(wq, condition, timeout, ret)   \
do {                                                                      \
        int __ret = 0;                                                    \
        if (!(condition)) {                                               \
          wait_queue_t __wait;                                            \
          unsigned long expire;                                           \
          init_waitqueue_entry(&__wait, current);                         \
                                                                          \
          expire = timeout + jiffies;                                     \
          add_wait_queue(&wq, &__wait);                                   \
          for (;;) {                                                      \
                  set_current_state(TASK_INTERRUPTIBLE);                  \
                  if (condition)                                          \
                          break;                                          \
                  if (jiffies > expire) {                                 \
                          ret = jiffies - expire;                         \
                          break;                                          \
                  }                                                       \
                  if (!signal_pending(current)) {                         \
                          schedule_timeout(timeout);                      \
                          continue;                                       \
                  }                                                       \
                  ret = -ERESTARTSYS;                                     \
                  break;                                                  \
          }                                                               \
          current->state = TASK_RUNNING;                                  \
          remove_wait_queue(&wq, &__wait);                                \
        }                                                                 \
} while (0)

/*
   retval == 0; condition met; we're good.
   retval < 0; interrupted by signal.
   retval > 0; timed out.
*/
#define wait_event_interruptible_timeout(wq, condition, timeout)        \
({                                                                      \
        int __ret = 0;                                                  \
        if (!(condition))                                               \
                __wait_event_interruptible_timeout(wq, condition,       \
                                                timeout, __ret);        \
        __ret;                                                          \
})

#endif  /* wait_event_interruptible_timeout not defined */
