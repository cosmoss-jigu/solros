#ifndef MIC_DMA_H
#define MIC_DMA_H

#ifdef CONFIG_PAGE_CACHE_DMA
struct dma_channel;
struct dma_completion_cb;

struct dma_operations {
	int (*do_dma) (struct dma_channel *chan, int flags, uint64_t src,
		uint64_t dst, size_t len, struct dma_completion_cb *comp_cb);
	int (*poll_dma_completion)(int poll_cookie, struct dma_channel *chan);
	int (*free_dma_channel)(struct dma_channel *chan);
	int (*open_dma_device)(int device_num, uint8_t *mmio_va_base,
		 void **dma_handle);
	void (*close_dma_device)(int device_num, void **dma_handle);
	int (*allocate_dma_channel)(void *dma_handle,
		 struct dma_channel **chan);
	int (*program_descriptors)(struct dma_channel *chan, uint64_t src,
			 uint64_t dst, size_t len);
	int do_dma_polling;	/* value indicates that the dma engine
				* must be polled to ensure completion.
				*/
};

struct file_dma {
	const struct dma_operations *dmaops;
};

void register_dma_for_fast_copy(const struct file_dma *fdma_callback);
void  unregister_dma_for_fast_copy(void);

inline bool dma_copy_begin(struct dma_channel **chan);
inline int dma_copy_end(struct dma_channel *chan, int cookie);
extern int wait_for_dma_finish(struct dma_channel *chan,
		read_descriptor_t *desc);

extern struct page **prepare_for_dma_read(read_descriptor_t *desc,
		unsigned long nrpages, struct dma_channel **chan,
		int *upages_pinned);

extern struct page **prepare_for_dma_write(struct iov_iter *i, loff_t pos,
		unsigned int *nr);

extern unsigned long fast_copy_to_user(read_descriptor_t *desc,
		struct page **upages, unsigned long *uidx, struct page *page,
		unsigned long offset, unsigned long size,
		struct dma_channel *chan);

extern size_t fast_copy_from_user(struct iov_iter *i, loff_t pos,
				struct page **pages, int nr_pages);
#endif

#endif
