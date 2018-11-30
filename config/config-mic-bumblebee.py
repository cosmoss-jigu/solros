#!/usr/bin/env python3

MIC_CONFIG                   = {}    # [micX:socket] = socket_id
                                     # [micX:nvme]   = {nvme_dev:mount_point, ...}
MIC_CONFIG["passwd"]         = "phi" # default root password of xeon phi
MIC_CONFIG["num"]            = 2
MIC_CONFIG["mic0:socket"]    = 0   # XXX: not sure
MIC_CONFIG["mic0:nvme"]      = {"nvme1":"nvme1"}
MIC_CONFIG["mic1:socket"]    = 1   # XXX: not sure
MIC_CONFIG["mic1:nvme"]      = {"nvme0":"nvme0"}

