#ifndef _FBLKMAP_H_
#define _FBLKMAP_H_

#include <linux/fiemap.h>

/* get block map flags */
#define FBM_FLAG_XATTR 0x01
#define FBM_FLAG_ALIGN 0x02
#define FBM_FLAG_SYNC  0x04
#define FBM_FLAG_ALLOC 0x08

/* block map info */
struct fblkmap_t {
	__u64  logical_addr;   /* logical file-offset in bytes */
	__u64  physical_addr;  /* physical disk-offset in bytes */
	__u32  len;
	__u32  flags;
};

int
fbm_get_block_map(int fd, int flags, off_t start, size_t len,
		  struct fblkmap_t **pfblkmap, int *ptotal_exts);

int
fbm_find_block_mapping(struct fblkmap_t *fblkmap, int n_exts, __u64 start);

int
fbm_find_block_mapping_force(int fd, int flags,
			     struct fblkmap_t **pfblkmap, int *ptotal_exts,
			     off_t start, size_t len);

void
fbm_print_block_map(FILE *fd, int count, struct fblkmap_t *fblkmap);

#endif /* _FBLKMAP_H_ */

