#!/usr/bin/env python3
import os
import sys
import subprocess
import time
import glob
from os.path import join

ROOT  = os.path.abspath(os.path.dirname(__file__))

def sh(cmd, out=None):
    print(";; %s" % cmd)
    p = subprocess.Popen(cmd, shell=True, stdout=out, stderr=out)
    p.wait()
    return p

def list_mic_path():
    return glob.glob("/sys/class/mic/mic*")

def list_mic_name():
    return map(lambda m: m[len("/var/mpss/"):],
               glob.glob("/var/mpss/mic?"))

def __is_mic_ready(micX):
    print("/sys/class/mic/%s/virtblk_file" % micX)
    return os.path.exists("%s/virtblk_file" % micX)

def __wait_for_mic_init(micX):
    while ( not __is_mic_ready(micX) ):
        print(";; Waiting for %s initalization ..." % micX)
        time.sleep(1)
        continue
    print(";; %s initialized" % micX)

def __is_nvme_ready(nvme):
    return os.path.exists("/dev/%s" % nvme)

def __wait_for_nvme_init(nvme):
    while (not __is_nvme_ready(nvme) ):
        print(";; Waiting for %s initalization ..." % nvme)
        time.sleep(1)
        continue
    print(";; %s initialized" % nvme)

def set_virtblk():
    __wait_for_nvme_init("nvme0n1")
    for mic in list_mic_path():
        __wait_for_mic_init(mic)
        sh('sudo sh -c "echo /dev/nvme0n1 > %s/virtblk_file"' % mic)
