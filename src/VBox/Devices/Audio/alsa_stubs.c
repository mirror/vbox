#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/ldr.h>
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/log.h>
#include <VBox/err.h>

#include <alsa/asoundlib.h>

#include "alsa_stubs.h"

#define VBOX_ALSA_LIB "libasound.so.2"

static RTSEMMUTEX mutexAsound;
static enum { NO = 0, YES, FAIL } isLibLoaded = NO;
static RTLDRMOD hLibAsound = NULL;

/**
 * Try to dynamically load the ALSA libraries.  This function is not
 * thread-safe, and should be called before attempting to use any
 * of the ALSA functions.
 *
 * @returns iprt status code
 */
int audioLoadAlsaLib(void)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("\n"));
    /* If this is not NO then the function has obviously been called twice,
       which is likely to be a bug. */
    if (NO != isLibLoaded)
    {
        AssertMsgFailed(("isLibLoaded == %s\n", YES == isLibLoaded ? "YES" : "NO"));
        return YES == isLibLoaded ? VINF_SUCCESS : VERR_NOT_SUPPORTED;
    }
    rc = RTSemMutexCreate(&mutexAsound);
    if (RT_FAILURE(rc))
    {
        LogFunc(("Failed to create mutex.\n"));
        isLibLoaded = FAIL;
    }
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrLoad(VBOX_ALSA_LIB, &hLibAsound);
        if (RT_FAILURE(rc))
        {
            LogFunc(("Failed to load library %s\n", VBOX_ALSA_LIB));
            isLibLoaded = FAIL;
        }
    }
    if (RT_SUCCESS(rc))
    {
        isLibLoaded = YES;
    }
    LogFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

#define PROXY_STUB(function, rettype, signature, shortsig, retval) \
extern rettype function signature; \
rettype function signature \
{ \
    static int isInitialised = 0; \
    static rettype (*pFunc) signature = NULL; \
    int rc; \
\
    if (0 != isInitialised) \
    { \
        return pFunc shortsig; \
    } \
    AssertReturn(YES == isLibLoaded, retval); \
    rc = RTSemMutexRequest(mutexAsound, RT_INDEFINITE_WAIT); \
    if (RT_SUCCESS(rc)) \
    { \
        rc = RTLdrGetSymbol(hLibAsound, #function, (void **)&pFunc); \
    } \
    if (RT_SUCCESS(rc)) \
    { \
        return pFunc shortsig; \
    } \
    LogFunc(("stub call failed, rc=%Rrc\n", rc)); \
    return retval; \
} \


PROXY_STUB(snd_pcm_hw_params_any, int,
           (snd_pcm_t *pcm, snd_pcm_hw_params_t *params),
           (pcm, params), -1)
PROXY_STUB(snd_pcm_close, int, (snd_pcm_t *pcm), (pcm), -1)
PROXY_STUB(snd_pcm_avail_update, snd_pcm_sframes_t, (snd_pcm_t *pcm),
           (pcm), -1)
PROXY_STUB(snd_pcm_hw_params_set_channels_near, int,
           (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val),
           (pcm, params, val), -1)
PROXY_STUB(snd_pcm_hw_params_set_period_time_near, int,
           (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir),
           (pcm, params, val, dir), -1)
PROXY_STUB(snd_pcm_prepare, int, (snd_pcm_t *pcm), (pcm), -1)
PROXY_STUB(snd_pcm_sw_params_sizeof, size_t, (void), (), ~0)
PROXY_STUB(snd_pcm_hw_params_set_period_size_near, int,
           (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir),
           (pcm, params, val, dir), -1)
PROXY_STUB(snd_pcm_hw_params_get_period_size, int,
           (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir),
           (params, frames, dir), -1)
PROXY_STUB(snd_pcm_hw_params, int,
           (snd_pcm_t *pcm, snd_pcm_hw_params_t *params),
           (pcm, params), -1)
PROXY_STUB(snd_pcm_hw_params_sizeof, size_t, (void), (), ~0)
PROXY_STUB(snd_pcm_state, snd_pcm_state_t, (snd_pcm_t *pcm), (pcm), ~0)
PROXY_STUB(snd_pcm_open, int,
           (snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode),
           (pcm, name, stream, mode), -1)
PROXY_STUB(snd_lib_error_set_handler, int, (snd_lib_error_handler_t handler),
           (handler), -1)
PROXY_STUB(snd_pcm_sw_params, int,
           (snd_pcm_t *pcm, snd_pcm_sw_params_t *params),
           (pcm, params), -1)
PROXY_STUB(snd_pcm_hw_params_get_period_size_min, int,
           (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir),
           (params, frames, dir), -1)
PROXY_STUB(snd_pcm_writei, snd_pcm_sframes_t,
           (snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size),
           (pcm, buffer, size), -1)
PROXY_STUB(snd_pcm_readi, snd_pcm_sframes_t,
           (snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size),
           (pcm, buffer, size), -1)
PROXY_STUB(snd_strerror, const char *, (int errnum), (errnum), NULL)
PROXY_STUB(snd_pcm_drop, int, (snd_pcm_t *pcm), (pcm), -1)
PROXY_STUB(snd_pcm_hw_params_get_buffer_size, int,
           (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val),
           (params, val), -1)
PROXY_STUB(snd_pcm_hw_params_set_rate_near, int,
           (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir),
           (pcm, params, val, dir), -1)
PROXY_STUB(snd_pcm_hw_params_set_access, int,
           (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t _access),
           (pcm, params, _access), -1)
PROXY_STUB(snd_pcm_hw_params_set_buffer_time_near, int,
           (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir),
           (pcm, params, val, dir), -1)
PROXY_STUB(snd_pcm_hw_params_set_buffer_size_near, int,
           (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val),
           (pcm, params, val), -1)
PROXY_STUB(snd_pcm_hw_params_get_buffer_size_min, int,
           (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val),
           (params, val), -1)
PROXY_STUB(snd_pcm_hw_params_set_format, int,
           (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val),
           (pcm, params, val), -1)
PROXY_STUB(snd_pcm_sw_params_current, int,
           (snd_pcm_t *pcm, snd_pcm_sw_params_t *params),
           (pcm, params), -1)
PROXY_STUB(snd_pcm_sw_params_set_start_threshold, int,
           (snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val),
           (pcm, params, val), -1)
