#include <cstdlib>
#include <mpi.h>
#include <cstdio>
#include <cstddef> // nullptr
#include <cassert> // assert
#include <unistd.h>

#define FILESIZE (1024*1024)

int main(int argc, char **argv) {
    int *buf, rank, nprocs, nints, bufsize;
    MPI_File fh;
    MPI_Status status;

    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    bufsize = FILESIZE/nprocs;
    buf = (int *) malloc(bufsize);
    assert(buf != nullptr);
    nints = bufsize/sizeof(int);
    
    // initialize buf using rank value
    for (int i=0; i<nints; i++) 
        buf[i] = rank;
   
    if (rank == 1) {
        assert(buf[0] == 1);
    }

    MPI_File_open(MPI_COMM_WORLD, "shared-write.dat",
            MPI_MODE_WRONLY|MPI_MODE_CREATE, MPI_INFO_NULL, &fh);

    MPI_File_seek(fh, rank*bufsize, MPI_SEEK_SET);
    MPI_File_write(fh, buf, nints, MPI_INT, &status);

    sleep(10);
    printf("waking up after 10s\n");
    MPI_File_write(fh, buf, nints, MPI_INT, &status);

    sleep(100);
    printf("waking up after 100s\n");
    MPI_File_write(fh, buf, nints, MPI_INT, &status);
    MPI_File_close(&fh);

    free(buf);
    MPI_Finalize();
    return 0;
}

