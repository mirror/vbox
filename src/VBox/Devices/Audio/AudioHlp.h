/* $Id$ */
/** @file
 * Audio helper routines.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Audio_AudioHlp_h
#define VBOX_INCLUDED_SRC_Audio_AudioHlp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <limits.h>

#include <iprt/circbuf.h>
#include <iprt/critsect.h>
#include <iprt/file.h>
#include <iprt/path.h>

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pdmaudioifs.h>

/** @name Audio calculation helper methods.
 * @{ */
uint32_t AudioHlpCalcBitrate(uint8_t cBits, uint32_t uHz, uint8_t cChannels);
/** @} */

/** @name Audio PCM properties helper methods.
 * @{ */
bool     AudioHlpPcmPropsAreValid(PCPDMAUDIOPCMPROPS pProps);
/** @}  */

/** @name Audio configuration helper methods.
 * @{ */
bool    AudioHlpStreamCfgIsValid(PCPDMAUDIOSTREAMCFG pCfg);
/** @}  */

/** @name Audio file (name) helper methods.
 * @{ */
int     AudioHlpFileNameSanitize(char *pszPath, size_t cbPath);
int     AudioHlpFileNameGet(char *pszFile, size_t cchFile, const char *pszPath, const char *pszName,
                            uint32_t uInstance, PDMAUDIOFILETYPE enmType, uint32_t fFlags);
/** @}  */

/** @name Audio string-ify methods.
 * @{ */
PDMAUDIOFMT AudioHlpStrToAudFmt(const char *pszFmt);
/** @}  */

/** @name Audio file methods.
 * @{ */
int     AudioHlpFileCreateAndOpen(PPDMAUDIOFILE *ppFile, const char *pszDir, const char *pszName,
                                  uint32_t iInstance, PCPDMAUDIOPCMPROPS pProps);
int     AudioHlpFileCreateAndOpenEx(PPDMAUDIOFILE *ppFile, PDMAUDIOFILETYPE enmType, const char *pszDir, const char *pszName,
                                    uint32_t iInstance, uint32_t fFilename, uint32_t fCreate,
                                    PCPDMAUDIOPCMPROPS pProps, uint64_t fOpen);
int     AudioHlpFileCreate(PDMAUDIOFILETYPE enmType, const char *pszFile, uint32_t fFlags, PPDMAUDIOFILE *ppFile);
void    AudioHlpFileDestroy(PPDMAUDIOFILE pFile);
int     AudioHlpFileOpen(PPDMAUDIOFILE pFile, uint32_t fOpen, PCPDMAUDIOPCMPROPS pProps);
int     AudioHlpFileClose(PPDMAUDIOFILE pFile);
int     AudioHlpFileDelete(PPDMAUDIOFILE pFile);
size_t  AudioHlpFileGetDataSize(PPDMAUDIOFILE pFile);
bool    AudioHlpFileIsOpen(PPDMAUDIOFILE pFile);
int     AudioHlpFileWrite(PPDMAUDIOFILE pFile, const void *pvBuf, size_t cbBuf, uint32_t fFlags);
/** @}  */

#define AUDIO_MAKE_FOURCC(c0, c1, c2, c3) RT_H2LE_U32_C(RT_MAKE_U32_FROM_U8(c0, c1, c2, c3))

#endif /* !VBOX_INCLUDED_SRC_Audio_AudioHlp_h */

