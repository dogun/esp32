#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <time.h>

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

FILE* data_fd = NULL;
char curr_file_name[16] = {0};
void write_data(char* data, int len) {
    // 获取当前时间的时间戳
    time_t current_time = time(NULL);
    if (current_time == -1) {
        perror("Failed to get current time");
        return;
    }

    // 将时间戳转换为本地时间结构体
    struct tm *local_time = localtime(&current_time);
    if (local_time == NULL) {
        perror("Failed to convert to local time");
        return;
    }

    // 提取年、月、日信息
    int year = local_time->tm_year + 1900;
    int month = local_time->tm_mon + 1;
    int day = local_time->tm_mday;
	
	char file_name[16] = {0};
	sprintf(file_name, "%04d%02d%02d.data", year, month, day);
	if (strcmp(curr_file_name, file_name) == 0 && data_fd != NULL) {
		//使用当前文件描述符
	}else {
		if (data_fd != NULL) fclose(data_fd);
		strcpy(curr_file_name, file_name);
		data_fd = fopen(file_name, "ab");
		if (data_fd == NULL) {
			perror("open file error");
			return;
		}
	}
	
    // 写入二进制内容
    size_t written = fwrite(data, len, sizeof(char), data_fd);
    if (written != 1) {
        perror("写入失败");
        fclose(data_fd);
        return;
    }
}

int main() {
    int fd;
    char buffer[1024];
    int n;

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
	int index = 0;
    while (1) {
		printf("START\n");
		n = read(fd, buffer + index, 200);
		printf("READ %d\n", n);
        if (n > 0) {
			index += n;
			buffer[index] = 0;
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
	// 关闭数据文件
	fclose(data_fd);

    return 0;
}
