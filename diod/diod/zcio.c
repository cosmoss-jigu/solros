#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/fsuid.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <fcntl.h>
#include <utime.h>
#include <stdarg.h>
#include <linux/nvme.h>

#include "9p.h"
#include "npfs.h"
#include "list.h"
#include "hash.h"
#include "hostlist.h"
#include "xpthread.h"

#include "diod_conf.h"
#include "diod_log.h"

#include "ioctx.h"
#include "xattr.h"
#include "fid.h"
#include "ops.h"
#include "fblkmap.h"

#define NVME_BLK_SIZE             512   /* default nvme block size */
#define NVME_BLK_SIZE_MASK     ~0x1ff
#define NVME_BLK_SIZE_SHIFT         9
#define NVME_MAX_DMA_SIZE   (256*512)   /* max DMA size
				         * - Intel DC3700:   256 sectors
				         * - PMC:         65,536 sectors */
#define NVME_IOV_BUFF_SIZE (1024*1024)  /* > 100MB */

#define nvme_addr_to_lba(a)     ((a) >> NVME_BLK_SIZE_SHIFT) /* XXX: not support a disk partition */
#define len_to_nvme_nblk(l)     (((l) >> NVME_BLK_SIZE_SHIFT) - 1)
#define is_nvme_blk_aligned(x)  (!((x) & ~NVME_BLK_SIZE_MASK))
#define round_up_to_nvme_blk(x) (((x) + ~NVME_BLK_SIZE_MASK) & NVME_BLK_SIZE_MASK)

#define max(x, y)               (((x) > (y)) ? (x) : (y))
#define min(x, y)               (((x) < (y)) ? (x) : (y))

#undef  ZCIO_DEBUG
#undef  ZCIO_DEBUG_DUMP_IOV
#ifdef  ZCIO_DEBUG
#    define zcio_dbg(fmt, ...)  fprintf(stderr, "[ZCIO] " fmt, ##__VA_ARGS__)
#else
#    define zcio_dbg(fmt, ...)  do {;} while(0)
#endif
#define HERE() zcio_dbg("--> HERE %s:%d\n", __func__, __LINE__)

struct nvme_cmd_build_opt {
	/* what you want */
	u8                opcode;
	size_t            count;
	off_t             offset;

	/* file offset to nmve block map */
	u32             n_fblkmap;
	struct fblkmap_t *fblkmap;

	/* xeon phi physical memory map */
	u32             n_memiov;
	struct p9_zc_io  *memiov;

	/* output: nvme user io comand vector */
	u32                n_iov;
	u32                iov_size;
	struct nvme_user_io *iov;
	size_t               io_count;
};

static __thread long __nvme_iov_buff[NVME_IOV_BUFF_SIZE/sizeof(long)];
static __thread int  __nvme_fd;

static inline
int open_nvme_dev(int file_fd)
{
	/* XXX: we should get from filefd.stat.st_dev. */
	if (!__nvme_fd) {
		char *nvme = diod_conf_get_dev_nvme();
		__nvme_fd = open(nvme, O_RDWR);
	}
	return __nvme_fd;
}

static inline
void close_nvme_dev(int file_fd)
{
	/* XXX: we should get from filefd.stat.st_dev. */
	/* XXX: do nothing */
}

static inline
int calc_n_iov(struct nvme_cmd_build_opt *opt)
{
	/* simply allocate the maximum */
	/* +1 for carry over, +2 sector alignment */
	return opt->n_fblkmap + (opt->count / NVME_MAX_DMA_SIZE) + 1 + 2;
}

static inline
int calc_sgl_mem_size(struct nvme_cmd_build_opt *opt)
{
	/* simply allocate the maximum */
	const static int u =
		sizeof(struct nvme_user_sglist) +
		sizeof(struct nvme_user_sg);
	/* +1 for carry over, +2 sector alignment */
	int p = (opt->count / PAGE_SIZE) + 1 + 2;
	return u * p;
}

static struct nvme_user_io *
calloc_iov(struct nvme_cmd_build_opt *opt, int n_iov)
{
	int iov_alloc_size;
	struct nvme_user_io *iov;

	/* calc. allocation size */
	iov_alloc_size = sizeof(struct nvme_user_io) * n_iov +
		calc_sgl_mem_size(opt);

	/* alloc */
	if (iov_alloc_size >= NVME_IOV_BUFF_SIZE)
		iov = (struct nvme_user_io *)__nvme_iov_buff;
	else
		iov = malloc(iov_alloc_size);

	/* clear */
	memset(iov, 0, sizeof(*iov) * n_iov);
	return iov;
}

static void
free_iov(struct nvme_user_io *iov)
{
	if ((void *)iov != (void *)__nvme_iov_buff)
		free(iov);
}

static int
build_nvme_uio_cmds(struct nvme_cmd_build_opt *opt)
{
	struct nvme_user_io *iov = NULL, *io;
	struct nvme_user_sglist *sgl;
	struct fblkmap_t *f;
	struct p9_zc_io *m;
	u64 file_offset, nvme_addr, mem_addr;
	u32 req_len, ext_len, mem_len, iov_len, sg_len, n_iov;
	int fi, ii, mi, si, rc;

	/* we assume that the file offset is block-aligned
	 * but io->count is not. */
	if (!is_nvme_blk_aligned(opt->offset)) {
		fprintf(stderr,
			"[P2P-ERR] Unaligned IO is not supported: "
			"offset: %lld   count: %lld\n",
			(long long int)opt->offset, (long long int)opt->count);
		rc = -EINVAL;
		goto err_out;
	}
	if (!is_nvme_blk_aligned(opt->count)) {
		opt->count = round_up_to_nvme_blk(opt->count);
	}
	NP_ASSERT( is_nvme_blk_aligned(opt->count) );

	file_offset = opt->offset;
	req_len     = opt->count;

	/* alloc iov */
	n_iov = calc_n_iov(opt);
	iov = calloc_iov(opt, n_iov);
	if (!iov) {
		rc = -ENOMEM;
		goto err_out;
	}
	sgl = (struct nvme_user_sglist *)&iov[n_iov];

	/* build iov */
	/* - init control variables */
	/*   - xeon phi memory vector */
	m           = &(opt->memiov[0]);
	mem_addr    = m->addr;
	mem_len     = m->len;
	mem_len     = round_up_to_nvme_blk(mem_len);
	NP_ASSERT( is_nvme_blk_aligned(mem_len) );

	/*   - nvme block vector */
	f           = &(opt->fblkmap[0]);
	nvme_addr   = f->physical_addr + (file_offset - f->logical_addr);
	ext_len     = f->len - (file_offset - f->logical_addr);
	ext_len     = round_up_to_nvme_blk(ext_len);
	zcio_dbg("f->physical_addr = %lld\n",
		 (unsigned long long int)f->physical_addr);
	zcio_dbg("f->logical_addr = %lld\n",
		 (unsigned long long int)f->logical_addr);
	zcio_dbg("f->len = %lld\n",
		 (unsigned long long int)f->len);
	zcio_dbg("file_offset = %lld\n",
		 (unsigned long long int)file_offset);
	NP_ASSERT( is_nvme_blk_aligned(ext_len) );

	/* - generate iov */
	for (fi = 0, ii = 0, mi = 0; ii < n_iov; ++ii) {
		NP_ASSERT(mi < opt->n_memiov);

		/* calc. io vector length */
		io = &iov[ii];
		iov_len     = min(NVME_MAX_DMA_SIZE, min(req_len, ext_len));
		iov_len     = round_up_to_nvme_blk(iov_len);

		/* build an io vector */
		io->opcode  = opt->opcode;
		io->slba    = nvme_addr_to_lba(nvme_addr);
		io->nblocks = len_to_nvme_nblk(iov_len);

		/* fill up scatter-gather list */
		io->sgl     = sgl;
		for (si = 0; 1; ++si) {
			/* generate a sg */
			sg_len = min(mem_len, iov_len);
			sgl->sg[si].len  = sg_len;
			sgl->sg[si].addr = mem_addr;

			/* update control variables */
			req_len   -= sg_len;
			iov_len   -= sg_len;
			ext_len   -= sg_len;
			mem_len   -= sg_len;
			mem_addr  += sg_len;
			nvme_addr += sg_len;

			/* move to the next memory vector */
			if (!mem_len && req_len) {
				m        = &(opt->memiov[++mi]);
				mem_addr = m->addr;
				mem_len  = m->len;

				NP_ASSERT( is_nvme_blk_aligned(mem_len) );
			}

			if (!iov_len)
				break;
		}
		sgl->nsg = si + 1;

		/* done? */
		if (!req_len) {
			sgl = (struct nvme_user_sglist *)&(sgl->sg[sgl->nsg]);
			break;
		}

		/* is a whole extent consumed? */
		if (!ext_len) {
			/* all all extents consumed? */
			if (++fi >= opt->n_fblkmap) {
				sgl = (struct nvme_user_sglist *)&(sgl->sg[sgl->nsg]);
				break;
			}

			/* if there are more extents */
			f = &(opt->fblkmap[fi]);
			nvme_addr = f->physical_addr;
			ext_len   = f->len;
			NP_ASSERT( is_nvme_blk_aligned(ext_len) );
		}

		/* update sgl for next iov */
		sgl = (struct nvme_user_sglist *)&(sgl->sg[sgl->nsg]);
	}

	/* set output values */
	opt->iov_size = (char *)sgl - (char *)iov;
	opt->iov      = iov;
	opt->n_iov    = ii + 1;
	opt->io_count = opt->count - req_len;
	return 0;
  err_out:
	/* clean up */
	free_iov(iov);
	opt->iov = NULL;
	return rc;
}

static void
dump_nvme_uio_cmds(struct nvme_cmd_build_opt *opt)
{
#ifdef ZCIO_DEBUG_DUMP_IOV
	struct nvme_user_io *iov, *io;
	struct nvme_user_sglist *sgl;
	int ii, i, n_iov;

	iov = opt->iov;
	n_iov = opt->n_iov;

	zcio_dbg("(opt->io_count %d) (opt->count %d) (opt->iov_size %d)\n",
		 (int)opt->io_count, (int)opt->count, (int)opt->iov_size);
	for (ii = 0 ; ii < n_iov; ++ii) {
		io = &iov[ii];
		sgl = io->sgl;
		zcio_dbg("nvme_uio[%d]:"
			 " (opcode %d) (slba 0x%llx) (nblocks %d) (sgl->nsg %d) \n",
			 ii, io->opcode, io->slba, io->nblocks, sgl->nsg);
		for (i = 0; i < sgl->nsg; ++i) {
			zcio_dbg("   (sg[%d] addr: 0x%llx  len: %d)\n",
				 i, sgl->sg[i].addr, sgl->sg[i].len);
			NP_ASSERT(sgl->sg[i].len <= NVME_MAX_DMA_SIZE);
		}
	}
#endif
}

static int
find_block_mapping(int fd, u8 nvme_opcode,
		   int *pn_fblkmap, struct fblkmap_t **pfblkmap,
		   size_t count, off_t offset)
{
	int fi;

	/* if it is the first time, load block map for entire range
	 * assuming that most files are not fragmented much. */
	if (*pfblkmap == NULL || *pn_fblkmap == 0) {
		int rc = fbm_get_block_map(fd, 0,
					   0, SIZE_MAX,
					   pfblkmap, pn_fblkmap);
		if (rc < 0)
			return rc;
	}

	/* find block mapping */
	switch(nvme_opcode) {
	case nvme_cmd_read:
		fi = fbm_find_block_mapping(*pfblkmap, *pn_fblkmap,
					    offset);
		break;
	case nvme_cmd_write:
		fi = fbm_find_block_mapping_force(fd, 0,
						  pfblkmap, pn_fblkmap,
						  offset, count);
		/* TODO: XXX: when and how UNWRITTEN tag is removed from ext flag? */
		break;
	default:
		return -EINVAL;
	}
	return fi;
}


static int
nvme_p2p(pthread_spinlock_t *lock, int fd, int nvme_opcode, int mic_id,
	 int *pn_fblkmap, struct fblkmap_t **pfblkmap,
	 int n_memiov, struct p9_zc_io *memiov,
	 size_t count, off_t offset)
{
	struct nvme_cmd_build_opt opt = {.iov = NULL};
	struct nvme_user_p2p_iov nvme_cmd;
	int nvme_fd = -1, fi, rc;

	/* reset errno */
	errno = 0;

	/* find block mapping */
	pthread_spin_lock(lock); {
		fi = find_block_mapping(fd, nvme_opcode,
					pn_fblkmap, pfblkmap,
					count, offset);
	} pthread_spin_unlock(lock);
	if (fi < 0) {
		if (!errno) {
			/* read on no-mapping doen not raise
			   an error */
			if (nvme_opcode == nvme_cmd_read)
				return 0;
			else
				errno = EINVAL;
		}
		goto err_out;
	}

	/* construct nvme commands
	 * - perform this read operation
	 * only for the first block-allocated extent */
	opt.opcode    = nvme_opcode;
	opt.count     = count;
	opt.offset    = offset;
	opt.n_fblkmap = *pn_fblkmap - fi;
	opt.fblkmap   = &(*pfblkmap)[fi];
	opt.n_memiov  = n_memiov;
	opt.memiov    = memiov;
	rc = build_nvme_uio_cmds(&opt);
	if (rc) {
		errno = rc;
		goto err_out;
	}
	nvme_cmd.cookie   = mic_id; /* (0:host), (1:mic0), ... */
	nvme_cmd.flags    = NVME_P2P_COOKIE_MIC_ID;
	nvme_cmd.n_iov    = opt.n_iov;
	nvme_cmd.iov_size = opt.iov_size;
	nvme_cmd.iov      = opt.iov;
	dump_nvme_uio_cmds(&opt);

	/* issue nvme commands */
	nvme_fd = open_nvme_dev(fd);
	if (nvme_fd == -1)
		goto err_out;
	rc = ioctl(nvme_fd, NVME_IOCTL_SUBMIT_P2P_IOV, &nvme_cmd);
	if (rc) {
		if (rc == -1) {
			fprintf(stderr,
				"ioctl(NVME_IOCTL_SUBMIT_P2P_IOV) "
				"failed with error: %s\n",
				strerror(errno));
		}
		else {
			fprintf(stderr,
				"ioctl(NVME_IOCTL_SUBMIT_P2P_IOV) "
				"failed with NVME error: 0x%x\n",
				rc);
		}
		goto err_out;
	}

	free_iov(opt.iov);
	close_nvme_dev(fd);

	/* Since opt.io_count can be aligned by disk block size,
	 * opt.io_count can be larger than count. So we choose
	 * the smaller one. */
	return min(opt.io_count, count);
  err_out:
	free_iov(opt.iov);
	if (nvme_fd != -1)
		close_nvme_dev(fd);
	if (!errno)
		errno = EIO;
	return -1;
}

int
pread_zc (pthread_spinlock_t *lock, int fd,
	  int *pn_fblkmap, struct fblkmap_t **pfblkmap,
	  int n_memiov, struct p9_zc_io *memiov,
	  size_t count, off_t offset)
{
	/* XXX: need to implement hole reading returning zero blocks */
	int mic_id = diod_conf_get_mic_id();
	int rc = nvme_p2p(lock, fd, nvme_cmd_read, mic_id,
			  pn_fblkmap, pfblkmap,
			  n_memiov, memiov,
			  count, offset);
	zcio_dbg("%s:%d n_memiov = %d rc = %d errno = %d\n", __func__, __LINE__,
		 n_memiov, rc, errno);
	return rc;
}

int
pwrite_zc (pthread_spinlock_t *lock, int fd,
	   int *pn_fblkmap, struct fblkmap_t **pfblkmap,
	   int n_memiov, struct p9_zc_io *memiov,
	   size_t count, off_t offset)
{
	/* XXX: need to implement truncating file size for non-block-sized file */
	int mic_id = diod_conf_get_mic_id();
	int rc = nvme_p2p(lock, fd, nvme_cmd_write, mic_id,
			  pn_fblkmap, pfblkmap,
			  n_memiov, memiov,
			  count, offset);
	zcio_dbg("%s:%d n_memiov = %d rc = %d errno = %d\n", __func__, __LINE__,
		 n_memiov, rc, errno);
	return rc;
}
