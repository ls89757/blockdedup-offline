import os
import sys
import gdb
errno = "0xffffff8b"
possible_addr = []
def set_all_possbile_b():
    for addr in possible_addr:
        gdb.execute(f'break *0x{addr}')
with open('./dump.vmlinux','r') as f:
    line = f.readline()
    while line:
        if errno in line:
            possible_addr.append(line.split(':')[0])
        line = f.readline()

f.close()

print(len(possible_addr))
print("addr[0]:", possible_addr[0])
set_all_possbile_b()
