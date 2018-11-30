# common args
SRC_ROOT := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
BLD_ROOT := ${SRC_ROOT}/build
TOOLS_ROOT := ${SRC_ROOT}/scripts
NJOB := ${shell nproc}

# host kernel
HOST_KNL_SRC := ${SRC_ROOT}/host-kernel

# mpss
MPSS_SRC := ${SRC_ROOT}/mpss-modules

# icc
ICC_SETUP := ${TOOLS_ROOT}/icc-setup.sh

# xeon phi configurations
PHI_KNL_BLD_ROOT := ${BLD_ROOT}/phi-kernel
PHI_KNL_CONF := ${CURDIR}/config/config-phi-kernel

PHI_NKNL := ${PHI_KNL_BLD_ROOT}/arch/k1om/boot/bzImage
PHI_NSYM := ${PHI_KNL_BLD_ROOT}/System.map

PHI_OKNL := /usr/share/mpss/boot/bzImage-2.6.38+mpss3.5.1-knightscorner
PHI_OSYM := /usr/share/mpss/boot/System.map-2.6.38+mpss3.5.1-knightscorner
PHI_CDIR := /var/mpss/common
PHI_VSN  := 2.6.38.8+mpss3.5.1

PHI_GCC := k1om-mpss-linux-gcc
MPSS_SYSROOT := /opt/mpss/3.5.1/sysroots/k1om-mpss-linux
MPSS_SETUP := /opt/mpss/3.5.1/environment-setup-k1om-mpss-linux

# nvme, 9p, &c
NVME_DEV := /dev/nvme0n1
NVME_MNT := /mnt/9p
DIOD_SERVER := ${SRC_ROOT}/diod/diod/diod