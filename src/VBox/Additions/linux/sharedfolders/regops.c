/* $Id$ */
/** @file
 * vboxsf - VBox Linux Shared Folders VFS, regular file inode and file operations.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Limitations: only COW memory mapping is supported
 */

#include "vfsmod.h"

#ifdef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE

static void *alloc_bounce_buffer(size_t * tmp_sizep, PRTCCPHYS physp, size_t
				 xfer_size, const char *caller)
{
	size_t tmp_size;
	void *tmp;

	/* try for big first. */
	tmp_size = RT_ALIGN_Z(xfer_size, PAGE_SIZE);
	if (tmp_size > 16U * _1K)
		tmp_size = 16U * _1K;
	tmp = kmalloc(tmp_size, GFP_KERNEL);
	if (!tmp) {
		/* fall back on a page sized buffer. */
		tmp = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!tmp) {
			LogRel(("%s: could not allocate bounce buffer for xfer_size=%zu %s\n", caller, xfer_size));
			return NULL;
		}
		tmp_size = PAGE_SIZE;
	}

	*tmp_sizep = tmp_size;
	*physp = virt_to_phys(tmp);
	return tmp;
}

static void free_bounce_buffer(void *tmp)
{
	kfree(tmp);
}

#else  /* !VBOXSF_USE_DEPRECATED_VBGL_INTERFACE */
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)

/*
 * inode compatibility glue.
 */
# include <iprt/asm.h>

DECLINLINE(loff_t) i_size_read(struct inode *inode)
{
	AssertCompile(sizeof(loff_t) == sizeof(uint64_t));
	return ASMAtomicReadU64((uint64_t volatile *)&inode->i_size);
}

DECLINLINE(void) i_size_write(struct inode *inode, loff_t i_size)
{
	AssertCompile(sizeof(inode->i_size) == sizeof(uint64_t));
	ASMAtomicWriteU64((uint64_t volatile *)&inode->i_size, i_size);
}

# endif /* < 2.6.0 */
#endif /* !VBOXSF_USE_DEPRECATED_VBGL_INTERFACE */

/* fops */
static int sf_reg_read_aux(const char *caller, struct sf_glob_info *sf_g,
			   struct sf_reg_info *sf_r, void *buf,
			   uint32_t * nread, uint64_t pos)
{
    /** @todo bird: yes, kmap() and kmalloc() input only. Since the buffer is
     *        contiguous in physical memory (kmalloc or single page), we should
     *        use a physical address here to speed things up. */
	int rc = VbglR0SfRead(&client_handle, &sf_g->map, sf_r->handle,
			      pos, nread, buf, false /* already locked? */ );
	if (RT_FAILURE(rc)) {
		LogFunc(("VbglR0SfRead failed. caller=%s, rc=%Rrc\n", caller,
			 rc));
		return -EPROTO;
	}
	return 0;
}

static int sf_reg_write_aux(const char *caller, struct sf_glob_info *sf_g,
			    struct sf_reg_info *sf_r, void *buf,
			    uint32_t * nwritten, uint64_t pos)
{
    /** @todo bird: yes, kmap() and kmalloc() input only. Since the buffer is
     *        contiguous in physical memory (kmalloc or single page), we should
     *        use a physical address here to speed things up. */
	int rc = VbglR0SfWrite(&client_handle, &sf_g->map, sf_r->handle,
			       pos, nwritten, buf,
			       false /* already locked? */ );
	if (RT_FAILURE(rc)) {
		LogFunc(("VbglR0SfWrite failed. caller=%s, rc=%Rrc\n",
			 caller, rc));
		return -EPROTO;
	}
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23) \
 && LINUX_VERSION_CODE <  KERNEL_VERSION(2, 6, 31)

void free_pipebuf(struct page *kpage)
{
	kunmap(kpage);
	__free_pages(kpage, 0);
}

void *sf_pipe_buf_map(struct pipe_inode_info *pipe,
		      struct pipe_buffer *pipe_buf, int atomic)
{
	return 0;
}

void sf_pipe_buf_get(struct pipe_inode_info *pipe, struct pipe_buffer *pipe_buf)
{
}

void sf_pipe_buf_unmap(struct pipe_inode_info *pipe,
		       struct pipe_buffer *pipe_buf, void *map_data)
{
}

int sf_pipe_buf_steal(struct pipe_inode_info *pipe,
		      struct pipe_buffer *pipe_buf)
{
	return 0;
}

static void sf_pipe_buf_release(struct pipe_inode_info *pipe,
				struct pipe_buffer *pipe_buf)
{
	free_pipebuf(pipe_buf->page);
}

int sf_pipe_buf_confirm(struct pipe_inode_info *info,
			struct pipe_buffer *pipe_buf)
{
	return 0;
}

static struct pipe_buf_operations sf_pipe_buf_ops = {
	.can_merge = 0,
	.map = sf_pipe_buf_map,
	.unmap = sf_pipe_buf_unmap,
	.confirm = sf_pipe_buf_confirm,
	.release = sf_pipe_buf_release,
	.steal = sf_pipe_buf_steal,
	.get = sf_pipe_buf_get,
};

#define LOCK_PIPE(pipe) \
    if (pipe->inode) \
        mutex_lock(&pipe->inode->i_mutex);

#define UNLOCK_PIPE(pipe) \
    if (pipe->inode) \
        mutex_unlock(&pipe->inode->i_mutex);

ssize_t
sf_splice_read(struct file *in, loff_t * poffset,
	       struct pipe_inode_info *pipe, size_t len, unsigned int flags)
{
	size_t bytes_remaining = len;
	loff_t orig_offset = *poffset;
	loff_t offset = orig_offset;
	struct inode *inode = GET_F_DENTRY(in)->d_inode;
	struct sf_glob_info *sf_g = GET_GLOB_INFO(inode->i_sb);
	struct sf_reg_info *sf_r = in->private_data;
	ssize_t retval;
	struct page *kpage = 0;
	size_t nsent = 0;

	TRACE();
	if (!S_ISREG(inode->i_mode)) {
		LogFunc(("read from non regular file %d\n", inode->i_mode));
		return -EINVAL;
	}
	if (!len) {
		return 0;
	}

	LOCK_PIPE(pipe);

	uint32_t req_size = 0;
	while (bytes_remaining > 0) {
		kpage = alloc_page(GFP_KERNEL);
		if (unlikely(kpage == NULL)) {
			UNLOCK_PIPE(pipe);
			return -ENOMEM;
		}
		req_size = 0;
		uint32_t nread = req_size =
		    (uint32_t) min(bytes_remaining, (size_t) PAGE_SIZE);
		uint32_t chunk = 0;
		void *kbuf = kmap(kpage);
		while (chunk < req_size) {
			retval =
			    sf_reg_read_aux(__func__, sf_g, sf_r, kbuf + chunk,
					    &nread, offset);
			if (retval < 0)
				goto err;
			if (nread == 0)
				break;
			chunk += nread;
			offset += nread;
			nread = req_size - chunk;
		}
		if (!pipe->readers) {
			send_sig(SIGPIPE, current, 0);
			retval = -EPIPE;
			goto err;
		}
		if (pipe->nrbufs < PIPE_BUFFERS) {
			struct pipe_buffer *pipebuf =
			    pipe->bufs +
			    ((pipe->curbuf + pipe->nrbufs) & (PIPE_BUFFERS -
							      1));
			pipebuf->page = kpage;
			pipebuf->ops = &sf_pipe_buf_ops;
			pipebuf->len = req_size;
			pipebuf->offset = 0;
			pipebuf->private = 0;
			pipebuf->flags = 0;
			pipe->nrbufs++;
			nsent += req_size;
			bytes_remaining -= req_size;
			if (signal_pending(current))
				break;
		} else {	/* pipe full */

			if (flags & SPLICE_F_NONBLOCK) {
				retval = -EAGAIN;
				goto err;
			}
			free_pipebuf(kpage);
			break;
		}
	}
	UNLOCK_PIPE(pipe);
	if (!nsent && signal_pending(current))
		return -ERESTARTSYS;
	*poffset += nsent;
	return offset - orig_offset;

 err:
	UNLOCK_PIPE(pipe);
	free_pipebuf(kpage);
	return retval;
}

#endif /* 2.6.23 <= LINUX_VERSION_CODE < 2.6.31 */


/** Companion to sf_lock_user_pages(). */
DECLINLINE(void) sf_unlock_user_pages(struct page **papPages, size_t cPages, bool fSetDirty)
{
    while (cPages-- > 0)
    {
	struct page *pPage = papPages[cPages];
	if (fSetDirty && !PageReserved(pPage))
	    SetPageDirty(pPage);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
	put_page(pPage);
#else
	page_cache_release(pPage);
#endif
    }
}


/** Wrapper around get_user_pages. */
DECLINLINE(int) sf_lock_user_pages(uintptr_t uPtrFrom, size_t cPages, bool fWrite, struct page **papPages)
{
# if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
	ssize_t cPagesLocked = get_user_pages_unlocked(uPtrFrom, cPages, papPages,
						       fWrite ? FOLL_WRITE | FOLL_FORCE : FOLL_FORCE);
# elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
	ssize_t cPagesLocked = get_user_pages_unlocked(uPtrFrom, cPages, fWrite, 1 /*force*/, papPages);
# elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
	ssize_t cPagesLocked = get_user_pages_unlocked(current, current->mm, uPtrFrom, cPages,
						       fWrite, 1 /*force*/, papPages);
# else
	struct task_struct *pTask = current;
	size_t cPagesLocked;
	down_read(&pTask->mm->mmap_sem);
	cPagesLocked = get_user_pages(current, current->mm, uPtrFrom, cPages, fWrite, 1 /*force*/, papPages, NULL);
	up_read(&pTask->mm->mmap_sem);
# endif
	if (cPagesLocked == cPages)
	    return 0;
	if (cPagesLocked < 0)
	    return cPagesLocked;

	sf_unlock_user_pages(papPages, cPagesLocked, false /*fSetDirty*/);

	/* We could use uPtrFrom + cPagesLocked to get the correct status here... */
	return -EFAULT;
}


#ifndef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
/**
 * Fallback case of sf_reg_read() that locks the user buffers and let the host
 * write directly to them.
 */
static ssize_t sf_reg_read_fallback(struct file *file, char /*__user*/ *buf, size_t size, loff_t *off,
				    struct sf_glob_info *sf_g, struct sf_reg_info *sf_r)
{
	/*
	 * Lock pages and execute the read, taking care not to pass the host
	 * more than it can handle in one go or more than we care to allocate
	 * page arrays for.  The latter limit is set at just short of 32KB due
	 * to how the physical heap works.
	 */
	struct page        *apPagesStack[16];
	struct page       **papPages     = &apPagesStack[0];
	struct page       **papPagesFree = NULL;
	VBOXSFREADPGLSTREQ *pReq;
	loff_t              offFile      = *off;
	ssize_t             cbRet        = -ENOMEM;
	size_t              cPages       = (((uintptr_t)buf & PAGE_OFFSET_MASK) + size + PAGE_OFFSET_MASK) >> PAGE_SHIFT;
	size_t              cMaxPages    = RT_MIN(RT_MAX(sf_g->cMaxIoPages, 1), cPages);

	pReq = (VBOXSFREADPGLSTREQ *)VbglR0PhysHeapAlloc(RT_UOFFSETOF_DYN(VBOXSFREADPGLSTREQ, PgLst.aPages[cMaxPages]));
	while (!pReq && cMaxPages > 4) {
		cMaxPages /= 2;
		pReq = (VBOXSFREADPGLSTREQ *)VbglR0PhysHeapAlloc(RT_UOFFSETOF_DYN(VBOXSFREADPGLSTREQ, PgLst.aPages[cMaxPages]));
	}
	if (pReq && cPages > RT_ELEMENTS(apPagesStack))
		papPagesFree = papPages = kmalloc(cMaxPages * sizeof(sizeof(papPages[0])), GFP_KERNEL);
	if (pReq && papPages) {
		cbRet = 0;
		for (;;) {
			/*
			 * Figure out how much to process now and lock the user pages.
			 */
			int    rc;
			size_t cbChunk = (uintptr_t)buf & PAGE_OFFSET_MASK;
			pReq->PgLst.offFirstPage = (uint16_t)cbChunk;
			cPages  = RT_ALIGN_Z(cbChunk + size, PAGE_SIZE) >> PAGE_SHIFT;
			if (cPages <= cMaxPages)
				cbChunk = size;
			else {
				cPages  = cMaxPages;
				cbChunk = (cMaxPages << PAGE_SHIFT) - cbChunk;
			}

			rc = sf_lock_user_pages((uintptr_t)buf, cPages, true /*fWrite*/, papPages);
			if (rc == 0) {
				size_t iPage = cPages;
				while (iPage-- > 0)
					pReq->PgLst.aPages[iPage] = page_to_phys(papPages[iPage]);
			} else {
				cbRet = rc;
				break;
			}

			/*
			 * Issue the request and unlock the pages.
			 */
			rc = VbglR0SfHostReqReadPgLst(sf_g->map.root, pReq, sf_r->handle, offFile, cbChunk, cPages);

			sf_unlock_user_pages(papPages, cPages, true /*fSetDirty*/);

			if (RT_SUCCESS(rc)) {
				/*
				 * Success, advance position and buffer.
				 */
				uint32_t cbActual = pReq->Parms.cb32Read.u.value32;
				AssertStmt(cbActual <= cbChunk, cbActual = cbChunk);
				cbRet   += cbActual;
				offFile += cbActual;
				buf      = (uint8_t *)buf + cbActual;
				size    -= cbActual;

				/*
				 * Are we done already?  If so commit the new file offset.
				 */
				if (!size || cbActual < cbChunk) {
					*off = offFile;
					break;
				}
			} else if (rc == VERR_NO_MEMORY && cMaxPages > 4) {
				/*
				 * The host probably doesn't have enough heap to handle the
				 * request, reduce the page count and retry.
				 */
				cMaxPages /= 4;
				Assert(cMaxPages > 0);
			} else {
				/*
				 * If we've successfully read stuff, return it rather than
				 * the error.  (Not sure if this is such a great idea...)
				 */
				if (cbRet > 0)
					*off = offFile;
				else
					cbRet = -EPROTO;
				break;
			}
		}
	}
	if (papPagesFree)
		kfree(papPages);
	if (pReq)
		VbglR0PhysHeapFree(pReq);
	return cbRet;
}
#endif /* VBOXSF_USE_DEPRECATED_VBGL_INTERFACE */


/**
 * Read from a regular file.
 *
 * @param file          the file
 * @param buf           the buffer
 * @param size          length of the buffer
 * @param off           offset within the file (in/out).
 * @returns the number of read bytes on success, Linux error code otherwise
 */
static ssize_t sf_reg_read(struct file *file, char /*__user*/ *buf, size_t size,
			   loff_t *off)
{
#ifdef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
	int err;
	void *tmp;
	RTCCPHYS tmp_phys;
	size_t tmp_size;
	size_t left = size;
	ssize_t total_bytes_read = 0;
	loff_t pos = *off;
#endif
	struct inode *inode = GET_F_DENTRY(file)->d_inode;
	struct sf_glob_info *sf_g = GET_GLOB_INFO(inode->i_sb);
	struct sf_reg_info *sf_r = file->private_data;

	TRACE();
	if (!S_ISREG(inode->i_mode)) {
		LogFunc(("read from non regular file %d\n", inode->i_mode));
		return -EINVAL;
	}

	/** @todo XXX Check read permission according to inode->i_mode! */

	if (!size)
		return 0;

#ifdef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
	tmp = alloc_bounce_buffer(&tmp_size, &tmp_phys, size, __PRETTY_FUNCTION__);
	if (!tmp)
		return -ENOMEM;

	while (left) {
		uint32_t to_read, nread;

		to_read = tmp_size;
		if (to_read > left)
			to_read = (uint32_t) left;

		nread = to_read;

		err = sf_reg_read_aux(__func__, sf_g, sf_r, tmp, &nread, pos);
		if (err)
			goto fail;

		if (copy_to_user(buf, tmp, nread)) {
			err = -EFAULT;
			goto fail;
		}

		pos += nread;
		left -= nread;
		buf += nread;
		total_bytes_read += nread;
		if (nread != to_read)
			break;
	}

	*off += total_bytes_read;
	free_bounce_buffer(tmp);
	return total_bytes_read;

 fail:
	free_bounce_buffer(tmp);
	return err;

#else  /* !VBOXSF_USE_DEPRECATED_VBGL_INTERFACE */
	/*
	 * For small requests, try use an embedded buffer provided we get a heap block
	 * that does not cross page boundraries (see host code).
	 */
	if (size <= PAGE_SIZE / 4 * 3 - RT_UOFFSETOF(VBOXSFREADEMBEDDEDREQ, abData[0]) /* see allocator */) {
		uint32_t const         cbReq = RT_UOFFSETOF(VBOXSFREADEMBEDDEDREQ, abData[0]) + size;
		VBOXSFREADEMBEDDEDREQ *pReq  = (VBOXSFREADEMBEDDEDREQ *)VbglR0PhysHeapAlloc(cbReq);
		if (   pReq
		    && (PAGE_SIZE - ((uintptr_t)pReq & PAGE_OFFSET_MASK)) >= cbReq) {
			ssize_t cbRet;
			int vrc = VbglR0SfHostReqReadEmbedded(sf_g->map.root, pReq, sf_r->handle, *off, (uint32_t)size);
			if (RT_SUCCESS(vrc)) {
				cbRet = pReq->Parms.cb32Read.u.value32;
				AssertStmt(cbRet <= (ssize_t)size, cbRet = size);
				if (copy_to_user(buf, pReq->abData, cbRet) == 0)
					*off += cbRet;
				else
					cbRet = -EFAULT;
			} else
				cbRet = -EPROTO;
		        VbglR0PhysHeapFree(pReq);
			return cbRet;
		}
		if (pReq)
			VbglR0PhysHeapFree(pReq);
	}

# if 0 /* Turns out this is slightly slower than locking the pages even for 4KB reads (4.19/amd64). */
	/*
	 * For medium sized requests try use a bounce buffer.
	 */
	if (size <= _64K /** @todo make this configurable? */) {
		void *pvBounce = kmalloc(size, GFP_KERNEL);
		if (pvBounce) {
			VBOXSFREADPGLSTREQ *pReq = (VBOXSFREADPGLSTREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
			if (pReq) {
				ssize_t cbRet;
				int vrc = VbglR0SfHostReqReadContig(sf_g->map.root, pReq, sf_r->handle, *off, (uint32_t)size,
								    pvBounce, virt_to_phys(pvBounce));
				if (RT_SUCCESS(vrc)) {
					cbRet = pReq->Parms.cb32Read.u.value32;
					AssertStmt(cbRet <= (ssize_t)size, cbRet = size);
					if (copy_to_user(buf, pvBounce, cbRet) == 0)
						*off += cbRet;
					else
						cbRet = -EFAULT;
				} else
					cbRet = -EPROTO;
				VbglR0PhysHeapFree(pReq);
				kfree(pvBounce);
				return cbRet;
			}
			kfree(pvBounce);
		}
	}
# endif

	return sf_reg_read_fallback(file, buf, size, off, sf_g, sf_r);
#endif /* !VBOXSF_USE_DEPRECATED_VBGL_INTERFACE */
}


#ifndef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
/**
 * Fallback case of sf_reg_write() that locks the user buffers and let the host
 * write directly to them.
 */
static ssize_t sf_reg_write_fallback(struct file *file, const char /*__user*/ *buf, size_t size, loff_t *off, loff_t offFile,
				     struct inode *inode, struct sf_inode_info *sf_i,
				     struct sf_glob_info *sf_g, struct sf_reg_info *sf_r)
{
	/*
	 * Lock pages and execute the write, taking care not to pass the host
	 * more than it can handle in one go or more than we care to allocate
	 * page arrays for.  The latter limit is set at just short of 32KB due
	 * to how the physical heap works.
	 */
	struct page         *apPagesStack[16];
	struct page        **papPages     = &apPagesStack[0];
	struct page        **papPagesFree = NULL;
	VBOXSFWRITEPGLSTREQ *pReq;
	ssize_t              cbRet        = -ENOMEM;
	size_t               cPages       = (((uintptr_t)buf & PAGE_OFFSET_MASK) + size + PAGE_OFFSET_MASK) >> PAGE_SHIFT;
	size_t               cMaxPages    = RT_MIN(RT_MAX(sf_g->cMaxIoPages, 1), cPages);

	pReq = (VBOXSFWRITEPGLSTREQ *)VbglR0PhysHeapAlloc(RT_UOFFSETOF_DYN(VBOXSFWRITEPGLSTREQ, PgLst.aPages[cMaxPages]));
	while (!pReq && cMaxPages > 4) {
		cMaxPages /= 2;
		pReq = (VBOXSFWRITEPGLSTREQ *)VbglR0PhysHeapAlloc(RT_UOFFSETOF_DYN(VBOXSFWRITEPGLSTREQ, PgLst.aPages[cMaxPages]));
	}
	if (pReq && cPages > RT_ELEMENTS(apPagesStack))
		papPagesFree = papPages = kmalloc(cMaxPages * sizeof(sizeof(papPages[0])), GFP_KERNEL);
	if (pReq && papPages) {
		cbRet = 0;
		for (;;) {
			/*
			 * Figure out how much to process now and lock the user pages.
			 */
			int    rc;
			size_t cbChunk = (uintptr_t)buf & PAGE_OFFSET_MASK;
			pReq->PgLst.offFirstPage = (uint16_t)cbChunk;
			cPages  = RT_ALIGN_Z(cbChunk + size, PAGE_SIZE) >> PAGE_SHIFT;
			if (cPages <= cMaxPages)
				cbChunk = size;
			else {
				cPages  = cMaxPages;
				cbChunk = (cMaxPages << PAGE_SHIFT) - cbChunk;
			}

			rc = sf_lock_user_pages((uintptr_t)buf, cPages, false /*fWrite*/, papPages);
			if (rc == 0) {
				size_t iPage = cPages;
				while (iPage-- > 0)
					pReq->PgLst.aPages[iPage] = page_to_phys(papPages[iPage]);
			} else {
				cbRet = rc;
				break;
			}

			/*
			 * Issue the request and unlock the pages.
			 */
			rc = VbglR0SfHostReqWritePgLst(sf_g->map.root, pReq, sf_r->handle, offFile, cbChunk, cPages);

			sf_unlock_user_pages(papPages, cPages, false /*fSetDirty*/);

			if (RT_SUCCESS(rc)) {
				/*
				 * Success, advance position and buffer.
				 */
				uint32_t cbActual = pReq->Parms.cb32Write.u.value32;
				AssertStmt(cbActual <= cbChunk, cbActual = cbChunk);
				cbRet   += cbActual;
				offFile += cbActual;
				buf      = (uint8_t *)buf + cbActual;
				size    -= cbActual;
				if (offFile > i_size_read(inode))
					i_size_write(inode, offFile);

				/*
				 * Are we done already?  If so commit the new file offset.
				 */
				if (!size || cbActual < cbChunk) {
					*off = offFile;
					break;
				}
			} else if (rc == VERR_NO_MEMORY && cMaxPages > 4) {
				/*
				 * The host probably doesn't have enough heap to handle the
				 * request, reduce the page count and retry.
				 */
				cMaxPages /= 4;
				Assert(cMaxPages > 0);
			} else {
				/*
				 * If we've successfully written stuff, return it rather than
				 * the error.  (Not sure if this is such a great idea...)
				 */
				if (cbRet > 0)
					*off = offFile;
				else
					cbRet = -EPROTO;
				break;
			}
			sf_i->force_restat = 1; /* mtime (and size) may have changed */
		}
	}
	if (papPagesFree)
		kfree(papPages);
	if (pReq)
		VbglR0PhysHeapFree(pReq);
	return cbRet;
}
#endif /* VBOXSF_USE_DEPRECATED_VBGL_INTERFACE */

/**
 * Write to a regular file.
 *
 * @param file          the file
 * @param buf           the buffer
 * @param size          length of the buffer
 * @param off           offset within the file
 * @returns the number of written bytes on success, Linux error code otherwise
 */
static ssize_t sf_reg_write(struct file *file, const char *buf, size_t size,
			    loff_t * off)
{
#ifdef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
	int err;
	void *tmp;
	RTCCPHYS tmp_phys;
	size_t tmp_size;
	size_t left = size;
	ssize_t total_bytes_written = 0;
#endif
	loff_t pos;
	struct inode *inode = GET_F_DENTRY(file)->d_inode;
	struct sf_inode_info *sf_i = GET_INODE_INFO(inode);
	struct sf_glob_info *sf_g = GET_GLOB_INFO(inode->i_sb);
	struct sf_reg_info *sf_r = file->private_data;

	TRACE();
	BUG_ON(!sf_i);
	BUG_ON(!sf_g);
	BUG_ON(!sf_r);

	if (!S_ISREG(inode->i_mode)) {
		LogFunc(("write to non regular file %d\n", inode->i_mode));
		return -EINVAL;
	}

	pos = *off;
	/** @todo This should be handled by the host, it returning the new file
	 *        offset when appending.  We may have an outdated i_size value here! */
	if (file->f_flags & O_APPEND) {
#ifdef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
		pos = inode->i_size;
		*off = pos;
#else
		pos = i_size_read(inode);
#endif
	}

	/** @todo XXX Check write permission according to inode->i_mode! */

	if (!size) {
#ifndef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
		if (file->f_flags & O_APPEND)  /** @todo check if this is the consensus behavior... */
			*off = pos;
#endif
		return 0;
	}

#ifdef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
	tmp = alloc_bounce_buffer(&tmp_size, &tmp_phys, size,
				  __PRETTY_FUNCTION__);
	if (!tmp)
		return -ENOMEM;

	while (left) {
		uint32_t to_write, nwritten;

		to_write = tmp_size;
		if (to_write > left)
			to_write = (uint32_t) left;

		nwritten = to_write;

		if (copy_from_user(tmp, buf, to_write)) {
			err = -EFAULT;
			goto fail;
		}

		err =
		    VbglR0SfWritePhysCont(&client_handle, &sf_g->map,
					  sf_r->handle, pos, &nwritten,
					  tmp_phys);
		err = RT_FAILURE(err) ? -EPROTO : 0;
		if (err)
			goto fail;

		pos += nwritten;
		left -= nwritten;
		buf += nwritten;
		total_bytes_written += nwritten;
		if (nwritten != to_write)
			break;
	}

	*off += total_bytes_written;
	if (*off > inode->i_size)
		inode->i_size = *off;

	sf_i->force_restat = 1;
	free_bounce_buffer(tmp);
	return total_bytes_written;

 fail:
	free_bounce_buffer(tmp);
	return err;
#else  /* !VBOXSF_USE_DEPRECATED_VBGL_INTERFACE */

	/*
	 * For small requests, try use an embedded buffer provided we get a heap block
	 * that does not cross page boundraries (see host code).
	 */
	if (size <= PAGE_SIZE / 4 * 3 - RT_UOFFSETOF(VBOXSFWRITEEMBEDDEDREQ, abData[0]) /* see allocator */) {
		uint32_t const          cbReq = RT_UOFFSETOF(VBOXSFWRITEEMBEDDEDREQ, abData[0]) + size;
		VBOXSFWRITEEMBEDDEDREQ *pReq  = (VBOXSFWRITEEMBEDDEDREQ *)VbglR0PhysHeapAlloc(cbReq);
		if (   pReq
		    && (PAGE_SIZE - ((uintptr_t)pReq & PAGE_OFFSET_MASK)) >= cbReq) {
			ssize_t cbRet;
			if (copy_from_user(pReq->abData, buf, size) == 0) {
				int vrc = VbglR0SfHostReqWriteEmbedded(sf_g->map.root, pReq, sf_r->handle, pos, (uint32_t)size);
				if (RT_SUCCESS(vrc)) {
					cbRet = pReq->Parms.cb32Write.u.value32;
					AssertStmt(cbRet <= (ssize_t)size, cbRet = size);
					pos += cbRet;
					*off = pos;
					if (pos > i_size_read(inode))
						i_size_write(inode, pos);
				} else
					cbRet = -EPROTO;
				sf_i->force_restat = 1; /* mtime (and size) may have changed */
			} else
				cbRet = -EFAULT;

		        VbglR0PhysHeapFree(pReq);
			return cbRet;
		}
		if (pReq)
			VbglR0PhysHeapFree(pReq);
	}

# if 0 /* Turns out this is slightly slower than locking the pages even for 4KB reads (4.19/amd64). */
	/*
	 * For medium sized requests try use a bounce buffer.
	 */
	if (size <= _64K /** @todo make this configurable? */) {
		void *pvBounce = kmalloc(size, GFP_KERNEL);
		if (pvBounce) {
			if (copy_from_user(pvBounce, buf, size) == 0) {
				VBOXSFWRITEPGLSTREQ *pReq = (VBOXSFWRITEPGLSTREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
				if (pReq) {
					ssize_t cbRet;
					int vrc = VbglR0SfHostReqWriteContig(sf_g->map.root, pReq, sf_r->handle, pos,
									     (uint32_t)size, pvBounce, virt_to_phys(pvBounce));
					if (RT_SUCCESS(vrc)) {
						cbRet = pReq->Parms.cb32Write.u.value32;
						AssertStmt(cbRet <= (ssize_t)size, cbRet = size);
						pos += cbRet;
						*off = pos;
						if (pos > i_size_read(inode))
							i_size_write(inode, pos);
					} else
						cbRet = -EPROTO;
					sf_i->force_restat = 1; /* mtime (and size) may have changed */
					VbglR0PhysHeapFree(pReq);
					kfree(pvBounce);
					return cbRet;
				}
				kfree(pvBounce);
			} else {
				kfree(pvBounce);
				return -EFAULT;
			}
		}
	}
# endif

	return sf_reg_write_fallback(file, buf, size, off, pos, inode, sf_i, sf_g, sf_r);
#endif /* !VBOXSF_USE_DEPRECATED_VBGL_INTERFACE */
}

/**
 * Open a regular file.
 *
 * @param inode         the inode
 * @param file          the file
 * @returns 0 on success, Linux error code otherwise
 */
static int sf_reg_open(struct inode *inode, struct file *file)
{
	int rc, rc_linux = 0;
	struct sf_glob_info *sf_g = GET_GLOB_INFO(inode->i_sb);
	struct sf_inode_info *sf_i = GET_INODE_INFO(inode);
	struct sf_reg_info *sf_r;
#ifdef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
	SHFLCREATEPARMS params;
#else
	VBOXSFCREATEREQ *pReq;
#endif
	SHFLCREATEPARMS *pCreateParms;  /* temp glue */

	TRACE();
	BUG_ON(!sf_g);
	BUG_ON(!sf_i);

	LogFunc(("open %s\n", sf_i->path->String.utf8));

	sf_r = kmalloc(sizeof(*sf_r), GFP_KERNEL);
	if (!sf_r) {
		LogRelFunc(("could not allocate reg info\n"));
		return -ENOMEM;
	}

	/* Already open? */
	if (sf_i->handle != SHFL_HANDLE_NIL) {
		/*
		 * This inode was created with sf_create_aux(). Check the CreateFlags:
		 * O_CREAT, O_TRUNC: inherent true (file was just created). Not sure
		 * about the access flags (SHFL_CF_ACCESS_*).
		 */
		sf_i->force_restat = 1;
		sf_r->handle = sf_i->handle;
		sf_i->handle = SHFL_HANDLE_NIL;
		sf_i->file = file;
		file->private_data = sf_r;
		return 0;
	}

#ifdef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
	RT_ZERO(params);
	pCreateParms = &params;
#else
	pReq = (VBOXSFCREATEREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq) + sf_i->path->u16Size);
	if (!pReq) {
		kfree(sf_r);
		LogRelFunc(("Failed to allocate a VBOXSFCREATEREQ buffer!\n"));
		return -ENOMEM;
	}
	memcpy(&pReq->StrPath, sf_i->path, SHFLSTRING_HEADER_SIZE + sf_i->path->u16Size);
	RT_ZERO(pReq->CreateParms);
	pCreateParms = &pReq->CreateParms;
#endif
	pCreateParms->Handle = SHFL_HANDLE_NIL;

	/* We check the value of pCreateParms->Handle afterwards to find out if
	 * the call succeeded or failed, as the API does not seem to cleanly
	 * distinguish error and informational messages.
	 *
	 * Furthermore, we must set pCreateParms->Handle to SHFL_HANDLE_NIL to
	 * make the shared folders host service use our fMode parameter */

	if (file->f_flags & O_CREAT) {
		LogFunc(("O_CREAT set\n"));
		pCreateParms->CreateFlags |= SHFL_CF_ACT_CREATE_IF_NEW;
		/* We ignore O_EXCL, as the Linux kernel seems to call create
		   beforehand itself, so O_EXCL should always fail. */
		if (file->f_flags & O_TRUNC) {
			LogFunc(("O_TRUNC set\n"));
			pCreateParms->CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS;
		} else
			pCreateParms->CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS;
	} else {
		pCreateParms->CreateFlags |= SHFL_CF_ACT_FAIL_IF_NEW;
		if (file->f_flags & O_TRUNC) {
			LogFunc(("O_TRUNC set\n"));
			pCreateParms->CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS;
		}
	}

	switch (file->f_flags & O_ACCMODE) {
	case O_RDONLY:
		pCreateParms->CreateFlags |= SHFL_CF_ACCESS_READ;
		break;

	case O_WRONLY:
		pCreateParms->CreateFlags |= SHFL_CF_ACCESS_WRITE;
		break;

	case O_RDWR:
		pCreateParms->CreateFlags |= SHFL_CF_ACCESS_READWRITE;
		break;

	default:
		BUG();
	}

	if (file->f_flags & O_APPEND) {
		LogFunc(("O_APPEND set\n"));
		pCreateParms->CreateFlags |= SHFL_CF_ACCESS_APPEND;
	}

	pCreateParms->Info.Attr.fMode = inode->i_mode;
#ifdef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
	LogFunc(("sf_reg_open: calling VbglR0SfCreate, file %s, flags=%#x, %#x\n", sf_i->path->String.utf8, file->f_flags, pCreateParms->CreateFlags));
	rc = VbglR0SfCreate(&client_handle, &sf_g->map, sf_i->path, pCreateParms);
#else
	LogFunc(("sf_reg_open: calling VbglR0SfHostReqCreate, file %s, flags=%#x, %#x\n", sf_i->path->String.utf8, file->f_flags, pCreateParms->CreateFlags));
	rc = VbglR0SfHostReqCreate(sf_g->map.root, pReq);
#endif
	if (RT_FAILURE(rc)) {
#ifdef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
		LogFunc(("VbglR0SfCreate failed flags=%d,%#x rc=%Rrc\n", file->f_flags, pCreateParms->CreateFlags, rc));
#else
		LogFunc(("VbglR0SfHostReqCreate failed flags=%d,%#x rc=%Rrc\n", file->f_flags, pCreateParms->CreateFlags, rc));
#endif
		kfree(sf_r);
#ifndef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
		VbglR0PhysHeapFree(pReq);
#endif
		return -RTErrConvertToErrno(rc);
	}

	if (pCreateParms->Handle == SHFL_HANDLE_NIL) {
		switch (pCreateParms->Result) {
		case SHFL_PATH_NOT_FOUND:
		case SHFL_FILE_NOT_FOUND:
			rc_linux = -ENOENT;
			break;
		case SHFL_FILE_EXISTS:
			rc_linux = -EEXIST;
			break;
		default:
			break;
		}
	}

	sf_i->force_restat = 1;
	sf_r->handle = pCreateParms->Handle;
	sf_i->file = file;
	file->private_data = sf_r;
#ifndef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
	VbglR0PhysHeapFree(pReq);
#endif
	return rc_linux;
}

/**
 * Close a regular file.
 *
 * @param inode         the inode
 * @param file          the file
 * @returns 0 on success, Linux error code otherwise
 */
static int sf_reg_release(struct inode *inode, struct file *file)
{
	int rc;
	struct sf_reg_info *sf_r;
	struct sf_glob_info *sf_g;
	struct sf_inode_info *sf_i = GET_INODE_INFO(inode);

	TRACE();
	sf_g = GET_GLOB_INFO(inode->i_sb);
	sf_r = file->private_data;

	BUG_ON(!sf_g);
	BUG_ON(!sf_r);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 25)
	/* See the smbfs source (file.c). mmap in particular can cause data to be
	 * written to the file after it is closed, which we can't cope with.  We
	 * copy and paste the body of filemap_write_and_wait() here as it was not
	 * defined before 2.6.6 and not exported until quite a bit later. */
	/* filemap_write_and_wait(inode->i_mapping); */
	if (inode->i_mapping->nrpages
	    && filemap_fdatawrite(inode->i_mapping) != -EIO)
		filemap_fdatawait(inode->i_mapping);
#endif
#ifdef VBOXSF_USE_DEPRECATED_VBGL_INTERFACE
	rc = VbglR0SfClose(&client_handle, &sf_g->map, sf_r->handle);
	if (RT_FAILURE(rc))
		LogFunc(("VbglR0SfClose failed rc=%Rrc\n", rc));
#else
	rc = VbglR0SfHostReqCloseSimple(sf_g->map.root, sf_r->handle);
	if (RT_FAILURE(rc))
		LogFunc(("VbglR0SfHostReqCloseSimple failed rc=%Rrc\n", rc));
	sf_r->handle = SHFL_HANDLE_NIL;
#endif

	kfree(sf_r);
	sf_i->file = NULL;
	sf_i->handle = SHFL_HANDLE_NIL;
	file->private_data = NULL;
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
static int sf_reg_fault(struct vm_fault *vmf)
#elif LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25)
static int sf_reg_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
static struct page *sf_reg_nopage(struct vm_area_struct *vma,
				  unsigned long vaddr, int *type)
# define SET_TYPE(t) *type = (t)
#else  /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0) */
static struct page *sf_reg_nopage(struct vm_area_struct *vma,
				  unsigned long vaddr, int unused)
# define SET_TYPE(t)
#endif
{
	struct page *page;
	char *buf;
	loff_t off;
	uint32_t nread = PAGE_SIZE;
	int err;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	struct vm_area_struct *vma = vmf->vma;
#endif
	struct file *file = vma->vm_file;
	struct inode *inode = GET_F_DENTRY(file)->d_inode;
	struct sf_glob_info *sf_g = GET_GLOB_INFO(inode->i_sb);
	struct sf_reg_info *sf_r = file->private_data;

	TRACE();
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25)
	if (vmf->pgoff > vma->vm_end)
		return VM_FAULT_SIGBUS;
#else
	if (vaddr > vma->vm_end) {
		SET_TYPE(VM_FAULT_SIGBUS);
		return NOPAGE_SIGBUS;
	}
#endif

	/* Don't use GFP_HIGHUSER as long as sf_reg_read_aux() calls VbglR0SfRead()
	 * which works on virtual addresses. On Linux cannot reliably determine the
	 * physical address for high memory, see rtR0MemObjNativeLockKernel(). */
	page = alloc_page(GFP_USER);
	if (!page) {
		LogRelFunc(("failed to allocate page\n"));
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25)
		return VM_FAULT_OOM;
#else
		SET_TYPE(VM_FAULT_OOM);
		return NOPAGE_OOM;
#endif
	}

	buf = kmap(page);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25)
	off = (vmf->pgoff << PAGE_SHIFT);
#else
	off = (vaddr - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT);
#endif
	err = sf_reg_read_aux(__func__, sf_g, sf_r, buf, &nread, off);
	if (err) {
		kunmap(page);
		put_page(page);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25)
		return VM_FAULT_SIGBUS;
#else
		SET_TYPE(VM_FAULT_SIGBUS);
		return NOPAGE_SIGBUS;
#endif
	}

	BUG_ON(nread > PAGE_SIZE);
	if (!nread) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25)
		clear_user_page(page_address(page), vmf->pgoff, page);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
		clear_user_page(page_address(page), vaddr, page);
#else
		clear_user_page(page_address(page), vaddr);
#endif
	} else
		memset(buf + nread, 0, PAGE_SIZE - nread);

	flush_dcache_page(page);
	kunmap(page);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25)
	vmf->page = page;
	return 0;
#else
	SET_TYPE(VM_FAULT_MAJOR);
	return page;
#endif
}

static struct vm_operations_struct sf_vma_ops = {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25)
	.fault = sf_reg_fault
#else
	.nopage = sf_reg_nopage
#endif
};

static int sf_reg_mmap(struct file *file, struct vm_area_struct *vma)
{
	TRACE();
	if (vma->vm_flags & VM_SHARED) {
		LogFunc(("shared mmapping not available\n"));
		return -EINVAL;
	}

	vma->vm_ops = &sf_vma_ops;
	return 0;
}

struct file_operations sf_reg_fops = {
	.read = sf_reg_read,
	.open = sf_reg_open,
	.write = sf_reg_write,
	.release = sf_reg_release,
	.mmap = sf_reg_mmap,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31)
/** @todo This code is known to cause caching of data which should not be
 * cached.  Investigate. */
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
	.splice_read = sf_splice_read,
# else
	.sendfile = generic_file_sendfile,
# endif
	.aio_read = generic_file_aio_read,
	.aio_write = generic_file_aio_write,
# endif
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35)
	.fsync = noop_fsync,
# else
	.fsync = simple_sync_file,
# endif
	.llseek = generic_file_llseek,
#endif
};

struct inode_operations sf_reg_iops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
	.revalidate = sf_inode_revalidate
#else
	.getattr = sf_getattr,
	.setattr = sf_setattr
#endif
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)

static int sf_readpage(struct file *file, struct page *page)
{
	struct inode *inode = GET_F_DENTRY(file)->d_inode;
	struct sf_glob_info *sf_g = GET_GLOB_INFO(inode->i_sb);
	struct sf_reg_info *sf_r = file->private_data;
	uint32_t nread = PAGE_SIZE;
	char *buf;
	loff_t off = (loff_t)page->index << PAGE_SHIFT;
	int ret;

	TRACE();

	buf = kmap(page);
	ret = sf_reg_read_aux(__func__, sf_g, sf_r, buf, &nread, off);
	if (ret) {
		kunmap(page);
		if (PageLocked(page))
			unlock_page(page);
		return ret;
	}
	BUG_ON(nread > PAGE_SIZE);
	memset(&buf[nread], 0, PAGE_SIZE - nread);
	flush_dcache_page(page);
	kunmap(page);
	SetPageUptodate(page);
	unlock_page(page);
	return 0;
}

static int sf_writepage(struct page *page, struct writeback_control *wbc)
{
	struct address_space *mapping = page->mapping;
	struct inode *inode = mapping->host;
	struct sf_glob_info *sf_g = GET_GLOB_INFO(inode->i_sb);
	struct sf_inode_info *sf_i = GET_INODE_INFO(inode);
	struct file *file = sf_i->file;
	struct sf_reg_info *sf_r = file->private_data;
	char *buf;
	uint32_t nwritten = PAGE_SIZE;
	int end_index = inode->i_size >> PAGE_SHIFT;
	loff_t off = ((loff_t) page->index) << PAGE_SHIFT;
	int err;

	TRACE();

	if (page->index >= end_index)
		nwritten = inode->i_size & (PAGE_SIZE - 1);

	buf = kmap(page);

	err = sf_reg_write_aux(__func__, sf_g, sf_r, buf, &nwritten, off);
	if (err < 0) {
		ClearPageUptodate(page);
		goto out;
	}

	if (off > inode->i_size)
		inode->i_size = off;

	if (PageError(page))
		ClearPageError(page);
	err = 0;

 out:
	kunmap(page);

	unlock_page(page);
	return err;
}

# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)

int sf_write_begin(struct file *file, struct address_space *mapping, loff_t pos,
		   unsigned len, unsigned flags, struct page **pagep,
		   void **fsdata)
{
	TRACE();

	return simple_write_begin(file, mapping, pos, len, flags, pagep,
				  fsdata);
}

int sf_write_end(struct file *file, struct address_space *mapping, loff_t pos,
		 unsigned len, unsigned copied, struct page *page, void *fsdata)
{
	struct inode *inode = mapping->host;
	struct sf_glob_info *sf_g = GET_GLOB_INFO(inode->i_sb);
	struct sf_reg_info *sf_r = file->private_data;
	void *buf;
	unsigned from = pos & (PAGE_SIZE - 1);
	uint32_t nwritten = len;
	int err;

	TRACE();

	buf = kmap(page);
	err = sf_reg_write_aux(__func__, sf_g, sf_r, buf + from, &nwritten, pos);
	kunmap(page);

	if (err >= 0) {
		if (!PageUptodate(page) && nwritten == PAGE_SIZE)
			SetPageUptodate(page);

		pos += nwritten;
		if (pos > inode->i_size)
			inode->i_size = pos;
	}

	unlock_page(page);
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
	put_page(page);
#  else
	page_cache_release(page);
#  endif

	return nwritten;
}

# endif	/* KERNEL_VERSION >= 2.6.24 */

# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 10)
/**
 * This is needed to make open accept O_DIRECT as well as dealing with direct
 * I/O requests if we don't intercept them earlier.
 */
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
static ssize_t sf_direct_IO(struct kiocb *iocb, struct iov_iter *iter)
#  elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
static ssize_t sf_direct_IO(struct kiocb *iocb, struct iov_iter *iter, loff_t offset)
#  elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static ssize_t sf_direct_IO(int rw, struct kiocb *iocb, struct iov_iter *iter, loff_t offset)
#  elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 6)
static ssize_t sf_direct_IO(int rw, struct kiocb *iocb, const struct iovec *iov, loff_t offset, unsigned long nr_segs)
#  elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 55)
static int sf_direct_IO(int rw, struct kiocb *iocb, const struct iovec *iov, loff_t offset, unsigned long nr_segs)
#  elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 41)
static int sf_direct_IO(int rw, struct file *file, const struct iovec *iov, loff_t offset, unsigned long nr_segs)
#  elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 35)
static int sf_direct_IO(int rw, struct inode *inode, const struct iovec *iov, loff_t offset, unsigned long nr_segs)
#  elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 26)
static int sf_direct_IO(int rw, struct inode *inode, char *buf, loff_t offset, size_t count)
#  else
static int sf_direct_IO(int rw, struct inode *inode, struct kiobuf *, unsigned long, int)
#  endif
{
	TRACE();
	return -EINVAL;
}
# endif

struct address_space_operations sf_reg_aops = {
	.readpage = sf_readpage,
	.writepage = sf_writepage,
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	.write_begin = sf_write_begin,
	.write_end = sf_write_end,
# else
	.prepare_write = simple_prepare_write,
	.commit_write = simple_commit_write,
# endif
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 10)
	.direct_IO = sf_direct_IO,
# endif
};

#endif /* LINUX_VERSION_CODE >= 2.6.0 */

