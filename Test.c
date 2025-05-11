#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define bmp180_IOCTL_MAGIC     'b'
#define bmp180_IOCTL_READ_TEMP _IOR(bmp180_IOCTL_MAGIC, 1, int)
#define bmp180_IOCTL_READ_PRESS _IOR(bmp180_IOCTL_MAGIC, 2, int)

int main() {
    int fd = open("/dev/bmp180", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    int temp = 0, press = 0;

    if (ioctl(fd, bmp180_IOCTL_READ_TEMP, &temp) == -1) {
        perror("ioctl read temp");
        close(fd);
        return 1;
    }

    if (ioctl(fd, bmp180_IOCTL_READ_PRESS, &press) == -1) {
        perror("ioctl read pressure");
        close(fd);
        return 1;
    }

    printf("Nhiệt độ: %.1f °C\n", temp / 10.0);
    printf("Áp suất: %d Pa\n", press);

    close(fd);
    return 0;
}
