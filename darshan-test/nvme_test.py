import os
import sys
from mpi4py import MPI
comm = MPI.COMM_WORLD
rank = comm.Get_rank()

if len(sys.argv) != 3:
    sys.exit("No enough args")
   
srcfile = os.path.join(sys.argv[1], "test%s.dat" % rank)
destfile = os.path.join(sys.argv[2], "test%s.dat" % rank)


# initialize
with open(srcfile, "w") as f:
    f.write("This is a test from %s\n" % rank )

# copy from source to destination
with open(srcfile, "r") as f1, open(destfile, "w") as f2:
    f2.write(f1.read())


