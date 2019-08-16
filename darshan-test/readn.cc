#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <errno.h>
#include <mpi.h>

int fd;

ssize_t read_n(char* buf, int len) {
    ssize_t ret;
    ssize_t nr = 0;
    while (len !=0 && (ret = read(fd, buf, len)) !=0) {
        if (ret == -1) {
            if (errno == EINTR)
                continue;
            perror("read");
            break;
        }
        len -= ret;
        buf += ret;
        nr += ret;
    }
    return nr;
}

int main() 
{
    MPI_Init(nullptr, nullptr);
    char buf[10];
    ssize_t ret;
    fd = open("file.txt", O_RDONLY);
    if (fd == -1) {
        perror("open file.txt");    
        return -1;
    };
    while (read_n(buf, 1) != 0) {
        //printf("%s", buf);
    }
    MPI_Finalize();
}

