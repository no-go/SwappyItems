#include <cstdio>
#include <iostream> // sizeof

// open, close read, write
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h> // lseek()
#include <fcntl.h>

int main(int argc, char** argv) {
    if(argc != 3) {
        printf("Usage: %s /dev/unusedSwapPartition 13.3\n", argv[0]);
        return 1;
    }
    
    int file = open(argv[1], O_RDWR | O_NONBLOCK);
    if (file == -1) {
        perror("open device failed");
        return 2;
    }
    
    double val;
    lseek(file, 115, SEEK_SET);
    read(file, &val, sizeof(double));
    printf("alte Daten: %lf\n", val);
    
    val = atof(argv[2]);
    lseek(file, 115, SEEK_SET);
    write(file, (char *) &val, sizeof(double));
    
    close(file);
    return 0;
}
