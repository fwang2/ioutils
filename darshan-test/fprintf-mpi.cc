#include <stdio.h>
#include <mpi.h>

char buf[] = "Test Message for Darshan I/O";
int main() {
    // 29 chars
    MPI_Init(nullptr, nullptr);
    fprintf(stdout, "%s\n", buf);
    MPI_Finalize();
}