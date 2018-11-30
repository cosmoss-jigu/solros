#ifndef _NVME_PHI_H_
#define _NVME_PHI_H_
#include <linux/nvme.h>
#include <linux/bitops.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kdev_t.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/poison.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/t10-pi.h>
#include <linux/types.h>
#include <scsi/sg.h>
#include <asm-generic/io-64-nonatomic-lo-hi.h>
#include <modules/scif.h>

struct sync_cmd_info {
	struct task_struct *task;
	u32 result;
	int status;
};

struct nvme_io_command {
	struct nvme_iod *iod;
	struct nvme_command c;
	struct request *req;
	struct sync_cmd_info cmdinfo;
};

#define NVME_MAX_DMA_PAGES 32 /* Max DMA size of Intel NVMe DC 3700 is 256 blocks (i.e., 32 pages) */
#define MIC_ID_MAX         9  /* Due to physical memory layout of Xeon Phi,
			       * 8 (+1 for host) is the maximum installable Xeon Phi. */
#define P2P_BATCH_MIC_IO   1  /* batch operations for a mic uio */
#define NVME_MAX_BATCH    128 /* batch operations for a mic uio */

/* debug configuration */
#undef P2P_DEBUG
#undef P2P_DEBUG_FAKE_IO

#ifdef  P2P_DEBUG
#    define p2p_dbg(fmt, ...) printk(KERN_ERR "[NVME-P2P] " fmt, ##__VA_ARGS__)
#else
#    define p2p_dbg(fmt, ...) do {;} while(0)
#endif
#define p2p_log(fmt, ...)     printk(KERN_ERR "[NVME-P2P] " fmt, ##__VA_ARGS__)

/* per-cookie interface */
int nvme_submit_p2p_iov_scif(struct nvme_ns *ns, struct nvme_user_p2p_iov *p2p_iov);
int nvme_submit_p2p_iov_mic(struct nvme_ns *ns, struct nvme_user_p2p_iov *p2p_iov);

/* batch io commands submission */
int nvme_submit_io_cmdv(struct nvme_dev *dev, struct nvme_ns *ns,
			struct nvme_io_command *iocv, int n);
#endif
