#!/usr/bin/env python3

NUM_SOCKET = 1
NUM_PHYSICAL_CPU_PER_SOCKET = 4
SMT_LEVEL = 2

OS_CPU_ID = {} # socket_id, physical_cpu_id, smt_id
OS_CPU_ID[0,0,0] = 0
OS_CPU_ID[0,0,1] = 4
OS_CPU_ID[0,1,0] = 1
OS_CPU_ID[0,1,1] = 5
OS_CPU_ID[0,2,0] = 2
OS_CPU_ID[0,2,1] = 6
OS_CPU_ID[0,3,0] = 3
OS_CPU_ID[0,3,1] = 7

