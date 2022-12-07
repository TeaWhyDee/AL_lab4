#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define WR_VALUE _IOW('a','a',int32_t*);
#define RD_VALUE _IOR('a','b',int32_t*);

int main(int argc, char *argv[]) {
    int fd;
    fd = open("/dev/mychardev-0", O_RDWR);

    ioctl(fd, 1, 100000);
}
