/* $Id$ */
/** @file
 * IEM - VT-x instruction implementation.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/**
 * Implements 'VMCALL'.
 */
IEM_CIMPL_DEF_0(iemCImpl_vmcall)
{
    if (true /*EMAreHypercallsEnabled(pVCpu)*/)
    {
        VBOXSTRICTRC rcStrict = GIMHypercall(pVCpu, IEM_GET_CTX(pVCpu));
        if (RT_SUCCESS(rcStrict))
        {
            if (rcStrict == VINF_SUCCESS)
                iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            if (   rcStrict == VINF_SUCCESS
                || rcStrict == VINF_GIM_HYPERCALL_CONTINUING)
                return VINF_SUCCESS;
            AssertMsgReturn(rcStrict == VINF_GIM_R3_HYPERCALL, ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)), VERR_IEM_IPE_4);
            return rcStrict;
        }
        AssertMsgReturn(   rcStrict == VERR_GIM_HYPERCALL_ACCESS_DENIED
                        || rcStrict == VERR_GIM_HYPERCALLS_NOT_AVAILABLE
                        || rcStrict == VERR_GIM_NOT_ENABLED
                        || rcStrict == VERR_GIM_HYPERCALL_MEMORY_READ_FAILED
                        || rcStrict == VERR_GIM_HYPERCALL_MEMORY_WRITE_FAILED,
                        ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)), VERR_IEM_IPE_4);

        /* Raise #UD on all failures. */
    }
    return iemRaiseUndefinedOpcode(pVCpu);
}

