import os
import sys
import subprocess
from mpi4py import MPI
comm = MPI.COMM_WORLD
rank = comm.Get_rank()

subprocess.call(["dd","if=/dev/urandom of=out%s.data bs=1024 count=10"%rank],shell=True)

