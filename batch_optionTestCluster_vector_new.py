#!/usr/bin/python3

import sys
import subprocess

list = []

with open(sys.argv[1], 'r') as f:
    for line in f:
        q = line.strip()
        length = len(q.split(' '))
        list.append((q, length))

for (q, length) in list:
    print(q + " " + str(length))
    comm2 = './run_optionTestCluster_vector_new.sh "' + q + '" ' + str(length)
    subprocess.call(comm2, shell = True)
