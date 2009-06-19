/** @file
 * Statistics declaration shared between slirp and DrvNAT.
 */

#ifndef ___slirp_statistics_h
#define ___slirp_statistics_h

#define COUNTING_COUNTER(name, desc)    extern DECLHIDDEN(uint32_t) g_offSlirpStat##name
#define PROFILE_COUNTER(name, desc)     extern DECLHIDDEN(uint32_t) g_offSlirpStat##name

RT_C_DECLS_BEGIN
#include "counters.h"
RT_C_DECLS_END

#undef COUNTING_COUNTER
#undef PROFILE_COUNTER

#endif

