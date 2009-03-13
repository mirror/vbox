#ifndef VBOX_ARCH_SYS_ARCH_H_
#define VBOX_ARCH_SYS_ARCH_H_

#include <iprt/semaphore.h>
#include <iprt/thread.h>
#ifdef RT_OS_DARWIN
#include <sys/time.h>
#endif

/** NULL value for a mbox. */
#define SYS_MBOX_NULL NULL

/** NULL value for a mutex semaphore. */
#define SYS_SEM_NULL NIL_RTSEMEVENT

/** The IPRT event semaphore ID just works fine for this type. */
typedef RTSEMEVENT sys_sem_t;

/** Forward declaration of the actual mbox type. */
struct sys_mbox;

/** The opaque type of a mbox. */
typedef struct sys_mbox *sys_mbox_t;

/** The IPRT thread ID just works fine for this type. */
typedef RTTHREAD sys_thread_t;

#if SYS_LIGHTWEIGHT_PROT
/** This is just a dummy. The implementation doesn't need anything. */
typedef void *sys_prot_t;
#endif

#endif /* !VBOX_ARCH_SYS_ARCH_H_ */
