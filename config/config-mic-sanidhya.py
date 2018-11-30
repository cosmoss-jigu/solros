#!/usr/bin/env python3

MIC_CONFIG                   = {}    # [micX:socket] = socket_id
                                     # [micX:nvme]   = {nvme_dev:mount_point, ...}
MIC_CONFIG["passwd"]         = "phi" # default root password of xeon phi
MIC_CONFIG["num"]            = 1
MIC_CONFIG["mic0:socket"]    = 0
MIC_CONFIG["mic0:nvme"]      = {"nvme0":"nvme0"}
