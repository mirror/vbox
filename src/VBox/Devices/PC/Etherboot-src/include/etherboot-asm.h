#ifndef ETHERBOOT_ASM_H
#define ETHERBOOT_ASM_H


/* Macro to add a leading underscore to symbols. This is necessary e.g. for
 * building Etherboot under Windows with mingw. */

#ifdef __WIN32__
#define GSYM(x) _##x
#else /* !__WIN32__ */
#define GSYM(x) x
#endif /* !__WIN32__ */


#ifdef __WIN32__
#define HACK_ADDR32 addr32
#define HACK_DATA32 data32
#else /* !__WIN32__ */
#define HACK_ADDR32
#define HACK_DATA32
#endif /* !__WIN32__ */


#endif /* ETHERBOOT_ASM_H */
