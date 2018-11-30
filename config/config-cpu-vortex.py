#!/usr/bin/env python3

NUM_SOCKET = 1
NUM_PHYSICAL_CPU_PER_SOCKET = 18
SMT_LEVEL = 2

OS_CPU_ID = {} # socket_id, physical_cpu_id, smt_id
OS_CPU_ID[0,0,0] = 0   # 0, 0, 0
OS_CPU_ID[0,0,1] = 18   # 0, 0, 1
OS_CPU_ID[0,1,0] = 1   # 0, 1, 0
OS_CPU_ID[0,1,1] = 19   # 0, 1, 1
OS_CPU_ID[0,2,0] = 2   # 0, 2, 0
OS_CPU_ID[0,2,1] = 20   # 0, 2, 1
OS_CPU_ID[0,3,0] = 3   # 0, 3, 0
OS_CPU_ID[0,3,1] = 21   # 0, 3, 1
OS_CPU_ID[0,4,0] = 4   # 0, 4, 0
OS_CPU_ID[0,4,1] = 22   # 0, 4, 1
OS_CPU_ID[0,5,0] = 5   # 0, 8, 0
OS_CPU_ID[0,5,1] = 23   # 0, 8, 1
OS_CPU_ID[0,6,0] = 6   # 0, 9, 0
OS_CPU_ID[0,6,1] = 24   # 0, 9, 1
OS_CPU_ID[0,7,0] = 7   # 0, 10, 0
OS_CPU_ID[0,7,1] = 25   # 0, 10, 1
OS_CPU_ID[0,8,0] = 8   # 0, 11, 0
OS_CPU_ID[0,8,1] = 26   # 0, 11, 1
OS_CPU_ID[0,9,0] = 9   # 0, 16, 0
OS_CPU_ID[0,9,1] = 27   # 0, 16, 1
OS_CPU_ID[0,10,0] = 10   # 0, 17, 0
OS_CPU_ID[0,10,1] = 28   # 0, 17, 1
OS_CPU_ID[0,11,0] = 11   # 0, 18, 0
OS_CPU_ID[0,11,1] = 29   # 0, 18, 1
OS_CPU_ID[0,12,0] = 12   # 0, 19, 0
OS_CPU_ID[0,12,1] = 30   # 0, 19, 1
OS_CPU_ID[0,13,0] = 13   # 0, 20, 0
OS_CPU_ID[0,13,1] = 31   # 0, 20, 1
OS_CPU_ID[0,14,0] = 14   # 0, 24, 0
OS_CPU_ID[0,14,1] = 32   # 0, 24, 1
OS_CPU_ID[0,15,0] = 15   # 0, 25, 0
OS_CPU_ID[0,15,1] = 33   # 0, 25, 1
OS_CPU_ID[0,16,0] = 16   # 0, 26, 0
OS_CPU_ID[0,16,1] = 34   # 0, 26, 1
OS_CPU_ID[0,17,0] = 17   # 0, 27, 0
OS_CPU_ID[0,17,1] = 35   # 0, 27, 1

