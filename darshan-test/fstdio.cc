#include <stdio.h>

char buf[] = "Test Message for Darshan I/O";
int main() {
    // 29 chars
    fprintf(stdout, "%s", buf);
}