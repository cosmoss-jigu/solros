#!/usr/bin/env python3

NUM_SOCKET = 2
NUM_PHYSICAL_CPU_PER_SOCKET = 6
SMT_LEVEL = 2

OS_CPU_ID = {} # socket_id, physical_cpu_id, smt_id
OS_CPU_ID[0,0,0] = 0   # 0, 0, 0
OS_CPU_ID[0,0,1] = 12   # 0, 0, 1
OS_CPU_ID[0,1,0] = 1   # 0, 1, 0
OS_CPU_ID[0,1,1] = 13   # 0, 1, 1
OS_CPU_ID[0,2,0] = 2   # 0, 2, 0
OS_CPU_ID[0,2,1] = 14   # 0, 2, 1
OS_CPU_ID[0,3,0] = 3   # 0, 3, 0
OS_CPU_ID[0,3,1] = 15   # 0, 3, 1
OS_CPU_ID[0,4,0] = 4   # 0, 4, 0
OS_CPU_ID[0,4,1] = 16   # 0, 4, 1
OS_CPU_ID[0,5,0] = 5   # 0, 5, 0
OS_CPU_ID[0,5,1] = 17   # 0, 5, 1
OS_CPU_ID[1,0,0] = 6   # 1, 0, 0
OS_CPU_ID[1,0,1] = 18   # 1, 0, 1
OS_CPU_ID[1,1,0] = 7   # 1, 1, 0
OS_CPU_ID[1,1,1] = 19   # 1, 1, 1
OS_CPU_ID[1,2,0] = 8   # 1, 2, 0
OS_CPU_ID[1,2,1] = 20   # 1, 2, 1
OS_CPU_ID[1,3,0] = 9   # 1, 3, 0
OS_CPU_ID[1,3,1] = 21   # 1, 3, 1
OS_CPU_ID[1,4,0] = 10   # 1, 4, 0
OS_CPU_ID[1,4,1] = 22   # 1, 4, 1
OS_CPU_ID[1,5,0] = 11   # 1, 5, 0
OS_CPU_ID[1,5,1] = 23   # 1, 5, 1

