#ifndef MIC_DMA_LOCAL
#define MIC_DMA_LOCAL

#include <asm/i387.h>
#include <linux/backing-dev.h>
#include <linux/mm.h>

#define VFS_OPT_READ  0x1
#define VFS_OPT_WRITE 0x2

#define kernel_client segment_eq(get_fs(), KERNEL_DS)


/* read optimized */
#define vfs_opt_read_enabled(x)  (bdi_cap_pcache_dma(x) && !kernel_client)
/* write optimized */
#define vfs_opt_write_enabled(x) (bdi_cap_pcache_dma(x) && !kernel_client)

size_t calculate_pages(unsigned long len, unsigned long offset);
#endif
