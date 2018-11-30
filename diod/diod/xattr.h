typedef struct xattr_struct *Xattr;

int xattr_open (Npfid *fid, Npstr *name, u64 *sizep);
int xattr_create (Npfid *fid, Npstr *name, u64 size, u32 flags);
int xattr_close (Npfid *fid);

int xattr_pread (Xattr xattr, void *buf, size_t count, off_t offset);
int xattr_pread_zc (Xattr x, u32 n_iov, struct p9_zc_io *iov,
		    size_t count, off_t offset);
int xattr_pwrite (Xattr xattr, void *buf, size_t count, off_t offset);
int xattr_pwrite_zc (Xattr x, u32 n_iov, struct p9_zc_io *iov,
		     size_t count, off_t offset);


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
