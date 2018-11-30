obj-$(CONFIG_NET_PCN_LINK) += pcnlink.o
pcnlink-objs               := af_pcnlink.o

# settings for ring buffer
ccflags-y    += -I$(src)/../../lib/pci-ring-buffer/include
EXTRA_CFLAGS += -DRING_BUFFER_CONF_KERNEL \
		-DRING_BUFFER_CONF_NO_MMAP \
		-DRING_BUFFER_CONF_NO_DOUBLE_MMAP

# settings for pcnlink
pcnlink-objs               += $(addprefix src/liblink/, \
				netmsg.o \
				link.o)
pcnlink-objs               += $(addprefix src/libpcnlink/, \
				pcnlink.o)

ccflags-y    += -I$(src)/src/include
EXTRA_CFLAGS += -DPCIE_CLOUD_NETWORK_CONF_KERNEL
EXTRA_CFLAGS += -DPCIE_CLOUD_NETWORK_CONF_SHARED_LISTEN
# EXTRA_CFLAGS += -DPCIE_CLOUD_NETWORK_DEBUG
