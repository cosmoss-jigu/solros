#!/usr/bin/env python3

MIC_CONFIG                   = {}    # [micX:socket] = socket_id
                                     # [micX:nvme]   = {nvme_dev:mount_point, ...}
MIC_CONFIG["passwd"]         = "phi" # default root password of xeon phi
MIC_CONFIG["num"]            = 4
MIC_CONFIG["mic0:socket"]    = 0
MIC_CONFIG["mic0:nvme"]      = {"nvme1":"nvme1"}
MIC_CONFIG["mic1:socket"]    = 0
MIC_CONFIG["mic1:nvme"]      = {"nvme2":"nvme2"}
MIC_CONFIG["mic2:socket"]    = 1
MIC_CONFIG["mic2:nvme"]      = {"nvme4":"nvme4"}
MIC_CONFIG["mic3:socket"]    = 1
MIC_CONFIG["mic3:nvme"]      = {"nvme5":"nvme5"}
