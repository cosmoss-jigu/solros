#!/bin/bash

# dir config
SRC_ROOT=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/..
BLD_ROOT=$SRC_ROOT/build
TOOLS_ROOT=$SRC_ROOT/scripts

# my mic id
MY_MIC_ID=0 # host

# nvme, 9p, &c
NVME_DEV=/dev/nvme1n1
NVME_MNT=/mnt/9p
NFS_HOST_DIR=$NVME_MNT
NFS_MIC_DIR=/mnt/nfs

DIOD_SERVER=$SRC_ROOT/diod/diod/diod

P9_TRANS=prb                  # {prb, tcp}
P9_SRV_ADDR_PRB=0             # host in SCIF address
P9_SRV_ADDR_TCP=172.31.1.254  # host in TCP accress
P9_SRV_PORT=564
P9_CACHE=loose
P9_SRV_NWTHREADS=`nproc`
P9_SRV_DBG_ON=1
P9_SRV_DBG_OFF=0
P9_CLN_DBG_ON=4095
P9_CLN_DBG_OFF=0

# mic connection
SSHPASS="sshpass -p phi"
MIC_X="root@mic0"
