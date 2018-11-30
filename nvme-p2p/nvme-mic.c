#include "nvme-phi.h"

/**************************************************************************
                      Xeon Phi memory from host view
		      ==============================

* PCIe BAR address regions
 - determined at mic probing
 - can check using 'lspci -vv'
 - aperture BAR: 0x383c00000000 (64-bit, prefetchable)     [size=8G]
   * physical address: mic_ctx.aper.pa (DMA-able)
   * virtual address:  mic_ctx.aper.va (ioremapped address)
   * address length:  mic_ctx.aper.len
 - MMIO BAR:        0xfbd00000 (64-bit, non-prefetchable) [size=128K]
   * physical address: mic_ctx.mmio.pa
   * virtual address:  mic_ctx.mmio.va (ioremapped address)
   * address length:  mic_ctx.mmio.len

* How to access physical address 0x1dcd5f000 in Xeon Phi
 (1) xeon phi aper.pa base:                            0x383c00000000
 (2) xeon phi aper.va base:                        0xffffc90040000000
 (3) xeon phi physical address:                           0x1dcd5f000
 --------------------------------------------------------------------
 (4) host-accessable physical address: (1) + (3) =     0x383ddcd5f000
 (5) host-accessable virtual  address: (2) + (3) = 0xffffc9021cd5f000
**************************************************************************/

static struct nvme_iod *nvme_map_mic_pages(struct nvme_dev *dev,
					   struct mic_dma_info *mdi,
					   struct nvme_user_sglist *sgl,
					   unsigned int length)
{
	struct nvme_iod *iod;
	struct scatterlist *sg;
	int nsg, i;

	/* alloc iod */
	nsg = sgl->nsg;
	iod = nvme_alloc_iod_for_p2p(nsg, length, dev, 0, GFP_KERNEL);
	if (!iod)
		return ERR_PTR(-ENOMEM);

	/* fill iod */
	sg = iod->sg;
	sg_init_table(sg, nsg);
	for (i = 0; i < nsg; ++i) {
		sg[i].page_link   = 0;
		sg[i].dma_address = scif_get_mic_dma_addr(mdi, sgl->sg[i].addr);
		sg[i].length      = sgl->sg[i].len;
		sg[i].dma_length  = sgl->sg[i].len;
		sg[i].offset      = 0;

		/* log out if enabled */
		p2p_dbg("(sg[%d].dma_addr  0x%llx)  (sg[%d].dma_length  %d)\n",
			i, sg[i].dma_address, i, sg[i].dma_length);
	}
	iod->nents = nsg;

	return iod;
}

static int _nvme_create_io_cmd(struct nvme_ns *ns,
			       struct mic_dma_info *mdi,
			       struct nvme_user_io *io,
			       struct nvme_io_command *ioc)
{
	struct nvme_dev *dev = ns->dev;
	struct nvme_iod *iod;
	struct nvme_command *c;
	unsigned int length;
	int rc = 0;

	memset(ioc, 0, sizeof(*ioc));

	length = (io->nblocks + 1) << ns->lba_shift;
	p2p_dbg("(nblocks %d) (lba_shift %d) (length %d)\n",
		io->nblocks, ns->lba_shift, length);

	switch (io->opcode) {
	case nvme_cmd_write:
	case nvme_cmd_read:
	case nvme_cmd_compare:
		iod = nvme_map_mic_pages(dev, mdi, io->sgl, length);
		ioc->iod = iod;
		break;
	default:
		rc = -EINVAL;
		goto err_out;
	}

	if (IS_ERR(iod)) {
		rc = PTR_ERR(iod);
		goto err_out;
	}

	c = &ioc->c;
	c->rw.opcode = io->opcode;
	c->rw.flags = io->flags;
	c->rw.nsid = cpu_to_le32(ns->ns_id);
	c->rw.slba = cpu_to_le64(io->slba);
	c->rw.length = cpu_to_le16(io->nblocks);
	c->rw.control = cpu_to_le16(io->control);
	c->rw.dsmgmt = cpu_to_le32(io->dsmgmt);
	c->rw.reftag = cpu_to_le32(io->reftag);
	c->rw.apptag = cpu_to_le16(io->apptag);
	c->rw.appmask = cpu_to_le16(io->appmask);

	length = nvme_setup_prps(dev, iod, length, GFP_KERNEL);
	c->rw.prp1 = cpu_to_le64(sg_dma_address(iod->sg));
	c->rw.prp2 = cpu_to_le64(iod->first_dma);

	if (unlikely(length != (io->nblocks + 1) << ns->lba_shift)) {
		rc = -ENOMEM;
		goto err_out;
	}

	return rc;
  err_out:
	p2p_log("[ERROR] %s:%d (task %p) (rc %d)\n",
		__func__, __LINE__, current, rc);
	return rc;
}

static void _nvme_free_io_cmd(struct nvme_ns *ns,
			      struct nvme_io_command *ioc)
{
	if (ioc->req) {
		blk_mq_free_request(ioc->req);
		ioc->req = NULL;
	}
	nvme_free_iod(ns->dev, ioc->iod);
}

static void _nvme_free_io_cmdv(struct nvme_ns *ns,
			       struct nvme_io_command *iocv,
			       int n)
{
	int i;
	for (i = 0; i < n; ++i)
		_nvme_free_io_cmd(ns, &iocv[i]);
}

static
int _nvme_create_io_cmdv(struct nvme_ns *ns,
			 struct mic_dma_info *mdi,
			 struct nvme_user_p2p_iov *p2p_iov,
			 struct nvme_io_command **_iocv)
{
	struct nvme_user_io *iov;
	struct nvme_io_command *iocv = NULL;
	int n_iov, i, rc = 0;

	/* alloc iocv */
	n_iov = p2p_iov->n_iov;
	iocv  = kzalloc(sizeof(*iocv) * n_iov, GFP_KERNEL);
	if (!iocv) {
		rc = -ENOMEM;
		goto err_out;
	}

	/* create iocv */
	iov   = p2p_iov->iov;
	for (i = 0; i < n_iov; ++i) {
		rc = _nvme_create_io_cmd(ns, mdi, &iov[i], &iocv[i]);
		if (rc)
			goto err_out;
	}

	/* pass out the created iocv */
	*_iocv = iocv;
	return rc;
  err_out:
	p2p_log("[ERROR] %s:%d (task %p) (rc %d)\n",
		__func__, __LINE__, current, rc);
	if (iocv)
		_nvme_free_io_cmdv(ns, iocv, n_iov);
	return rc;
}

static int _nvme_submit_p2p_io_mic(struct nvme_ns *ns,
				   struct mic_dma_info *mdi,
				   struct nvme_user_io *io)
{
	struct nvme_dev *dev = ns->dev;
	struct nvme_io_command ioc;
	int rc;

	rc = _nvme_create_io_cmd(ns, mdi, io, &ioc);
	if (rc)
		goto err_out;

#ifndef P2P_DEBUG_FAKE_IO
	rc = nvme_submit_io_cmd(dev, ns, &ioc.c, NULL);
	if (rc)
		goto err_out;
#else
	rc = 0;
#endif
	_nvme_free_io_cmd(ns, &ioc);
	return rc;
  err_out:
	p2p_log("[ERROR] %s:%d (task %p) (rc %d)\n",
		__func__, __LINE__, current, rc);
	return rc;
}

static
int _nvme_submit_p2p_iov_mic_batch(struct nvme_ns *ns,
				   struct mic_dma_info *mdi,
				   struct nvme_user_p2p_iov *p2p_iov)
{
	struct nvme_io_command *iocv = NULL;
	int n_iov, rc = 0;

	/* create iocv */
	n_iov = p2p_iov->n_iov;
	rc = _nvme_create_io_cmdv(ns, mdi, p2p_iov, &iocv);
	if (rc)
		goto err_out;

	/* submit iocv */
#ifndef P2P_DEBUG_FAKE_IO
	rc = nvme_submit_io_cmdv(ns->dev, ns, iocv, n_iov);
	if (rc)
		goto err_out;
#endif

	/* free iocv */
	_nvme_free_io_cmdv(ns, iocv, n_iov);
	return rc;
  err_out:
	p2p_log("[ERROR] %s:%d (task %p) (rc %d)\n",
		__func__, __LINE__, current, rc);
	if (iocv)
		_nvme_free_io_cmdv(ns, iocv, n_iov);
	return rc;
}

static
int _nvme_submit_p2p_iov_mic(struct nvme_ns *ns,
			     struct mic_dma_info *mdi,
			     struct nvme_user_p2p_iov *p2p_iov)
{
	struct nvme_user_io *iov;
	int n_iov, i, rc = 0;

	/* perform nvme uio */
	n_iov = p2p_iov->n_iov;
	iov   = p2p_iov->iov;
	for (i = 0; i < n_iov; ++i) {
		/* perform a single nvme uio */
		rc = _nvme_submit_p2p_io_mic(ns, mdi, &iov[i]);
		if (rc)
			break;
	}
	return rc;
}

static int get_mic_dma_info(int node, struct mic_dma_info **mdi)
{
	static struct mic_dma_info  *mic_dma_info[MIC_ID_MAX];
	static struct mic_dma_info __mic_dma_info[MIC_ID_MAX];
	int rc;

	/* sanity check */
	if (unlikely(node <= 0 || node >= MIC_ID_MAX)) {
		p2p_log("[ERROR] Invalid MIC ID: %d\n", node);
		return -EINVAL;
	}

	/* initialize mic dma info if needed */
	if (unlikely(mic_dma_info[node] == NULL)) {
		/* Since scif_mic_dma_info() is idempotent,
		 * we don't need locking. */
		rc = scif_mic_dma_info(node, &__mic_dma_info[node]);
		if (unlikely(rc))
			return rc;

		/* Since x86 is a TSO (total store order) architecture,
		 * we don't need wmb() here. */

		/* Since pointer assignment operation is atomic
		 * and idempotent, we don't need locking. */
		mic_dma_info[node] = &__mic_dma_info[node];

		p2p_dbg("mic%d (va %p) (pa 0x%llx) (len %lld)\n",
			node, mic_dma_info[node]->va,
			mic_dma_info[node]->pa, mic_dma_info[node]->len);
	}

	/* set output */
	*mdi = mic_dma_info[node];
	return rc;
}

static void dump_p2p_iov(struct nvme_user_p2p_iov *p2p_iov)
{
	struct nvme_user_io *iov;
	struct nvme_user_sglist *sgl;
	int n_iov, n_sg, i, j;

	n_iov = p2p_iov->n_iov;
	iov   = p2p_iov->iov;

	p2p_dbg("p2p_iov: (n_iov  %d) (iov_size  %d)\n",
		p2p_iov->n_iov, p2p_iov->iov_size);

	for (i = 0; i < n_iov; ++i) {
		p2p_dbg("(io[%d]->opcode 0x%llx) "
			"(io[%d]->slba 0x%llx) "
			"(io[%d]->nblocks %d)\n",
			i, iov[i].opcode,
			i, iov[i].slba,
			i, iov[i].nblocks);
		sgl  = iov[i].sgl;
		n_sg = sgl->nsg;
		for (j = 0; j < n_sg; ++j) {
			p2p_dbg("    (sg[%d]->addr 0x%llx) (sg[%d]->len %d)\n",
				j, sgl->sg[j].addr, j, sgl->sg[j].len);
		}
	}
}

int nvme_submit_p2p_iov_mic(struct nvme_ns *ns, struct nvme_user_p2p_iov *p2p_iov)
{
	struct mic_dma_info *mdi;
	int node, rc = 0;

	/* We assume that each
	 * - NVME_MAX_DMA_PAGES needs to be handled at 9p server.
	 * - reduce # of nvme uio operations by changing
	 *   (addr, length) -> (sg addr, n_sg)
	 */

	/* get mic dma info from mic id */
	node = (int)p2p_iov->cookie;
	rc = get_mic_dma_info(node, &mdi);
	if (unlikely(rc))
		goto err_out;

#ifdef P2P_DEBUG
	/* dump p2p_iov */
	dump_p2p_iov(p2p_iov);
#endif

	/* perform nvme uio */
#if P2P_BATCH_MIC_IO
	rc = _nvme_submit_p2p_iov_mic_batch(ns, mdi, p2p_iov);
#else
	rc = _nvme_submit_p2p_iov_mic(ns, mdi, p2p_iov);
#endif
	if (unlikely(rc))
		goto err_out;
	return rc;
  err_out:
	p2p_log("[ERROR] %s:%d (task %p) (rc %d)\n",
		__func__, __LINE__, current, rc);
	return rc;
}
