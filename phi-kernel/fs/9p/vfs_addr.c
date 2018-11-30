/*
 *  linux/fs/9p/vfs_addr.c
 *
 * This file contians vfs address (mmap) ops for 9P2000.
 *
 *  Copyright (C) 2005 by Eric Van Hensbergen <ericvh@gmail.com>
 *  Copyright (C) 2002 by Ron Minnich <rminnich@lanl.gov>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 *  Free Software Foundation
 *  51 Franklin Street, Fifth Floor
 *  Boston, MA  02111-1301  USA
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/inet.h>
#include <linux/pagemap.h>
#include <linux/idr.h>
#include <linux/sched.h>
#include <linux/aio.h>
#ifdef CONFIG_9P_FS_BULK_PAGE_IO
#include <linux/task_io_accounting_ops.h>
#include <linux/slab.h>
#endif
#include <net/9p/9p.h>
#include <net/9p/client.h>

#include "v9fs.h"
#include "v9fs_vfs.h"
#include "cache.h"
#include "fid.h"

/**
 * v9fs_fid_readpage - read an entire page in from 9P
 *
 * @fid: fid being read
 * @page: structure to page
 *
 */
static int v9fs_fid_readpage(struct p9_fid *fid, struct page *page)
{
	int retval;
	loff_t offset;
	char *buffer;
	struct inode *inode;

	inode = page->mapping->host;
	p9_debug(P9_DEBUG_VFS, "\n");

	BUG_ON(!PageLocked(page));
	retval = v9fs_readpage_from_fscache(inode, page);
	if (retval == 0)
		return retval;

	buffer = kmap(page);
	offset = page_offset(page);

	retval = v9fs_fid_readn(fid, buffer, NULL, PAGE_CACHE_SIZE, offset);
	if (retval < 0) {
		v9fs_uncache_page(inode, page);
		goto done;
	}

	memset(buffer + retval, 0, PAGE_CACHE_SIZE - retval);
	flush_dcache_page(page);
	SetPageUptodate(page);

	v9fs_readpage_to_fscache(inode, page);
	retval = 0;

done:
	kunmap(page);
	unlock_page(page);
	return retval;
}


#ifdef CONFIG_9P_FS_BULK_PAGE_IO
#define list_to_page(head) (list_entry((head)->prev, struct page, lru))

static int v9fs_read_cache_pages(struct p9_fid *fid,
                                 struct address_space *mapping,
                                 struct list_head *pages,
                                 unsigned nr_pages)
{
        struct inode *inode;
        struct page *page = NULL;
        ssize_t ret = 0, index = 0, read_size;
        struct page **page_array = NULL;
        loff_t offset;


        page_array = kmalloc(sizeof(struct page *) * nr_pages, GFP_NOFS);
        if (!page_array)
                return -ENOMEM;

        /* check whether page belong to the v9fs_readpage_from_fscache */
        while(!list_empty(pages)) {
                page = list_to_page(pages);
		/* BUG_ON(!PageLocked(page)); */

                list_del(&page->lru);
                if (add_to_page_cache_lru(page, mapping,
                                          page->index, GFP_KERNEL)) {
                        read_cache_pages_invalidate_page(mapping, page);
			unlock_page(page);
			page_cache_release(page);
                        continue;
                }
                page_array[index++] = page;
        }

        /* time to read data here */
	inode     = page_array[0]->mapping->host;
        offset    = page_offset(page_array[0]);
	read_size = index * PAGE_SIZE;
        ret       = v9fs_fid_bulk_readn(fid, page_array, index, offset);
        if (unlikely(ret != read_size)) {
		/* invalidate pages */
                read_cache_pages_invalidate_pages(mapping, pages);

		/* unlock & release pages */
		nr_pages = index;
		for (index = 0; index < nr_pages; ++index) {
			page = page_array[index];
			unlock_page(page);
			page_cache_release(page);
		}
                goto err_out;
        }

        /*
         * Everything is present, now clean up the mess
         */
        nr_pages = index;
        for (index = 0; index < nr_pages; ++index) {
                page = page_array[index];
                flush_dcache_page(page);
                SetPageUptodate(page);

                v9fs_readpage_to_fscache(inode, page);
                unlock_page(page);
                page_cache_release(page);
        }
        task_io_account_read(PAGE_SIZE * nr_pages);
        ret = 0;

     err_out:
        kfree(page_array);
        return ret;
}
#endif

/**
 * v9fs_vfs_readpage - read an entire page in from 9P
 *
 * @filp: file being read
 * @page: structure to page
 *
 */

static int v9fs_vfs_readpage(struct file *filp, struct page *page)
{
	return v9fs_fid_readpage(filp->private_data, page);
}

/**
 * v9fs_vfs_readpages - read a set of pages from 9P
 *
 * @filp: file being read
 * @mapping: the address space
 * @pages: list of pages to read
 * @nr_pages: count of pages to read
 *
 */

static int v9fs_vfs_readpages(struct file *filp, struct address_space *mapping,
			     struct list_head *pages, unsigned nr_pages)
{
	int ret = 0;
	struct inode *inode;

	inode = mapping->host;
	p9_debug(P9_DEBUG_VFS, "inode: %p file: %p\n", inode, filp);

	ret = v9fs_readpages_from_fscache(inode, mapping, pages, &nr_pages);
	if (ret == 0)
		return ret;

#ifndef CONFIG_9P_FS_BULK_PAGE_IO
	ret = read_cache_pages(mapping, pages,
			       (void *)v9fs_vfs_readpage, filp);
#else
	ret = v9fs_read_cache_pages(filp->private_data, mapping,
				    pages, nr_pages);
#endif
	p9_debug(P9_DEBUG_VFS, "  = %d\n", ret);
	return ret;
}

/**
 * v9fs_release_page - release the private state associated with a page
 *
 * Returns 1 if the page can be released, false otherwise.
 */

static int v9fs_release_page(struct page *page, gfp_t gfp)
{
	if (PagePrivate(page))
		return 0;
	return v9fs_fscache_release_page(page, gfp);
}

/**
 * v9fs_invalidate_page - Invalidate a page completely or partially
 *
 * @page: structure to page
 * @offset: offset in the page
 */

static void v9fs_invalidate_page(struct page *page, unsigned long offset)
{
	/*
	 * If called with zero offset, we should release
	 * the private state assocated with the page
	 */
	if (offset == 0)
		v9fs_fscache_invalidate_page(page);
}

static int v9fs_vfs_writepage_locked(struct page *page)
{
	char *buffer;
	int retval, len;
	loff_t offset, size;
	mm_segment_t old_fs;
	struct v9fs_inode *v9inode;
	struct inode *inode = page->mapping->host;

	v9inode = V9FS_I(inode);
	size = i_size_read(inode);
	if (page->index == size >> PAGE_CACHE_SHIFT)
		len = size & ~PAGE_CACHE_MASK;
	else
		len = PAGE_CACHE_SIZE;

	set_page_writeback(page);

	buffer = kmap(page);
	offset = page_offset(page);

	old_fs = get_fs();
	set_fs(get_ds());
	/* We should have writeback_fid always set */
	BUG_ON(!v9inode->writeback_fid);

	retval = v9fs_file_write_internal(inode,
					  v9inode->writeback_fid,
					  (__force const char __user *)buffer,
					  NULL,
					  len, &offset, 0);
	if (retval > 0)
		retval = 0;

	set_fs(old_fs);
	kunmap(page);
	end_page_writeback(page);
	return retval;
}

static int v9fs_vfs_writepage(struct page *page, struct writeback_control *wbc)
{
	int retval;

	p9_debug(P9_DEBUG_VFS, "page %p\n", page);

	retval = v9fs_vfs_writepage_locked(page);
	if (retval < 0) {
		if (retval == -EAGAIN) {
			redirty_page_for_writepage(wbc, page);
			retval = 0;
		} else {
			SetPageError(page);
			mapping_set_error(page->mapping, retval);
		}
	} else
		retval = 0;

	unlock_page(page);
	return retval;
}

#ifdef CONFIG_9P_FS_BULK_PAGE_IO
static int v9fs_vfs_writepages_locked(struct p9_fid *fid,
				      struct address_space *mapping,
				      struct page **pages,
				      unsigned nr_pages)
{
        loff_t offset = page_offset(pages[0]);
	mm_segment_t old_fs;
	struct page *page;
	int rc, i;

	p9_debug(P9_DEBUG_VFS, "pages %p   nr_pages %d\n", pages, nr_pages);

	/* tell the pages will be written soon */
        for (i = 0; i < nr_pages; ++i) {
		page = pages[i];
		BUG_ON(!PageLocked(page));
		set_page_writeback(page);
	}

	/* write pages */
	old_fs = get_fs();
	set_fs(get_ds());

	rc = v9fs_fid_bulk_writen(fid, pages, nr_pages, offset);
	if (unlikely(rc < 0))
                goto err_out;
	BUG_ON(rc != (nr_pages * PAGE_SIZE));
	rc = 0;
err_out:
	/* unlock pages */
	set_fs(old_fs);
        for (i = 0; i < nr_pages; ++i) {
		page = pages[i];
		end_page_writeback(page);
		unlock_page(page);
	}
        return rc;
}

int v9fs_vfs_writepages(struct address_space *mapping,
			struct writeback_control *wbc)
{
	enum {
		MAX_WRITE_PAGES = (16 * 1024 * 1024) / PAGE_SIZE,
	};
	struct inode *inode = mapping->host;
	struct v9fs_inode *v9inode = V9FS_I(inode);
	struct p9_fid *fid = v9inode->writeback_fid;
	int ret = 0;
	int done = 0;
	struct pagevec *pvec = NULL;
	int nr_pages;
	struct page **dirty_pages;
	int nr_dirty_pages;
	pgoff_t uninitialized_var(writeback_index);
	pgoff_t index;
	pgoff_t end;		/* Inclusive */
	pgoff_t done_index;
	int cycled;
	int range_whole = 0;
	int tag;

	/* init pvec */
	pvec = kmalloc(sizeof(*pvec) +
		       (sizeof(struct page *) * MAX_WRITE_PAGES),
		       GFP_NOFS);
	if (!pvec)
		return -ENOMEM;
	pagevec_init(pvec, 0);

	if (wbc->range_cyclic) {
		writeback_index = mapping->writeback_index; /* prev offset */
		index = writeback_index;
		if (index == 0)
			cycled = 1;
		else
			cycled = 0;
		end = -1;
	} else {
		index = wbc->range_start >> PAGE_CACHE_SHIFT;
		end = wbc->range_end >> PAGE_CACHE_SHIFT;
		if (wbc->range_start == 0 && wbc->range_end == LLONG_MAX)
			range_whole = 1;
		cycled = 1; /* ignore range_cyclic tests */
	}
	if (wbc->sync_mode == WB_SYNC_ALL)
		tag = PAGECACHE_TAG_TOWRITE;
	else
		tag = PAGECACHE_TAG_DIRTY;
retry:
	if (wbc->sync_mode == WB_SYNC_ALL)
		tag_pages_for_writeback(mapping, index, end);
	done_index = index;
	while (!done && (index <= end)) {
		int i;

		nr_pages = pagevec_lookup_tag(pvec, mapping, &index, tag,
			      min(end - index, (pgoff_t)MAX_WRITE_PAGES-1) + 1);
		if (nr_pages == 0)
			break;
		dirty_pages = pvec->pages;
		nr_dirty_pages = 0;

		for (i = 0; i < nr_pages; i++) {
			struct page *page = pvec->pages[i];

			/*
			 * At this point, the page may be truncated or
			 * invalidated (changing page->mapping to NULL), or
			 * even swizzled back from swapper_space to tmpfs file
			 * mapping. However, page->index will not change
			 * because we have a reference on the page.
			 */
			if (page->index > end) {
				/*
				 * can't be range_cyclic (1st pass) because
				 * end == -1 in that case.
				 */
				done = 1;
				break;
			}

			done_index = page->index + 1;

			/* grab the page lock */
			lock_page(page);

			/*
			 * Page truncated or invalidated. We can freely skip it
			 * then, even for data integrity operations: the page
			 * has disappeared concurrently, so there could be no
			 * real expectation of this data interity operation
			 * even if there is now a new, dirty page at the same
			 * pagecache address.
			 */
			if (unlikely(page->mapping != mapping)) {
continue_unlock:
				BUG_ON(!PageLocked(page));
				unlock_page(page);
flush_dirty_pages:
				/* flush out a contiguous range of dirty pages */
				if (nr_dirty_pages) {
					ret = v9fs_vfs_writepages_locked(
						fid, mapping, dirty_pages, nr_dirty_pages);
					dirty_pages    = &dirty_pages[nr_dirty_pages];
					nr_dirty_pages = 0;
					if (unlikely(ret)) {
						done = 1;
						break;
					}
				}
				continue;
			}

			if (!PageDirty(page)) {
				/* someone wrote it for us */
				goto continue_unlock;
			}

			if (PageWriteback(page)) {
				if (wbc->sync_mode != WB_SYNC_NONE)
					wait_on_page_writeback(page);
				else
					goto continue_unlock;
			}

			BUG_ON(PageWriteback(page));
			if (!clear_page_dirty_for_io(page))
				goto continue_unlock;

			/* Increase write-drity page count */
			++nr_dirty_pages;

			/*
			 * We stop writing back only if we are not doing
			 * integrity sync. In case of integrity sync we have to
			 * keep going until we have written all the pages
			 * we tagged for writeback prior to entering this loop.
			 */
			if (--wbc->nr_to_write <= 0 &&
			    wbc->sync_mode == WB_SYNC_NONE) {
				done = 1;
				break;
			}
		}

		/* flush out a contiguous range of dirty pages */
		if (nr_dirty_pages) {
			ret = v9fs_vfs_writepages_locked(
				fid, mapping, dirty_pages, nr_dirty_pages);
			if (unlikely(ret)) {
				done = 1;
				break;
			}
		}

		pagevec_release(pvec);
		cond_resched();
	}
	if (!cycled && !done) {
		/*
		 * range_cyclic:
		 * We hit the last page and there is more work to be done: wrap
		 * back to the start of the file
		 */
		cycled = 1;
		index = 0;
		end = writeback_index - 1;
		goto retry;
	}
	if (wbc->range_cyclic || (range_whole && wbc->nr_to_write > 0))
		mapping->writeback_index = done_index;

	kfree(pvec);
	return ret;
}
#endif

/**
 * v9fs_launder_page - Writeback a dirty page
 * Returns 0 on success.
 */

static int v9fs_launder_page(struct page *page)
{
	int retval;
	struct inode *inode = page->mapping->host;

	v9fs_fscache_wait_on_page_write(inode, page);
	if (clear_page_dirty_for_io(page)) {
		retval = v9fs_vfs_writepage_locked(page);
		if (retval)
			return retval;
	}
	return 0;
}

/**
 * v9fs_direct_IO - 9P address space operation for direct I/O
 * @rw: direction (read or write)
 * @iocb: target I/O control block
 * @iov: array of vectors that define I/O buffer
 * @pos: offset in file to begin the operation
 * @nr_segs: size of iovec array
 *
 * The presence of v9fs_direct_IO() in the address space ops vector
 * allowes open() O_DIRECT flags which would have failed otherwise.
 *
 * In the non-cached mode, we shunt off direct read and write requests before
 * the VFS gets them, so this method should never be called.
 *
 * Direct IO is not 'yet' supported in the cached mode. Hence when
 * this routine is called through generic_file_aio_read(), the read/write fails
 * with an error.
 *
 */
static ssize_t
v9fs_direct_IO(int rw, struct kiocb *iocb, const struct iovec *iov,
	       loff_t pos, unsigned long nr_segs)
{
	/*
	 * FIXME
	 * Now that we do caching with cache mode enabled, We need
	 * to support direct IO
	 */
	p9_debug(P9_DEBUG_VFS, "v9fs_direct_IO: v9fs_direct_IO (%s) off/no(%lld/%lu) EINVAL\n",
		 iocb->ki_filp->f_path.dentry->d_name.name,
		 (long long)pos, nr_segs);

	return -EINVAL;
}

static int v9fs_write_begin(struct file *filp, struct address_space *mapping,
			    loff_t pos, unsigned len, unsigned flags,
			    struct page **pagep, void **fsdata)
{
	int retval = 0;
	struct page *page;
	struct v9fs_inode *v9inode;
	pgoff_t index = pos >> PAGE_CACHE_SHIFT;
	struct inode *inode = mapping->host;


	p9_debug(P9_DEBUG_VFS, "filp %p, mapping %p\n", filp, mapping);

	v9inode = V9FS_I(inode);
start:
	page = grab_cache_page_write_begin(mapping, index, flags);
	if (!page) {
		retval = -ENOMEM;
		goto out;
	}
	BUG_ON(!v9inode->writeback_fid);
	if (PageUptodate(page))
		goto out;

	if (len == PAGE_CACHE_SIZE)
		goto out;

	retval = v9fs_fid_readpage(v9inode->writeback_fid, page);
	page_cache_release(page);
	if (!retval)
		goto start;
out:
	*pagep = page;
	return retval;
}

static int v9fs_write_end(struct file *filp, struct address_space *mapping,
			  loff_t pos, unsigned len, unsigned copied,
			  struct page *page, void *fsdata)
{
	loff_t last_pos = pos + copied;
	struct inode *inode = page->mapping->host;

	p9_debug(P9_DEBUG_VFS, "filp %p, mapping %p\n", filp, mapping);

	if (unlikely(copied < len)) {
		/*
		 * zero out the rest of the area
		 */
		unsigned from = pos & (PAGE_CACHE_SIZE - 1);

		zero_user(page, from + copied, len - copied);
		flush_dcache_page(page);
	}

	if (!PageUptodate(page))
		SetPageUptodate(page);
	/*
	 * No need to use i_size_read() here, the i_size
	 * cannot change under us because we hold the i_mutex.
	 */
	if (last_pos > inode->i_size) {
		inode_add_bytes(inode, last_pos - inode->i_size);
		i_size_write(inode, last_pos);
	}
	set_page_dirty(page);
	unlock_page(page);
	page_cache_release(page);

	return copied;
}


const struct address_space_operations v9fs_addr_operations = {
	.readpage = v9fs_vfs_readpage,
	.readpages = v9fs_vfs_readpages,
#ifdef CONFIG_9P_FS_BULK_PAGE_IO
	.writepages = v9fs_vfs_writepages,
#endif
	.set_page_dirty = __set_page_dirty_nobuffers,
	.writepage = v9fs_vfs_writepage,
	.write_begin = v9fs_write_begin,
	.write_end = v9fs_write_end,
	.releasepage = v9fs_release_page,
	.invalidatepage = v9fs_invalidate_page,
	.launder_page = v9fs_launder_page,
	.direct_IO = v9fs_direct_IO,
};
