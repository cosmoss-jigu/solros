#!/usr/bin/env python3
import os
import sys
import socket
import pprint
import importlib
import string

def join(*args):
    return os.path.normpath(os.path.join(*args))

# dir config
ROOT        = join(os.path.dirname(__file__), "..")
TOOLS_ROOT  = join(ROOT, "scripts")
CONFIG_ROOT = join(ROOT, "config")
sys.path.append(CONFIG_ROOT)

# host name
HOSTNAME    = socket.gethostname()

# limits
MAX_MIC                      = 8
MAX_NVME                     = 10

# executable path
BIN_PATH                     = {}
BIN_PATH["fs:release:"]      = join(ROOT, "diod/diod/diod")
BIN_PATH["fs:debug:"]        = join(ROOT, "diod/diod/diod")
BIN_PATH["net:release:sock"] = join(ROOT, "network/build/Release-x86_64/pcnsrv/pcnsrv")
BIN_PATH["net:debug:sock"]   = join(ROOT, "network/build/Debug-x86_64/pcnsrv/pcnsrv")
BIN_PATH["net:release:mtcp"] = join(ROOT, "network/build/Release-x86_64/pcnsrv_mtcp/pcnsrv_mtcp")
BIN_PATH["net:debug:mtcp"]   = join(ROOT, "network/build/Debug-x86_64/pcnsrv_mtcp/pcnsrv_mtcp")

# fs config
FS_CONFIG                    = {}
FS_CONFIG["threads"]         = 5
FS_CONFIG["base_port"]       = 7000
FS_CONFIG["cln_base_port"]   = 3000
FS_CONFIG["port_spacing"]    = 120

# net config
NET_CONFIG                   = {}
NET_CONFIG["threads"]        = 4
NET_CONFIG["base_port"]      = 20000
NET_CONFIG["port_spacing"]   = 1000
NET_CONFIG["rxq_size"]       = (128 * 1024 * 1024)

# topology
# : shoud be overriden by machine specific configuration
CPU_CONFIG                   = {}    # [socketX]     = (cpu id, ...)
MIC_CONFIG                   = {}    # [micX:socket] = socket_id
                                     # [micX:nvme]   = {nvme_dev:mount_point, ...}
                                     # [num]         = # mics

# load machine specific configurations
def is_proper_conf(key):
  return key.isupper() and not key.startswith("_")

def import_confs():
    # default machine-dependent configs
    confs  = ["config-cpu-" + HOSTNAME]
    confs += ["config-mic-" + HOSTNAME]

    # user-provided configs
    if "APP_CONFIG" in os.environ:
        confs += os.environ["APP_CONFIG"].split(",")

    # import existing ones
    for conf in confs:
        pn  = join(CONFIG_ROOT, conf + ".py")
        if not os.path.exists(pn):
            continue
        c = importlib.import_module(conf)
        for (k, v) in c.__dict__.items():
            if is_proper_conf(k):
                globals()[k] = v

def build_cpu_config():
    for s in range(0, NUM_SOCKET):
        cpu_ids = set()
        for c in range(0, NUM_PHYSICAL_CPU_PER_SOCKET):
            for m in range(0, SMT_LEVEL):
                cpu_ids.add( OS_CPU_ID[s, c, m])
        CPU_CONFIG[s] = sorted(cpu_ids)

def print_confs():
  import copy
  conf = {}
  for (k, v) in copy.copy(globals()).items():
    if is_proper_conf(k):
      conf[k] = v
  pprint.pprint(conf)

# access functions
def get_cpu_list(socket):
    return CPU_CONFIG[socket]

def get_cpu_list_str(socket):
    cpu_list = get_cpu_list(socket)
    cpus = str(cpu_list[0])
    for c in cpu_list[1:]:
        cpus += "," + str(c)
    return cpus

def get_mic_id(mic):
    if mic == "host":
        return 0
    elif mic[0:3] == "mic":
        return int(mic[3:]) + 1
    else:
        return -1

def get_mic_conf(key):
    return MIC_CONFIG.get(key, None)

def get_nvme_id(nvme):
    if nvme[0:4] == "nvme":
        return int(nvme[4:])
    else:
        return -1

def get_nvme_path(nvme):
    return "/dev/" + nvme + "n1"

def get_all_nvme_devs():
    all_nvmes = set()
    nmic = MIC_CONFIG.get("num", 0)
    for i in range(0, nmic):
        k = "mic%s:nvme" % i
        nvmes = MIC_CONFIG.get(k, [])
        for n in nvmes:
            nvme_dev = get_nvme_path(n)
            all_nvmes.add(nvme_dev)
    return sorted(all_nvmes)

def get_bin_path(svc, mode, opt):
    return BIN_PATH.get(":".join([svc, mode, opt]), None)

def get_fs_conf(key):
    return FS_CONFIG.get(key, None)

def get_fs_port(mic, nvme):
    mic_id  = get_mic_id(mic)
    nvme_id = get_nvme_id(nvme)
    port    = FS_CONFIG["base_port"]
    port   += mic_id  * MAX_NVME * FS_CONFIG["port_spacing"]
    port   += nvme_id * FS_CONFIG["port_spacing"]
    return port

def get_fs_client_port(nth):
    port    = FS_CONFIG["cln_base_port"]
    port   += nth * FS_CONFIG["port_spacing"]
    return port

def get_net_conf(key):
    return NET_CONFIG.get(key, None)

def get_net_server_port():
    port   = NET_CONFIG["base_port"]
    return port

def get_net_client_port(mic):
    mic_id = get_mic_id(mic)
    port   = get_net_server_port()
    port  += (mic_id + 1) * NET_CONFIG["port_spacing"]
    return port

# load configurations
import_confs()
build_cpu_config()

# test main
if __name__ == "__main__":
    import_confs()
    build_cpu_config()
    print_confs()
