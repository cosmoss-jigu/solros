obj-$(CONFIG_NET_9P) := 9pnet.o
obj-$(CONFIG_NET_9P_VIRTIO) += 9pnet_virtio.o
obj-$(CONFIG_NET_9P_RDMA) += 9pnet_rdma.o
obj-$(CONFIG_NET_9P_PRB) += 9pnet_prb.o

9pnet-objs := \
	mod.o \
	client.o \
	error.o \
	util.o \
	protocol.o \
	trans_fd.o \
	trans_common.o \

9pnet_virtio-objs := \
	trans_virtio.o \

9pnet_rdma-objs := \
	trans_rdma.o \

9pnet_prb-objs := \
	trans_prb.o \

ifeq ($(CONFIG_NET_9P_PRB),m)
	ccflags-y += -I$(src)/../../lib/pci-ring-buffer/include
	EXTRA_CFLAGS += -DRING_BUFFER_CONF_KERNEL \
			-DRING_BUFFER_CONF_NO_MMAP \
			-DRING_BUFFER_CONF_NO_DOUBLE_MMAP
endif
