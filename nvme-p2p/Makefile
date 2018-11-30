LEVEL = ..
include $(LEVEL)/Makefile.inc

#
# By default, the build is done against the running linux kernel source.
# To build against a different kernel source tree, set SYSSRC:
#
#    make SYSSRC=/path/to/kernel/source

ifdef SYSSRC
 KERNEL_SOURCES	 = $(SYSSRC)
else
 KERNEL_UNAME	:= $(shell uname -r)
 KERNEL_SOURCES	 = /lib/modules/$(KERNEL_UNAME)/build
endif

default: modules
install: modules_install
	cp nvme-p2p-blacklist.conf /etc/modprobe.d
	cp 80-nvme-p2p.rules /etc/udev/rules.d
	update-initramfs -u -k $(KERNEL_UNAME)

.PHONY: install default

%::
	$(MAKE) -C $(KERNEL_SOURCES) \
        KBUILD_EXTRA_SYMBOLS=$(MPSS_SRC)/Module.symvers \
	    HOST_KNL_SRC=$(HOST_KNL_SRC) \
        M=$$PWD $@
