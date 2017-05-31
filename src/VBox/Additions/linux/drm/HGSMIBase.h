/** @file
 * VBox Host Guest Shared Memory Interface (HGSMI) buffer management.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ___VBox_Graphics_HGSMIBase_h___
#define ___VBox_Graphics_HGSMIBase_h___

#include <VBoxVideoIPRT.h>
#include <HGSMIChannels.h>
#include <HGSMIChSetup.h>
#include <HGSMIDefs.h>
#include <linux/genalloc.h>

typedef struct HGSMIGUESTCOMMANDCONTEXT {
	struct gen_pool *guest_pool;
} HGSMIGUESTCOMMANDCONTEXT, *PHGSMIGUESTCOMMANDCONTEXT;

void *hgsmi_buffer_alloc(struct gen_pool *guest_pool, size_t size,
			 u8 channel, u16 channel_info);
void hgsmi_buffer_free(struct gen_pool *guest_pool, void *buf);
int hgsmi_buffer_submit(struct gen_pool *guest_pool, void *buf);

/*
 * Translate CamelCase names used in osindependent code to standard
 * kernel style names.
 */
#define VBoxHGSMIBufferAlloc(ctx, size, ch, ch_info) \
	hgsmi_buffer_alloc((ctx)->guest_pool, size, ch, ch_info)
#define VBoxHGSMIBufferFree(ctx, buf)	hgsmi_buffer_free((ctx)->guest_pool, buf)
#define VBoxHGSMIBufferSubmit(ctx, buf)	hgsmi_buffer_submit((ctx)->guest_pool, buf)

#endif
