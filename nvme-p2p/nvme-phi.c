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
#include "nvme-phi.h"

#define STACK_IOV_SIZE 2048
#define convert_uptr_to_kptr(_u, _t) ( \
		(_t)((char *)(_kbase) + ((char *)(_u) - (char *)(_ubase))) )

static int build_p2p_iov(struct nvme_user_p2p_iov __user *up2p_iov,
			 struct nvme_user_p2p_iov *p2p_iov,
			 void *stack_iov, int stack_iov_size)
{
	struct nvme_user_io *iov = NULL;
	void *_ubase, *_kbase;
	int i, rc;

	/* copy p2p_iov */
	if (copy_from_user(p2p_iov, up2p_iov, sizeof(*p2p_iov))) {
		rc = -EFAULT;
		goto err_out;
	}
	if (!p2p_iov->iov_size) {
		rc = -EINVAL;
		goto err_out;
	}

	/* alloc and copy iov */
	iov = stack_iov;
	if (p2p_iov->iov_size > stack_iov_size) {
		iov = kmalloc(p2p_iov->iov_size, GFP_KERNEL);
		if (!iov) {
			rc = -ENOMEM;
			goto err_out;
		}
	}
	if (copy_from_user(iov, p2p_iov->iov, p2p_iov->iov_size)) {
		rc = -EFAULT;
		goto err_out;
	}
	_ubase = p2p_iov->iov;
	_kbase = iov;
	p2p_iov->iov = iov;

	/* reinterprete user pointers to kernel pointers */
	for (i = 0; i < p2p_iov->n_iov; ++i) {
		iov[i].sgl = convert_uptr_to_kptr(
			iov[i].sgl, struct nvme_user_sglist *);
	}
	return 0;

err_out:
	/* clean up */
	if (iov != stack_iov)
		kfree(iov);
	p2p_iov->iov = NULL;
	return rc;
}


static int nvme_submit_p2p_iov(struct nvme_ns *ns,
			       struct nvme_user_p2p_iov __user *up2p_iov)
{
	long __stack_buff[STACK_IOV_SIZE/sizeof(long)];
	void *stack_iov =__stack_buff;
	struct nvme_user_p2p_iov p2p_iov = {.iov = NULL};
	int rc;

	/* get a p2p iovector */
	rc = build_p2p_iov(up2p_iov, &p2p_iov, stack_iov, STACK_IOV_SIZE);
	if (rc)
		goto err_out;

	/* perform nvme io operations */
	switch(p2p_iov.flags) {
	case NVME_P2P_COOKIE_SCIF:
		/* XXX: need to implement user p2p io vector parser */
		rc = nvme_submit_p2p_iov_scif(ns, &p2p_iov);
		break;
	case NVME_P2P_COOKIE_MIC_ID:
		rc = nvme_submit_p2p_iov_mic(ns, &p2p_iov);
		break;
	}
err_out:
	if (p2p_iov.iov != stack_iov)
		kfree(p2p_iov.iov);
	return rc;
}

int nvme_p2p_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd,
		   unsigned long arg)
{
	struct nvme_ns *ns = bdev->bd_disk->private_data;
	int rc;

	switch (cmd) {
	case NVME_IOCTL_SUBMIT_P2P_IOV:
		rc = nvme_submit_p2p_iov(ns, (void __user *)arg);
		if (unlikely(rc))
			p2p_log("[ERROR] %s:%d (task %p) (rc %d)\n",
				__func__, __LINE__, current, rc);
		return rc;
	default:
		return -ENOTTY;
	}
}

