#!/usr/bin/env python3
import os
import sys
import subprocess
import glob
import config
import utils

def install_ko(kbld_dir, tgt_dir, version):
    tgt_base = os.path.join(tgt_dir, "lib/modules", version)
    utils.sh("rm -rf %s" % tgt_base)

    p = utils.sh("cd %s; find . -name *.ko" % kbld_dir, out=subprocess.PIPE)
    for ko in p.stdout.readlines():
        ko = ko.decode("utf-8").strip()
        if ko.startswith("./mpss"):
            tgt_ko = os.path.join(tgt_base, "extra", ko[len("./mpss/"):])
        else:
            tgt_ko = os.path.join(tgt_base, "kernel", ko[len("./"):])
        tgt_ko_dir = '/'.join(tgt_ko.split("/")[0:-1])
        src_ko = os.path.join(kbld_dir, ko[len("./"):])
        utils.sh("mkdir -p %s; cp -f %s %s" % (tgt_ko_dir, src_ko, tgt_ko))

def install_shadow():
    shadow_awk = os.path.join(config.TOOLS_ROOT, "shadow.awk")
    shadow_mic = os.path.join(config.TOOLS_ROOT, "shadow.mic")
    utils.sh("sudo rm %s" % shadow_mic)
    utils.sh("sudo awk -f %s /etc/shadow > %s" % (shadow_awk, shadow_mic))
    utils.sh("sudo chown root:shadow %s" % shadow_mic)
    utils.sh("sudo chmod 640 %s" % shadow_mic)

    for mic in utils.list_mic_name():
        utils.sh("sudo mkdir -p /var/mpss/%s/etc" % mic)
        utils.sh("sudo cp %s /var/mpss/%s/etc/shadow" % (shadow_mic, mic))

def install_mic_scripts():
    scripts = glob.glob(config.TOOLS_ROOT + "/*_mic")
    for mic in utils.list_mic_name():
        utils.sh("sudo mkdir -p /var/mpss/%s/usr/bin/" % mic)
        for script in scripts:
            mic_script = script.split("/")[-1][:-len("_mic")]
            utils.sh("sudo cp %s /var/mpss/%s/usr/bin/%s" %
                     (script, mic, mic_script))
        with open("/var/mpss/%s/usr/bin/config-x.sh" % mic, "w") as f:
            my_mic_id = int(mic[3:]) + 1
            f.write("#!/bin/bash\n")
            f.write("MY_MIC_ID=%s\n" % str(my_mic_id))

if __name__ == "__main__":
    kbld_dir   = sys.argv[1]
    common_dir = sys.argv[2]
    version    = sys.argv[3]

    install_ko(kbld_dir, common_dir, version)
    install_shadow()
    install_mic_scripts()
