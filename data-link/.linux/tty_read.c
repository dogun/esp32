#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>

// 配置串口参数
int set_serial_params(int fd, int baudrate, int databits, int stopbits, char parity) {
    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        perror("tcgetattr");
        return -1;
    }

    // 设置波特率
    switch (baudrate) {
        case 9600:
            cfsetispeed(&options, B9600);
            cfsetospeed(&options, B9600);
            break;
        case 115200:
            cfsetispeed(&options, B115200);
            cfsetospeed(&options, B115200);
            break;
        default:
            fprintf(stderr, "Unsupported baudrate\n");
            return -1;
    }

    // 关闭硬件流控制
    options.c_cflag &= ~CRTSCTS;

    // 设置数据位
    options.c_cflag &= ~CSIZE;
    switch (databits) {
        case 7:
            options.c_cflag |= CS7;
            break;
        case 8:
            options.c_cflag |= CS8;
            break;
        default:
            fprintf(stderr, "Unsupported data bits\n");
            return -1;
    }

    // 设置校验位
    switch (parity) {
        case 'n':
        case 'N':
            options.c_cflag &= ~PARENB;
            options.c_cflag &= ~PARODD;
            break;
        case 'o':
        case 'O':
            options.c_cflag |= PARENB;
            options.c_cflag |= PARODD;
            break;
        case 'e':
        case 'E':
            options.c_cflag |= PARENB;
            options.c_cflag &= ~PARODD;
            break;
        default:
            fprintf(stderr, "Unsupported parity\n");
            return -1;
    }

    // 设置停止位
    switch (stopbits) {
        case 1:
            options.c_cflag &= ~CSTOPB;
            break;
        case 2:
            options.c_cflag |= CSTOPB;
            break;
        default:
            fprintf(stderr, "Unsupported stop bits\n");
            return -1;
    }

    // 启用接收
    options.c_cflag |= CREAD | CLOCAL;

    // 设置本地模式
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // 设置输入模式
    options.c_iflag &= ~(IXON | IXOFF | IXANY);

    // 设置输出模式
    options.c_oflag &= ~OPOST;

    // 设置读取超时时间
    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = 100;

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        perror("tcsetattr");
        return -1;
    }

    return 0;
}

int main() {
    int fd;
    char buffer[256];
    ssize_t n;

START:
    // 打开串口设备
    fd = open("/dev/ttyS0", O_RDONLY | O_NOCTTY);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    // 配置串口参数
    if (set_serial_params(fd, 9600, 8, 1, 'N') == -1) {
        close(fd);
        return -1;
    }

    // 循环读取串口数据
    while (1) {
		printf("START\n");
		n = read(fd, buffer, 200);
		printf("READ %d\n", n);
        if (n > 0) {
			buffer[n] = 0;
			printf("Received: %s\n", buffer);
        } else if(n == 0) {
			perror("read 0");
			close(fd);
			goto START;
		} else {
            perror("READ ERROR");
            break;
        }
    }

    // 关闭串口设备
    close(fd);

    return 0;
}
