/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - R/W Memory Functions Template.
 */

/*
 * Copyright (C) 2011-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/* Check template parameters. */
#ifndef TMPL_MEM_TYPE
# error "TMPL_MEM_TYPE is undefined"
#endif
#ifndef TMPL_MEM_TYPE_ALIGN
# define TMPL_MEM_TYPE_ALIGN     (sizeof(TMPL_MEM_TYPE) - 1)
#endif
#ifndef TMPL_MEM_FN_SUFF
# error "TMPL_MEM_FN_SUFF is undefined"
#endif
#ifndef TMPL_MEM_FMT_TYPE
# error "TMPL_MEM_FMT_TYPE is undefined"
#endif
#ifndef TMPL_MEM_FMT_DESC
# error "TMPL_MEM_FMT_DESC is undefined"
#endif


/**
 * Standard fetch function.
 *
 * This is used by CImpl code, so it needs to be kept even when IEM_WITH_SETJMP
 * is defined.
 */
VBOXSTRICTRC RT_CONCAT(iemMemFetchData,TMPL_MEM_FN_SUFF)(PVMCPUCC pVCpu, TMPL_MEM_TYPE *puDst,
                                                         uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT
{
    /* The lazy approach for now... */
    TMPL_MEM_TYPE const *puSrc;
    VBOXSTRICTRC rc = iemMemMap(pVCpu, (void **)&puSrc, sizeof(*puSrc), iSegReg, GCPtrMem,
                                IEM_ACCESS_DATA_R, TMPL_MEM_TYPE_ALIGN);
    if (rc == VINF_SUCCESS)
    {
        *puDst = *puSrc;
        rc = iemMemCommitAndUnmap(pVCpu, (void *)puSrc, IEM_ACCESS_DATA_R);
        Log9(("IEM RD " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, *puDst));
    }
    return rc;
}


#ifdef IEM_WITH_SETJMP
/**
 * Safe/fallback fetch function that longjmps on error.
 */
TMPL_MEM_TYPE
RT_CONCAT3(iemMemFetchData,TMPL_MEM_FN_SUFF,SafeJmp)(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeReadPath++;
# endif
    TMPL_MEM_TYPE const *puSrc = (TMPL_MEM_TYPE const *)iemMemMapJmp(pVCpu, sizeof(*puSrc), iSegReg, GCPtrMem,
                                                                     IEM_ACCESS_DATA_R, TMPL_MEM_TYPE_ALIGN);
    TMPL_MEM_TYPE const  uRet = *puSrc;
    iemMemCommitAndUnmapJmp(pVCpu, (void *)puSrc, IEM_ACCESS_DATA_R);
    Log9(("IEM RD " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, uRet));
    return uRet;
}
#endif /* IEM_WITH_SETJMP */



/**
 * Standard fetch function.
 *
 * This is used by CImpl code, so it needs to be kept even when IEM_WITH_SETJMP
 * is defined.
 */
VBOXSTRICTRC RT_CONCAT(iemMemStoreData,TMPL_MEM_FN_SUFF)(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem,
                                                         TMPL_MEM_TYPE uValue) RT_NOEXCEPT
{
    /* The lazy approach for now... */
    TMPL_MEM_TYPE *puDst;
    VBOXSTRICTRC rc = iemMemMap(pVCpu, (void **)&puDst, sizeof(*puDst), iSegReg, GCPtrMem, IEM_ACCESS_DATA_W, TMPL_MEM_TYPE_ALIGN);
    if (rc == VINF_SUCCESS)
    {
        *puDst = uValue;
        rc = iemMemCommitAndUnmap(pVCpu, puDst, IEM_ACCESS_DATA_W);
        Log8(("IEM WR " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, uValue));
    }
    return rc;
}


#ifdef IEM_WITH_SETJMP
/**
 * Stores a data byte, longjmp on error.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 * @param   u8Value             The value to store.
 */
void RT_CONCAT3(iemMemStoreData,TMPL_MEM_FN_SUFF,SafeJmp)(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem,
                                                          TMPL_MEM_TYPE uValue) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeWritePath++;
# endif
    Log8(("IEM WR " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, uValue));
    TMPL_MEM_TYPE *puDst = (TMPL_MEM_TYPE *)iemMemMapJmp(pVCpu, sizeof(*puDst), iSegReg, GCPtrMem,
                                                         IEM_ACCESS_DATA_W, TMPL_MEM_TYPE_ALIGN);
    *puDst = uValue;
    iemMemCommitAndUnmapJmp(pVCpu, puDst, IEM_ACCESS_DATA_W);
}
#endif /* IEM_WITH_SETJMP */


#ifdef IEM_WITH_SETJMP

/**
 * Maps a data buffer for read+write direct access (or via a bounce buffer),
 * longjmp on error.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pbUnmapInfo         Pointer to unmap info variable.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
TMPL_MEM_TYPE *
RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,RwSafeJmp)(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                     uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeWritePath++;
# endif
    Log8(("IEM RW/map " TMPL_MEM_FMT_DESC " %d|%RGv\n", iSegReg, GCPtrMem));
    *pbUnmapInfo = 1 | ((IEM_ACCESS_TYPE_READ  | IEM_ACCESS_TYPE_WRITE) << 4); /* zero is for the TLB hit */
    return (TMPL_MEM_TYPE *)iemMemMapJmp(pVCpu, sizeof(TMPL_MEM_TYPE), iSegReg, GCPtrMem, IEM_ACCESS_DATA_RW, TMPL_MEM_TYPE_ALIGN);
}


/**
 * Maps a data buffer for writeonly direct access (or via a bounce buffer),
 * longjmp on error.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pbUnmapInfo         Pointer to unmap info variable.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
TMPL_MEM_TYPE *
RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,WoSafeJmp)(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                     uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeWritePath++;
# endif
    Log8(("IEM WO/map " TMPL_MEM_FMT_DESC " %d|%RGv\n", iSegReg, GCPtrMem));
    *pbUnmapInfo = 1 | (IEM_ACCESS_TYPE_WRITE << 4); /* zero is for the TLB hit */
    return (TMPL_MEM_TYPE *)iemMemMapJmp(pVCpu, sizeof(TMPL_MEM_TYPE), iSegReg, GCPtrMem, IEM_ACCESS_DATA_W, TMPL_MEM_TYPE_ALIGN);
}


/**
 * Maps a data buffer for readonly direct access (or via a bounce buffer),
 * longjmp on error.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pbUnmapInfo         Pointer to unmap info variable.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
TMPL_MEM_TYPE const *
RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,RoSafeJmp)(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                     uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeWritePath++;
# endif
    Log8(("IEM WO/map " TMPL_MEM_FMT_DESC " %d|%RGv\n", iSegReg, GCPtrMem));
    *pbUnmapInfo = 1 | (IEM_ACCESS_TYPE_READ << 4); /* zero is for the TLB hit */
    return (TMPL_MEM_TYPE *)iemMemMapJmp(pVCpu, sizeof(TMPL_MEM_TYPE), iSegReg, GCPtrMem, IEM_ACCESS_DATA_R, TMPL_MEM_TYPE_ALIGN);
}

#endif /* IEM_WITH_SETJMP */


/* clean up */
#undef TMPL_MEM_TYPE
#undef TMPL_MEM_TYPE_ALIGN
#undef TMPL_MEM_FN_SUFF
#undef TMPL_MEM_FMT_TYPE
#undef TMPL_MEM_FMT_DESC

