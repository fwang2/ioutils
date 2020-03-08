import os
import sys
import subprocess
import shlex
from mpi4py import MPI
comm = MPI.COMM_WORLD
rank = comm.Get_rank()

cmd = "dd if=/dev/urandom of=out%s.dat bs=1024 count=10" % rank
args = shlex.split(cmd)
output = subprocess.Popen(args) 
