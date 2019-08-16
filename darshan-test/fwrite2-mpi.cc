#include <stdio.h>
#include <mpi.h>

FILE *fp;
char buf[] = "test message for darshan i/o one";

void do_read()
{
    int c;
    fp = fopen("file.txt", "r");
    while (1)
    {
        c = fgetc(fp);
        if (feof(fp))
        {
            break;
        }
        // printf("%c", c);
    }
    fclose(fp);
}

void do_write(int rank) {
    char 
    fp = fopen("file.txt" , "w" );
    fwrite(buf , sizeof(char), sizeof(buf) , fp );
    fclose(fp);
  
}

int main () {
    int rank;
    MPI_Init(nullptr, nullptr);
    do_write(rank);
    do_read(rank);
    MPI_Finalize();
    return(0);
}
