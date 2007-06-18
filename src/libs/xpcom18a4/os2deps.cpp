/* The usual story: drag stuff from the libraries into the link. */


#include <plstr.h>
#include <prio.h>
#include <nsDeque.h>
#include <nsHashSets.h>
#include <nsIPipe.h>

uintptr_t deps[] =
{
    (uintptr_t)PL_strncpy,
    (uintptr_t)PL_strchr,
    (uintptr_t)PL_HashString,
    (uintptr_t)PR_DestroyPollableEvent,
    (uintptr_t)NS_NewPipe2,
    0
};

void foodep(void)
{
    nsVoidHashSetSuper *a = new nsVoidHashSetSuper();
    a->Init(123);
    nsDeque *b = new nsDeque((nsDequeFunctor*)0);
}

