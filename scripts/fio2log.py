#!/usr/bin/env python3
import os
import sys
import pdb

'''
## FIO: workload=randread numjobs=1 bs=32M
## /root/fio/fio --size=128G --group_reporting --time_based --runtime=30 --directory=/mnt/9p --fi\
lename=fio-test.bin --name=randread-1-32M --numjobs=1 --bs=32M --iodepth=1 --ioengine=psync --dir\
ect=1 --rw=randread
randread-1-32M: (g=0): rw=randread, bs=32M-32M/32M-32M/32M-32M, ioengine=psync, iodepth=1
fio-2.2.9
Starting 1 process

randread-1-32M: (groupid=0, jobs=1): err= 0: pid=4669: Tue Nov 17 04:12:43 2015
  read : io=27616MB, bw=942563KB/s, iops=28, runt= 30002msec
    clat (msec): min=30, max=54, avg=34.69, stdev= 4.83
     lat (msec): min=30, max=54, avg=34.70, stdev= 4.83
    clat percentiles (usec):
     |  1.00th=[30848],  5.00th=[31104], 10.00th=[31104], 20.00th=[31360],
     | 30.00th=[31616], 40.00th=[31616], 50.00th=[31872], 60.00th=[32384],
     | 70.00th=[36096], 80.00th=[39168], 90.00th=[41728], 95.00th=[44288],
     | 99.00th=[50432], 99.50th=[51456], 99.90th=[54016], 99.95th=[54016],
     | 99.99th=[54016]
    bw (KB  /s): min=855880, max=1044398, per=100.00%, avg=944139.19, stdev=61784.74
    lat (msec) : 50=98.84%, 100=1.16%
  cpu          : usr=0.03%, sys=4.77%, ctx=865, majf=0, minf=35
  IO depths    : 1=100.0%, 2=0.0%, 4=0.0%, 8=0.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued    : total=r=863/w=0/d=0, short=r=0/w=0/d=0, drop=r=0/w=0/d=0
     latency   : target=0, window=0, percentile=100.00%, depth=1

Run status group 0 (all jobs):
   READ: io=27616MB, aggrb=942563KB/s, minb=942563KB/s, maxb=942563KB/s, mint=30002msec, maxt=300\
02msec
'''


CUR_DIR        = os.path.abspath(os.path.dirname(__file__))
FIO_START_STR  = "## FIO: "
FIO_RESULT_STR = "runt="
FIO_OP_SEP     = " : "

def emit_fio_result(result):
    tc_name = ":".join([result["workload"],
                        result["bs"], result["numjobs"]])
    print("## %s" % tc_name)
    print("# %s"  % " ".join(result.keys()))
    print("%s\n"   % " ".join( list(map(lambda x: str(convert_unit(x, result[x])), result)) ))

def convert_unit(key, val):
    conv_tbl = {"KB":1024,
                "MB":1024*1024,
                "GB":1024*1024*1024,
                "KB/s":1024,
                "MB/s":1024*1024,
                "GB/s":1024*1024*1024,
                "K":1024,
                "M":1024*1024,
                "G":1024*1024*1024,
                "msec":1}
    iops_tbl = {"K":1000,
                "M":1000*1000,
                "G":1000*1000*1000}
    if key == "iops":
        for unit in iops_tbl:
            if val.endswith(unit):
                val = val[:-len(unit)]
                return float(val) * float(iops_tbl[unit])
    else:
        for unit in conv_tbl:
            if val.endswith(unit):
                val = val[:-len(unit)]
                return float(val) * float(conv_tbl[unit])
    return val

def convert_fio_2_log(fio_file):
    with open(fio_file) as fd_log:
        result = dict()
        for l in fd_log:
            l = l.strip()
            # start of a new fio result
            if l.startswith(FIO_START_STR):
                confs = l[len(FIO_START_STR):].split()
                for conf in confs:
                   kv = conf.split("=")
                   key = kv[0].strip()
                   val = kv[1].strip()
                   result[key] = val
            # end of the fio result
            elif FIO_RESULT_STR in l:
                pos = l.find(FIO_OP_SEP)
                if pos is not -1:
                    l = l[pos + len(FIO_OP_SEP):]
                confs = l.split(",")
                for conf in confs:
                    kv = conf.split("=")
                    key = kv[0].strip()
                    val = kv[1].strip()
                    result[key] = val
                emit_fio_result(result)
                result = dict()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: fio2log.py {fio log file}")
        exit(1)
    fio_file = sys.argv[1]
    convert_fio_2_log(fio_file)
