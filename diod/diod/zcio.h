#include <pthread.h>

struct fblkmap_t;

int pread_zc (pthread_spinlock_t *lock, int fd,
	  int *pn_fblkmap, struct fblkmap_t **pfblkmap,
	  int n_memiov, struct p9_zc_io *memiov,
	  size_t count, off_t offset);

int pwrite_zc (pthread_spinlock_t *lock, int fd,
	   int *pn_fblkmap, struct fblkmap_t **pfblkmap,
	   int n_memiov, struct p9_zc_io *memiov,
	   size_t count, off_t offset);

static inline
int is_zc_file(Npfid *fid)
{
	/* test if a fid is zero-copy-able or not */
	/* XXX: need to implement */
	return 1;
}
