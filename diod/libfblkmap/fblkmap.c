#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/ioctl.h>
#include <linux/fd.h>
#include <linux/fiemap.h>
#include "fblkmap.h"

#ifndef FS_IOC_FIEMAP
#define FS_IOC_FIEMAP   _IOWR('f', 11, struct fiemap)
#endif

#define FIEMAP_MIS_ALIGNED          (FIEMAP_EXTENT_NOT_ALIGNED | \
				     FIEMAP_EXTENT_DATA_INLINE | \
				     FIEMAP_EXTENT_DATA_TAIL)

#define FIEMAP_UNWRITTEN            (FIEMAP_EXTENT_UNKNOWN | \
				     FIEMAP_EXTENT_DELALLOC)

#define FIEMAP_MALLOC_UNIT_MASK     0x7
#define FIEMAP_TEMP_STACK_MEM_SIZE  (64*1024)
#define FIEMAP_DISK_BLOCK_BITMAP    ~0x1FF


#ifndef min
# define min(__x, __y) (((__x) < (__y)) ? (__x) : (__y))
#endif

#undef  FBM_DEBUG
#ifdef  FBM_DEBUG
#    define fbm_dbg(fmt, ...) fprintf(stderr, "[ZCIO] " fmt, ##__VA_ARGS__)
#else
#    define fbm_dbg(fmt, ...) do {;} while(0)
#endif
#define HERE() fbm_dbg("--> HERE %s:%d\n", __func__, __LINE__)

static int fbm_fallocate(int fd, off_t offset, off_t len)
{
#ifdef USE_FALLOCATE_TO_EXTEND
	/* This is unsafe since the number of UNWRITTEN is limited. */
	return fallocate(fd, 0, offset, len);
#else
	const static char zero_buff[16 * 1024 * 1024] __attribute__ ((aligned (4096)));
	off_t aligned_offset = offset & FIEMAP_DISK_BLOCK_BITMAP;
	off_t aligned_len    = (len + (aligned_offset - offset) +
				~FIEMAP_DISK_BLOCK_BITMAP) & FIEMAP_DISK_BLOCK_BITMAP;
	ssize_t written;

	/* This is a safe method to expand a file.
	 * This code assumes that fd is opened in an O_DIRECT mode. */
	offset = aligned_offset;
	len    = aligned_len;
	while ( len > 0 &&
		(written = pwrite(fd, zero_buff,
				  min(len, sizeof(zero_buff)),
				  offset)) > 0) {
		offset += written;
		len    -= written;
	}
	return written >= 0 ? 0 : -1;
#endif /* USE_FALLOCATE_TO_EXTEND */
}

static struct fblkmap_t *fbm_realloc(struct fblkmap_t *ptr, int n)
{
	struct fblkmap_t *fbm;

	n = (n + FIEMAP_MALLOC_UNIT_MASK) & ~FIEMAP_MALLOC_UNIT_MASK;
	fbm =  realloc(ptr, n * sizeof(*ptr));
	return fbm;
}

static int check_fie_flags(struct fiemap_extent *fm_ext, int n_exts, __u32 mask)
{
	__u32 flags;
	int i;

	for (i = 0; i < n_exts; ++i) {
		flags = fm_ext[i].fe_flags & mask;
		if (flags)
			return flags;
	}

	return 0;
}

static int find_fie_hole(struct fiemap_extent *fm_ext, int n_exts,
			 __u64 start_addr, __u64 end_addr,
			 int start_idx, __u64 *hole_start, __u32 *hole_len)
{
	__u64 end_ext;
	int last_ext = n_exts - 1;
	int i = start_idx;

	/* init. */
	*hole_len = 0;

	/* is it the very first? */
	if (i == -1) {
		if (start_addr < fm_ext[0].fe_logical) {
			*hole_start = start_addr;
			end_ext     = min(end_addr, fm_ext[0].fe_logical);
			*hole_len   = end_ext - start_addr;
			goto out;
		}

		/* keep processing all the rest */
		++i;
	}

	/* find a hole in between extents */
	for(; i < last_ext; ++i) {
		end_ext = fm_ext[i].fe_logical + fm_ext[i].fe_length;
		if (end_ext != fm_ext[i+1].fe_logical) {
			*hole_start = end_ext;
			*hole_len   = fm_ext[i+1].fe_logical - end_ext;
			break;
		}
	}
	end_ext = fm_ext[last_ext].fe_logical + fm_ext[last_ext].fe_length;

	/* find a hole after a last extent */
	if (*hole_len == 0 && end_ext < end_addr) {
		*hole_start = end_ext;
		*hole_len   = end_addr - end_ext;
	}
out:
	return i + 1;
}

int fbm_get_block_map(int fd, int flags, off_t start, size_t len,
		      struct fblkmap_t **pfblkmap, int *ptotal_exts)
{
	__u64 __fiemap[FIEMAP_TEMP_STACK_MEM_SIZE / sizeof(__u64)];
	const static int __ext_count =
		(sizeof(__fiemap) - sizeof(struct fiemap)) /
		sizeof(struct fiemap_extent);
	struct fiemap *fiemap = (struct fiemap *)__fiemap;
	struct fiemap_extent *fm_ext;
	__u32 ioctl_flags = 0, ext_mask, n_exts, total_exts;
	struct fblkmap_t *fblkmap, *old_fblkmap;
	__u64 end_addr;
	int i, rc = 0, ext_count = 0, fiemap_size;

	/* init */
	*pfblkmap    = fblkmap = old_fblkmap = NULL;
	*ptotal_exts = total_exts = 0;
	end_addr     = start + len;

	/* transform to ioctl flags */
	if (flags & FBM_FLAG_XATTR)
		ioctl_flags |= FIEMAP_FLAG_XATTR;
	if (flags & FBM_FLAG_SYNC)
		ioctl_flags |= FIEMAP_FLAG_SYNC;

	/* get fiemap */
retry:
	/* do ioctl */
	do {
		fiemap->fm_start          = start;
		fiemap->fm_length         = len;
		fiemap->fm_flags          = ioctl_flags;
		fiemap->fm_mapped_extents = 0;
		fiemap->fm_extent_count   = ext_count;
		fiemap->fm_reserved       = 0;
		rc = ioctl(fd, FS_IOC_FIEMAP, (unsigned long)fiemap);
		if (rc < 0) {
			rc = -errno;
			goto err_out;
		}

		/* if we read all extents at once, go ahead */
		if (fiemap->fm_mapped_extents <= fiemap->fm_extent_count)
			break;

		/* if we could not read all extents due to buffer size,
		 * increase buffer size then retry */
		ext_count = fiemap->fm_mapped_extents;
		fiemap_size = sizeof(struct fiemap) +
			(sizeof(struct fiemap_extent) * ext_count);
		if (ext_count > __ext_count)
			fiemap = (struct fiemap*)calloc(1, fiemap_size);
		if (!fiemap) {
			errno = ENOMEM;
			rc = -ENOMEM;
			goto err_out;
		}
	} while(1);
	fm_ext = &fiemap->fm_extents[0];

	/* no extent map? */
	n_exts = fiemap->fm_mapped_extents;
	if (n_exts == 0) {
		/* force to fill the hole */
		if (flags & FBM_FLAG_ALLOC) {
			rc = fbm_fallocate(fd, start, len);
			if (rc) {
				rc = -errno;
				goto err_out;
			}
			fbm_dbg("falloc: start = %llu  len = %llu\n",
				(unsigned long long)start,
				(unsigned long long)len);
			goto retry;
		}
		/* or done */
		goto done;
	}

	/* is every extent need to be aligned or mapped? */
	ext_mask  = (flags & FBM_FLAG_ALIGN) ? FIEMAP_MIS_ALIGNED : 0;
	ext_mask |= (flags & FBM_FLAG_SYNC)  ? FIEMAP_UNWRITTEN   : 0;
	if (ext_mask) {
		rc = check_fie_flags(fm_ext, n_exts, ext_mask);
		if (rc & FIEMAP_MIS_ALIGNED) {
			rc = -EFAULT;
			goto err_out;
		}
		if (rc & FIEMAP_UNWRITTEN) {
			rc = -EBUSY;
			goto err_out;
		}
	}

	/* should an entire requested range be mapped? */
	if (flags & FBM_FLAG_ALLOC) {
		__u64 hole_start;
		__u32 hole_len;
		int hole_count = 0;
		i = -1;

		do {
			/* find a hole */
			i = find_fie_hole(fm_ext, n_exts, start, end_addr,
					  i, &hole_start, &hole_len);

			/* fill the hole */
			if (hole_len > 0) {
				rc = fbm_fallocate(fd, hole_start, hole_len);
				if (rc) {
					rc = -errno;
					goto err_out;
				}
				++hole_count;
			}

			/* done? */
			if (i >= n_exts - 1)
				break;
		} while (1);

		/* if we filled one or more holes, re-get the block map. */
		if (hole_count)
			goto retry;
	}
done:

	/* we have no extents. */
	if (n_exts == 0) {
		rc = 0;
		goto succ_out;
	}

	/* we have something. */
	old_fblkmap = fblkmap;
	fblkmap = fbm_realloc(old_fblkmap, n_exts);
	if (fblkmap == NULL) {
		rc = -ENOMEM;
		errno = ENOMEM;
		goto err_out;
	}

	for (i = 0; i < n_exts; ++i) {
		struct fblkmap_t *m = &fblkmap[total_exts + i];
		m->logical_addr     = fm_ext[i].fe_logical;
		m->physical_addr    = fm_ext[i].fe_physical;
		m->len              = fm_ext[i].fe_length;
		m->flags            = fm_ext[i].fe_flags;
	}
	total_exts += n_exts;

 succ_out:
	*pfblkmap = fblkmap;
	*ptotal_exts = total_exts;
	return rc;
 err_out:
	free(old_fblkmap);
	if ((void *)fiemap != (void *)__fiemap)
		free(fiemap);
	return rc;
}

void fbm_print_block_map(FILE *fd, int n_exts, struct fblkmap_t *fblkmap)
{
	int i;

	for (i = 0; i < n_exts; ++i) {
		struct fblkmap_t *f = &fblkmap[i];
		fprintf(fd, "%d [%llx..%llx) --> [%llx..%llx) len: %d flag: 0x%x\n",
		       i,
		       f->logical_addr,  f->logical_addr + f->len,
		       f->physical_addr, f->physical_addr + f->len,
		       f->len, f->flags);
	}
}

static int __try_append_block_maps(struct fblkmap_t *fbm_l, int cnt_l,
				   struct fblkmap_t *fbm_r, int cnt_r,
				   struct fblkmap_t **pfbm3, int *pcnt3)
{
	struct fblkmap_t *fbm_l_last;
	__u64 dist, end_laddr_l, end_paddr_l, end_laddr_r;
	int cnt3;

	/* if there is overlap, check a bit more */
	fbm_l_last = &fbm_l[cnt_l - 1];
	if (fbm_r->logical_addr < fbm_l_last->logical_addr) {
		/* non-appendable overlap spanning two or more extents */
		return 0;
	}
	end_laddr_l = fbm_l_last->logical_addr + fbm_l_last->len;
	dist = fbm_r->logical_addr - fbm_l_last->logical_addr;
	if (fbm_r->logical_addr < end_laddr_l &&
	    fbm_r->physical_addr == (fbm_l_last->physical_addr + dist)) {
		/* two are partially overlapped but contiguous */
		cnt3 = cnt_l + cnt_r - 1;
		if (cnt_r > 1) {
			fbm_l = fbm_realloc(fbm_l, cnt3);
			if (!fbm_l)
				goto nomem_out;
		}
		end_laddr_r = fbm_r[0].logical_addr + fbm_r[0].len;
		fbm_l_last->len = end_laddr_r - fbm_l_last->logical_addr;
		fbm_dbg("len = %d\n", fbm_l_last->len);
		if (cnt_r > 1)
			memcpy(&fbm_l[cnt_l], &fbm_r[1], sizeof(*fbm_r) * (cnt_r - 1));
		goto append_out;
	}

	/* if two are discontiguous, just append them */
	end_paddr_l = fbm_l_last->physical_addr + fbm_l_last->len;
	if (end_laddr_l <  fbm_r->logical_addr ||
	    end_paddr_l != fbm_r->physical_addr) {
		cnt3 = cnt_l + cnt_r;
		fbm_l = fbm_realloc(fbm_l, cnt3);
		if (!fbm_l)
			goto nomem_out;
		memcpy(&fbm_l[cnt_l], fbm_r, sizeof(*fbm_r) * cnt_r);
		goto append_out;
	}

	/* here, two are contiguous */
	cnt3 = cnt_l + cnt_r - 1;
	if (cnt_r > 1) {
		fbm_l = fbm_realloc(fbm_l, cnt3);
		if (!fbm_l)
			goto nomem_out;
	}
	fbm_l_last->len += fbm_r[0].len;
	if (cnt_r > 1)
		memcpy(&fbm_l[cnt_l], &fbm_r[1], sizeof(*fbm_r) * (cnt_r - 1));
append_out:
	free(fbm_r);
	*pfbm3 = fbm_l;
	*pcnt3 = cnt3;
	return 1;
nomem_out:
	errno = ENOMEM;
	return -ENOMEM;
}

static int try_append_block_maps(struct fblkmap_t *fbm1, int cnt1,
				 struct fblkmap_t *fbm2, int cnt2,
				 struct fblkmap_t **pfbm3, int *pcnt3)
{
	if (cnt1 == 0) {
		*pfbm3 = fbm2;
		*pcnt3 = cnt2;
		return 1;
	}
	else if (cnt2 == 0) {
		*pfbm3 = fbm1;
		*pcnt3 = cnt1;
		return 1;
	}
	else if (fbm1->logical_addr < fbm2->logical_addr)
		return __try_append_block_maps(fbm1, cnt1, fbm2, cnt2,
					       pfbm3, pcnt3);
	else if (fbm1->logical_addr == fbm2->logical_addr &&
		 fbm1->len < fbm2->len) {
		return __try_append_block_maps(fbm1, cnt1, fbm2, cnt2,
					       pfbm3, pcnt3);
	}
	else
		return __try_append_block_maps(fbm2, cnt2, fbm1, cnt1,
					       pfbm3, pcnt3);
}

static void merge_two_regions(struct fblkmap_t *fbm_l, int *c_l,
			      struct fblkmap_t *fbm_r, int *c_r,
			      struct fblkmap_t *fbm_m, int *c_m)
{
	__u64 dist, end_laddr_l, end_laddr_r;

	/* copy the left to the merged one */
	*fbm_m = *fbm_l;
	*c_l += 1;

	/* <--- l --->    */
	/*  <--- r ?      */
	end_laddr_l = fbm_l->logical_addr + fbm_l->len;
	dist = fbm_r->logical_addr - fbm_l->logical_addr;
	if (fbm_r->logical_addr <= end_laddr_l &&
	    fbm_r->physical_addr == (fbm_l->physical_addr + dist)) {
		end_laddr_r = fbm_r->logical_addr + fbm_r->len;

		/* <--- l --->    */
		/*  <--- r -----> */
		if (end_laddr_l < end_laddr_r)
			fbm_m->len = end_laddr_r - fbm_l->logical_addr;
		/* <--- l --->    */
		/*  <--- r ->     */
		else
			; /* do nothing */
		*c_r += 1;
	}
}

static int comp_fblkmap_sorting(const void *_x, const void *_y)
{
	struct fblkmap_t *x = (struct fblkmap_t *)_x;
	struct fblkmap_t *y = (struct fblkmap_t *)_y;
	__u64 x_s = x->logical_addr;
	__u64 y_s = y->logical_addr;

	if (x_s == y_s)
		return 0;

	if (x_s < y_s)
		return -1;

	if (y_s < x_s)
		return 1;

	/* it is for gcc happy. */
	return 0;
}

static int merge_block_maps(struct fblkmap_t *fbm1, int cnt1,
			    struct fblkmap_t *fbm2, int cnt2,
			    struct fblkmap_t **pfbm3, int *pcnt3)
{
	struct fblkmap_t *fbm3;
	int c1, c2, c3, cnt3, rc;

	/* first, try to append two block maps */
	rc = try_append_block_maps(fbm1, cnt1, fbm2, cnt2, pfbm3, pcnt3);
	if (rc == 1)
		return 0;
	else if (rc < 0)
		return -errno;

	fbm_dbg("==> SLOW PATH\n");
	/* alloc for a merged fbm */
	cnt3 = cnt1 + cnt2; /* simply allocate the maximum possible */
	fbm3 = fbm_realloc(NULL, cnt3);
	if (!fbm3) {
		errno = ENOMEM;
		return -ENOMEM;
	}

	/* merge fbm1 and fbm2 into fbm3 */
	for(c1 = c2 = c3 = 0; c1 < cnt1 || c2 < cnt2; ++c3) {
		if (c1 >= cnt1)
			fbm3[c3] = fbm2[c2++];
		else if (c2 >= cnt2)
			fbm3[c3] = fbm1[c1++];
		else if (fbm1[c1].logical_addr < fbm2[c2].logical_addr)
			merge_two_regions(&fbm1[c1], &c1, &fbm2[c2], &c2,
					  &fbm3[c3], &c3);
		else
			merge_two_regions(&fbm2[c2], &c2, &fbm1[c1], &c1,
					  &fbm3[c3], &c3);
	}
	free(fbm1);
	free(fbm2);

	/* sort fbm3 */
	qsort(fbm3, c3, sizeof(fbm3[0]), comp_fblkmap_sorting);

	/* pass out results */
	*pfbm3 = fbm3;
	*pcnt3 = c3;
	return 0;
}

static int comp_fblkmap_overlapping(const void *_x, const void *_y)
{
	struct fblkmap_t *x = (struct fblkmap_t *)_x;
	struct fblkmap_t *y = (struct fblkmap_t *)_y;
	__u64 x_s = x->logical_addr;
	__u64 y_s = y->logical_addr;
	__u64 x_e = x->logical_addr + x->len - 1;
	__u64 y_e = y->logical_addr + y->len - 1;

	/* equality testing == overlapping testing */
	if (x_s == y_s)
		return 0;

	if (x_s < y_s) {
		if (y_s <= x_e)
			return 0;
		return -1;
	}

	if (y_s < x_s) {
		if (x_s <= y_e)
			return 0;
		return 1;
	}

	/* it is for gcc happy. */
	return 0;
}

int fbm_find_block_mapping(struct fblkmap_t *fblkmap, int n_exts, __u64 start)
{
	struct fblkmap_t key, *res;

	/* sanity check */
	if (n_exts == 0 || fblkmap == NULL)
		goto err_out;

	/* lookup */
	key.logical_addr = start;
	key.len          = 1;
	res = bsearch(&key, fblkmap, n_exts, sizeof(fblkmap[0]),
		      comp_fblkmap_overlapping);
	if (res)
		return res - fblkmap;
err_out:
	return -1;
}

int fbm_find_block_mapping_force(int fd, int flags,
				 struct fblkmap_t **pfblkmap, int *ptotal_exts,
				 off_t start, size_t len)
{
	struct fblkmap_t *req_fblkmap, *merged_fblkmap;
	__u64 ext_end_addr, req_end_addr;
	int rc, idx, req_total_exts, merged_exts;

	/* look up block map */
  lookup:
	idx = fbm_find_block_mapping(*pfblkmap, *ptotal_exts, start);
	if (idx == -1)
		goto alloc_out;

	/* check whether the found extent
	   completely covers the requested region. */
	req_end_addr = start + len;
	ext_end_addr = (*pfblkmap)[idx].logical_addr + (*pfblkmap)[idx].len;
	if (ext_end_addr < req_end_addr) {
		len  -= (ext_end_addr - start);
		start = ext_end_addr;
		goto alloc_out;
	}

	/* bingo */
	return idx;

  alloc_out:
	/* allocate requested region */
	rc = fbm_get_block_map(fd, flags | FBM_FLAG_ALLOC,
			       start, len,
			       &req_fblkmap, &req_total_exts);
	if (rc) {
		errno = rc;
		goto err_out;
	}

	/* there was no existing extent */
	if (*pfblkmap == NULL) {
		*pfblkmap = req_fblkmap;
		*ptotal_exts = req_total_exts;
		goto lookup;
	}

	/* if there was, merge two extents */
	rc = merge_block_maps(*pfblkmap, *ptotal_exts,
			      req_fblkmap, req_total_exts,
			      &merged_fblkmap, &merged_exts);
	if (rc) goto err_out;

	*pfblkmap = merged_fblkmap;
	*ptotal_exts = merged_exts;
	goto lookup;

  err_out:
	return errno;
}
