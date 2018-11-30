#include "nvme-phi.h"

static unsigned int _count_phi_dma_segments(struct scif_range *pages)
{
	/* intentionally overestimate the number of segments
	 * to make mapping process in a single pass
	 */
	return pages->nr_pages;
}

static void _build_sg_from_scif_range(struct scif_range *pages,
				      struct nvme_iod *iod, int nseg)
{
	struct scatterlist *sg = iod->sg;
	__u64 prev_addr  = (__u64)pages->phys_addr[0] - PAGE_SIZE;
	int i, seg_idx = 0, seg_len = 0;

	sg_init_table(sg, nseg);

	for (i = 0; i < pages->nr_pages; ++i) {
		++seg_len;
		if (pages->phys_addr[i] != (prev_addr + PAGE_SIZE) || /* contigous? */
		    seg_len >= NVME_MAX_DMA_PAGES || /* max dma? */
		    i == (pages->nr_pages - 1)) {    /* last one? */
			/* init sg */
			sg[i].page_link   = 0;
			sg[i].dma_address = pages->phys_addr[i];
			sg[i].length      = PAGE_SIZE * seg_len;
			sg[i].dma_length  = sg[i].length;
			sg[i].offset      = 0;

			/* reset for next segment */
			seg_len = 0;
			++seg_idx;
		}
		prev_addr = (__u64)pages->phys_addr[i];
	}
	WARN_ON(nseg != seg_idx);

	sg_mark_end(&sg[seg_idx - 1]);
	iod->nents = seg_idx;
}

static struct nvme_iod *nvme_map_scif_pages(struct nvme_dev *dev,
					   scif_epd_t epd,
					   int write,
					   u64 addr,
					   unsigned int length,
					   struct scif_range **scif_pages)
{
	struct nvme_iod *iod;
	struct scif_range *pages = NULL;
	int err, nseg;

	/* sanity check */
	if (!length || length > INT_MAX - PAGE_SIZE || (length % PAGE_SIZE))
		return ERR_PTR(-EINVAL);

	/* get xeon phi pages */
	err = scif_get_pages(epd, (off_t)addr, length, scif_pages);
	if (err)
		goto err_out;

	/* alloc iod */
	nseg = _count_phi_dma_segments(*scif_pages);
	pages = *scif_pages;
	iod = nvme_alloc_iod_for_p2p(nseg, length, dev, 0, GFP_KERNEL);
	if (!iod)
		goto put_pages;

	/* fill iod */
	_build_sg_from_scif_range(pages, iod, nseg);
	return iod;

 put_pages:
	scif_put_pages(pages);
 err_out:
	return ERR_PTR(err);
}

static scif_epd_t _convert_to_kernel_epd(struct fd fd)
{
	/* harded coded struct
	 * at pcie-cloud/mpss-modules/micscif/micscif_fd.c */
	struct mic_priv {
		scif_epd_t      epd;
	};

	struct file *filp = fd.file;
	struct mic_priv *priv = (struct mic_priv *)((filp)->private_data);
	scif_epd_t kernel_epd = priv->epd;

	return kernel_epd;
}

static int nvme_submit_p2p_io_scif(struct nvme_ns *ns, scif_epd_t epd,
				    struct nvme_user_io *io)
{
	struct nvme_dev *dev = ns->dev;
	struct nvme_iod *iod;
	struct scif_range *scif_pages;
	struct nvme_command c;
	unsigned length;
	int write, rc;

	length = (io->nblocks + 1) << ns->lba_shift;
	write = io->opcode & 1;

	switch (io->opcode) {
	case nvme_cmd_write:
	case nvme_cmd_read:
	case nvme_cmd_compare:
		iod = nvme_map_scif_pages(dev, epd, write,
					 io->addr, length, &scif_pages);
		break;
	default:
		rc = -EINVAL;
		goto err_out;
	}

	if (IS_ERR(iod)) {
		rc = PTR_ERR(iod);
		goto err_out;
	}

	memset(&c, 0, sizeof(c));
	c.rw.opcode = io->opcode;
	c.rw.flags = io->flags;
	c.rw.nsid = cpu_to_le32(ns->ns_id);
	c.rw.slba = cpu_to_le64(io->slba);
	c.rw.length = cpu_to_le16(io->nblocks);
	c.rw.control = cpu_to_le16(io->control);
	c.rw.dsmgmt = cpu_to_le32(io->dsmgmt);
	c.rw.reftag = cpu_to_le32(io->reftag);
	c.rw.apptag = cpu_to_le16(io->apptag);
	c.rw.appmask = cpu_to_le16(io->appmask);
	c.rw.prp1 = cpu_to_le64(sg_dma_address(iod->sg));
	c.rw.prp2 = cpu_to_le64(iod->first_dma);
	rc = nvme_submit_io_cmd(dev, ns, &c, NULL);

	scif_put_pages(scif_pages);
	nvme_free_iod(dev, iod);
  err_out:
	return rc;
}

int nvme_submit_p2p_iov_scif(struct nvme_ns *ns, struct nvme_user_p2p_iov *p2p_iov)
{
	struct fd epfd = fdget(p2p_iov->cookie);
	scif_epd_t epd = _convert_to_kernel_epd(epfd);
	int n_iov = p2p_iov->n_iov;
	struct nvme_user_io *iov = p2p_iov->iov;
	int rc = 0, i;

	for (i = 0; i < n_iov; ++i) {
		rc = nvme_submit_p2p_io_scif(ns, epd, &iov[i]);
		if (rc)
			goto fdput_out;
	}
  fdput_out:
	fdput(epfd);
	return rc;
}
