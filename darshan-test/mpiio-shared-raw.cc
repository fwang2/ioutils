#include <cstdlib>
#include <mpi.h>
#include <cstdio>
#include <cstddef> // nullptr
#include <cassert> // assert

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

    // write
    for (int i = 0; i < nints; i++)
        buf[i] = rank;   

    MPI_File_open(MPI_COMM_WORLD, "shared-write.dat",
            MPI_MODE_CREATE|MPI_MODE_WRONLY, MPI_INFO_NULL, &fh);

    MPI_File_seek(fh, rank*bufsize, MPI_SEEK_SET);
    MPI_File_write(fh, buf, nints, MPI_INT, &status);
    MPI_File_close(&fh);

    // reset
    for (int i = 0; i < nints; i++)
        buf[i] = 0;


    // read after write
    MPI_File_open(MPI_COMM_WORLD, "shared-write.dat",
            MPI_MODE_RDONLY, MPI_INFO_NULL, &fh);

    MPI_File_seek(fh, rank*bufsize, MPI_SEEK_SET);
    MPI_File_read(fh, buf, nints, MPI_INT, &status);

    // verify read content
    if (rank == 1) {
        assert(buf[0] == 1);
        //for (int i = 0; i < nints; i++) 
        //    printf("buf[%d] -> %d\n", i, buf[i]);
    }
    MPI_File_close(&fh);
    free(buf);
    MPI_Finalize();
    return 0;
}

