#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <mpi.h>

char buf[] = "test message for darshan i/o";

int main(int argc, char* argv[]) {
    MPI_File fh;
    int rank;
    MPI_Init(0,0);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_File_open(MPI_COMM_WORLD, "test.out",
            MPI_MODE_CREATE|MPI_MODE_WRONLY,
            MPI_INFO_NULL, &fh);
    if (rank == 0) 
        MPI_File_write(fh, buf, strlen(buf), MPI_CHAR, MPI_STATUS_IGNORE);
    MPI_File_close(&fh);
    MPI_Finalize();
    return 0;
}

