#include <stdio.h>

FILE *fp;
char buf[] = "test message for darshan i/o";

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

void do_write() {
    fp = fopen( "file.txt" , "a" );
    fwrite(buf , sizeof(char), sizeof(buf) , fp );
    fclose(fp);
  
}

int main () {

    do_write();
    do_read();
    return(0);
}
