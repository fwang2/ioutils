#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

char buf[] = "test message for darshan i/o";
int fd;

void do_write() {

    fd = open(".", O_CREAT|O_WRONLY|O_APPEND);
    write(fd, buf); 
    close(fd);
}

int main () {

    do_write();
    return(0);
}

