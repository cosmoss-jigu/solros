#!/usr/bin/env python3
import os
import sys
import re
import copy
import optparse
import subprocess
import threading
import random
import multiprocessing as mp
import machineconfig as mc

from functools  import wraps
from os.path    import join
from contextlib import contextmanager

ROOT                 = os.path.abspath(os.path.dirname(__file__))
DEBUG_FLAG           = []
MIC_SHUTDOWN_TIMEOUT = 120

class color:
    HEADER    = '\033[95m'
    OKBLUE    = '\033[94m'
    OKGREEN   = '\033[92m'
    WARNING   = '\033[93m'
    FAIL      = '\033[91m'
    ENDC      = '\033[0m'
    BOLD      = '\033[1m'
    UNDERLINE = '\033[4m'

# utils
def get_all_cmds():
    for k, f in globals().items():
        if k.startswith("cmd_"):
            yield (k[4:], f)

def get_cmd(cmd):
    func = "cmd_%s" % cmd
    return globals().get(func, None)

def invoke_cmd(cmd, args):
    func = get_cmd(cmd)
    print("%s>>>>>> %s%s" % (color.HEADER, func.__doc__, color.ENDC))
    return func(args)

def os_system_timeout(cmd, timeout_sec):
    """Execute `cmd` in a subprocess and enforce timeout `timeout_sec` seconds.
    Return subprocess exit code on natural completion of the subprocess.
    Raise an exception if timeout expires before subprocess completes."""
    # http://www.ostricher.com/2015/01/python-subprocess-with-timeout/
    proc = subprocess.Popen(cmd, shell=True)
    proc_thread = threading.Thread(target=proc.communicate)
    proc_thread.start()
    proc_thread.join(timeout_sec)
    if proc_thread.is_alive():
        # Process still running - kill it and raise timeout error
        try:
            proc.kill()
        except OSError as e:
            # The process finished between the `is_alive()` and `kill()`
            return proc.returncode
        # OK, the process was definitely killed
        print_err("Process #%d killed after %f seconds" % (proc.pid, timeout_sec))
    # Process completed naturally - return exit code
    return proc.returncode

def host_sh(cmd, verbose = -1, bg = False, timeout = 0):
    if verbose is -1:
        verbose = opts.verbose
    if not verbose:
        cmd += " > /dev/null 2>&1"
    if bg:
        cmd += " &"
    print("%s[HOST]>>> %s%s" % (color.OKGREEN, cmd, color.ENDC))
    if opts.dryrun:
        return 0
    if timeout == 0:
        rc = os.system(cmd)
    else:
        rc = os_system_timeout(cmd, timeout)
    return rc

def host_sh_pin(socket, cmd, verbose = -1, bg = False):
    tscmd = "sudo taskset --cpu-list %s %s" % (mc.get_cpu_list_str(socket), cmd)
    return host_sh(tscmd, verbose, bg)

def mic_sh(mic, cmd, verbose = -1, bg = False):
    if verbose is -1:
        verbose = opts.verbose
    pw = mc.get_mic_conf("passwd")
    if not verbose:
        cmd += " > /dev/null 2>&1"
    if bg:
        cmd += " &"
    rcmd = ("sshpass -p '%s' ssh root@%s \"%s\"" % (pw, mic, cmd))
    print("%s[%s]>>> %s%s" % (color.OKGREEN, mic, cmd, color.ENDC))
    if opts.dryrun:
        return 0
    return os.system(rcmd)

def broadcast_message(msg):
    if opts.verbose:
        os.system("echo \"%s\" | wall" % msg)

def is_debug_on(machine, module):
    key = "%s:%s" % (machine, module)
    return key in DEBUG_FLAG

def get_build_mod(machine, module):
    return "debug" if is_debug_on(machine, module) else "release"

def print_err(msg):
    print("%s--- %s%s" % (color.FAIL, msg, color.ENDC))

def print_msg(msg):
    print("%s--- %s%s" % (color.OKBLUE, msg, color.ENDC))

# cmds
def __umount_all_nvmes():
    for nvme_dev in mc.get_all_nvme_devs():
        host_sh("sudo umount %s" % nvme_dev)

def __rmmods(machine, modules):
    is_host = True if machine == "host" else False
    for _ in range(0, len(modules)):
        for module in modules:
            if is_host:
                host_sh("sudo rmmod %s" % module)
            else:
                mic_sh(machine, "rmmod %s" % module)

def __is_prog_running(prog):
    return os.system("pgrep %s > /dev/null 2>&1" % prog) == 0

def __show_mic_temp():
    print("%s>>> MIC temperature" % (color.OKBLUE))
    os.system("sudo micsmc -t")
    print("%s" % (color.ENDC))

def cmd_shutdown(args):
    """shutdown all MICs and relevant services"""
    broadcast_message(">>>>>>>>>> All MICs are shutting down... <<<<<<<<<<<")

    host_sh("sudo service mpss stop", timeout = MIC_SHUTDOWN_TIMEOUT)
    # TODO: echo 1 > /sys/bus/pci/drivers/mic/0000:06:00.0/reset
    host_sh("sudo service nfs-kernel-server stop")
    host_sh("sudo pkill -9 diod")
    host_sh("sudo pkill -9 pcnsrv")
    host_sh("sudo pkill -9 pcnsrv_mtcp")
    __umount_all_nvmes()
    __rmmods("host", ["nvme-p2p", "mic"])
    host_sh("sudo ifdown mic0 mic1 mic2 mic3 mic4 mic5 mic6 mic7")
    return 0

def cmd_boot(args):
    """boot all MICs"""
    broadcast_message(">>>>>>>>>> All MICs are booting up... <<<<<<<<<<<")

    host_sh("sudo modprobe mic")
    host_sh("sudo modprobe nvme-p2p")
    host_sh("sudo ifup mic0 mic1 mic2 mic3 mic4 mic5 mic6 mic7")
    host_sh("sudo service nfs-kernel-server restart")
    host_sh("sudo service mpss start")
    __show_mic_temp()
    broadcast_message(">>>>>>>>>> All MICs are ready... <<<<<<<<<<<")
    return 0

def cmd_reboot(args):
    """reboot all MICs"""
    cmd_shutdown(args)
    cmd_boot(args)

def cmd_fs_up(args):
    """mount 9p for all MICs"""
    if __is_prog_running("diod"):
        print_err("Diod server is already running. fs_down first.")
        return -1

    diod     = mc.get_bin_path("fs", get_build_mod("host", "fs"), "")
    threads  = mc.get_fs_conf("threads")
    for m in range(0, mc.get_mic_conf("num")):
        mic      = "mic%s" % m
        mic_id   = mc.get_mic_id(mic)
        mic_dbg  = 4095 if is_debug_on("mic", "fs") else 0
        nvme_mnt = mc.get_mic_conf(mic + ":nvme")
        host_dbg = 1 if is_debug_on("host", "fs") else 0
        mic_sock = mc.get_mic_conf(mic + ":socket")

        if nvme_mnt is None:
            continue

        print_msg("Load kernel modules for %s" % mic)
        mic_sh(mic, "modprobe pci_ring_buffer")
        mic_sh(mic, "modprobe 9pnet debug=%s" % mic_dbg)
        mic_sh(mic, "modprobe 9pnet_prb")
        mic_sh(mic, "modprobe 9p")

        print_msg("Mount NVMes for %s" % mic)
        for (i, nvme) in enumerate(nvme_mnt):
            host_mnt = os.path.join("/mnt", nvme)
            mic_mnt  = os.path.join("/mnt", nvme_mnt[nvme])
            nvme_dev = mc.get_nvme_path(nvme)
            fs_port  = mc.get_fs_port(mic, nvme)
            cln_port = mc.get_fs_client_port(i)

            print_msg("Mount host:%s to %s:%s" % (host_mnt, mic, mic_mnt))
            host_sh("sudo mkdir -p %s" % host_mnt)
            host_sh("sudo mount %s %s" % (nvme_dev, host_mnt))
            host_sh("sudo chmod 777 %s" % host_mnt)
            host_sh_pin(mic_sock,
                        "sudo %s --foreground --debug %d --nwthreads %d --no-auth "
                        "--export %s --nvme %s --mic %s --listen 0.0.0.0:%s"
                        % (diod, host_dbg, threads, host_mnt,
                           nvme_dev, mic_id, fs_port),
                        verbose = True,
                        bg = True)
            mic_sh(mic, "mkdir -p %s" % mic_mnt)
            mic_sh(mic, "mount -t 9p -n 0 %s "
                   "-oaname=%s,version=9p2000.L,uname=root,access=user,"
                   "trans=prb,cache=loose,port=%s,client-id=%d,client-port=%d,debug=%d"
                   % (mic_mnt, host_mnt, fs_port, mic_id, cln_port, mic_dbg))
    return 0

def cmd_fs_down(args):
    """umount 9p for all MICs"""
    for m in range(0, mc.get_mic_conf("num")):
        mic      = "mic%s" % m
        mic_id   = mc.get_mic_id(mic)
        nvme_mnt = mc.get_mic_conf(mic + ":nvme")
        if nvme_mnt is None:
            continue

        for nvme in nvme_mnt:
            mic_mnt  = os.path.join("/mnt", nvme_mnt[nvme])
            mic_sh(mic, "umount %s" % mic_mnt)

        print_msg("Unload kernel modules for %s" % mic)
        __rmmods(mic, ["9p", "9pnet_prb", "9pnet", "pci_ring_buffer"])

    host_sh("sudo pkill -9 diod")
    return 0

def __get_sock_type(args):
    if len(args) <= 1:
        return "sock"
    if args[0] == "sock" or args[0] == "mtcp":
        return args[0]
    return None

def __get_socket_id_for_mic(args):
    if len(args) <= 1 or args[:-1] != "mic":
        return -1
    mic = args[0]
    return mc.get_mic_conf(mic + ":socket")

def cmd_net_up(args):
    """bring up network for MICs: {null, mic0, mic1, mic2, mic3} """ # TODO {sock* | mtcp}"""
    if __is_prog_running("pcnsrv") or __is_prog_running("pcnsrv_mtcp"):
        print_err("pcnsrv server is already running. net_down first.")
        return -1

    print_msg("Launch pcnsrv...")
    sock_t  = "sock" # TODO __get_sock_type(args)
    if sock_t is None:
        __print_usage("Unknown socket type, '%s'" % args[0])
    pcnsrv  = mc.get_bin_path("net", get_build_mod("host", "net"), sock_t)
    srvport = mc.get_net_server_port()
    threads = mc.get_net_conf("threads")
    qsize   = mc.get_net_conf("rxq_size")

    if not opts.gdb:
        host_sh("sudo %s --port %s --ncpu %s --qsize %s" %
                (pcnsrv, srvport, threads, qsize), verbose = True, bg = True)
    else:
        mic_sock = __get_socket_id_for_mic(args)
        if mic_sock < 0:
            # mic id is not specified
            host_sh("sudo gdb --args %s --port %s --ncpu %s --qsize %s" %
                    (pcnsrv, srvport, threads, qsize), bg = False)
        else:
            # mic id is specified
            host_sh_pin(mic_sock,
                        "sudo gdb --args %s --port %s --ncpu %s --qsize %s" %
                        (pcnsrv, srvport, threads, qsize), bg = False)

    for m in range(0, mc.get_mic_conf("num")):
        mic      = "mic%s" % m
        mic_id   = mc.get_mic_id(mic)
        cliport = mc.get_net_client_port(mic)

        print_msg("Connect %s to pcnsrv..." % mic)
        mic_sh(mic, "modprobe pci_ring_buffer")
        mic_sh(mic, "modprobe pcnlink server_id=0 server_port=%s "
               "client_id=%s client_port=%s qsize=%s" %
               (srvport, mic_id, cliport, qsize))
    return 0

def cmd_net_down(args):
    """shutdown network for all MICs"""
    for m in range(0, mc.get_mic_conf("num")):
        mic      = "mic%s" % m
        __rmmods(mic, ["pcnlink", "pci_ring_buffer"])
    host_sh("sudo pkill -9 pcnsrv")
    host_sh("sudo pkill -9 pcnsrv_mtcp")
    return 0

def cmd_show_config(args):
    """show current configuration"""
    mc.print_confs()
    return 0

def cmd_mic_status(args):
    """show current MIC status"""
    os.system("sudo micsmc --all")
    return 0

def __append_if_no(l, f):
    if os.system("sudo grep \"%s\" %s > /dev/null 2>&1" % (l, f)) == 0:
        return
    os.system("sudo sh -c \"echo \\\"%s\\\" >> %s\"" % (l, f))

def cmd_nfs_setup(args):
    """export /var/mpss/nfs to mic*:/mnt/nfs via NFS"""
    host_sh("sudo apt-get install -y nfs-kernel-server")
    host_sh("sudo mkdir -p /var/mpss/nfs")
    host_sh("sudo touch /var/mpss/nfs/hello.from.`hostname`")

    print_msg("Updating /etc/exports...")
    __append_if_no("/var/mpss/nfs      mic0(rw,no_root_squash,no_subtree_check) mic1(rw,no_root_squash,no_subtree_check) mic2(rw,no_root_squash,no_subtree_check) mic3(rw,no_root_squash,no_subtree_check) mic4(rw,no_root_squash,no_subtree_check) mic5(rw,no_root_squash,no_subtree_check) mic6(rw,no_root_squash,no_subtree_check) mic7(rw,no_root_squash,no_subtree_check)",
                   "/etc/exports")

    print_msg("Updating /etc/hosts.allow...")
    for mic_id in range(1, 9):
        __append_if_no("ALL:172.31.%s.1" % mic_id, "/etc/hosts.allow")
    host_sh("sudo exportfs -a")

    print_msg("Updating /var/mpss/common/etc/fstab...")
    host_sh("sudo mkdir -p /var/mpss/common/etc")
    __append_if_no("172.31.1.254:/var/mpss/nfs/     /mnt/nfs        nfs             defaults                1 1", "/var/mpss/common/etc/fstab")

    cmd_boot(args)
    host_sh("sudo micctrl --addnfs=172.31.1.254:/var/mpss/nfs/ --dir=/mnt/nfs")
    cmd_reboot(args)
    return 0

def cmd_swapon(args):
    """Create swap space using virtblk in hostfs"""
    for m in range(0, mc.get_mic_conf("num")):
        mic      = "mic%s" % m
        print_msg("create swap space using host file at %s" % mic)
        host_sh("sudo bash -c 'echo /mnt/nvme1/swapdev > /sys/class/mic/%s/virtblk_file' " % (mic))
        mic_sh(mic, "modprobe mic_virtblk")
        mic_sh(mic, "mkswap /dev/vda")
        mic_sh(mic, "swapon /dev/vda")
        mic_sh(mic, "cat /proc/swaps")
    return 0

# main
def __print_usage(note):
    print_err(note)
    parser.print_help()
    print("Commands:")
    for (cmd, func) in sorted(get_all_cmds()):
        print("  %-15s%s" % (cmd, func.__doc__))
    exit(0)

def __check_debug_opt():
    debug_opt = copy.copy(opts.debug)
    for hm_fn in debug_opt.split(","):
        s = hm_fn.split(":")
        if s[0] != "host" and s[0] != "mic":
            __print_usage("Incorrect machine type, '%s'" % s[0])
        if s[1] != "fs" and s[1] != "net":
            __print_usage("Incorrect module type, '%s'" % s[1])
    return True

def __build_debug_flag():
    debug_opt = copy.copy(opts.debug)
    return debug_opt.split(",")

if __name__ == '__main__':
    usage = "Usage: %prog [options] command [command-specific options]"
    parser = optparse.OptionParser(usage)
    parser.add_option("--verbose", help="print out log", action="store_true")
    parser.add_option("--dryrun",  help="dry run", action="store_true")
    parser.add_option("--debug",   help="debug out for {host:mic}:{fs:net}",
                      default=None)
    parser.add_option("--gdb",     help="attach gdb", action="store_true")
    (opts, args) = parser.parse_args()

    if len(args) == 0:
        __print_usage("Need to specify a command")

    cmd = args[0]
    if get_cmd(cmd) is None:
        __print_usage("Can't find the command, '%s'" % cmd)
    if opts.debug and __check_debug_opt():
        DEBUG_FLAG = __build_debug_flag()

    exit( invoke_cmd(cmd, args[1:]))
